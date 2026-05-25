#include <M5Cardputer.h>
#include <Wire.h>
#include <atomic>
#include <freertos/task.h>

#include "config.h"
#include "device_list.h"
#include "oui_lookup.h"
#include "oui_db_spiffs.h"
#include "ota_update.h"
#include "gps.h"
#include "alert.h"
#include "ble_scanner.h"
#include "wifi_scanner.h"
#include "spi_bus.h"
#include "lora_tx.h"
#include "sd_logger.h"
#include "display.h"

// PI4IOE5V6408 I2C port expander on the Cap LoRa 1262 Cap-Bus
#define PI4IOE_ADDR  0x43
#define PI4IOE_SDA   8
#define PI4IOE_SCL   9

// ── Inter-core flags ──────────────────────────────────────────────────────────
static std::atomic<bool> s_force_sweep{false};
static std::atomic<bool> s_scan_paused{false};

// ── Queues ────────────────────────────────────────────────────────────────────
static QueueHandle_t s_alert_queue;   // VendorId byte  → Core 1 alert buzzer
static QueueHandle_t s_lora_queue;    // LoraPacket     → lora_task TX
static QueueHandle_t s_sd_queue;      // Device copy    → Core 1 SD logger

// ── New-device callback (called under device_list mutex, Core 0) ─────────────
// Must be non-blocking. Feeds both the alert buzzer queue and the LoRa TX queue.
static void on_new_device(const Device* d) {
    uint8_t v = (uint8_t)d->vendor;
    xQueueSend(s_alert_queue, &v, 0);

    LoraPacket pkt = lora_pkt_from_device(d);
    xQueueSend(s_lora_queue, &pkt, 0);

    xQueueSend(s_sd_queue, d, 0);   // copy Device by value into queue
}

// ── Scan task — Core 0, priority 5 ───────────────────────────────────────────
static void scan_task(void* /*arg*/) {
    ble_scanner_init();
    wifi_scanner_init();
    ble_scanner_start();

    uint32_t last_sweep_ms = 0;

    for (;;) {
        if (s_scan_paused.load()) {
            ble_scanner_pause();
            while (s_scan_paused.load()) vTaskDelay(pdMS_TO_TICKS(100));
            ble_scanner_resume();
            last_sweep_ms = millis();
            continue;
        }

        uint32_t now = millis();
        bool do_sweep = s_force_sweep.exchange(false) ||
                        (now - last_sweep_ms >= WIFI_SWEEP_INTERVAL_MS);

        if (do_sweep) {
            ble_scanner_pause();
            wifi_scanner_sweep();
            ble_scanner_resume();
            last_sweep_ms = millis();
        }

        wifi_scanner_process();
        device_list_evict(millis());
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ── LoRa TX task — Core 0, priority 2 (below scan_task) ─────────────────────
// Blocks on the queue; each packet is a blocking ~500 ms LoRa transmit.
// Running on Core 0 keeps the UI (Core 1) fully responsive during TX.
static void lora_task(void* /*arg*/) {
    LoraPacket pkt;
    for (;;) {
        if (xQueueReceive(s_lora_queue, &pkt, portMAX_DELAY) == pdTRUE) {
            lora_tx_send(&pkt);
        }
    }
}

// ── Board init ───────────────────────────────────────────────────────────────
static void cap_lora_init() {
    Wire1.begin(PI4IOE_SDA, PI4IOE_SCL);
    Wire1.setClock(400000);
    // P0 starts LOW; lora_tx_init() will set it HIGH when the radio is ready
    Wire1.beginTransmission(PI4IOE_ADDR);
    Wire1.write(0x01);
    Wire1.write(0x00);
    Wire1.endTransmission();
}

// ── Arduino entry points ──────────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    cap_lora_init();
    spi_bus_init();   // HSPI shared by LoRa + TF card
    spiffs_init();
    spiffs_load_oui_db();
    gps_init();
    alert_init();
    device_list_init();
    display_init();

    lora_tx_init();   // sets PI4IOE P0 HIGH, calls radio.begin() on g_hspi
    sd_logger_init(); // mounts TF card on g_hspi, opens GPX + CSV files

    s_alert_queue = xQueueCreate(8,  sizeof(uint8_t));
    s_lora_queue  = xQueueCreate(16, sizeof(LoraPacket));
    s_sd_queue    = xQueueCreate(8,  sizeof(Device));

    device_list_set_new_device_cb(on_new_device);

    xTaskCreatePinnedToCore(scan_task, "scan", 8192, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(lora_task, "lora", 4096, nullptr, 2, nullptr, 0);
}

void loop() {
    M5Cardputer.update();
    gps_update();
    alert_tick();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

        for (char c : keys.word) {
            switch (c) {
                case 'q': case 'Q':
                    M5Cardputer.Display.fillScreen(TFT_BLACK);
                    M5Cardputer.Display.setTextDatum(TC_DATUM);
                    M5Cardputer.Display.drawString("Goodbye", 120, 60);
                    M5Cardputer.Display.setTextDatum(TL_DATUM);
                    delay(1000);
                    esp_restart();
                    break;

                case 'w': case 'W':
                    s_force_sweep.store(true);
                    break;

                case 'u': case 'U':
                    s_scan_paused.store(true);
                    delay(250);
                    ota_update_run();
                    s_scan_paused.store(false);
                    break;
            }
        }
    }

    uint8_t vendor_byte;
    while (xQueueReceive(s_alert_queue, &vendor_byte, 0) == pdTRUE) {
        alert_new_device((VendorId)vendor_byte);
    }

    Device sd_dev;
    while (xQueueReceive(s_sd_queue, &sd_dev, 0) == pdTRUE) {
        sd_logger_log_device(&sd_dev);
    }
    sd_logger_tick();

    display_render();
    delay(50);
}

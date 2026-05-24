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
#include "display.h"

// PI4IOE5V6408 I2C port expander on the Cap LoRa 1262 Cap-Bus
#define PI4IOE_ADDR  0x43
#define PI4IOE_SDA   8
#define PI4IOE_SCL   9

// ── Inter-core flags ──────────────────────────────────────────────────────────
static std::atomic<bool> s_force_sweep{false};  // W key → immediate WiFi sweep
static std::atomic<bool> s_scan_paused{false};  // U key → pause scan during OTA

// ── Alert queue (Core 0 → Core 1) ────────────────────────────────────────────
static QueueHandle_t s_alert_queue;

// ── Scan task (Core 0) ────────────────────────────────────────────────────────
static void scan_task(void* /*arg*/) {
    ble_scanner_init();
    wifi_scanner_init();
    ble_scanner_start();

    uint32_t last_sweep_ms = 0;

    for (;;) {
        // Yield when OTA update is running on Core 1
        if (s_scan_paused.load()) {
            ble_scanner_pause();
            while (s_scan_paused.load()) vTaskDelay(pdMS_TO_TICKS(100));
            ble_scanner_resume();
            last_sweep_ms = millis();   // reset sweep timer after pause
            continue;
        }

        uint32_t now = millis();
        bool do_sweep = s_force_sweep.exchange(false) ||
                        (now - last_sweep_ms >= WIFI_SWEEP_INTERVAL_MS);

        if (do_sweep) {
            ble_scanner_pause();
            wifi_scanner_sweep();        // blocks ~4.5 s (3 ch × 1.5 s)
            ble_scanner_resume();
            last_sweep_ms = millis();
        }

        wifi_scanner_process();
        device_list_evict(millis());
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ── Board init ───────────────────────────────────────────────────────────────
static void cap_lora_init() {
    Wire1.begin(PI4IOE_SDA, PI4IOE_SCL);
    Wire1.setClock(400000);
    Wire1.beginTransmission(PI4IOE_ADDR);
    Wire1.write(0x01);  // Output register
    Wire1.write(0x00);  // All outputs LOW — LoRa TX not used in v0.1
    Wire1.endTransmission();
}

// ── Arduino entry points ──────────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    cap_lora_init();
    spiffs_init();
    spiffs_load_oui_db();   // hot-load from SPIFFS if available; no-op if absent
    gps_init();
    alert_init();
    device_list_init();
    display_init();

    s_alert_queue = xQueueCreate(8, sizeof(uint8_t));

    xTaskCreatePinnedToCore(scan_task, "scan", 8192, nullptr, 5, nullptr, 0);
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
                    M5Cardputer.Display.drawCentreString("Goodbye", 120, 60, 2);
                    delay(1000);
                    esp_restart();
                    break;

                case 'w': case 'W':
                    s_force_sweep.store(true);
                    break;

                case 'u': case 'U':
                    s_scan_paused.store(true);
                    delay(250);                  // let scan_task see the flag
                    ota_update_run();            // blocks until done
                    s_scan_paused.store(false);
                    break;
            }
        }
    }

    // Fire buzzer alerts for new devices found on Core 0
    uint8_t vendor_byte;
    while (xQueueReceive(s_alert_queue, &vendor_byte, 0) == pdTRUE) {
        alert_new_device((VendorId)vendor_byte);
    }

    display_render();
    delay(50);
}

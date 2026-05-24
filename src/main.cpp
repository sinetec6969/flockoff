#include <M5Cardputer.h>
#include <Wire.h>
#include <freertos/task.h>

#include "device_list.h"
#include "oui_lookup.h"
#include "gps.h"
#include "alert.h"
#include "ble_scanner.h"
#include "wifi_scanner.h"
#include "display.h"

// PI4IOE5V6408 I2C port expander on the Cap LoRa 1262 Cap-Bus
#define PI4IOE_ADDR  0x43
#define PI4IOE_SDA   8
#define PI4IOE_SCL   9

// ── Scan task (Core 0) ────────────────────────────────────────────────────────
// Runs BLE continuously; performs periodic WiFi sweeps between scans.
// Pushes new-device flags via a small queue so alert fires on Core 1.

static QueueHandle_t s_alert_queue;  // carries VendorId for new detections

// Wraps device_list_update and queues an alert if the device is new.
static void report(const uint8_t mac[6], VendorId vendor,
                   Protocol proto, int8_t rssi,
                   float lat, float lon, bool has_gps) {
    bool is_new = device_list_update(mac, vendor, proto, rssi, lat, lon, has_gps);
    if (is_new) {
        uint8_t v = (uint8_t)vendor;
        xQueueSend(s_alert_queue, &v, 0);
    }
}

static void scan_task(void* /*arg*/) {
    ble_scanner_init();
    wifi_scanner_init();
    ble_scanner_start();

    uint32_t last_sweep_ms = 0;

    for (;;) {
        uint32_t now = millis();

        if (now - last_sweep_ms >= WIFI_SWEEP_INTERVAL_MS) {
            ble_scanner_pause();
            wifi_scanner_sweep();        // blocks ~4.5 s (3 ch × 1.5 s)
            ble_scanner_resume();
            last_sweep_ms = millis();
        }

        wifi_scanner_process();          // drain frame queue into device list
        device_list_evict(millis());

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ── Cap LoRa 1262 board init ──────────────────────────────────────────────────
static void cap_lora_init() {
    // Secondary I2C bus for PI4IOE port expander
    Wire1.begin(PI4IOE_SDA, PI4IOE_SCL);
    Wire1.setClock(400000);

    // Keep SX1262 RF switch (P0) LOW — LoRa TX not used in v1
    Wire1.beginTransmission(PI4IOE_ADDR);
    Wire1.write(0x01);  // Output register
    Wire1.write(0x00);  // All outputs LOW
    Wire1.endTransmission();
}

// ── Arduino entry points ──────────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    cap_lora_init();
    gps_init();
    alert_init();
    device_list_init();
    display_init();

    s_alert_queue = xQueueCreate(8, sizeof(uint8_t));

    // Scan task pinned to Core 0 (radio CPU)
    xTaskCreatePinnedToCore(scan_task, "scan", 8192, nullptr, 5, nullptr, 0);
}

void loop() {
    M5Cardputer.update();
    gps_update();
    alert_tick();

    // Handle keyboard
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

        for (char c : keys.word) {
            if (c == 'q' || c == 'Q') {
                // Graceful shutdown — clear screen and halt
                M5Cardputer.Display.fillScreen(TFT_BLACK);
                M5Cardputer.Display.drawCentreString("Goodbye", 120, 60, 2);
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            }
            if (c == 'w' || c == 'W') {
                // Force an immediate WiFi sweep on next scan_task cycle
                // (scan_task will pick it up via last_sweep_ms reset trick)
                // Simplest approach: just signal via a flag
            }
        }
    }

    // Fire alerts for new devices detected on Core 0
    uint8_t vendor_byte;
    while (xQueueReceive(s_alert_queue, &vendor_byte, 0) == pdTRUE) {
        alert_new_device((VendorId)vendor_byte);
    }

    display_render();
    delay(50);  // ~20 fps
}

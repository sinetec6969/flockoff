#include "wifi_scanner.h"
#include "oui_lookup.h"
#include "device_list.h"
#include "gps.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/queue.h>

// 802.11 frame offsets
#define FC_OFFSET     0
#define ADDR1_OFFSET  4
#define ADDR2_OFFSET  10
#define ADDR3_OFFSET  16
#define FIXED_HDR_LEN 24

// Management frame: type=00 subtype=0100 → FC byte0 = 0x40
#define FC0_PROBE_REQ 0x40

typedef struct {
    uint8_t addr1[6];
    uint8_t addr2[6];
    int8_t  rssi;
    bool    is_wildcard_probe;
} WifiFrame;

static QueueHandle_t s_queue;

static void IRAM_ATTR promiscuous_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;

    const wifi_promiscuous_pkt_t* pkt =
        reinterpret_cast<const wifi_promiscuous_pkt_t*>(buf);
    uint16_t len = pkt->rx_ctrl.sig_len;

    if (len < FIXED_HDR_LEN + 2) return;  // need header + at least one IE tag+len

    const uint8_t* p = pkt->payload;
    WifiFrame frame{};
    memcpy(frame.addr1, p + ADDR1_OFFSET, 6);
    memcpy(frame.addr2, p + ADDR2_OFFSET, 6);
    frame.rssi = (int8_t)pkt->rx_ctrl.rssi;

    // Wildcard probe request: FC byte0 == 0x40, SSID IE (tag 0) length 0
    if (p[FC_OFFSET] == FC0_PROBE_REQ) {
        frame.is_wildcard_probe = (p[FIXED_HDR_LEN] == 0 && p[FIXED_HDR_LEN + 1] == 0);
    }

    xQueueSendFromISR(s_queue, &frame, nullptr);
}

void wifi_scanner_init() {
    s_queue = xQueueCreate(64, sizeof(WifiFrame));
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
}

void wifi_scanner_sweep() {
    static const uint8_t CHANNELS[] = {1, 6, 11};

    esp_wifi_set_promiscuous_rx_cb(promiscuous_cb);
    esp_wifi_set_promiscuous(true);

    for (uint8_t ch : CHANNELS) {
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        vTaskDelay(pdMS_TO_TICKS(WIFI_DWELL_MS));
    }

    esp_wifi_set_promiscuous(false);
}

void wifi_scanner_process() {
    WifiFrame frame;
    while (xQueueReceive(s_queue, &frame, 0) == pdTRUE) {
        GpsFix fix = gps_get_fix();

        // addr2 = transmitter (camera sending probe / data)
        bool a2_la = (frame.addr2[0] & 0x02) != 0;
        bool a2_mc = (frame.addr2[0] & 0x01) != 0;
        if (!a2_mc) {
            VendorId v = oui_lookup(frame.addr2);

            // Tighten Flock match: prefer wildcard-probe confirmation
            if (v == VENDOR_FLOCK && !frame.is_wildcard_probe && a2_la) {
                v = VENDOR_UNKNOWN;  // LA non-probe — too ambiguous
            }
            if (v != VENDOR_UNKNOWN) {
                device_list_update(frame.addr2, v, PROTO_WIFI, frame.rssi,
                                   fix.lat, fix.lon, fix.valid);
            }
        }

        // addr1 = receiver — catches sleeping cameras named as destination
        bool a1_mc = (frame.addr1[0] & 0x01) != 0;
        if (!a1_mc) {
            VendorId v = oui_lookup(frame.addr1);
            if (v != VENDOR_UNKNOWN) {
                device_list_update(frame.addr1, v, PROTO_WIFI, frame.rssi,
                                   fix.lat, fix.lon, fix.valid);
            }
        }
    }
}

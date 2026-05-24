#include "display.h"
#include "device_list.h"
#include "oui_lookup.h"
#include "gps.h"
#include <M5Cardputer.h>

// Screen: 240 × 135 px (landscape)
#define SCR_W  240
#define SCR_H  135
#define LH     10    // line height px (font size 1 = 6×8, +2 leading)

// Colours (RGB565)
#define C_BG      0x0000
#define C_BAR     0x2104  // dark grey
#define C_WHITE   0xFFFF
#define C_GREEN   0x07E0
#define C_GREY    0x8410
#define C_YELLOW  0xFFE0

static M5Canvas s_canvas(&M5Cardputer.Display);

// Scan-mode indicator blinks each render cycle
static bool s_blink = false;

static void draw_mac(int x, int y, const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    s_canvas.drawString(buf, x, y);
}

void display_init() {
    M5Cardputer.Display.setRotation(1);   // landscape
    s_canvas.createSprite(SCR_W, SCR_H);
    s_canvas.setTextSize(1);
    s_canvas.setTextDatum(TL_DATUM);
}

void display_render() {
    s_blink = !s_blink;
    s_canvas.fillSprite(C_BG);

    // ── Status bar ─────────────────────────────────────────────
    s_canvas.fillRect(0, 0, SCR_W, LH + 2, C_BAR);
    s_canvas.setTextColor(C_WHITE, C_BAR);
    s_canvas.drawString("FlockOff", 2, 2);

    // BLE indicator (always-on)
    s_canvas.setTextColor(s_blink ? C_GREEN : C_GREY, C_BAR);
    s_canvas.drawString("BLE", SCR_W - 54, 2);

    // GPS fix indicator
    GpsFix fix = gps_get_fix();
    s_canvas.setTextColor(fix.valid ? C_GREEN : C_YELLOW, C_BAR);
    s_canvas.drawString(fix.valid ? "GPS" : "---", SCR_W - 28, 2);

    // ── Device list ─────────────────────────────────────────────
    int y = LH + 4;
    int max_rows = (SCR_H - LH - 4 - LH - 2) / (LH * 2);  // 2 lines per device

    device_list_lock();
    int total = device_list_active_count();
    int shown = 0;
    for (int i = 0; i < total && shown < max_rows; i++) {
        const Device* d = device_list_get(i);
        if (!d) break;

        uint16_t vc = vendor_color(d->vendor);

        // Line 1: vendor  MAC  proto
        s_canvas.setTextColor(vc, C_BG);
        char l1[32];
        snprintf(l1, sizeof(l1), "%-5s %02X:%02X:%02X:%02X:%02X:%02X %s",
                 vendor_name(d->vendor),
                 d->mac[0], d->mac[1], d->mac[2],
                 d->mac[3], d->mac[4], d->mac[5],
                 d->proto == PROTO_BLE ? "BLE" : "WiFi");
        s_canvas.drawString(l1, 2, y);
        y += LH;

        // Line 2: RSSI  GPS coords or "no fix"
        s_canvas.setTextColor(C_GREY, C_BG);
        char l2[32];
        if (d->has_gps) {
            snprintf(l2, sizeof(l2), "  %4ddBm %.4f %.4f",
                     d->rssi, d->lat, d->lon);
        } else {
            snprintf(l2, sizeof(l2), "  %4ddBm  (no fix)", d->rssi);
        }
        s_canvas.drawString(l2, 2, y);
        y += LH;
        shown++;
    }
    device_list_unlock();

    if (total == 0) {
        s_canvas.setTextColor(C_GREY, C_BG);
        s_canvas.drawString("Scanning...", 2, y);
    }

    // ── Footer ──────────────────────────────────────────────────
    int fy = SCR_H - LH - 1;
    s_canvas.fillRect(0, fy, SCR_W, LH + 1, C_BAR);
    s_canvas.setTextColor(C_GREY, C_BAR);
    char footer[40];
    snprintf(footer, sizeof(footer), "[Q]uit  [W]ifi sweep  %d detected", total);
    s_canvas.drawString(footer, 2, fy + 1);

    s_canvas.pushSprite(0, 0);
}

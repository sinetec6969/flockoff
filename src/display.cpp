#include "display.h"
#include "device_list.h"
#include "oui_lookup.h"
#include "gps.h"
#include "sd_logger.h"
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
#define C_RED     0xF800
#define C_GREY    0x8410
#define C_YELLOW  0xFFE0

static M5Canvas s_canvas(&M5Cardputer.Display);

#define MAX_ROWS 5   // max devices visible at once (2 lines each × 10 px = 100 px)

static bool s_blink      = false;
static int  s_selected   = 0;   // index of highlighted device (0-based, active list)
static int  s_scroll_top = 0;   // first visible device index

static void draw_mac(int x, int y, const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    s_canvas.drawString(buf, x, y);
}

// Returns '^' (approaching), 'v' (receding), or '=' (stable).
// Compares the newest blended RSSI to the reading 4 updates ago.
// Needs at least 4 readings; returns '=' until then.
static char trend_char(const Device* d) {
    if (d->hist_count < 4) return '=';
    uint8_t newest = (d->hist_idx + 7) & 7;          // (idx-1+8)%8
    uint8_t old    = (d->hist_idx + 4) & 7;          // (idx-4+8)%8
    int delta = (int)d->rssi_hist[newest] - (int)d->rssi_hist[old];
    if (delta >=  3) return '^';
    if (delta <= -3) return 'v';
    return '=';
}

static uint16_t trend_color(char t) {
    if (t == '^') return C_GREEN;
    if (t == 'v') return C_RED;
    return C_GREY;
}

void display_nav(int delta) {
    int total = device_list_active_count();   // acquires + releases lock internally
    if (total == 0) { s_selected = 0; s_scroll_top = 0; return; }
    s_selected += delta;
    if (s_selected < 0)       s_selected = 0;
    if (s_selected >= total)  s_selected = total - 1;
    if (s_selected < s_scroll_top)               s_scroll_top = s_selected;
    if (s_selected >= s_scroll_top + MAX_ROWS)   s_scroll_top = s_selected - MAX_ROWS + 1;
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

    // SD indicator
    s_canvas.setTextColor(sd_logger_available() ? C_GREEN : C_GREY, C_BAR);
    s_canvas.drawString("SD", SCR_W - 80, 2);

    // BLE indicator (blinks to show scanner is alive)
    s_canvas.setTextColor(s_blink ? C_GREEN : C_GREY, C_BAR);
    s_canvas.drawString("BLE", SCR_W - 54, 2);

    // GPS fix indicator
    GpsFix fix = gps_get_fix();
    s_canvas.setTextColor(fix.valid ? C_GREEN : C_YELLOW, C_BAR);
    s_canvas.drawString(fix.valid ? "GPS" : "---", SCR_W - 28, 2);

    // ── Device list ─────────────────────────────────────────────
    // active_count() acquires its own lock — call it BEFORE device_list_lock()
    // to avoid a double-take on the non-recursive mutex.
    int total = device_list_active_count();

    // Clamp selection in case devices were evicted since last nav
    if (total == 0) {
        s_selected = 0; s_scroll_top = 0;
    } else {
        if (s_selected   >= total)              s_selected   = total - 1;
        if (s_scroll_top >  total - MAX_ROWS)   s_scroll_top = (total > MAX_ROWS) ? total - MAX_ROWS : 0;
    }

    int y = LH + 4;
    device_list_lock();
    for (int row = 0; row < MAX_ROWS; row++) {
        int idx = s_scroll_top + row;
        if (idx >= total) break;
        const Device* d = device_list_get(idx);
        if (!d) break;

        bool sel = (idx == s_selected);
        uint16_t vc = vendor_color(d->vendor);

        // Selection cursor: vendor-coloured ">" on left edge
        if (sel) {
            s_canvas.setTextColor(vc, C_BG);
            s_canvas.drawString(">", 0, y);
        }

        // Line 1: vendor  MAC  proto
        s_canvas.setTextColor(vc, C_BG);
        char l1[32];
        snprintf(l1, sizeof(l1), "%-5s %02X:%02X:%02X:%02X:%02X:%02X %s",
                 vendor_name(d->vendor),
                 d->mac[0], d->mac[1], d->mac[2],
                 d->mac[3], d->mac[4], d->mac[5],
                 d->proto == PROTO_BLE ? "BLE" : "WiFi");
        s_canvas.drawString(l1, 7, y);
        y += LH;

        // Line 2: RSSI + trend arrow + GPS coords / no-fix
        char t = trend_char(d);
        s_canvas.setTextColor(trend_color(t), C_BG);
        char l2[36];
        if (d->has_gps) {
            snprintf(l2, sizeof(l2), "  %4ddBm%c %.4f %.4f",
                     d->rssi, t, d->lat, d->lon);
        } else {
            snprintf(l2, sizeof(l2), "  %4ddBm%c (no fix)", d->rssi, t);
        }
        s_canvas.drawString(l2, 7, y);
        y += LH;
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
    char footer[48];
    if (total > 0) {
        snprintf(footer, sizeof(footer), "[Q]uit [W]ifi [J/K]nav  %d/%d",
                 s_selected + 1, total);
    } else {
        snprintf(footer, sizeof(footer), "[Q]uit [W]ifi sweep");
    }
    s_canvas.drawString(footer, 2, fy + 1);

    s_canvas.pushSprite(0, 0);
}

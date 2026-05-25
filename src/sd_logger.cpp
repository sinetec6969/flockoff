#include "sd_logger.h"
#include "spi_bus.h"
#include "gps.h"
#include "config.h"
#include <SD.h>
#include <Arduino.h>

// ── GPX streaming helpers ─────────────────────────────────────────────────────
// We keep the file valid at all times by overwriting the footer on each append:
//   [HEADER]  <trkpt …/> <trkpt …/>  [FOOTER]
//              ↑ s_footer_pos ──────────────────^
// Seek back to s_footer_pos, write the new point, then rewrite FOOTER + flush.
static const char GPX_HEADER[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<gpx version=\"1.1\" creator=\"FlockOff v0.3\"\n"
    "     xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
    "<trk><name>FlockOff</name><trkseg>\n";
static const char GPX_FOOTER[] = "</trkseg></trk>\n</gpx>\n";

static bool     s_ok          = false;
static File     s_track;
static File     s_detect;
static uint32_t s_last_ms     = 0;
static uint32_t s_footer_pos  = 0;

static void trkpt_append(float lat, float lon, float alt_m, const char* desc) {
    s_track.seek(s_footer_pos, SeekSet);
    if (desc && desc[0]) {
        s_track.printf(
            "<trkpt lat=\"%.6f\" lon=\"%.6f\"><ele>%.1f</ele>"
            "<desc>%s</desc></trkpt>\n",
            lat, lon, alt_m, desc);
    } else {
        s_track.printf(
            "<trkpt lat=\"%.6f\" lon=\"%.6f\"><ele>%.1f</ele></trkpt>\n",
            lat, lon, alt_m);
    }
    s_footer_pos = s_track.position();
    s_track.print(GPX_FOOTER);
    s_track.flush();
}

// ── Public API ────────────────────────────────────────────────────────────────
bool sd_logger_init() {
    // g_hspi already started by spi_bus_init(); SD.begin() won't re-init it.
    if (!SD.begin(SD_CS_PIN, g_hspi, 25000000)) {
        Serial.println("[SD] mount failed — no card?");
        return false;
    }

    SD.mkdir("/flockoff");

    char path[44];
    uint32_t stamp = millis();
    snprintf(path, sizeof(path), "/flockoff/trk%lu.gpx", stamp);
    s_track = SD.open(path, FILE_WRITE);
    if (!s_track) { Serial.println("[SD] track open failed"); return false; }
    s_track.print(GPX_HEADER);
    s_footer_pos = s_track.position();
    s_track.print(GPX_FOOTER);
    s_track.flush();

    snprintf(path, sizeof(path), "/flockoff/det%lu.csv", stamp);
    s_detect = SD.open(path, FILE_WRITE);
    if (!s_detect) {
        s_track.close();
        Serial.println("[SD] detect open failed");
        return false;
    }
    s_detect.print("uptime_ms,vendor,mac,proto,rssi,lat,lon\n");
    s_detect.flush();

    s_ok = true;
    Serial.printf("[SD] logging → /flockoff/trk%lu.gpx  det%lu.csv\n", stamp, stamp);
    return true;
}

void sd_logger_tick() {
    if (!s_ok) return;
    uint32_t now = millis();
    if (now - s_last_ms < SD_TRACK_INTERVAL_MS) return;
    s_last_ms = now;

    GpsFix fix = gps_get_fix();
    if (!fix.valid) return;
    trkpt_append(fix.lat, fix.lon, fix.alt_m, nullptr);
}

void sd_logger_log_device(const Device* d) {
    if (!s_ok) return;

    const char* vname = (d->vendor == VENDOR_RING)  ? "RING"  :
                        (d->vendor == VENDOR_FLOCK) ? "FLOCK" : "UNK";
    const char* pname = (d->proto  == PROTO_BLE)    ? "BLE"   : "WIFI";

    s_detect.printf("%lu,%s,%02X:%02X:%02X:%02X:%02X:%02X,%s,%d,%.6f,%.6f\n",
        millis(), vname,
        d->mac[0], d->mac[1], d->mac[2],
        d->mac[3], d->mac[4], d->mac[5],
        pname, (int)d->rssi,
        d->has_gps ? d->lat : 0.0f,
        d->has_gps ? d->lon : 0.0f);
    s_detect.flush();

    if (d->has_gps) {
        char desc[48];
        snprintf(desc, sizeof(desc), "%s %02X:%02X:%02X %s %ddBm",
                 vname, d->mac[0], d->mac[1], d->mac[2],
                 pname, (int)d->rssi);
        trkpt_append(d->lat, d->lon, 0.0f, desc);
    }
}

bool sd_logger_available() { return s_ok; }

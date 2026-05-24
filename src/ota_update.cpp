#include "ota_update.h"
#include "oui_db_spiffs.h"
#include "config.h"
#include <M5Cardputer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>

// ── helpers ──────────────────────────────────────────────────────────────────

static void status(const char* line1, const char* line2 = "") {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextDatum(TC_DATUM);
    M5Cardputer.Display.drawString(line1, 120, 50);
    if (line2[0]) M5Cardputer.Display.drawString(line2, 120, 64);
    M5Cardputer.Display.setTextDatum(TL_DATUM);
}

// Blocking keyboard line input — shows prompt and echoes typed chars.
// Enter confirms; Del backspaces; ESC returns empty string.
static String kbd_readline(const char* prompt, bool mask = false) {
    String buf;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5Cardputer.Display.setTextDatum(TC_DATUM);
    M5Cardputer.Display.drawString(prompt, 120, 40);
    M5Cardputer.Display.setTextDatum(TL_DATUM);
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    auto redraw = [&]() {
        M5Cardputer.Display.fillRect(0, 58, 240, 14, TFT_BLACK);
        String stars;
        for (size_t i = 0; i < buf.length(); i++) stars += mask ? '*' : buf[i];
        stars += '_';
        M5Cardputer.Display.setTextDatum(TC_DATUM);
        M5Cardputer.Display.drawString(stars, 120, 60);
        M5Cardputer.Display.setTextDatum(TL_DATUM);
    };
    redraw();

    for (;;) {
        M5Cardputer.update();
        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            delay(20);
            continue;
        }
        auto ks = M5Cardputer.Keyboard.keysState();
        if (ks.enter) break;
        if (ks.del && buf.length() > 0) buf.remove(buf.length() - 1);
        for (char c : ks.word) if (c >= 32) buf += c;
        redraw();
        delay(30);
    }
    return buf;
}

// ── credential storage (NVS) ─────────────────────────────────────────────────

static bool load_creds(String& ssid, String& pass) {
    Preferences p;
    p.begin(WIFI_CREDS_NS, true);
    ssid = p.getString("ssid", "");
    pass = p.getString("pass", "");
    p.end();
    return ssid.length() > 0;
}

static void save_creds(const String& ssid, const String& pass) {
    Preferences p;
    p.begin(WIFI_CREDS_NS, false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();
}

// ── main update flow ─────────────────────────────────────────────────────────

void ota_update_run() {
    String ssid, pass;

    if (!load_creds(ssid, pass)) {
        // First time — prompt for credentials and save
        ssid = kbd_readline("WiFi SSID:");
        if (ssid.length() == 0) { status("Update cancelled."); delay(1500); return; }
        pass = kbd_readline("WiFi Password:", true);
        save_creds(ssid, pass);
    }

    // Connect
    status("Connecting to WiFi...", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
        delay(300);
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(true);
        status("WiFi failed.", "Check credentials (hold U to re-enter).");
        delay(3000);
        return;
    }

    // Fetch
    status("Fetching OUI DB...", OUI_DB_URL);

    WiFiClientSecure sc;
    sc.setInsecure();           // skip cert validation — acceptable for this tool
    HTTPClient http;
    http.begin(sc, OUI_DB_URL);
    http.setTimeout(HTTP_TIMEOUT_MS);
    int code = http.GET();

    if (code != 200) {
        http.end();
        WiFi.disconnect(true);
        char msg[32];
        snprintf(msg, sizeof(msg), "HTTP %d", code);
        status("Fetch failed.", msg);
        delay(3000);
        return;
    }

    // Stream directly to SPIFFS
    status("Writing to SPIFFS...");
    File f = SPIFFS.open(OUI_SPIFFS_PATH, "w");
    if (!f) {
        http.end();
        WiFi.disconnect(true);
        status("SPIFFS write failed.");
        delay(3000);
        return;
    }
    int written = http.writeToStream(&f);
    f.close();
    http.end();
    WiFi.disconnect(true);

    if (written <= 0) {
        status("Stream error.", "File may be corrupt.");
        delay(3000);
        return;
    }

    // Hot-reload
    status("Reloading OUI table...");
    bool ok = spiffs_load_oui_db();
    char result[48];
    if (ok) snprintf(result, sizeof(result), "%d entries loaded", spiffs_loaded_count());
    else    snprintf(result, sizeof(result), "Parse failed — old table kept");
    status(ok ? "Update complete!" : "Update failed.", result);
    delay(2500);
}

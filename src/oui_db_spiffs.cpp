#include "oui_db_spiffs.h"
#include "oui_lookup.h"
#include "config.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <stdlib.h>

static int s_loaded = 0;

static uint32_t pack_prefix(const char* p) {
    unsigned b0 = 0, b1 = 0, b2 = 0;
    sscanf(p, "%x:%x:%x", &b0, &b1, &b2);
    return ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | b2;
}

static int entry_cmp(const void* a, const void* b) {
    uint32_t ka = ((const OuiEntry*)a)->oui;
    uint32_t kb = ((const OuiEntry*)b)->oui;
    return (ka > kb) - (ka < kb);
}

bool spiffs_init() {
    return SPIFFS.begin(true);  // true = format on first mount
}

bool spiffs_load_oui_db() {
    if (!SPIFFS.exists(OUI_SPIFFS_PATH)) return false;

    File f = SPIFFS.open(OUI_SPIFFS_PATH, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    JsonArray ring_oui  = doc["vendors"]["ring"]["oui"].as<JsonArray>();
    JsonArray flock_oui = doc["vendors"]["flock"]["oui"].as<JsonArray>();

    // Count allocations needed
    size_t normal_n = 0, la_n = 0;
    for (JsonObject o : ring_oui)  normal_n++;
    for (JsonObject o : flock_oui) {
        if (o["locally_administered"] | false) la_n++;
        else                                   normal_n++;
    }

    if (normal_n == 0) return false;

    OuiEntry* table = (OuiEntry*)malloc(normal_n * sizeof(OuiEntry));
    uint32_t* la    = la_n ? (uint32_t*)malloc(la_n * sizeof(uint32_t)) : nullptr;
    if (!table) return false;

    size_t ni = 0, li = 0;
    for (JsonObject o : ring_oui) {
        table[ni++] = {pack_prefix(o["prefix"]), VENDOR_RING};
    }
    for (JsonObject o : flock_oui) {
        uint32_t key = pack_prefix(o["prefix"]);
        if (o["locally_administered"] | false) {
            if (la) la[li++] = key;
        } else {
            table[ni++] = {key, VENDOR_FLOCK};
        }
    }

    qsort(table, ni, sizeof(OuiEntry), entry_cmp);
    oui_set_dynamic(table, ni, la, li);
    s_loaded = (int)ni;
    return true;
}

int spiffs_loaded_count() { return s_loaded; }

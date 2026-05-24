#include "oui_lookup.h"
#include <stdlib.h>

static OuiEntry* s_dyn       = nullptr;
static size_t    s_dyn_n     = 0;
static uint32_t* s_dyn_la    = nullptr;
static size_t    s_dyn_la_n  = 0;

void oui_set_dynamic(OuiEntry* table, size_t count,
                     uint32_t* la_table, size_t la_count) {
    free(s_dyn);
    free(s_dyn_la);
    s_dyn      = table;
    s_dyn_n    = count;
    s_dyn_la   = la_table;
    s_dyn_la_n = la_count;
}

static VendorId bsearch_table(const OuiEntry* tbl, size_t n, uint32_t key) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (tbl[mid].oui == key) return tbl[mid].vendor;
        if (tbl[mid].oui  < key) lo = mid + 1;
        else                     hi = mid;
    }
    return VENDOR_UNKNOWN;
}

VendorId oui_lookup(const uint8_t mac[6]) {
    bool la = (mac[0] & 0x02) != 0;
    uint32_t key = ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | mac[2];

    if (la) {
        // Check dynamic LA list first, then static
        for (size_t i = 0; i < s_dyn_la_n; i++)
            if (s_dyn_la[i] == key) return VENDOR_FLOCK;
        for (size_t i = 0; i < OUI_LA_FLOCK_SIZE; i++)
            if (OUI_LA_FLOCK[i] == key) return VENDOR_FLOCK;
        return VENDOR_UNKNOWN;
    }

    // Dynamic table takes priority (it's the freshest data)
    if (s_dyn && s_dyn_n) {
        VendorId v = bsearch_table(s_dyn, s_dyn_n, key);
        if (v != VENDOR_UNKNOWN) return v;
    }

    return bsearch_table(OUI_TABLE, OUI_TABLE_SIZE, key);
}

const char* vendor_name(VendorId v) {
    switch (v) {
        case VENDOR_RING:  return "RING";
        case VENDOR_FLOCK: return "FLOCK";
        default:           return "?";
    }
}

uint16_t vendor_color(VendorId v) {
    // RGB565
    switch (v) {
        case VENDOR_RING:  return 0xF800; // red
        case VENDOR_FLOCK: return 0xFD20; // orange
        default:           return 0xFFFF; // white
    }
}

#include "oui_lookup.h"

VendorId oui_lookup(const uint8_t mac[6]) {
    bool la = (mac[0] & 0x02) != 0;
    uint32_t key = ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | mac[2];

    if (la) {
        for (size_t i = 0; i < OUI_LA_FLOCK_SIZE; i++) {
            if (OUI_LA_FLOCK[i] == key) return VENDOR_FLOCK;
        }
        return VENDOR_UNKNOWN;
    }

    // Binary search on sorted OUI_TABLE
    size_t lo = 0, hi = OUI_TABLE_SIZE;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (OUI_TABLE[mid].oui == key) return OUI_TABLE[mid].vendor;
        if (OUI_TABLE[mid].oui  < key) lo = mid + 1;
        else                           hi = mid;
    }
    return VENDOR_UNKNOWN;
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

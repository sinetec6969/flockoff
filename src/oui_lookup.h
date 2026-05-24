#pragma once
#include "oui_db.h"

// Look up the first 3 bytes of mac[6] (canonical big-endian order).
// Returns VENDOR_UNKNOWN for unrecognized OUIs and non-Flock LA MACs.
VendorId oui_lookup(const uint8_t mac[6]);

const char* vendor_name(VendorId v);
uint16_t    vendor_color(VendorId v);  // RGB565 for display

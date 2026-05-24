#pragma once
#include "oui_db.h"

// Look up the first 3 bytes of mac[6] (canonical big-endian order).
// Returns VENDOR_UNKNOWN for unrecognized OUIs and non-Flock LA MACs.
VendorId oui_lookup(const uint8_t mac[6]);

const char* vendor_name(VendorId v);
uint16_t    vendor_color(VendorId v);  // RGB565 for display

// Install a heap-allocated dynamic table that overrides the static one.
// Takes ownership of both pointers (freed on next call).
void oui_set_dynamic(OuiEntry* table, size_t count,
                     uint32_t* la_table, size_t la_count);

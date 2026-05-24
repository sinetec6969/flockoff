#pragma once

// Mount SPIFFS and, if /oui.json exists, parse it and install a dynamic OUI
// table that overrides the baked-in static table from oui_db.h.
// Returns true when the dynamic table was successfully installed.
bool spiffs_init();
bool spiffs_load_oui_db();
int  spiffs_loaded_count();   // entries in the currently-active dynamic table

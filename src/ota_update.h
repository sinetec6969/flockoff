#pragma once

// Run the interactive OUI-database update flow.
// Reads WiFi credentials from NVS; prompts user to enter them via keyboard
// if not yet stored. Connects, fetches OUI_DB_URL, writes to SPIFFS,
// then calls spiffs_load_oui_db() to hot-reload the table.
// Blocks until complete (or failed). Draws status to the display directly.
void ota_update_run();

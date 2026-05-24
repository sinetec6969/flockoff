#pragma once

// OUI database fetched from this URL when the user triggers an update.
// Points to the raw oui.json in the main branch — update the URL if you fork.
#define OUI_DB_URL       "https://raw.githubusercontent.com/sinetec6969/flockoff/main/data/oui.json"

#define OUI_SPIFFS_PATH  "/oui.json"
#define WIFI_CREDS_NS    "flockoff"      // NVS namespace for stored credentials
#define WIFI_TIMEOUT_MS  15000u
#define HTTP_TIMEOUT_MS  20000u

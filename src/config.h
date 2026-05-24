#pragma once

// OUI database fetched from this URL when the user triggers an update.
// Points to the raw oui.json in the main branch — update the URL if you fork.
#define OUI_DB_URL       "https://raw.githubusercontent.com/sinetec6969/flockoff/main/data/oui.json"

#define OUI_SPIFFS_PATH  "/oui.json"
#define WIFI_CREDS_NS    "flockoff"      // NVS namespace for stored credentials
#define WIFI_TIMEOUT_MS  15000u
#define HTTP_TIMEOUT_MS  20000u

// ── LoRa telemetry (SX1262, Cap LoRa 1262) ───────────────────────────────────
// Change LORA_FREQ for your region: 915.0 (US), 868.0 (EU), 433.0 (Asia)
#define LORA_FREQ       915.0f
#define LORA_BW         125.0f   // kHz
#define LORA_SF         9        // SF9 — good range, ~500 ms airtime for 22 B
#define LORA_CR         7        // coding rate 4/7
#define LORA_PWR        22       // dBm (SX1262 max)
#define LORA_PREAMBLE   8
#define LORA_SYNC_WORD  0x34     // private network
#define LORA_TCXO_V     1.8f    // TCXO reference voltage on Cap LoRa 1262

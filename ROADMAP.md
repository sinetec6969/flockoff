# FlockOff — Project Roadmap

Target hardware: M5Stack Cardputer ADV (ESP32-S3) + Cap LoRa 1262 (SX1262 + ATGM336H GPS)

---

## Decisions (locked)

| Question | Decision |
|----------|----------|
| SD card logging | No — v2 stretch goal |
| Scanning strategy | BLE-priority: BLE always-on, periodic WiFi channel sweeps |
| OUI database storage | SPIFFS — runtime-updatable over WiFi |
| GPS | v1 — via Cap LoRa 1262 (ATGM336H GNSS, UART) |
| Framework | Arduino + PlatformIO + M5Cardputer library |

---

## Hardware Reference

### Cardputer ADV (ESP32-S3FN8)
- BLE 5.0 (BLE only — no Bluetooth Classic)
- WiFi 2.4 GHz 802.11b/g/n (supports promiscuous/monitor mode via `esp_wifi.h`)
- Display: 1.14" TFT 135×240
- Keyboard: QWERTY mini

### Cap LoRa 1262 (via Cap-Bus)

**LoRa — SX1262, SPI:**
| Signal | GPIO |
|--------|------|
| MOSI | G14 |
| MISO | G39 |
| SCK | G40 |
| NSS (CS) | G5 |
| IRQ | G4 |
| RST | G3 |
| BUSY | G6 |

**GPS — ATGM336H-6N (AT6668), UART 115200 8N1:**
| Signal | GPIO |
|--------|------|
| GPS TX → ESP RX | G13 |
| GPS RX → ESP TX | G15 |

**Port expander — PI4IOE5V6408, I2C:**
| Signal | Value |
|--------|-------|
| SDA | G8 |
| SCL | G9 |
| I2C addr | 0x43 |
| RF antenna switch | Set P0 HIGH to enable LoRa TX |

---

## Phase 0: Intelligence Gathering

### 0.1 OUI Database — DONE
- Ring LLC: 13 OUIs (`data/oui.json`)
- Flock Safety: 1 OUI — `B4:1E:52` (registered 2024-05-09)
- Source of truth: `data/oui.json` → generate `src/oui_db.h` via `tools/gen_oui_header.py`

### 0.2 OUI Gaps to Fill
- [ ] Confirm whether pre-2024 Flock cameras used a third-party cellular chipset OUI (Quectel, Sierra Wireless, etc.)
- [ ] Check for any Amazon-registered OUIs used by Ring devices (in addition to Ring LLC OUIs)
- [ ] Monitor IEEE OUI feed quarterly for new Ring/Flock registrations

### 0.3 BLE Fingerprinting
- Ring pairing service UUID confirmed: `9760d077-a234-4686-9e00-fcbbee3373f7`
- [ ] Capture full Ring advertisement payload (nRF Sniffer / Wireshark + BLE sniffing adapter)
- [ ] Put a Flock camera into provisioning mode and capture its BLE advertisement
- [ ] Document all Ring BLE device name strings emitted during advertisement

### 0.4 WiFi Fingerprinting
- [ ] Verify Flock provisioning SSID pattern (needs live camera in setup mode)
- [ ] Capture Ring probe request frames — check for embedded hostname
- [ ] Check for vendor-specific IEs in Ring beacon frames

---

## Phase 1: Toolchain Setup

1. Install VS Code + PlatformIO
2. Create PlatformIO project — board: `m5stack-stamps3` (or Cardputer ADV custom target)
3. Add dependencies to `platformio.ini`:
   - `m5stack/M5Cardputer`
   - `m5stack/M5GFX`
   - `sandeepmistry/LoRa` or `jgromes/RadioLib` (for SX1262)
   - `bblanchon/ArduinoJson` (for SPIFFS OUI DB parsing)
4. Flash M5Cardputer hello-world — verify display + keyboard
5. Test BLE passive scan example — confirm advertisement capture
6. Test WiFi promiscuous mode — confirm 802.11 frame capture
7. Test GPS UART read — confirm NMEA sentences flowing on G13
8. Test LoRa TX/RX basic ping (RF switch: set PI4IOE P0 HIGH via I2C first)

---

## Phase 2: Architecture

### 2.1 OUI Lookup
- `data/oui.json` is the source of truth (version-controlled)
- `tools/gen_oui_header.py` — converts JSON → `src/oui_db.h` (static fallback, baked into flash)
- SPIFFS holds `oui.json` — loaded at boot, overrides the static table
- Lookup: sorted array of packed `uint32_t` (3-byte OUI → top byte = vendor enum), binary search
- Vendor enum: `VENDOR_RING`, `VENDOR_FLOCK`, `VENDOR_UNKNOWN`

### 2.2 Scanning Architecture

```
┌─────────────────────────────────────────┐
│           Main Loop (Core 1)            │
│  UI render / keyboard / alert dispatch  │
└──────────┬──────────────────────────────┘
           │ shared device list (FreeRTOS mutex)
┌──────────▼──────────────────────────────┐
│         Scan Task (Core 0)              │
│                                         │
│  BLE: always-on passive advertisement   │
│       capture (NimBLE scan continuous)  │
│                                         │
│  WiFi: periodic sweep (configurable     │
│        dwell, default 3s every 30s)     │
│        promiscuous mode, ch 1-13        │
│        parse 802.11 management frames   │
│                                         │
│  OUI match → push to device list        │
└─────────────────────────────────────────┘
           │
┌──────────▼──────────────────────────────┐
│         GPS Task (Core 0, low prio)     │
│  UART read NMEA → update gps_fix struct │
│  Attached to each logged detection      │
└─────────────────────────────────────────┘
```

**BLE-priority rationale:** BLE and WiFi promiscuous mode conflict on ESP32. BLE runs continuously; WiFi monitor sweeps are gated — BLE scan is paused for the sweep window, then resumed. This catches Ring during pairing (BLE) and on-network (WiFi beacon/probe).

### 2.3 WiFi Sweep
- Pause BLE (`NimBLEDevice::getScan()->stop()`)
- Set WiFi to promiscuous, channel-hop 1→6→11 (2s each)
- Collect beacon + probe request frames, extract source MAC, OUI-match
- Resume BLE

### 2.4 Display Layout (135×240 TFT)

```
┌───────────────────┐
│ FlockOff  BLE ●   │  ← mode indicator, scan heartbeat
├───────────────────┤
│ ▲ RING            │  ← vendor tag (red=Ring, orange=Flock)
│   54:E0:19:xx:xx  │  ← MAC (last 3 bytes shown)
│   BLE  -67 dBm    │  ← protocol + RSSI
│   37.4N 122.1W    │  ← GPS coords at detection time
├───────────────────┤
│ ▲ FLOCK           │
│   B4:1E:52:xx:xx  │
│   WiFi -72 dBm    │
│   37.4N 122.1W    │
├───────────────────┤
│ [Q]uit [S]weep    │  ← keyboard shortcuts
│ GPS: 3D fix  4dev │  ← GPS status, total device count
└───────────────────┘
```

### 2.5 Alert System
- New Ring detect: single short buzz + red screen flash
- New Flock detect: double buzz + orange screen flash
- No SD logging in v1

### 2.6 SPIFFS OUI Update Flow
- On demand (key press): connect to WiFi, fetch latest `oui.json` from a configurable URL
- Write to SPIFFS, reload lookup table without reboot

---

## Phase 3: Implementation Order

| Step | File | Description |
|------|------|-------------|
| 1 | `tools/gen_oui_header.py` | JSON → static C header generator |
| 2 | `src/oui_db.h` | Generated static OUI table (fallback) |
| 3 | `src/oui_lookup.h/.cpp` | Binary search, returns `VendorId` enum |
| 4 | `src/oui_db_spiffs.h/.cpp` | Load/reload OUI table from SPIFFS JSON |
| 5 | `src/gps.h/.cpp` | UART NMEA parse (TinyGPS++ or minmea), `GpsFix` struct |
| 6 | `src/ble_scanner.h/.cpp` | NimBLE passive scan, OUI match, push to device list |
| 7 | `src/wifi_scanner.h/.cpp` | Promiscuous mode, channel hop, 802.11 frame parse |
| 8 | `src/device_list.h/.cpp` | Dedup by MAC, RSSI averaging, 5-min timeout eviction |
| 9 | `src/display.h/.cpp` | M5GFX UI, device list render, GPS status bar |
| 10 | `src/alert.h/.cpp` | Buzzer patterns + screen flash |
| 11 | `src/ota_update.h/.cpp` | SPIFFS OUI DB WiFi fetch + write |
| 12 | `src/main.cpp` | FreeRTOS task setup, dual-core pin, keyboard handler |

---

## Phase 4: Stretch Goals (v2+)

- [ ] LoRa telemetry: broadcast detections to a base station (mesh alert network)
- [ ] SD card logging with GPX track
- [ ] RSSI direction finding (rotate + sweep)
- [ ] BLE UUID matching for Ring (catches randomized MACs during pairing)
- [ ] Flock ALPR LTE band sniffing (requires RF hardware beyond Cardputer)
- [ ] OTA firmware update (not just OUI DB)

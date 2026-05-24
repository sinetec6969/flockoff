# FlockOff

Passive BLE + WiFi surveillance-device detector for the **M5Stack Cardputer ADV** (ESP32-S3).

Identifies **Ring** (doorbell/camera) and **Flock Safety** (ALPR/license-plate-reader) devices nearby by matching MAC OUIs against a verified database, then alerts you with vendor-specific buzzer patterns and logs the GPS fix at time of detection.

---

## Hardware

| Component | Part |
|---|---|
| Main board | M5Stack Cardputer ADV (ESP32-S3FN8) |
| GPS + LoRa cap | M5Stack Cap LoRa 1262 (ATGM336H GNSS, SX1262) |

The Cap LoRa 1262 snaps onto the Cardputer ADV Cap-Bus — no wiring required.

---

## Features (v0.0.1)

- **BLE passive scan** — always-on NimBLE advertisement capture
  - OUI match on 13 Ring LLC prefixes
  - Fallback: Ring pairing service UUID `9760d077-a234-4686-9e00-fcbbee3373f7` (catches randomised MACs)
  - Fallback: Flock battery manufacturer ID `0x09C8` (XUNTONG)
- **WiFi promiscuous sweep** — channels 1 / 6 / 11, every 30 s
  - OUI match on transmitter (addr2) and receiver (addr1, catches sleeping cameras)
  - Wildcard probe-request tightening for Flock (high-confidence signature)
- **32 Flock OUIs**: 1 Flock Safety-registered + 30 field-observed (LiteOn WCBN3510A, Silicon Labs, Espressif, Samsung, UGSI chipsets) + 1 locally-administered special case
- **13 Ring OUIs**: all IEEE MA-L, registered to Ring LLC
- **GPS tagging** — ATGM336H GNSS via UART; lat/lon attached to each detection
- **Vendor alerts** — Ring: 2 high beeps · Flock: 3 lower beeps
- **Live display** — 240×135 double-buffered canvas: vendor, MAC, protocol, RSSI, GPS coords
- **Dual-core** — scan task pinned to Core 0 (radio), UI on Core 1 (Arduino loop)
- **SPIFFS-ready** — OUI database designed for over-the-air updates (v1 uses baked-in static table)

---

## Detection logic

```
BLE (always-on, Core 0)
  └─ advertisement → reverse NimBLE native bytes → oui_lookup()
       ├─ OUI match            → RING / FLOCK
       ├─ Ring service UUID    → RING  (pairing mode, randomised MAC)
       └─ Flock mfr ID 0x09C8 → FLOCK (battery pack)

WiFi sweep (every 30 s, BLE paused during sweep)
  └─ promiscuous 802.11 frame
       ├─ addr2 (transmitter) → oui_lookup()
       │    └─ Flock: require wildcard-probe OR non-LA MAC
       └─ addr1 (receiver)   → oui_lookup()  ← catches sleeping cameras
```

---

## Building

```bash
# Install PlatformIO
pip3 install platformio

# Clone and build
git clone https://github.com/sinetec6969/flockoff.git
cd flockoff
pio run

# Flash (Cardputer ADV connected via USB)
pio run -t upload
```

First build downloads all toolchains and libraries (~15 min). Subsequent builds are fast.

**Build stats (v0.0.1):** RAM 17% · Flash 35% — plenty of headroom.

---

## OUI database

Source of truth: [`data/oui.json`](data/oui.json)  
Generated header: [`src/oui_db.h`](src/oui_db.h) (do not edit — regenerate with the tool below)

To update the OUI database:
```bash
# Edit data/oui.json, then:
python3 tools/gen_oui_header.py
pio run
```

All OUIs verified against the [IEEE MA-L registry](https://standards-oui.ieee.org/oui/oui.txt). Field-observed Flock OUIs sourced from [@NitekryDPaul and DeFlockJoplin](https://github.com/colonelpanichacks/flock-you).

---

## Project structure

```
flockoff/
├── data/
│   └── oui.json              # OUI database (source of truth)
├── src/
│   ├── main.cpp              # Setup, dual-core tasks, keyboard
│   ├── oui_db.h              # Generated sorted OUI table
│   ├── oui_lookup.{h,cpp}    # Binary-search OUI lookup
│   ├── device_list.{h,cpp}   # Thread-safe device tracker
│   ├── gps.{h,cpp}           # TinyGPS++ on UART1 (G13/G15)
│   ├── alert.{h,cpp}         # Non-blocking buzzer sequencer
│   ├── ble_scanner.{h,cpp}   # NimBLE passive scan + fingerprints
│   ├── wifi_scanner.{h,cpp}  # Promiscuous 802.11, addr1+addr2
│   └── display.{h,cpp}       # M5Canvas double-buffered UI
├── tools/
│   └── gen_oui_header.py     # JSON → C header generator
├── platformio.ini
├── partitions.csv            # 3 MB app + 1 MB SPIFFS (8 MB flash)
├── README.md
└── ROADMAP.md
```

---

## Keyboard shortcuts

| Key | Action |
|---|---|
| `Q` | Quit / restart |
| `W` | Force immediate WiFi sweep |

---

## Roadmap

See [`ROADMAP.md`](ROADMAP.md) for the full plan. Short version:

- **v0.1** — SPIFFS OUI DB over-the-air update, force-sweep key wired up
- **v0.2** — LoRa telemetry (broadcast detections to base station)
- **v0.3** — SD card logging with GPX track
- **v1.0** — Direction finding (RSSI sweep), Ring BLE UUID matching for all device types

---

## Credits & prior art

- [@NitekryDPaul](https://github.com/colonelpanichacks/flock-you) — Flock WiFi OUI field research and addr1 sleeping-camera technique
- [DeFlockJoplin](https://github.com/DeflockJoplin/flock-you) — wildcard-probe tightening and 31st OUI
- [Ryan Ohoro](https://www.ryanohoro.com/post/spotting-flock-safety-s-falcon-cameras) — Flock BLE battery advertisement research
- [CEHRP](https://www.cehrp.org/dissection-of-flock-safety-camera/) — Flock camera hardware dissection
- [Tenable TechBlog](https://medium.com/tenable-techblog/inside-amazons-ring-alarm-system-9731bc519974) — Ring BLE service UUID

---

## License

MIT

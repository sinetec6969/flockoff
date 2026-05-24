#include "lora_tx.h"
#include "config.h"
#include <RadioLib.h>
#include <Wire.h>
#include <Arduino.h>

// ── SX1262 pin assignments (Cap LoRa 1262, Cap-Bus) ──────────────────────────
#define LORA_NSS   5
#define LORA_IRQ   4
#define LORA_RST   3
#define LORA_BUSY  6
#define LORA_SCK   40
#define LORA_MISO  39
#define LORA_MOSI  14

// PI4IOE5V6408 port expander — RF antenna switch on P0
#define PI4IOE_ADDR  0x43

static SPIClass   s_spi(HSPI);
static SX1262     s_radio = new Module(LORA_NSS, LORA_IRQ, LORA_RST, LORA_BUSY,
                                       s_spi, SPISettings(2000000, MSBFIRST, SPI_MODE0));
static bool       s_ready = false;

static void rf_switch_on() {
    Wire1.beginTransmission(PI4IOE_ADDR);
    Wire1.write(0x01);   // Output register
    Wire1.write(0x01);   // P0 HIGH — connect LoRa to antenna
    Wire1.endTransmission();
}

bool lora_tx_init() {
    // Wire1 already started by cap_lora_init() in main.cpp
    rf_switch_on();

    s_spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    int state = s_radio.begin(LORA_FREQ,
                              LORA_BW,
                              LORA_SF,
                              LORA_CR,
                              LORA_SYNC_WORD,
                              LORA_PWR,
                              LORA_PREAMBLE,
                              LORA_TCXO_V,
                              false);   // DC-DC regulator (not LDO)

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] begin() failed: %d\n", state);
        return false;
    }

    s_radio.setDio1Action(nullptr);   // no interrupt-driven RX needed
    s_ready = true;
    Serial.printf("[LoRa] ready @ %.1f MHz SF%d BW%.0f\n",
                  LORA_FREQ, LORA_SF, LORA_BW);
    return true;
}

void lora_tx_send(const LoraPacket* pkt) {
    if (!s_ready) return;
    int state = s_radio.transmit((uint8_t*)pkt, sizeof(LoraPacket));
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] TX error: %d\n", state);
    }
}

LoraPacket lora_pkt_from_device(const Device* d) {
    LoraPacket p{};
    p.magic     = LORA_PKT_MAGIC;
    p.vendor    = (uint8_t)d->vendor;
    memcpy(p.mac, d->mac, 6);
    p.proto     = (uint8_t)d->proto;
    p.rssi      = d->rssi;
    p.lat       = d->has_gps ? d->lat : 0.0f;
    p.lon       = d->has_gps ? d->lon : 0.0f;
    p.uptime_ms = millis();
    return p;
}

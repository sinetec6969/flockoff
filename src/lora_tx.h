#pragma once
#include "device_list.h"
#include <stdint.h>

// One-way detection broadcast over SX1262 LoRa (Cap LoRa 1262 cap module).
// Packets are queued by on_new_device() and drained by lora_task on Core 0.

#define LORA_PKT_MAGIC  0xF1u   // FlockOff detection frame

typedef struct __attribute__((packed)) {
    uint8_t  magic;         // LORA_PKT_MAGIC
    uint8_t  vendor;        // VendorId
    uint8_t  mac[6];
    uint8_t  proto;         // Protocol
    int8_t   rssi;
    float    lat;           // 0.0 when no GPS fix
    float    lon;
    uint32_t uptime_ms;
} LoraPacket;               // 22 bytes

// Returns true on successful radio init. Safe to call before scan tasks start.
bool lora_tx_init();

// Transmit one packet — blocking (~500 ms at SF9/125 kHz). Call from a
// dedicated task, not from loop() or a scanner callback.
void lora_tx_send(const LoraPacket* pkt);

// Fill a LoraPacket from a Device struct.
LoraPacket lora_pkt_from_device(const Device* d);

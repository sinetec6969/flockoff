#pragma once
#include <SPI.h>

// LoRa SX1262 (CS=G5) and the internal TF card (CS=G12) share HSPI:
//   SCK=G40  MOSI=G14  MISO=G39
// Both devices use this single SPIClass so the Arduino SPI transaction
// semaphore provides mutual exclusion — no extra mutex needed.
extern SPIClass g_hspi;

void spi_bus_init();  // call once from setup(), before lora_tx_init / sd_logger_init

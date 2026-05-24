#pragma once
#include <stdint.h>

// ATGM336H-6N via Cap LoRa 1262 Cap-Bus UART
// GPS_TX (module→ESP) = G13,  GPS_RX (ESP→module) = G15
#define GPS_UART_RX  13
#define GPS_UART_TX  15
#define GPS_BAUD     115200

typedef struct {
    float   lat;
    float   lon;
    float   alt_m;
    float   hdop;
    uint8_t satellites;
    bool    valid;
} GpsFix;

void   gps_init();
void   gps_update();   // feed bytes from loop()
GpsFix gps_get_fix();

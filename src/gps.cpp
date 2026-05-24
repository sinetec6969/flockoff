#include "gps.h"
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

static TinyGPSPlus    s_gps;
static HardwareSerial s_serial(1);  // UART1
static GpsFix         s_fix{};

void gps_init() {
    s_serial.begin(GPS_BAUD, SERIAL_8N1, GPS_UART_RX, GPS_UART_TX);
}

void gps_update() {
    while (s_serial.available()) {
        s_gps.encode(s_serial.read());
    }
    if (s_gps.location.isValid()) {
        s_fix.lat   = (float)s_gps.location.lat();
        s_fix.lon   = (float)s_gps.location.lng();
        s_fix.valid = true;
    }
    if (s_gps.altitude.isValid())   s_fix.alt_m     = (float)s_gps.altitude.meters();
    if (s_gps.hdop.isValid())       s_fix.hdop      = (float)s_gps.hdop.hdop();
    if (s_gps.satellites.isValid()) s_fix.satellites = (uint8_t)s_gps.satellites.value();
}

GpsFix gps_get_fix() { return s_fix; }

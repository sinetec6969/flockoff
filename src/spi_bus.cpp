#include "spi_bus.h"

SPIClass g_hspi(HSPI);

void spi_bus_init() {
    g_hspi.begin(40, 39, 14);  // SCK, MISO, MOSI — CS managed per-device
}

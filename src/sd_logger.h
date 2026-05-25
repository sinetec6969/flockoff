#pragma once
#include "device_list.h"

// SD card GPX track + CSV detection log.
// sd_logger_tick() and sd_logger_log_device() must be called from loop() only
// (Core 1) — SD access is not thread-safe across cores.
bool sd_logger_init();
void sd_logger_tick();                    // append GPS track point every SD_TRACK_INTERVAL_MS
void sd_logger_log_device(const Device* d);
bool sd_logger_available();

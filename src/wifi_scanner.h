#pragma once
#include <stdint.h>

#define WIFI_SWEEP_INTERVAL_MS  30000u   // sweep every 30 s
#define WIFI_DWELL_MS           1500u    // per-channel dwell

void wifi_scanner_init();

// Hop channels 1/6/11, capture frames, then return.
// Call with BLE paused; BLE resumes after.
void wifi_scanner_sweep();

// Drain the capture queue — call from scan_task between sweeps.
void wifi_scanner_process();

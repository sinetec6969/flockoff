#pragma once
#include "oui_db.h"
#include <stdint.h>

#define MAX_DEVICES        64
#define DEVICE_TIMEOUT_MS  300000u   // 5 min

typedef enum : uint8_t { PROTO_BLE, PROTO_WIFI } Protocol;

typedef struct {
    uint8_t  mac[6];
    VendorId vendor;
    Protocol proto;
    int8_t   rssi;
    float    lat;
    float    lon;
    bool     has_gps;
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    bool     active;
    bool     alerted;
} Device;

// Callback fired (under mutex) when a brand-new device is inserted.
// Must be non-blocking — use xQueueSend(..., 0) style calls only.
typedef void (*NewDeviceCb)(const Device* d);
void device_list_set_new_device_cb(NewDeviceCb cb);

void          device_list_init();

// Returns true when this is a newly seen device.
bool          device_list_update(const uint8_t mac[6], VendorId vendor,
                                  Protocol proto, int8_t rssi,
                                  float lat, float lon, bool has_gps);

void          device_list_evict(uint32_t now_ms);
int           device_list_active_count();

// Caller must hold lock across get().
void          device_list_lock();
void          device_list_unlock();
const Device* device_list_get(int idx);  // 0-based over active entries

#include "device_list.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include <Arduino.h>

static Device            s_devs[MAX_DEVICES];
static SemaphoreHandle_t s_mutex;
static NewDeviceCb       s_new_device_cb = nullptr;

void device_list_set_new_device_cb(NewDeviceCb cb) { s_new_device_cb = cb; }

void device_list_init() {
    memset(s_devs, 0, sizeof(s_devs));
    s_mutex = xSemaphoreCreateMutex();
}

void device_list_lock()   { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void device_list_unlock() { xSemaphoreGive(s_mutex); }

bool device_list_update(const uint8_t mac[6], VendorId vendor,
                         Protocol proto, int8_t rssi,
                         float lat, float lon, bool has_gps) {
    uint32_t now = millis();
    device_list_lock();

    // Search for existing entry
    int free_slot = -1;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!s_devs[i].active) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (memcmp(s_devs[i].mac, mac, 6) == 0) {
            // Update existing — blend RSSI with 25% new weight
            s_devs[i].rssi = (int8_t)((s_devs[i].rssi * 3 + rssi) / 4);
            s_devs[i].last_seen_ms = now;
            s_devs[i].proto = proto;
            if (has_gps) {
                s_devs[i].lat     = lat;
                s_devs[i].lon     = lon;
                s_devs[i].has_gps = true;
            }
            device_list_unlock();
            return false;
        }
    }

    if (free_slot < 0) {
        // Evict oldest entry
        uint32_t oldest_ms = UINT32_MAX;
        for (int i = 0; i < MAX_DEVICES; i++) {
            if (s_devs[i].last_seen_ms < oldest_ms) {
                oldest_ms  = s_devs[i].last_seen_ms;
                free_slot  = i;
            }
        }
    }

    Device* d        = &s_devs[free_slot];
    memcpy(d->mac, mac, 6);
    d->vendor        = vendor;
    d->proto         = proto;
    d->rssi          = rssi;
    d->lat           = lat;
    d->lon           = lon;
    d->has_gps       = has_gps;
    d->first_seen_ms = now;
    d->last_seen_ms  = now;
    d->active        = true;
    d->alerted       = false;
    if (s_new_device_cb) s_new_device_cb(d);  // called under mutex — must be non-blocking
    device_list_unlock();
    return true;
}

void device_list_evict(uint32_t now_ms) {
    device_list_lock();
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (s_devs[i].active &&
            (now_ms - s_devs[i].last_seen_ms) > DEVICE_TIMEOUT_MS) {
            s_devs[i].active = false;
        }
    }
    device_list_unlock();
}

int device_list_active_count() {
    int count = 0;
    device_list_lock();
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (s_devs[i].active) count++;
    }
    device_list_unlock();
    return count;
}

// Returns the nth active device — caller must hold lock.
const Device* device_list_get(int idx) {
    int seen = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (s_devs[i].active) {
            if (seen == idx) return &s_devs[i];
            seen++;
        }
    }
    return nullptr;
}

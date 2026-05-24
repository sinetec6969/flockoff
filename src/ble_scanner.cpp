#include "ble_scanner.h"
#include "oui_lookup.h"
#include "device_list.h"
#include "gps.h"
#include <NimBLEDevice.h>

// Ring pairing/setup service UUID — present even when MAC is randomised
static const char* RING_SVC_UUID = "9760d077-a234-4686-9e00-fcbbee3373f7";

// Flock battery BLE manufacturer ID (XUNTONG)
static const uint16_t FLOCK_MFR_ID = 0x09C8;

class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        NimBLEAddress bleAddr = dev->getAddress();
        const uint8_t* native = bleAddr.getNative();

        // NimBLE stores BLE address little-endian; reverse to canonical OUI order
        uint8_t mac[6];
        for (int i = 0; i < 6; i++) mac[i] = native[5 - i];

        VendorId vendor = oui_lookup(mac);

        // Fallback fingerprints when OUI doesn't match (randomised MACs, etc.)
        if (vendor == VENDOR_UNKNOWN) {
            if (dev->haveServiceUUID() &&
                dev->isAdvertisingService(NimBLEUUID(RING_SVC_UUID))) {
                vendor = VENDOR_RING;
            } else if (dev->haveManufacturerData()) {
                const std::string& mfr = dev->getManufacturerData();
                if (mfr.size() >= 2) {
                    uint16_t id = (uint8_t)mfr[0] | ((uint8_t)mfr[1] << 8);
                    if (id == FLOCK_MFR_ID) vendor = VENDOR_FLOCK;
                }
            }
        }

        if (vendor == VENDOR_UNKNOWN) return;

        GpsFix fix = gps_get_fix();
        bool is_new = device_list_update(mac, vendor, PROTO_BLE,
                                         (int8_t)dev->getRSSI(),
                                         fix.lat, fix.lon, fix.valid);
        (void)is_new;  // alert_new_device() called from scan_task via queue flag
    }
};

static NimBLEScan*    s_scan = nullptr;
static ScanCallbacks  s_cb;

void ble_scanner_init() {
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_N0);  // 0 dBm — enough for detection

    s_scan = NimBLEDevice::getScan();
    s_scan->setAdvertisedDeviceCallbacks(&s_cb, false);
    s_scan->setActiveScan(false);   // passive — don't send scan requests
    s_scan->setInterval(100);       // 62.5 ms window (units of 0.625 ms)
    s_scan->setWindow(99);
    s_scan->setMaxResults(0);       // don't accumulate — callback only
}

void ble_scanner_start()  { s_scan->start(0, false); }  // 0 = indefinite
void ble_scanner_pause()  { s_scan->stop(); }
void ble_scanner_resume() { s_scan->start(0, false); }

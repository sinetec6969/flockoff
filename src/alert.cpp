#include "alert.h"
#include <M5Cardputer.h>

struct Tone { uint16_t freq; uint16_t ms; };

// Ring: two short high beeps.  Flock: three shorter lower beeps.
static const Tone RING_PAT[]  = {{1400,80},{0,50},{1400,80},{0,0}};
static const Tone FLOCK_PAT[] = {{900,60},{0,40},{900,60},{0,40},{900,60},{0,0}};

static const Tone* s_pat      = nullptr;
static int         s_idx      = 0;
static uint32_t    s_until_ms = 0;

void alert_init() {
    M5Cardputer.Speaker.setVolume(128);
}

void alert_new_device(VendorId vendor) {
    s_pat   = (vendor == VENDOR_RING) ? RING_PAT : FLOCK_PAT;
    s_idx   = 0;
    s_until_ms = 0;
}

void alert_tick() {
    if (!s_pat) return;
    uint32_t now = millis();
    if (now < s_until_ms) return;

    const Tone& t = s_pat[s_idx];
    if (t.ms == 0) {
        M5Cardputer.Speaker.stop();
        s_pat = nullptr;
        return;
    }
    if (t.freq == 0) M5Cardputer.Speaker.stop();
    else             M5Cardputer.Speaker.tone(t.freq, 0);
    s_until_ms = now + t.ms;
    s_idx++;
}

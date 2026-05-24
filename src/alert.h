#pragma once
#include "oui_db.h"

void alert_init();
void alert_new_device(VendorId vendor);
void alert_tick();   // call from loop() — non-blocking pattern sequencer

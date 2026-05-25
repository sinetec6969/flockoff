#pragma once

void display_init();
void display_render();       // call from loop() — full redraw
void display_nav(int delta); // +1=down, -1=up through device list (call from loop())

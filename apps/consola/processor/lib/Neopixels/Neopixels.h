#pragma once

#include <Adafruit_NeoPixel.h>
#include "shared/Config.h"

class Neopixels {
public:
    void begin();
    void update();
    void setPixelColor(int index, uint32_t color);
private:
    Adafruit_NeoPixel strip;
};

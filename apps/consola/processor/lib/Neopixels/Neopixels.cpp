#include "Neopixels.h"

void Neopixels::begin() {
    strip = Adafruit_NeoPixel(TOTAL_NEOPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.setBrightness(128);
    strip.show(); // Initialize all pixels to 'off'
}

void Neopixels::update() {
    strip.show();
}

void Neopixels::setPixelColor(int index, uint32_t color) {
    strip.setPixelColor(index, color);
}

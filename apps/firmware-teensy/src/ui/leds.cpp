#include "leds.h"

// Global brightness cap: 80/255 ≈ 31%.
// At full brightness, 16× WS2812B can draw ~960 mA — well above USB current budget.
// 80/255 keeps total LED current under ~250 mA on a clean power rail.
static constexpr uint8_t GLOBAL_BRIGHTNESS = 80;

void Leds::init() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(_leds, NUM_LEDS);
    FastLED.setBrightness(GLOBAL_BRIGHTNESS);
    clear();
}

void Leds::set(uint8_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx >= NUM_LEDS) return;
    _leds[idx] = CRGB(r, g, b);
}

void Leds::set_ring(uint8_t active_pos, CRGB color_active, CRGB color_idle) {
    if (active_pos >= NUM_RING) return;
    for (uint8_t i = 0; i < NUM_RING; i++) {
        _leds[i] = (i == active_pos) ? color_active : color_idle;
    }
}

void Leds::set_keycap(uint8_t btn_idx, CRGB color) {
    if (btn_idx >= NUM_KEYCAP) return;
    _leds[NUM_RING + btn_idx] = color;
}

void Leds::set_all(CRGB color) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        _leds[i] = color;
    }
}

void Leds::show() {
    FastLED.show();
}

void Leds::clear() {
    set_all(CRGB::Black);
    FastLED.show();
}

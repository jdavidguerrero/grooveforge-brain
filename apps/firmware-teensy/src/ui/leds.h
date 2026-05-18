#pragma once

#include <Arduino.h>
#include <FastLED.h>

/// Total WS2812B LEDs in the chain (12 ring + 4 keycap underglow).
static constexpr uint8_t NUM_LEDS    = 16;

/// LEDs 0-11: ring around ENC NAV.
static constexpr uint8_t NUM_RING    = 12;

/// LEDs 12-15: underglow under Kailh Choc V2 keycaps (B1-B4).
static constexpr uint8_t NUM_KEYCAP  = 4;

/// Data pin for WS2812B chain (01-architecture.md §3.3).
static constexpr uint8_t DATA_PIN    = 28;

/**
 * @brief Controls 16 WS2812B LEDs via FastLED on GPIO28.
 *
 * LED layout (single chain):
 *   Index 0-11  → 12-LED ring around ENC NAV
 *   Index 12-15 → keycap underglow for B1-B4 (in order)
 *
 * FastLED is used instead of WS2812Serial because pin 28 on Teensy 4.1
 * is not a UART TX pin — WS2812Serial requires specific UART TX pins
 * (1, 8, 14, 17, 20, 24, 29, 35). FastLED bit-bangs on any digital pin.
 */
class Leds {
public:
    /**
     * @brief Initialize FastLED with WS2812B, GRB ordering, on DATA_PIN.
     *        Sets global brightness to 80/255 (safe for direct USB power).
     */
    void init();

    /**
     * @brief Set a single LED by index.
     * @param idx   LED index 0–15.
     * @param r     Red component 0–255.
     * @param g     Green component 0–255.
     * @param b     Blue component 0–255.
     */
    void set(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Set ring LEDs with one active position and idle color for the rest.
     * @param active_pos   Ring position to highlight (0–11).
     * @param color_active CRGB color for the active LED.
     * @param color_idle   CRGB color for all other ring LEDs.
     */
    void set_ring(uint8_t active_pos, CRGB color_active, CRGB color_idle);

    /**
     * @brief Set a single keycap underglow LED.
     * @param btn_idx Button index 0–3 (maps to LED index 12–15).
     * @param color   CRGB color.
     */
    void set_keycap(uint8_t btn_idx, CRGB color);

    /**
     * @brief Set all LEDs to the same color.
     * @param color CRGB color.
     */
    void set_all(CRGB color);

    /**
     * @brief Push pending changes to the LED hardware.
     *        Call after all set_*() calls for the current frame.
     */
    void show();

    /**
     * @brief Turn off all LEDs and immediately push to hardware.
     */
    void clear();

private:
    CRGB _leds[NUM_LEDS];
};

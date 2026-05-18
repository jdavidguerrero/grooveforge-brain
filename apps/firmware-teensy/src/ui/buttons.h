#pragma once

#include <Arduino.h>
#include <Bounce2.h>

/// Number of physical buttons (Kailh Choc V2, GPIO 2-5).
static constexpr uint8_t NUM_BUTTONS = 4;

/// Duration threshold for "held" state detection, in milliseconds.
static constexpr uint32_t HELD_THRESHOLD_MS = 500;

/**
 * @brief Manages four Kailh Choc V2 switches on GPIO 2–5.
 *
 * Uses Bounce2 for debounce. Supports press-event detection and
 * sustained-hold detection (>500 ms).
 *
 * Pin assignment (01-architecture.md §3.3):
 *   B1=GPIO2, B2=GPIO3, B3=GPIO4, B4=GPIO5
 */
class Buttons {
public:
    /**
     * @brief Initialize all button pins with INPUT_PULLUP and attach Bounce2.
     */
    void init();

    /**
     * @brief Update Bounce2 state for all buttons. Call once per loop().
     */
    void update();

    /**
     * @brief Check if a button was pressed this cycle (falling edge).
     * @param idx Button index 0–3 (maps to B1–B4).
     * @return true on the cycle where the press was detected.
     */
    bool pressed(uint8_t idx);

    /**
     * @brief Check if a button is currently held down beyond the threshold.
     * @param idx Button index 0–3.
     * @return true if button has been continuously pressed for >500 ms.
     */
    bool held(uint8_t idx);

private:
    // Teensyduino Bounce2 bundle: Button is a global class (no namespace prefix).
    Button _btns[NUM_BUTTONS];
};

#pragma once

#include <Arduino.h>
#include <Encoder.h>
#include <Bounce2.h>

/**
 * @brief Wraps three quadrature encoders (ALPS EC11) with push-switch debounce.
 *
 * Uses Teensyduino-bundled Encoder.h for interrupt-driven quadrature decoding
 * and Bounce2 for push-switch debounce on each encoder's SW pin.
 *
 * Pin assignment (01-architecture.md §3.3):
 *   ENC L  : A=10, B=11, SW=12
 *   ENC R  : A=13, B=14, SW=15
 *   ENC NAV: A=16, B=17, SW=26
 */
class Encoders {
public:
    /**
     * @brief Initialize switch pins with INPUT_PULLUP and attach Bounce2.
     *        Encoder objects are constructed at instantiation (interrupt-driven).
     */
    void init();

    /**
     * @brief Read left encoder delta since last call.
     * @return Signed detent count (positive = CW). Resets internal accumulator.
     */
    int32_t read_enc_l();

    /**
     * @brief Read right encoder delta since last call.
     * @return Signed detent count (positive = CW). Resets internal accumulator.
     */
    int32_t read_enc_r();

    /**
     * @brief Read nav encoder delta since last call.
     * @return Signed detent count (positive = CW). Resets internal accumulator.
     */
    int32_t read_enc_nav();

    /**
     * @brief Check if left encoder switch was pressed this cycle.
     * @return true on falling edge (button pressed), false otherwise.
     */
    bool sw_l_pressed();

    /**
     * @brief Check if right encoder switch was pressed this cycle.
     * @return true on falling edge (button pressed), false otherwise.
     */
    bool sw_r_pressed();

    /**
     * @brief Check if nav encoder switch was pressed this cycle.
     * @return true on falling edge (button pressed), false otherwise.
     */
    bool sw_nav_pressed();

    /**
     * @brief Update Bounce2 state for all three switches. Call once per loop().
     */
    void update();

private:
    // Encoder objects own their interrupt callbacks — constructed at declaration.
    Encoder _enc_l{10, 11};
    Encoder _enc_r{13, 14};
    Encoder _enc_nav{16, 17};

    // Last consumed absolute position (in raw pulses, multiple of PULSES_PER_DETENT).
    // Tracks partial pulses across loop() cycles — avoids the readAndReset() loss bug.
    int32_t _last_l   = 0;
    int32_t _last_r   = 0;
    int32_t _last_nav = 0;

    // Teensyduino Bounce2 bundle: Button is a global class (no namespace prefix).
    Button _sw_l;
    Button _sw_r;
    Button _sw_nav;
};

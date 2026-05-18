#pragma once

#include <Arduino.h>
#include "encoders.h"
#include "buttons.h"
#include "leds.h"

/// Param IDs for on_param_changed callback.
static constexpr uint8_t PARAM_CUTOFF    = 0;
static constexpr uint8_t PARAM_RESONANCE = 1;

/// Button IDs passed to on_button callback.
/// B1-B4 use indices 0-3 directly.
static constexpr uint8_t BTN_CONFIRM     = 4;   ///< Single push ENC NAV
static constexpr uint8_t BTN_MODE_SWITCH = 5;   ///< Double push ENC NAV within window

/// Callback types.
using ParamChangedCb = void (*)(uint8_t param_id, float delta);
using ButtonCb       = void (*)(uint8_t btn_idx);
using NavCb          = void (*)(int32_t delta);

/**
 * @brief Routes encoder/button events to application-level callbacks.
 *
 * UIManager owns references to Encoders, Buttons, and Leds.
 * Call update() every loop iteration. Register callbacks via
 * on_param_changed(), on_button(), and on_nav() before entering loop().
 *
 * Double-push detection: two consecutive ENC NAV SW presses within
 * DOUBLE_PUSH_WINDOW_MS emit BTN_MODE_SWITCH instead of two BTN_CONFIRM events.
 */
class UIManager {
public:
    /// Maximum interval between two presses to count as double-push (ms).
    static constexpr uint32_t DOUBLE_PUSH_WINDOW_MS = 400;

    /**
     * @brief Attach peripheral objects. Must be called before update().
     * @param enc  Pointer to initialized Encoders instance.
     * @param btn  Pointer to initialized Buttons instance.
     * @param leds Pointer to initialized Leds instance.
     */
    void init(Encoders* enc, Buttons* btn, Leds* leds);

    /**
     * @brief Register callback fired when ENC L or ENC R changes.
     * @param cb Function receiving (param_id, delta).
     *           delta is in normalized units: 1 detent = 0.01f.
     */
    void on_param_changed(ParamChangedCb cb);

    /**
     * @brief Register callback fired on button press events.
     * @param cb Function receiving btn_idx (0-3 = B1-B4, 4 = confirm, 5 = mode switch).
     */
    void on_button(ButtonCb cb);

    /**
     * @brief Register callback fired when ENC NAV rotates.
     * @param cb Function receiving signed detent delta.
     */
    void on_nav(NavCb cb);

    /**
     * @brief Poll all peripherals and dispatch callbacks. Call once per loop().
     */
    void update();

private:
    Encoders*      _enc  = nullptr;
    Buttons*       _btn  = nullptr;
    Leds*          _leds = nullptr;

    ParamChangedCb _param_cb = nullptr;
    ButtonCb       _btn_cb   = nullptr;
    NavCb          _nav_cb   = nullptr;

    // Double-push state: timestamp of first NAV SW press; 0 = no pending press.
    uint32_t _nav_sw_first_press_ms = 0;
};

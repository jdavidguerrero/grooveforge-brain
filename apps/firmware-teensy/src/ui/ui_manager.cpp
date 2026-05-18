#include "ui_manager.h"

// Scaling factor: converts one encoder detent into a normalized parameter delta.
// 0.01f means 100 detents = full sweep (0.0 to 1.0). Coarse enough for live use.
static constexpr float PARAM_DELTA_PER_DETENT = 0.01f;

void UIManager::init(Encoders* enc, Buttons* btn, Leds* leds) {
    _enc  = enc;
    _btn  = btn;
    _leds = leds;
}

void UIManager::on_param_changed(ParamChangedCb cb) {
    _param_cb = cb;
}

void UIManager::on_button(ButtonCb cb) {
    _btn_cb = cb;
}

void UIManager::on_nav(NavCb cb) {
    _nav_cb = cb;
}

void UIManager::update() {
    if (!_enc || !_btn) return;

    // --- Update peripherals ---
    _enc->update();
    _btn->update();

    // --- ENC L → PARAM_CUTOFF ---
    int32_t delta_l = _enc->read_enc_l();
    if (delta_l != 0 && _param_cb) {
        _param_cb(PARAM_CUTOFF, static_cast<float>(delta_l) * PARAM_DELTA_PER_DETENT);
    }

    // --- ENC R → PARAM_RESONANCE ---
    int32_t delta_r = _enc->read_enc_r();
    if (delta_r != 0 && _param_cb) {
        _param_cb(PARAM_RESONANCE, static_cast<float>(delta_r) * PARAM_DELTA_PER_DETENT);
    }

    // --- ENC NAV rotation ---
    int32_t delta_nav = _enc->read_enc_nav();
    if (delta_nav != 0 && _nav_cb) {
        _nav_cb(delta_nav);
    }

    // --- ENC NAV SW — double-push detection ---
    if (_enc->sw_nav_pressed()) {
        uint32_t now = millis();

        if (_nav_sw_first_press_ms == 0) {
            // First press: record timestamp, defer action.
            _nav_sw_first_press_ms = now;
        } else {
            uint32_t elapsed = now - _nav_sw_first_press_ms;
            _nav_sw_first_press_ms = 0;

            if (elapsed <= DOUBLE_PUSH_WINDOW_MS) {
                // Second press within window → mode switch.
                if (_btn_cb) _btn_cb(BTN_MODE_SWITCH);
            } else {
                // First press expired; treat deferred press as confirm, then start new window.
                if (_btn_cb) _btn_cb(BTN_CONFIRM);
                _nav_sw_first_press_ms = now;
            }
        }
    }

    // Flush any pending first-press as confirm once window expires.
    if (_nav_sw_first_press_ms != 0) {
        uint32_t elapsed = millis() - _nav_sw_first_press_ms;
        if (elapsed > DOUBLE_PUSH_WINDOW_MS) {
            _nav_sw_first_press_ms = 0;
            if (_btn_cb) _btn_cb(BTN_CONFIRM);
        }
    }

    // --- ENC L SW (filter bypass toggle) ---
    if (_enc->sw_l_pressed() && _btn_cb) {
        // ENC L push is handled at application level — UIManager forwards as btn 6.
        // No named constant here: application decides pin 25 (CD4066) toggle logic.
        // We reuse _btn_cb with a dedicated ID to stay decoupled from GPIO.
        _btn_cb(6);  // BTN_FILTER_BYPASS — application maps this to GPIO25
    }

    // --- ENC R SW (resonance reset) ---
    if (_enc->sw_r_pressed() && _btn_cb) {
        _btn_cb(7);  // BTN_RESONANCE_RESET — application resets resonance parameter to 0
    }

    // --- B1-B4 ---
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        if (_btn->pressed(i) && _btn_cb) {
            _btn_cb(i);
        }
    }
}

#include "encoders.h"

// EC11 encoders generate 4 raw quadrature pulses per mechanical detent.
// Dividing by 4 converts raw counts to user-visible "click" units.
//
// IMPORTANT: We do NOT use readAndReset() because it discards partial pulses
// between calls (e.g. raw=2 → 2/4=0, those 2 pulses are lost forever).
// Instead we track absolute position and consume only whole detents, letting
// sub-detent pulses accumulate across loop() cycles.
static constexpr int32_t PULSES_PER_DETENT = 4;

void Encoders::init() {
    // Initialize tracked positions to current encoder values
    _last_l   = _enc_l.read();
    _last_r   = _enc_r.read();
    _last_nav = _enc_nav.read();

    _sw_l.attach(12, INPUT_PULLUP);
    _sw_l.interval(5);
    _sw_l.setPressedState(LOW);

    _sw_r.attach(15, INPUT_PULLUP);
    _sw_r.interval(5);
    _sw_r.setPressedState(LOW);

    _sw_nav.attach(26, INPUT_PULLUP);
    _sw_nav.interval(5);
    _sw_nav.setPressedState(LOW);
}

int32_t Encoders::read_enc_l() {
    int32_t current = _enc_l.read();
    int32_t delta_raw = current - _last_l;
    int32_t detents = delta_raw / PULSES_PER_DETENT;
    _last_l += detents * PULSES_PER_DETENT;  // consume only whole detents
    return detents;
}

int32_t Encoders::read_enc_r() {
    int32_t current = _enc_r.read();
    int32_t delta_raw = current - _last_r;
    int32_t detents = delta_raw / PULSES_PER_DETENT;
    _last_r += detents * PULSES_PER_DETENT;
    return detents;
}

int32_t Encoders::read_enc_nav() {
    int32_t current = _enc_nav.read();
    int32_t delta_raw = current - _last_nav;
    int32_t detents = delta_raw / PULSES_PER_DETENT;
    _last_nav += detents * PULSES_PER_DETENT;
    return detents;
}

bool Encoders::sw_l_pressed() {
    return _sw_l.pressed();
}

bool Encoders::sw_r_pressed() {
    return _sw_r.pressed();
}

bool Encoders::sw_nav_pressed() {
    return _sw_nav.pressed();
}

void Encoders::update() {
    _sw_l.update();
    _sw_r.update();
    _sw_nav.update();
}

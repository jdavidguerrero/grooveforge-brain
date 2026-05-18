#include "encoders.h"

// EC11 encoders generate 4 raw quadrature pulses per mechanical detent.
// Dividing by 4 converts raw counts to user-visible "click" units.
static constexpr int32_t PULSES_PER_DETENT = 4;

void Encoders::init() {
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
    int32_t raw = _enc_l.readAndReset();
    return raw / PULSES_PER_DETENT;
}

int32_t Encoders::read_enc_r() {
    int32_t raw = _enc_r.readAndReset();
    return raw / PULSES_PER_DETENT;
}

int32_t Encoders::read_enc_nav() {
    int32_t raw = _enc_nav.readAndReset();
    return raw / PULSES_PER_DETENT;
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

#include "buttons.h"

// Physical GPIO pins for B1-B4, indexed 0-3.
static constexpr uint8_t BTN_PINS[NUM_BUTTONS] = {2, 3, 4, 5};

void Buttons::init() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        _btns[i].attach(BTN_PINS[i], INPUT_PULLUP);
        _btns[i].interval(5);
        _btns[i].setPressedState(LOW);
    }
}

void Buttons::update() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        _btns[i].update();
    }
}

bool Buttons::pressed(uint8_t idx) {
    if (idx >= NUM_BUTTONS) return false;
    return _btns[idx].pressed();
}

bool Buttons::held(uint8_t idx) {
    if (idx >= NUM_BUTTONS) return false;
    // currentDuration() returns ms since last state change.
    // isPressed() guards against reporting held when released.
    // duration() returns ms in the current state (pressed or released).
    return _btns[idx].isPressed() &&
           (_btns[idx].duration() > HELD_THRESHOLD_MS);
}

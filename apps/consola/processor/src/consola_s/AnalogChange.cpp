#include "consola_s/AnalogChange.h"
AnalogChange::AnalogChange(uint16_t maximum, uint8_t deadband)
    : maximum_(maximum ? maximum : 1), deadband_(deadband ? deadband : 1) {}
bool AnalogChange::sample(int32_t raw, uint8_t& midi) {
    if (raw < 0) raw = 0;
    if (raw > maximum_) raw = maximum_;
    const bool first = !initialized_;
    if (first) filtered_q8_ = raw * 256;
    else filtered_q8_ += (raw * 256 - filtered_q8_) / 4;
    initialized_ = true;
    midi = (filtered_q8_ * 127 + int32_t(maximum_) * 128) / (int32_t(maximum_) * 256);
    const int difference = int(midi) - last_;
    if (first || difference >= deadband_ || difference <= -deadband_ ||
        (midi != last_ && (midi == 0 || midi == 127))) {
        last_ = midi;
        return true;
    }
    return false;
}

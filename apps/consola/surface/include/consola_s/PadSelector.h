#pragma once
#include <stdint.h>

/** Three-position resistor ladder at 0/half/full scale, ADC12. */
class PadSelector {
public:
    /** Publish initial position and changes, retaining position across ±80-count noise. */
    bool sample(uint16_t raw, uint8_t& midi) {
        uint8_t next = raw < THRESHOLD_LOW ? 0 : raw < THRESHOLD_HIGH ? 1 : 2;
        if (initialized_) {
            next = position_;
            if (position_ == 0 && raw >= THRESHOLD_LOW + HYSTERESIS) next = raw >= THRESHOLD_HIGH + HYSTERESIS ? 2 : 1;
            if (position_ == 1 && raw < THRESHOLD_LOW - HYSTERESIS) next = 0;
            if (position_ == 1 && raw >= THRESHOLD_HIGH + HYSTERESIS) next = 2;
            if (position_ == 2 && raw < THRESHOLD_HIGH - HYSTERESIS) next = raw < THRESHOLD_LOW - HYSTERESIS ? 0 : 1;
        }
        if (initialized_ && position_ == next) return false;
        initialized_ = true;
        position_ = next;
        midi = next == 0 ? 0 : next == 1 ? 64 : 127;
        return true;
    }
    static constexpr uint16_t THRESHOLD_LOW = 1365, THRESHOLD_HIGH = 2730, HYSTERESIS = 80;
private:
    uint8_t position_ = 0;
    bool initialized_ = false;
};

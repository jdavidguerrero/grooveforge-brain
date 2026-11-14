#pragma once
#include <stdint.h>

/** Single owner of ADC1. Call every 50 us; never waits for hardware.
 * Slots 0..3 are FSRs (400 us frames), 4=CUE, 5=selector (10 ms).
 * A lost conversion expires after 100 us; it never becomes a zero sample.
 */
class AdcScan {
public:
    struct Step { int8_t sample = -1, start = -1; bool timeout = false, frame = false; };
    explicit AdcScan(uint8_t mask) : mask_(mask) {}
    Step tick(uint32_t now, bool ready) {
        Step step;
        if (active_ >= 0) {
            if (ready) step.sample = active_;
            else if (uint32_t(now - conversion_) >= 100) step.timeout = true;
            else return step;
            active_ = -1;
        }
        if (!pending_ && (!begun_ || uint32_t(now - frame_) >= 400)) {
            pending_ = mask_ & 0x0f;
            if (!begun_ || uint32_t(now - slow_) >= 10000) {
                pending_ |= mask_ & 0x30;
                slow_ = now;
            }
            begun_ = true;
            frame_ = now;
            step.frame = true;
        }
        for (uint8_t i = 0; i < 6; ++i) {
            if (!(pending_ & (1u << i))) continue;
            pending_ &= ~(1u << i);
            active_ = step.start = i;
            conversion_ = now;
            break;
        }
        return step;
    }
private:
    uint8_t mask_, pending_ = 0;
    int8_t active_ = -1;
    bool begun_ = false;
    uint32_t frame_ = 0, slow_ = 0, conversion_ = 0;
};

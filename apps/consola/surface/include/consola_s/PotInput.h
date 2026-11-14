#pragma once
#include <stdint.h>
#include "AnalogChange.h"

/** Retains pot output on low ADC samples; see consola/18 §10 for electrical ambiguity. */
class PotInput {
public:
    /** First valid ADC12 count; lower readings freeze the filter immediately. */
    static constexpr uint16_t VALID_MIN = 41;
    /** Low samples must span strictly more than this duration to confirm open. */
    static constexpr uint32_t CONFIRM_US = 200000;
    /** Longer acquisition gaps restart pending qualification. */
    static constexpr uint32_t MAX_SAMPLE_GAP_US = 20000;
    /** Feed one newly acquired sample; only valid samples can produce MIDI output. */
    bool sample(uint16_t raw, uint32_t now_us, uint8_t& midi);
    /** Confirmed low-input diagnosis, retained until a valid sample arrives. */
    bool disconnected() const { return open_; }
private:
    AnalogChange filter_;
    uint32_t low_since_ = 0, last_sample_ = 0;
    bool suspect_ = false, open_ = false;
};

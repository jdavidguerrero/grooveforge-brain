#include "consola_s/PotInput.h"

bool PotInput::sample(uint16_t raw, uint32_t now_us, uint8_t& midi) {
    if (raw >= VALID_MIN) {
        suspect_ = open_ = false;
        return filter_.sample(raw, midi);
    }
    if (!suspect_ || uint32_t(now_us - last_sample_) > MAX_SAMPLE_GAP_US) {
        suspect_ = true;
        low_since_ = now_us;
    }
    last_sample_ = now_us;
    if (uint32_t(now_us - low_since_) > CONFIRM_US) open_ = true;
    return false;
}

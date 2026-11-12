#include "consola_s/FsrVelocity.h"

FsrVelocity::FsrVelocity(Calibration calibration, uint32_t window_us, uint32_t guard_us, bool require_rest)
    : calibration_(calibration), window_us_(window_us), guard_us_(guard_us),
      state_(require_rest ? State::Rest : State::Armed) {}

FsrVelocity::Event FsrVelocity::sample(uint16_t value, uint32_t now_us) {
    const Event NONE{false, false, 0};
    if (calibration_.maximum <= calibration_.threshold ||
        calibration_.release >= calibration_.threshold) return NONE;
    if (state_ == State::Rest) {
        if (value > calibration_.release) resting_ = false;
        else if (!resting_) { resting_ = true; rest_since_ = now_us; }
        else if (uint32_t(now_us - rest_since_) >= 3000) state_ = State::Armed;
        return NONE;
    }
    if (state_ == State::Guard) {
        if (value <= calibration_.release && now_us - attack_us_ >= guard_us_)
            state_ = State::Armed;
        return NONE;
    }
    if (state_ == State::Armed) {
        if (value >= calibration_.threshold) {
            peak_ = value;
            attack_us_ = now_us;
            state_ = State::Peak;
        }
        return NONE;
    }
    if (state_ == State::Peak) {
        if (value > peak_) peak_ = value;
        if (now_us - attack_us_ < window_us_) return NONE;
        const uint16_t top = peak_ > calibration_.maximum ? calibration_.maximum : peak_;
        const uint8_t velocity = 1 + uint32_t(top - calibration_.threshold) * 126 /
                                      (calibration_.maximum - calibration_.threshold);
        state_ = State::Held;
        return {true, true, velocity};
    }
    if (value <= calibration_.release) {
        state_ = State::Guard;
        return {true, false, 0};
    }
    return NONE;
}

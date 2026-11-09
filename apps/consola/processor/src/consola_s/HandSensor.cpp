#include "consola_s/HandSensor.h"

HandSensor::HandSensor(Calibration calibration) : calibration_(calibration) {}

HandSensor::Event HandSensor::update_presence(bool candidate, uint8_t value) {
    bool presence_changed = false;
    if (candidate == present_) confirmations_ = 0;
    else if (++confirmations_ >= 2) {
        present_ = candidate;
        confirmations_ = 0;
        presence_changed = true;
    }
    if (!present_) {
        if (presence_changed) last_midi_ = 0;
        return {presence_changed, presence_changed, false, 0};
    }
    const int delta = int(value) - last_midi_;
    const bool cc_changed = presence_changed || delta >= 2 || delta <= -2 ||
        (value != last_midi_ && (value == 0 || value == 127));
    if (cc_changed) last_midi_ = value;
    return {cc_changed, presence_changed, true, value};
}

HandSensor::Event HandSensor::sample(int32_t raw, uint32_t now_ms) {
    last_seen_ = now_ms;
    seen_ = true;
    if (updated_ && now_ms - last_update_ < 40) return {false,false,present_,last_midi_};
    last_update_ = now_ms;
    updated_ = true;
    if (calibration_.near <= calibration_.far || calibration_.far < 0 ||
        calibration_.near > 26400 || calibration_.hysteresis < 0 ||
        calibration_.hysteresis > calibration_.far || raw < 0 || raw > 26400) {
        filtered_ = false;
        return update_presence(false,last_midi_);
    }
    if (!filtered_) filtered_q8_ = raw * 256;
    else filtered_q8_ += (raw * 256 - filtered_q8_) / 4;
    filtered_ = true;
    const int32_t filtered = (filtered_q8_ + 128) / 256;
    const int32_t margin = present_ ? calibration_.hysteresis : 0;
    const bool candidate = filtered >= calibration_.far - margin &&
                           filtered <= calibration_.near + margin;
    int32_t bounded = filtered;
    if (bounded < calibration_.far) bounded = calibration_.far;
    if (bounded > calibration_.near) bounded = calibration_.near;
    const uint8_t midi = (bounded-calibration_.far) * 127 /
                        (calibration_.near-calibration_.far);
    return update_presence(candidate,midi);
}

HandSensor::Event HandSensor::expire(uint32_t now_ms) {
    if (!seen_ || now_ms - last_seen_ < 200) return {false,false,present_,last_midi_};
    const bool changed = present_;
    seen_ = updated_ = filtered_ = present_ = false;
    confirmations_ = last_midi_ = 0;
    return {changed,changed,false,0};
}

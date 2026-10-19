#include "lfo.h"

#include <math.h>

namespace dsp {

namespace {
constexpr float RATE_MIN = 0.05f, RATE_MAX = 20.0f;
constexpr float TWO_PI = 6.28318530718f;
}  // namespace

void Lfo::set_rate_hz(float hz) { rate_hz_ = hz < RATE_MIN ? RATE_MIN : hz > RATE_MAX ? RATE_MAX : hz; }

void Lfo::set_sync(bool on, float bpm, float beats_per_cycle) {
    sync_ = on;
    if (bpm > 0.0f && beats_per_cycle > 0.0f) sync_hz_ = bpm / 60.0f / beats_per_cycle;
}

float Lfo::rate_hz() const { return sync_ ? sync_hz_ : rate_hz_; }

float Lfo::tick(float dt_s) {
    phase_ += rate_hz() * dt_s;
    if (phase_ >= 1.0f) {
        phase_ -= floorf(phase_);
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        held_ = float(rng_ >> 8) / 8388607.5f - 1.0f;   // 24 bits → [-1, 1]
    }
    value_ = sample();
    return value_;
}

float Lfo::sample() {
    const float p = phase_;
    switch (shape_) {
        case LfoShape::Sine: return sinf(TWO_PI * p);
        case LfoShape::Tri:  return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
        case LfoShape::Saw:  return 2.0f * p - 1.0f;
        case LfoShape::Sqr:  return p < 0.5f ? 1.0f : -1.0f;
        case LfoShape::SampleHold: return held_;
    }
    return 0.0f;
}

}  // namespace dsp

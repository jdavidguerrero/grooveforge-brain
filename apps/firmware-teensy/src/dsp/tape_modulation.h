#pragma once

#include <math.h>

namespace dsp {

/** @brief Control compartido de wow/flutter y azimut del efecto TAPE. */
class TapeModulation {
public:
    struct Result { float pitch; float cutoff; };

    /** @brief Avanza los LFO usando segundos y devuelve desviación de pitch y cutoff en Hz. */
    Result tick(float dt, float wow, float flutter, float age) {
        wow_phase_ += 0.5f * dt;
        wow_phase2_ += 0.5f * 0.73f * dt;
        flutter_phase_ += 6.0f * dt;
        flutter_phase2_ += 6.0f * 1.05f * dt;
        if (wow_phase_ > 1.0f) wow_phase_ -= 1.0f;
        if (wow_phase2_ > 1.0f) wow_phase2_ -= 1.0f;
        if (flutter_phase_ > 1.0f) flutter_phase_ -= 1.0f;
        if (flutter_phase2_ > 1.0f) flutter_phase2_ -= 1.0f;
        constexpr float CYCLE_RADIANS = 6.2831853071795864769f;
        const float w = sinf(CYCLE_RADIANS * wow_phase_) + 0.3f * sinf(CYCLE_RADIANS * wow_phase2_);
        const float f = sinf(CYCLE_RADIANS * flutter_phase_) + 0.3f * sinf(CYCLE_RADIANS * flutter_phase2_);
        const float cents = wow * 18.0f * w + flutter * 10.0f * f;
        const float base = 20000.0f * powf(0.3f, age);
        float cutoff = (wow > 0.0f || flutter > 0.0f) ? base * (1.0f + 0.12f * (wow * w + 0.5f * flutter * f)) : base;
        cutoff = cutoff < 2500.0f ? 2500.0f : cutoff > 20000.0f ? 20000.0f : cutoff;
        return {powf(2.0f, cents / 1200.0f) - 1.0f, cutoff};
    }

private:
    float wow_phase_ = 0.0f, wow_phase2_ = 0.0f;
    float flutter_phase_ = 0.0f, flutter_phase2_ = 0.0f;
};

}  // namespace dsp

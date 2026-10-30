#pragma once

namespace dsp {

/** @brief Calcula la respuesta de velocity del Prophet para una profundidad normalizada. */
inline float prophet_velocity_gain(float velocity, float depth) {
    return 1.0f - depth * (1.0f - velocity);
}

/** @brief Cutoff del Prophet antes de POLY-MOD, WHEEL-MOD y keyboard tracking. */
inline float prophet_envelope_cutoff(float base_hz, float amount, float velocity_gain, float envelope,
                                     float lfo_multiplier, float voice_multiplier, float drift_multiplier) {
    return (base_hz + amount * velocity_gain * envelope * 6000.0f) * lfo_multiplier * voice_multiplier * drift_multiplier;
}

}  // namespace dsp

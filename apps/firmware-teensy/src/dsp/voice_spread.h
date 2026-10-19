#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file voice_spread.h
 * @brief Diferencias fijas entre voces ("voice cards"): desafinación y cutoff por voz, deterministas por semilla.
 *
 * En un poly analógico cada voz es un circuito distinto; esa diferencia mínima es parte del sonido (el VINTAGE del
 * Prophet-5 rev 4 la imita). Valores por engine en theory/fidelidad-engines.md §10.
 */

namespace dsp {

struct VoiceSpread {
    float pitch = 1.0f;    ///< multiplica la frecuencia de la voz
    float cutoff = 1.0f;   ///< multiplica el cutoff de la voz
};

/// @param cents  desafinación máxima (±) · @param cutoff_frac  variación máxima del cutoff (±, 0.06 = 6 %)
inline VoiceSpread voice_spread(uint32_t seed, float cents, float cutoff_frac) {
    uint32_t s = seed ? seed : 0x9E3779B9u;
    auto next = [&s]() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return float(int32_t(s)) * (1.0f / 2147483648.0f);   // [-1, 1)
    };
    next();
    VoiceSpread v;
    v.pitch = powf(2.0f, next() * cents / 1200.0f);
    v.cutoff = 1.0f + next() * cutoff_frac;
    return v;
}

}  // namespace dsp

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

namespace dsp {

/// UNISON (theory/09 de Studio §5): razón de frecuencia de la voz `k` de `n`, repartidas simétricas en ±cents/2
/// (el ancho total entre la más grave y la más aguda es `cents`). Con n = 1 no desafina.
inline float unison_ratio(uint8_t k, uint8_t n, float cents) {
    if (n <= 1) return 1.0f;
    const float x = float(k) / float(n - 1) - 0.5f;   // −0.5 … +0.5
    return powf(2.0f, x * cents / 1200.0f);
}

/// Ganancia para que N voces al unísono suenen parecido a una (suma incoherente: 1/√N).
inline float unison_gain(uint8_t n) { return n <= 1 ? 1.0f : 1.0f / sqrtf(float(n)); }

}  // namespace dsp

namespace dsp {

/// DRIFT (theory/09 de Studio §6): en cada nota, una desafinación aleatoria de hasta ±12 ¢ y un cutoff ±5 % por
/// voz, escalados por `amount` 0–1. Distinto de VoiceSpread (fijo por voz): cambia nota a nota, como un VCO que
/// todavía no se asentó.
struct Drift {
    float amount = 0.0f;
    uint32_t state = 0x2545F491u;

    float rnd() {   // [-1, 1)
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return float(int32_t(state)) * (1.0f / 2147483648.0f);
    }
    float pitch() { return amount > 0.0f ? powf(2.0f, rnd() * 12.0f * amount / 1200.0f) : 1.0f; }
    float cutoff() { return amount > 0.0f ? 1.0f + rnd() * 0.05f * amount : 1.0f; }
};

}  // namespace dsp

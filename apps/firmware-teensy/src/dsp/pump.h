#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file pump.h
 * @brief Curva del PUMP (ducking sincronizado al reloj común), lógica pura (theory/09 de Studio §2).
 *
 * En el inicio de cada ciclo de `rate_beats` (corrido por `offset_ms`) la ganancia cae a 1 − depth y se recupera
 * en `release_ms` con la forma elegida, sin pasar del largo del ciclo.
 */
namespace dsp {

enum class PumpShape : uint8_t { Smooth = 0, Exp, Kick };

struct PumpParams {
    float depth = 0.5f;        ///< 0–1
    PumpShape shape = PumpShape::Kick;
    float rate_beats = 1.0f;   ///< 1 = cada beat (1/4), 0.5 = 1/8, 2 = cada dos beats
    float offset_ms = 0.0f;    ///< corre el ciclo (+ = más tarde)
    float release_ms = 180.0f;
};

/// Ganancia 0–1 en la posición `pos_beats` del reloj, a `bpm`.
inline float pump_gain(const PumpParams& p, double pos_beats, float bpm) {
    if (p.depth <= 0.0f || bpm <= 0.0f || p.rate_beats <= 0.0f) return 1.0f;
    const double ms_per_beat = 60000.0 / double(bpm);
    const double cycle_ms = double(p.rate_beats) * ms_per_beat;
    double t = pos_beats * ms_per_beat - double(p.offset_ms);   // ms desde el origen, corrido
    t = fmod(t, cycle_ms);
    if (t < 0.0) t += cycle_ms;
    const float rel = fminf(p.release_ms, float(cycle_ms) * 0.95f);
    const float x = float(t);
    float r;   // 0 = abajo del todo, 1 = recuperado
    switch (p.shape) {
        case PumpShape::Smooth:
            r = x >= rel ? 1.0f : 0.5f - 0.5f * cosf(3.14159265f * x / rel);
            break;
        case PumpShape::Exp:
            r = 1.0f - expf(-4.6f * x / rel);
            break;
        case PumpShape::Kick:
        default: {
            const float attack = 3.0f, hold = 25.0f;
            if (x < attack) r = 1.0f - x / attack;                       // cae en 3 ms (sin click)
            else if (x < attack + hold) r = 0.0f;
            else r = 1.0f - expf(-4.6f * (x - attack - hold) / fmaxf(rel - attack - hold, 1.0f));
            break;
        }
    }
    if (r > 1.0f) r = 1.0f;
    if (r < 0.0f) r = 0.0f;
    return 1.0f - p.depth * (1.0f - r);
}

}  // namespace dsp

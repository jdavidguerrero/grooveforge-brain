#pragma once

/**
 * @file ladder_core.h
 * @brief Un paso del modelo de ladder de D'Angelo & Välimäki (ICASSP 2013), sin E/S (código custom).
 *
 * El objeto de audio MoogLadder4P lo llama por muestra; los tests native lo comparan contra la versión con `tanhf`.
 * Cada tanh se calcula una sola vez por muestra (5 en total; antes eran 7). theory/fidelidad-engines.md §1.
 */

namespace dsp {

struct LadderState {
    float V[4] = {};    ///< salida de cada etapa
    float dV[4] = {};   ///< derivadas de la muestra anterior (integración trapezoidal, ec. 15)
};

/**
 * @param vin     entrada en el dominio de voltaje del modelo
 * @param g       4·fs·A·VT (ecs. 13–14), precomputado por frequency()
 * @param k       resonancia 0–4 (4 = auto-oscilación)
 * @param inv2vt  1 / (2·VT)
 * @param inv2fs  1 / (2·fs)
 * @tparam TANH   función tanh a usar (rápida en el firmware, `tanhf` como referencia en los tests)
 * @return V4, la salida del ladder
 */
template <float (*TANH)(float)>
inline float ladder_step(LadderState& s, float vin, float g, float k, float inv2vt, float inv2fs) {
    const float t0 = TANH(s.V[0] * inv2vt);
    const float t1 = TANH(s.V[1] * inv2vt);
    const float t2 = TANH(s.V[2] * inv2vt);
    const float t3 = TANH(s.V[3] * inv2vt);
    const float tin = TANH((vin + k * s.V[3]) * inv2vt);
    const float d0 = -g * (t0 + tin);
    const float d1 = g * (t0 - t1);
    const float d2 = g * (t1 - t2);
    const float d3 = g * (t2 - t3);
    s.V[0] += (d0 + s.dV[0]) * inv2fs;
    s.V[1] += (d1 + s.dV[1]) * inv2fs;
    s.V[2] += (d2 + s.dV[2]) * inv2fs;
    s.V[3] += (d3 + s.dV[3]) * inv2fs;
    s.dV[0] = d0; s.dV[1] = d1; s.dV[2] = d2; s.dV[3] = d3;
    return s.V[3];
}

}  // namespace dsp

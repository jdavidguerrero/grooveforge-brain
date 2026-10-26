#pragma once

#include <stdint.h>

#include <gf_params.h>

#include "../engines/dx7_core.h"
#include "page_params.h"

/**
 * @file groove_macros.h
 * @brief Macros DRIFT · GRIT · POCKET de la página GROOVE (theory/09 de Studio §6), lógica pura.
 *
 * Son desplazamientos sobre el patch: el sketch le manda al engine `macro_patch(p)` en vez de `p`, y el patch
 * guardado (lo que editan los encoders y va al preset) no cambia. DRIFT vive en los engines (cambia nota a nota).
 */
namespace studio {

inline float macro_amount(const Patch& p, uint8_t slot) { return p.groove[GF_GROOVE_MACRO][slot] * 0.01f; }

/// Patch con GRIT y POCKET aplicados, para el engine `e`.
inline Patch macro_patch(const Patch& p, uint8_t e) {
    Patch q = p;
    auto toward = [](float v, float target, float a) { return v + (target - v) * a; };
    auto add = [](float v, float d, float hi) { return v + d > hi ? hi : v + d; };
    const float pocket = macro_amount(p, GF_MACRO_POCKET);
    if (pocket > 0.0f) {   // tiempos cortos y FLT ENV AMT alto: notas que se apagan solas, el "pocket" de un bajo
        // Solo acorta: un tiempo que ya era más corto que el destino queda igual; FLT ENV AMT solo sube
        auto shorter = [&](float v, float target) { return v > target ? toward(v, target, pocket) : v; };
        q.decay_ms = shorter(p.decay_ms, 200.0f);
        q.release_ms = shorter(p.release_ms, 250.0f);
        q.f_decay_ms = shorter(p.f_decay_ms, 150.0f);
        q.f_release_ms = shorter(p.f_release_ms, 200.0f);
        if (p.env_amt < 0.8f) q.env_amt = toward(p.env_amt, 0.8f, pocket);
    }
    const float grit = macro_amount(p, GF_MACRO_GRIT);
    const float gmod = grit * macro_amount(p, GF_MACRO_GRIT_MOD);   // profundidad por preset
    if (grit > 0.0f) {
        q.drive = add(p.drive, 0.6f * grit, 1.0f);
        switch (e) {
            case 2:   // Prophet POLY-MOD: FILT ENV y OSC B (suenan en los destinos que estén encendidos)
            case 3:   // OB-6 X-MOD: FILT ENV y VCO 2
                q.mod[0][0] = add(p.mod[0][0], 40.0f * gmod, 100.0f);
                q.mod[0][1] = add(p.mod[0][1], 40.0f * gmod, 100.0f);
                break;
            case 4: {  // DX7: más FM = moduladores más fuertes y más feedback (las portadoras no cambian el nivel)
                const int alg = int(p.osc[0][2] + 0.5f);
                for (int k = 0; k < 4; ++k)
                    if (!dx7_is_carrier(alg, k)) q.osc[k][1] = add(p.osc[k][1], 20.0f * gmod, 99.0f);
                q.osc[0][3] = add(p.osc[0][3], float(int(3.0f * gmod + 0.5f)), 7.0f);
                break;
            }
            default: break;
        }
    }
    return q;
}

/// Resonancia normalizada 0–1 con GRIT (+20 % del rango × GRIT RES).
inline float macro_resonance(float reso_norm, const Patch& p) {
    const float r = reso_norm + 0.2f * macro_amount(p, GF_MACRO_GRIT) * macro_amount(p, GF_MACRO_GRIT_RES);
    return r > 1.0f ? 1.0f : r;
}

/// Profundidades de velocity que recibe el engine (theory/10 §2): las del preset + POCKET (tope 1).
/// POCKET suma 65 % al volumen y 50 % al filtro: en el Model D y el Juno (0 % por defecto) devuelve la velocity.
inline void macro_vel_depths(const Patch& p, float* amp, float* filter) {
    const float pocket = macro_amount(p, GF_MACRO_POCKET) * macro_amount(p, GF_MACRO_POCKET_VEL);
    const float a = p.vel_amp * 0.01f + 0.65f * pocket, f = p.vel_cut * 0.01f + 0.5f * pocket;
    *amp = a > 1.0f ? 1.0f : a;
    *filter = f > 1.0f ? 1.0f : f;
}

}  // namespace studio

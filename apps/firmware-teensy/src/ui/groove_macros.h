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
        q.decay_ms = toward(p.decay_ms, 200.0f, pocket);
        q.release_ms = toward(p.release_ms, 250.0f, pocket);
        q.f_decay_ms = toward(p.f_decay_ms, 150.0f, pocket);
        q.f_release_ms = toward(p.f_release_ms, 200.0f, pocket);
        q.env_amt = toward(p.env_amt, 0.8f, pocket);
    }
    const float grit = macro_amount(p, GF_MACRO_GRIT);
    if (grit > 0.0f) {
        q.drive = add(p.drive, 0.6f * grit, 1.0f);
        switch (e) {
            case 2:   // Prophet POLY-MOD: FILT ENV y OSC B (suenan en los destinos que estén encendidos)
            case 3:   // OB-6 X-MOD: FILT ENV y VCO 2
                q.mod[0][0] = add(p.mod[0][0], 40.0f * grit, 100.0f);
                q.mod[0][1] = add(p.mod[0][1], 40.0f * grit, 100.0f);
                break;
            case 4: {  // DX7: más FM = moduladores más fuertes y más feedback (las portadoras no cambian el nivel)
                const int alg = int(p.osc[0][2] + 0.5f);
                for (int k = 0; k < 4; ++k)
                    if (!dx7_is_carrier(alg, k)) q.osc[k][1] = add(p.osc[k][1], 20.0f * grit, 99.0f);
                q.osc[0][3] = add(p.osc[0][3], float(int(3.0f * grit + 0.5f)), 7.0f);
                break;
            }
            default: break;
        }
    }
    return q;
}

/// Resonancia normalizada 0–1 con GRIT (+20 % del rango).
inline float macro_resonance(float reso_norm, const Patch& p) {
    const float r = reso_norm + 0.2f * macro_amount(p, GF_MACRO_GRIT);
    return r > 1.0f ? 1.0f : r;
}

/// Velocity que llega al engine: el Model D y el Juno la ignoran (como los originales) salvo con POCKET.
inline float macro_velocity(float vel, uint8_t e, const Patch& p) {
    if (e > 1) return vel;
    const float pocket = macro_amount(p, GF_MACRO_POCKET);
    return 1.0f + (vel - 1.0f) * pocket;
}

}  // namespace studio

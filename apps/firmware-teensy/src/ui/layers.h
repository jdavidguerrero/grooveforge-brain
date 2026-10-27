#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file layers.h
 * @brief Capas híbridas de dos engines, v1 (variants/studio/theory/12 §3b), lógica pura.
 *
 * Las 8 parejas de la guía de híbridos de Juan (paleta-v1 §6). Cada capa apunta a sus dos mitades HY por nombre;
 * MORPH hace el crossfade de igual potencia entre A (ataque / definición) y B (cuerpo / peso).
 */
namespace studio {

struct LayerDef {
    const char* name;
    uint8_t eng_a;
    const char* preset_a;
    uint8_t eng_b;
    const char* preset_b;
    float gain_a_db, gain_b_db;   ///< mezcla base con MORPH al 50 % (paleta-v1 §6)
    int8_t transpose_b;           ///< semitonos de B respecto de la nota tocada
    uint8_t delay_b_ms;           ///< retraso del ataque de B: deja libre el transiente de A
};

constexpr uint8_t LAYER_COUNT = 8;

// Engines: 0 Model D · 1 Juno · 2 Prophet · 3 OB-6 · 4 DX7 · 5 ARP
inline const LayerDef& layer_def(uint8_t i) {
    static const LayerDef L[LAYER_COUNT] = {
        {"WOODBASS",     4, "HY WOODBASS A",    0, "HY WOODBASS B",    -6, 0, 0, 10},
        {"KALIMBA",      4, "HY KALIMBA A",     3, "HY KALIMBA B",     0, -6, 0, 0},
        {"FRENCH STAB",  1, "HY FRENCH STAB A", 2, "HY FRENCH STAB B", 0, -3, 0, 0},
        {"ACID ARP",     5, "HY ACID ARP A",    0, "HY ACID ARP B",    0, -4, 0, 0},
        {"DESERT LEAD",  3, "HY DESERT LEAD A", 2, "HY DESERT LEAD B", 0, -3, 0, 0},
        {"SHAKER ZAP",   5, "HY SHAKER ZAP A",  4, "HY SHAKER ZAP B",  0, -12, 0, 0},
        {"ROLLER SUB",   0, "HY ROLLER SUB A",  1, "HY ROLLER SUB B",  0, -6, 0, 0},
        {"RETRO BRASS",  2, "HY RETRO BRASS A", 1, "HY RETRO BRASS B", 0, -10, 0, 0}};
    return L[i < LAYER_COUNT ? i : 0];
}

/// Ganancias lineales de A y B: crossfade de igual potencia sobre la mezcla base. `morph` 0–1:
/// 0 = solo A, 0.5 = las dos a su ganancia base, 1 = solo B.
inline void layer_gains(const LayerDef& d, float morph, float* ga, float* gb) {
    if (morph < 0.0f) morph = 0.0f;
    if (morph > 1.0f) morph = 1.0f;
    const float x = morph * 1.5707963f;
    const float wa = fminf(1.0f, cosf(x) * 1.4142136f), wb = fminf(1.0f, sinf(x) * 1.4142136f);
    *ga = wa * powf(10.0f, d.gain_a_db / 20.0f);
    *gb = wb * powf(10.0f, d.gain_b_db / 20.0f);
}

/// Nota que toca B (transpuesta y dentro de 0–127).
inline uint8_t layer_b_note(uint8_t note, int8_t transpose) {
    const int n = int(note) + transpose;
    return uint8_t(n < 0 ? 0 : n > 127 ? 127 : n);
}

/// CC 18 → preset de capa: 0 = sin capa (sale a SYNTH), 1…count = el preset (el resto de 1–127 en tramos iguales).
inline uint8_t layer_from_cc(uint8_t value, uint8_t count = LAYER_COUNT) {
    if (value == 0 || count == 0) return 0;
    const unsigned i = 1u + (unsigned(value) - 1u) * count / 127u;
    return uint8_t(i > count ? count : i);
}

}  // namespace studio

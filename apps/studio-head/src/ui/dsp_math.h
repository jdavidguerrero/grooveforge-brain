#pragma once

#include <algorithm>
#include <cmath>

#include "state/studio_state.h"

/**
 * @file dsp_math.h
 * @brief Funciones de dibujo compartidas por las pantallas (mismas fórmulas que studio-data.js).
 */
namespace gf {

/// Posición 0–1 de una frecuencia en el eje logarítmico 20 Hz–20 kHz.
inline float freq_frac(float f) {
    return std::clamp(std::log(f / 20.0f) / std::log(1000.0f), 0.0f, 1.0f);
}

/// Frecuencia en la posición 0–1 del eje logarítmico.
inline float frac_freq(float x) { return 20.0f * std::pow(1000.0f, x); }

/// Respuesta aproximada de un low-pass de 4 polos (24 dB/oct) con pico de resonancia, en dB.
inline float filter_db(float f, float fc, float res) {
    const float r = f / fc;
    const float l2 = std::log2(r);
    return -10.0f * std::log10(1.0f + std::pow(r, 8.0f)) + res * 14.0f * std::exp(-(l2 * l2) / 0.05f);
}

/// Respuesta de magnitud en dB del filtro Oberheim SEM SVF de 2 polos (12 dB/oct) con morph continuo LP↔Notch↔HP o BP.
inline float sem_filter_db(float f, float fc, float res, float morph, bool bp_mode) {
    const float r = std::max(1e-4f, f / std::max(20.0f, fc));
    const float r2 = r * r;
    const float k = std::clamp(2.0f - 1.9f * res, 0.12f, 2.0f);
    // H_LP(j*r) = 1 / (1 - r^2 + j*k*r), H_BP(j*r) = j*k*r / (1 - r^2 + j*k*r), H_HP(j*r) = -r^2 / (1 - r^2 + j*k*r)
    const float den2 = (1.0f - r2) * (1.0f - r2) + (k * r) * (k * r);
    float num2 = 1.0f;
    if (bp_mode) {
        num2 = (r * std::max(0.5f, k)) * (r * std::max(0.5f, k));
    } else {
        const float m = std::clamp(morph, 0.0f, 1.0f);
        const float c_lp = 1.0f - m;
        const float c_hp = m;
        // Real part of numerator: c_lp - c_hp * r^2
        const float re = c_lp - c_hp * r2;
        num2 = re * re + 1e-6f;
    }
    return std::clamp(10.0f * std::log10(num2 / std::max(1e-8f, den2)), -60.0f, 14.0f);
}

/// Respuesta en dB según el engine activo (OB-6 = 2-pole SEM SVF con morph; resto = 4-pole 24 dB/oct).
inline float engine_filter_db(const StudioState& s, float f) {
    if (s.engine == 3) {
        return sem_filter_db(f, s.cutoff_hz, s.resonance, s.osc_val[3][0][6] / 100.0f, s.osc_val[3][0][7] > 0.5f);
    }
    return filter_db(f, s.cutoff_hz, s.resonance);
}

}  // namespace gf

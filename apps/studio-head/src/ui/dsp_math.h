#pragma once

#include <algorithm>
#include <cmath>

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

/// Respuesta aproximada de un low-pass de 4 polos con pico de resonancia, en dB.
inline float filter_db(float f, float fc, float res) {
    const float r = f / fc;
    const float l2 = std::log2(r);
    return -10.0f * std::log10(1.0f + std::pow(r, 8.0f)) + res * 14.0f * std::exp(-(l2 * l2) / 0.05f);
}

}  // namespace gf

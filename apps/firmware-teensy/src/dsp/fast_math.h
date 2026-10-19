#pragma once

/**
 * @file fast_math.h
 * @brief Funciones matemáticas rápidas para el loop de audio (código custom).
 *
 * theory/fidelidad-engines.md §1: `tanhf` cuesta ~200 ciclos en el Cortex-M7 de la Teensy 4.1; el aproximante de
 * Padé [7/6] cuesta ~25 (12 multiplicaciones y una división) con error < 2·10⁻⁵ en |x| < 4.
 */

namespace dsp {

/// tanh(x) por Padé [7/6], recortado a ±1 fuera de |x| ≤ 4.97 (donde el aproximante cruza 1).
inline float fast_tanh(float x) {
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2));
    return num / den;
}

}  // namespace dsp

#pragma once

/**
 * @file fast_math.h
 * @brief Funciones matemáticas rápidas para el loop de audio (código custom).
 *
 * theory/fidelidad-engines.md §1: `tanhf` cuesta ~200 ciclos en el Cortex-M7 de la Teensy 4.1; el aproximante de
 * Padé [7/6] cuesta ~25 (12 multiplicaciones y una división) con error < 2·10⁻⁵ en |x| < 4.
 */

#include <math.h>
#include <stdint.h>

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

/// 2^x: parte entera por exponente IEEE y parte fraccionaria por polinomio minimax de grado 5 (error relativo < 1·10⁻⁵).
/// La FM de audio del X-MOD (theory/fase2-engines.md §1) lo usa por muestra; `exp2f` costaba ~4 % de CPU en 6 voces.
inline float fast_exp2(float x) {
    if (x < -24.0f) x = -24.0f;
    if (x > 24.0f) x = 24.0f;
    const float fl = floorf(x);
    const float f = x - fl;
    const float p =
        1.0f + f * (0.6931531f + f * (0.2401536f + f * (0.0558263f + f * (0.0089893f + f * 0.0018776f))));
    union { float v; int32_t i; } u{p};
    u.i += int32_t(fl) << 23;
    return u.v;
}

}  // namespace dsp

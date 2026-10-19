#pragma once

#include "fast_math.h"
#include <math.h>
#include <stdint.h>

/**
 * @file sem_svf_core.h
 * @brief Filtro State-Variable de 2 polos (12 dB/oct) estilo Oberheim SEM / OB-6 con Morph LP↔NOTCH↔HP.
 *
 * Basado en integración trapezoidal Zero-Delay Feedback (Andrew Simper / Cytomic, 2013)
 * con saturación no lineal `fast_tanh` en el integrador de banda (emulando los OTAs CA3080 / CEM3320
 * del filtro SEM de Tom Oberheim en el Sequential OB-6).
 *
 * Controles distintivos del OB-6:
 *   - `morph` ∈ [0, 1]: `0.0 = LP`, `0.5 = NOTCH`, `1.0 = HP` (transición continua)
 *   - `bp_mode`: `true` selecciona la salida Band-Pass de 6 dB/oct
 *   - `drive` ∈ [1.0, 4.0]: saturación suave de entrada/resonador
 */

namespace dsp {

struct SemSvfState {
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
};

struct SemSvfCoeffs {
    float g = 0.05f;
    float k = 1.414f;     ///< amortiguamiento 2 - 2*res (k pequeño = alta resonancia)
    float a1 = 0.9f;
    float a2 = 0.045f;
    float a3 = 0.002f;
    float morph = 0.0f;   ///< 0 = LP, 0.5 = Notch (LP+HP), 1.0 = HP
    float drive = 1.0f;   ///< 1.0 .. 4.0
    float inv_drive_norm = 1.0f;
    bool bp_mode = false;
};

inline SemSvfCoeffs sem_svf_compute(float hz, float q, float morph, bool bp_mode, float drive, float fs) {
    if (hz < 20.0f) hz = 20.0f;
    if (hz > fs * 0.48f) hz = fs * 0.48f;
    if (q < 0.5f) q = 0.5f;
    if (q > 10.0f) q = 10.0f;
    if (morph < 0.0f) morph = 0.0f;
    if (morph > 1.0f) morph = 1.0f;
    if (drive < 0.5f) drive = 0.5f;
    if (drive > 4.0f) drive = 4.0f;

    SemSvfCoeffs c;
    // Pre-warping bilineal: g = tan(pi * fc / fs)
    c.g = tanf(3.14159265359f * hz / fs);
    // En el SEM de 2 polos (no auto-oscila hasta infinito, sino que satura cálidamente):
    // Mapeamos q ∈ [0.707, 5.0] a k ∈ [1.414, 0.08]
    const float res01 = (q - 0.707f) / (5.0f - 0.707f);
    const float r = res01 < 0.0f ? 0.0f : (res01 > 1.0f ? 1.0f : res01);
    c.k = 1.4142f * (1.0f - 0.94f * r);
    c.a1 = 1.0f / (1.0f + c.g * (c.g + c.k));
    c.a2 = c.g * c.a1;
    c.a3 = c.g * c.a2;
    c.morph = morph;
    c.bp_mode = bp_mode;
    c.drive = drive;
    c.inv_drive_norm = 1.0f / sqrtf(drive);
    return c;
}

inline float sem_svf_step(SemSvfState& s, float vin, const SemSvfCoeffs& c) {
    const float v0 = fast_tanh(vin * c.drive * 0.85f);
    const float v3 = v0 - s.ic2eq;
    const float v1 = c.a1 * s.ic1eq + c.a2 * v3;
    const float v2 = s.ic2eq + c.a2 * s.ic1eq + c.a3 * v3;
    // Saturación suave no lineal en el estado del resonador (carácter Oberheim SEM)
    s.ic1eq = fast_tanh(2.0f * v1 - s.ic1eq);
    s.ic2eq = 2.0f * v2 - s.ic2eq;

    const float lp = v2;
    const float bp = v1;
    const float hp = v0 - c.k * v1 - v2;

    float out;
    if (c.bp_mode) {
        out = bp;
    } else {
        // En el OB-6: morph=0 → 100% LP; morph=0.5 → 50% LP + 50% HP (Notch); morph=1.0 → 100% HP
        out = (1.0f - c.morph) * lp + c.morph * hp;
    }
    return out * c.inv_drive_norm;
}

}  // namespace dsp

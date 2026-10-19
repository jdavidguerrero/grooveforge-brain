#pragma once

#include <math.h>
#include <stdint.h>

#include "fast_math.h"

/**
 * @file zdf_ladder.h
 * @brief Ladder de 4 polos TPT / ZDF con no linealidad en el nodo de suma y carácter por chip (código custom).
 *
 * Zavalishin, *The Art of VA Filter Design* (rev. 2.1), cap. 5 (ladder TPT) y §6.6 ("cheap" nonlinear): la
 * realimentación se resuelve en la misma muestra, así que el filtro queda afinado también con resonancia (el modelo
 * D'Angelo anterior no pasaba de ~1.65 kHz al auto-oscilar). Teoría y carácter: theory/fidelidad-engines.md §9.1.
 */

namespace dsp {

/// Chip que se emula: misma topología (4 polos + realimentación global), distinta saturación y compensación.
enum class LadderCharacter : uint8_t { Moog = 0, Arp4072, Ir3109, Cem3320 };

struct LadderVoicing {
    float drive;       ///< ganancia antes del tanh de entrada (más alto = satura antes: el overdrive)
    float bass_comp;   ///< 0 = pierde graves con la resonancia (1 / (1 + k)); 1 = compensación completa
    float loop_limit;  ///< escala del límite del lazo: fija la amplitud de la auto-oscilación
    float stage_sat;   ///< saturación suave a la salida de cada etapa (transistores del ladder; 0 = lineal)
};

inline LadderVoicing ladder_voicing(LadderCharacter c) {
    switch (c) {
        case LadderCharacter::Arp4072: return {1.3f, 0.0f, 3.6f, 0.05f};
        case LadderCharacter::Ir3109:  return {1.1f, 0.35f, 2.5f, 0.02f};
        case LadderCharacter::Cem3320: return {1.0f, 0.5f, 3.0f, 0.02f};
        case LadderCharacter::Moog:
        default:                       return {1.6f, 0.0f, 4.5f, 0.05f};
    }
}

class ZdfLadder {
public:
    /// Cutoff en Hz (se recorta a 10 Hz – 0.45·fs). Precalcula G = g / (1 + g), g = tan(π·fc/fs).
    void set_cutoff(float hz, float fs) {
        if (hz < 10.0f) hz = 10.0f;
        if (hz > 0.45f * fs) hz = 0.45f * fs;
        const float g = tanf(3.14159265359f * hz / fs);
        G_ = g / (1.0f + g);
    }

    /// Resonancia k ∈ [0, 4.5]: con el límite del lazo, auto-oscila desde ~4.05 (MoogLadder4P lleva el tope a 4.4).
    void set_k(float k) { k_ = k < 0.0f ? 0.0f : k > 4.5f ? 4.5f : k; }

    void set_character(LadderCharacter c) {
        const LadderVoicing v = ladder_voicing(c);
        drive_ = v.drive;
        inv_drive_ = 1.0f / v.drive;
        comp_ = v.bass_comp;
        limit_ = v.loop_limit;
        inv_limit_ = 1.0f / v.loop_limit;
        stage_sat_ = v.stage_sat;
    }

    void reset() { s_[0] = s_[1] = s_[2] = s_[3] = 0.0f; }

    /// Una muestra (entrada y salida en ±1 nominal).
    float tick(float x) {
        const float G = G_, k = k_;
        const float H = 1.0f - G;
        // Contribución de los estados a la salida (y4 = G⁴·u + Σ)
        const float sigma = G * G * G * H * s_[0] + G * G * H * s_[1] + G * H * s_[2] + H * s_[3];
        const float G4 = G * G * G * G;
        // Entrada con saturación (par diferencial / OTA de entrada) y compensación de graves del chip
        const float xc = fast_tanh(drive_ * x) * inv_drive_ * (1.0f + comp_ * k);
        const float y4_lin = (G4 * xc + sigma) / (1.0f + k * G4);
        // Límite suave del lazo: ganancia 1 con señal chica (afinación exacta), comprime al crecer la oscilación
        float u = limit_ * fast_tanh((xc - k * y4_lin) * inv_limit_);
        for (int i = 0; i < 4; ++i) {
            const float v = G * (u - s_[i]);
            const float y = v + s_[i];
            s_[i] = y + v;
            // Saturación de la etapa sobre la señal que pasa a la siguiente (el estado queda lineal: afinación intacta)
            u = y / (1.0f + stage_sat_ * y * y);
        }
        return u;
    }

private:
    float G_ = 0.1f, k_ = 0.0f;
    float drive_ = 1.6f, inv_drive_ = 1.0f / 1.6f, comp_ = 0.0f;
    float limit_ = 3.5f, inv_limit_ = 1.0f / 3.5f;
    float stage_sat_ = 0.05f;
    float s_[4] = {};
};

}  // namespace dsp

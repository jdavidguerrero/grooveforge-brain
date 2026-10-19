#pragma once

#include "fast_math.h"

/**
 * @file output_stage_core.h
 * @brief Etapa de salida del synth: acople AC (pasa altos de 1 polo) y saturación suave (código custom).
 *
 * El VCA y el amplificador de salida de los originales no son ideales: el capacitor de acople quita el DC (el medio
 * seno del DX7, un pulse asimétrico) y la etapa comprime un poco los picos. Teoría: fidelidad-engines.md §11.6.
 */

namespace dsp {

class OutputStageCore {
public:
    /// Corte del acople (Hz) y saturación: `drive` = 1 es casi lineal; ganancia de señal chica siempre 1.
    void set(float hz, float drive, float fs) {
        r_ = 1.0f - 6.28318530718f * hz / fs;       // polo del bloqueador de DC (y = x − x₁ + r·y₁)
        drive_ = drive < 0.1f ? 0.1f : drive;
        inv_ = 1.0f / drive_;
    }

    float tick(float x) {
        const float y = x - x1_ + r_ * y1_;
        x1_ = x;
        y1_ = y;
        return fast_tanh(drive_ * y) * inv_;
    }

    void reset() { x1_ = y1_ = 0.0f; }

private:
    float r_ = 0.99886f;                  // 8 Hz a 44.1 kHz
    float drive_ = 1.2f, inv_ = 1.0f / 1.2f;
    float x1_ = 0.0f, y1_ = 0.0f;
};

}  // namespace dsp

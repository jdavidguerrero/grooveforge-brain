#pragma once

#include <math.h>
#include <stdint.h>

#include "fast_math.h"

/**
 * @file drive_core.h
 * @brief DRIVE de la cadena del synth (theory/fase3-fx.md §2), lógica pura. Código custom.
 *
 * Reemplaza al BIT, que metía demasiado ruido (Juan, 2026-10-27). Tres tipos:
 * - OVERDRIVE: tanh suave con un poco de asimetría (armónicos pares, "tubo"): calienta bajos y leads.
 * - DIST: más ganancia y recorte asimétrico más duro: indie tech, ácido.
 * - CRUSH: el bitcrusher domado (16 → 8 bits y 44.1 → 11 kHz como mucho): textura lo-fi sin el ruido del BIT.
 * TONE es un pasabajos de un polo después del recorte (2 → 18 kHz). El nivel se compensa para que subir AMOUNT no
 * suba el volumen tanto como la distorsión.
 */
namespace dsp {

class DriveCore {
public:
    enum Type : uint8_t { OVERDRIVE = 0, DIST = 1, CRUSH = 2 };

    void set_type(uint8_t t) { _type = t > CRUSH ? CRUSH : t; }
    void set_amount(float a) { _amt = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a; }
    /// TONE 0–1 → pasabajos de 2 kHz a 18 kHz.
    void set_tone(float t, float fs) {
        t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
        _g_tone = 1.0f - expf(-6.2831853f * 2000.0f * powf(9.0f, t) / fs);
    }
    void set_mix(float m) { _mix = m < 0.0f ? 0.0f : m > 1.0f ? 1.0f : m; }

    void render(const float* in, float* out, int n) {
        const float gain = _type == DIST ? 1.0f + 39.0f * _amt * _amt : 1.0f + 11.0f * _amt * _amt;
        const float comp = 1.0f / (1.0f + 0.35f * (gain - 1.0f) / (1.0f + 0.1f * gain));   // ~ nivel constante
        const int bits = 16 - int(8.0f * _amt + 0.5f);                    // CRUSH: 16 → 8 bits
        const float q = float(1 << (bits - 1));
        const int hold = 1 + int(3.0f * _amt + 0.5f);                      // CRUSH: 44.1 → 11 kHz
        for (int i = 0; i < n; ++i) {
            const float x = in[i];
            float y;
            if (_type == CRUSH) {
                if (++_count >= hold) {
                    _count = 0;
                    _held = floorf(x * q + 0.5f) / q;
                }
                y = _held;
            } else if (_type == DIST) {
                const float v = gain * x;
                y = v > 0.0f ? fast_tanh(v) : 0.8f * fast_tanh(1.25f * v);      // recorte asimétrico
            } else {
                const float v = gain * x + 0.08f;                             // asimetría suave: pares
                y = (fast_tanh(v) - fast_tanh(0.08f)) * comp * (1.0f + 0.2f * _amt);
            }
            if (_type == DIST) y *= comp * 1.6f;
            _lp += _g_tone * (y - _lp);
            out[i] = x + _mix * (_lp - x);
        }
    }

private:
    uint8_t _type = OVERDRIVE;
    float _amt = 0.35f, _mix = 1.0f, _g_tone = 1.0f, _lp = 0.0f, _held = 0.0f;
    int _count = 0;
};

}  // namespace dsp

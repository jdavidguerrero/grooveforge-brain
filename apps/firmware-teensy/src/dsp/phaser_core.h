#pragma once

#include <math.h>

/**
 * @file phaser_core.h
 * @brief Phaser de disco de 6 etapas (theory/fase3-fx.md §2), lógica pura. Código custom.
 *
 * Pasa-todos de primer orden barridos por un LFO triangular (200 Hz – 3 kHz, exponencial) con realimentación: las
 * muescas de los phasers de pedal de 4–6 etapas. Mezcla 50 / 50 con la señal seca (de ahí salen las muescas).
 */
namespace dsp {

class PhaserCore {
public:
    static constexpr int STAGES = 6;

    void set_rate(float hz) { _rate = hz < 0.02f ? 0.02f : hz > 10.0f ? 10.0f : hz; }
    /// Profundidad del barrido 0–1 (0 = muescas quietas en el centro).
    void set_depth(float d) { _depth = d < 0.0f ? 0.0f : d > 1.0f ? 1.0f : d; }
    /// Fase inicial del LFO en ciclos (R desfasado 0.25 para abrir el estéreo).
    void set_phase(float ph) { _ph = ph - floorf(ph); }

    void render(const float* in, float* out, int n, float fs) {
        const float inc = _rate / fs;
        for (int i = 0; i < n; ++i) {
            _ph += inc;
            if (_ph >= 1.0f) _ph -= 1.0f;
            if (_coefficient_phase == 0) {      // el barrido es lento: coeficiente cada 16 muestras
                const float tri = 1.0f - 4.0f * fabsf(_ph - 0.5f);                    // −1…1
                const float hz = 775.0f * powf(3000.0f / 200.0f, 0.5f * _depth * tri);   // centro ≈ √(200·3000)
                const float t = tanf(3.14159265f * (hz < fs * 0.45f ? hz : fs * 0.45f) / fs);
                _a = (t - 1.0f) / (t + 1.0f);                                     // coeficiente del pasa-todo
            }
            _coefficient_phase = (_coefficient_phase + 1) & 15;
            const float a = _a;
            float x = in[i] + FEEDBACK * _fb;
            for (int s = 0; s < STAGES; ++s) {
                const float y = a * x + _z[s];
                _z[s] = x - a * y;
                x = y;
            }
            _fb = x;
            out[i] = 0.5f * (in[i] + x);
        }
    }

private:
    static constexpr float FEEDBACK = 0.5f;
    float _rate = 0.5f, _depth = 0.6f, _ph = 0.0f, _fb = 0.0f, _a = 0.0f;
    float _z[STAGES] = {};
    unsigned _coefficient_phase = 0;
};

}  // namespace dsp

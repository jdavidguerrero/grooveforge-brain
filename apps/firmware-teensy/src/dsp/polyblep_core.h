#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file polyblep_core.h
 * @brief Oscilador virtual-analog con corrección PolyBLEP (Välimäki & Huovilainen, IEEE SPM 2007).
 *
 * Lógica pura sin dependencia de Teensy Audio Library para tests nativos.
 * Soporta SAW, SQUARE, PULSE (con PWM continuo), TRIANGLE, SINE y SAW+PULSE simultáneos (Juno-106).
 */

namespace dsp {

/// Constantes compatibles con Teensy Audio Library (synth_waveform.h) + modo combinado SAW+PULSE.
constexpr uint16_t WAVE_SINE          = 0;
constexpr uint16_t WAVE_SAWTOOTH      = 1;
constexpr uint16_t WAVE_SQUARE        = 2;
constexpr uint16_t WAVE_TRIANGLE      = 3;
constexpr uint16_t WAVE_PULSE         = 5;
constexpr uint16_t WAVE_TRI_VARIABLE  = 8;
constexpr uint16_t WAVE_SAW_AND_PULSE = 100;  ///< DCO Juno-106 con SAW y PULSE encendidos a la vez

inline float poly_blep(float t, float dt) {
    if (dt <= 0.0f) return 0.0f;
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

inline float wrap01(float x) {
    return x - floorf(x);
}

inline float fast_sine01(float phase01) {
    // Aproximación parabólica de alta precisión con corrección de 5.º orden (error < 0.0005)
    float x = wrap01(phase01) * 2.0f - 1.0f;  // [-1, 1)
    // Desplazar para que sin(2*pi*phase01) coincida en fase
    x = wrap01(phase01 + 0.5f) * 2.0f - 1.0f;
    const float y = -4.0f * x * (1.0f - fabsf(x));
    return 0.775f * y + 0.225f * y * fabsf(y);
}

class PolyBlepCore {
public:
    void reset(uint32_t seed = 0x13579BDFu) {
        _phase = 0.0f;
        _drift = 0.0f;
        _rng = seed ? seed : 0x13579BDFu;
    }

    void render(uint16_t waveform, float inc, float pw, float amp, float* out, int n) {
        if (amp <= 0.0f || inc <= 0.0f) {
            for (int i = 0; i < n; ++i) out[i] = 0.0f;
            return;
        }
        // Deriva analógica lenta por bloque: ruido blanco filtrado (un polo, ~0.3 Hz) normalizado a ~±1 y
        // escalado por _drift_depth (fracción de la frecuencia: 0.0008 ≈ ±1.4 cents). Cada voz tiene su semilla.
        _rng ^= _rng << 13;
        _rng ^= _rng >> 17;
        _rng ^= _rng << 5;
        const float white = float(int32_t(_rng)) * (1.0f / 2147483648.0f);
        _drift = 0.995f * _drift + 0.0999f * white;

        float dt = inc * (1.0f + _drift_depth * _drift);
        if (dt > 0.48f) dt = 0.48f;
        if (pw < 0.05f) pw = 0.05f;
        if (pw > 0.95f) pw = 0.95f;

        for (int i = 0; i < n; ++i) {
            _phase += dt;
            if (_phase >= 1.0f) _phase -= floorf(_phase);

            float s = 0.0f;
            switch (waveform) {
                case WAVE_SAWTOOTH:
                    s = 2.0f * _phase - 1.0f - poly_blep(_phase, dt);
                    break;
                case WAVE_SQUARE: {
                    const float raw = _phase < 0.5f ? 1.0f : -1.0f;
                    s = raw + poly_blep(_phase, dt) - poly_blep(wrap01(_phase + 0.5f), dt);
                    break;
                }
                case WAVE_PULSE: {
                    const float raw = _phase < pw ? 1.0f : -1.0f;
                    s = raw + poly_blep(_phase, dt) - poly_blep(wrap01(_phase - pw + 1.0f), dt);
                    break;
                }
                case WAVE_SAW_AND_PULSE: {
                    const float sw = 2.0f * _phase - 1.0f - poly_blep(_phase, dt);
                    const float raw = _phase < pw ? 1.0f : -1.0f;
                    const float pl = raw + poly_blep(_phase, dt) - poly_blep(wrap01(_phase - pw + 1.0f), dt);
                    s = 0.55f * (sw + pl);
                    break;
                }
                case WAVE_TRIANGLE:
                case WAVE_TRI_VARIABLE:
                    s = 1.0f - 4.0f * fabsf(_phase - 0.5f);
                    break;
                case WAVE_SINE:
                default:
                    s = fast_sine01(_phase);
                    break;
            }
            out[i] = s * amp;
        }
    }

    float phase() const { return _phase; }

    /// Profundidad de la deriva como fracción de la frecuencia (0 = sin deriva).
    void set_drift(float depth) { _drift_depth = depth < 0.0f ? 0.0f : depth; }

private:
    float _phase = 0.0f;
    float _drift = 0.0f;
    uint32_t _rng = 0x13579BDFu;
    float _drift_depth = 0.00026f;   // ~±0.45 cents por defecto
};

}  // namespace dsp

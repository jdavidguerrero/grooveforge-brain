#pragma once

#include "fast_math.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

/**
 * @file juno_chorus_core.h
 * @brief Emulación del Chorus BBD MN3009 y High-Pass Filter del Roland Juno-106.
 *
 * A diferencia de `AudioEffectChorus` de Teensy (que suma taps fijos sin LFO),
 * este núcleo implementa una línea de retardo fraccional modulada por un LFO
 * triangular con dos taps en antifase sumados en mono/bus, más el filtro paso-altos
 * de 4 posiciones del Juno-106 (`0 = Bass Boost`, `1 = Flat`, `2 = 225 Hz`, `3 = 720 Hz`).
 *
 * Modos del Chorus Juno-106:
 *   - 0: OFF (100% dry)
 *   - 1: CHORUS I   (rate = 0.513 Hz, depth = ±1.6 ms alrededor de 3.5 ms)
 *   - 2: CHORUS II  (rate = 0.863 Hz, depth = ±1.85 ms alrededor de 3.5 ms)
 *   - 3: CHORUS I+II (rate = 8.0 Hz, depth = ±0.35 ms — vibrato ensemble clásico de Roland)
 */

namespace dsp {

class JunoChorusCore {
public:
    static constexpr int BUF_LEN = 512;  // ~11.6 ms @ 44.1 kHz (potencia de 2 para máscara rápida)
    static constexpr int BUF_MASK = BUF_LEN - 1;

    void reset() {
        memset(_buf, 0, sizeof(_buf));
        _wr = 0;
        _lfoPhase = 0.0f;
        _lpState = 0.0f;
        _hpState = 0.0f;
        _bassState = 0.0f;
    }

    void setMode(uint8_t mode) { _mode = mode > 3 ? 3 : mode; }
    uint8_t mode() const { return _mode; }

    void setHpf(uint8_t hpf) { _hpf = hpf > 3 ? 3 : hpf; }
    uint8_t hpf() const { return _hpf; }

    void render(const float* in, float* out, int n, float fs) {
        // Configuración del LFO triangular según modo del Juno-106
        float rate_hz = 0.513f;
        float depth_ms = 1.6f;
        float wet_mix = 0.45f;
        if (_mode == 2) {
            rate_hz = 0.863f;
            depth_ms = 1.85f;
            wet_mix = 0.55f;
        } else if (_mode == 3) {
            rate_hz = 8.0f;
            depth_ms = 0.38f;
            wet_mix = 0.55f;
        }
        const float lfo_inc = rate_hz / fs;
        const float center_samp = 0.0035f * fs;
        const float depth_samp = (depth_ms * 0.001f) * fs;

        // Coeficientes de 1 polo para HPF / Bass Boost del Juno-106
        const float g_bass = 1.0f - expf(-6.2831853f * 85.0f / fs);
        const float g_hp2  = 1.0f - expf(-6.2831853f * 225.0f / fs);
        const float g_hp3  = 1.0f - expf(-6.2831853f * 720.0f / fs);
        // Filtro antialiasing BBD (~10 kHz)
        const float g_bbd  = 1.0f - expf(-6.2831853f * 10000.0f / fs);

        for (int i = 0; i < n; ++i) {
            float x = in[i];

            // 1. High-Pass Filter / Bass Boost de 4 posiciones del Juno-106
            if (_hpf == 0) {
                // Posición 0: Bass Boost (+3.5 dB debajo de 85 Hz)
                _bassState += g_bass * (x - _bassState);
                x = x + 0.5f * _bassState;
            } else if (_hpf == 2) {
                _hpState += g_hp2 * (x - _hpState);
                x = x - _hpState;
            } else if (_hpf == 3) {
                _hpState += g_hp3 * (x - _hpState);
                x = x - _hpState;
            }

            if (_mode == 0) {
                out[i] = x;
                continue;
            }

            // 2. Escribir en línea de retardo BBD con suave saturación de entrada
            _buf[_wr] = fast_tanh(x * 1.15f);

            // 3. LFO triangular en [-1, +1]
            _lfoPhase += lfo_inc;
            if (_lfoPhase >= 1.0f) _lfoPhase -= 1.0f;
            const float tri = 1.0f - 4.0f * fabsf(_lfoPhase - 0.5f);

            // Dos taps modulados (tap principal y tap secundario desfasado) para riqueza en mono
            const float d1 = center_samp + depth_samp * tri;
            const float d2 = center_samp * 1.35f - depth_samp * 0.7f * tri;

            const float w1 = read_interp(d1);
            const float w2 = read_interp(d2);
            const float wet_raw = 0.65f * w1 + 0.35f * w2;

            // Suavizado BBD de 10 kHz
            _lpState += g_bbd * (wet_raw - _lpState);

            _wr = (_wr + 1) & BUF_MASK;
            out[i] = (1.0f - wet_mix * 0.35f) * x + wet_mix * _lpState;
        }
    }

private:
    float read_interp(float delay_samples) const {
        if (delay_samples < 1.0f) delay_samples = 1.0f;
        if (delay_samples > float(BUF_LEN - 2)) delay_samples = float(BUF_LEN - 2);
        const float rp = float(_wr) - delay_samples + float(BUF_LEN);
        const int idx0 = int(rp) & BUF_MASK;
        const int idx1 = (idx0 + 1) & BUF_MASK;
        const float frac = rp - floorf(rp);
        return _buf[idx0] + frac * (_buf[idx1] - _buf[idx0]);
    }

    float _buf[BUF_LEN] = {};
    int _wr = 0;
    float _lfoPhase = 0.0f;
    float _lpState = 0.0f;
    float _hpState = 0.0f;
    float _bassState = 0.0f;
    uint8_t _mode = 1;  // Default: Chorus I
    uint8_t _hpf = 1;   // Default: Flat (posición 1)
};

}  // namespace dsp

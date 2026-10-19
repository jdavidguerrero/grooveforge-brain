#pragma once

#include "../dsp/polyblep_core.h"
#include <Arduino.h>
#include <Audio.h>

/**
 * @file polyblep_waveform.h
 * @brief Reemplazo drop-in de AudioSynthWaveform con PolyBLEP anti-aliasing y apagado real en ISR.
 */

class AudioSynthPolyBlep : public AudioStream {
public:
    /// SAW y PULSE a la vez (el DCO del Juno-106 suma las dos formas).
    static constexpr uint16_t WAVEFORM_SAW_PULSE = dsp::WAVE_SAW_AND_PULSE;

    AudioSynthPolyBlep() : AudioStream(0, nullptr) {
        static uint32_t s_seed = 0x2468ACE1u;
        s_seed = s_seed * 1664525u + 1013904223u;
        _core.reset(s_seed);
    }

    void begin(uint16_t waveform) {
        __disable_irq();
        _waveform = waveform;
        __enable_irq();
    }

    void begin(float amp, float freq, uint16_t waveform) {
        __disable_irq();
        _amp = amp;
        _inc = freq / AUDIO_SAMPLE_RATE_EXACT;
        _waveform = waveform;
        __enable_irq();
    }

    void frequency(float hz) {
        if (hz < 0.0f) hz = 0.0f;
        const float inc = hz / AUDIO_SAMPLE_RATE_EXACT;
        __disable_irq();
        _inc = inc;
        __enable_irq();
    }

    void amplitude(float n) {
        if (n < 0.0f) n = 0.0f;
        if (n > 1.0f) n = 1.0f;
        __disable_irq();
        _amp = n;
        __enable_irq();
    }

    void pulseWidth(float pw) {
        if (pw < 0.05f) pw = 0.05f;
        if (pw > 0.95f) pw = 0.95f;
        __disable_irq();
        _pw = pw;
        __enable_irq();
    }

    void enable(bool on) { _enabled = on; }

    /// Semilla del ruido de deriva: una distinta por voz para que no deriven todas igual.
    void setSeed(uint32_t seed) {
        __disable_irq();
        _core.reset(seed);
        __enable_irq();
    }

    /// Profundidad de la deriva analógica (fracción de la frecuencia; 0.0008 ≈ ±1.4 cents).
    void setDrift(float depth) {
        __disable_irq();
        _core.set_drift(depth);
        __enable_irq();
    }

    void update() override {
        if (!_enabled || _amp <= 0.0f) return;
        audio_block_t* block = allocate();
        if (!block) return;

        uint16_t wf;
        float inc, pw, amp;
        __disable_irq();
        wf = _waveform;
        inc = _inc;
        pw = _pw;
        amp = _amp;
        __enable_irq();

        float tmp[AUDIO_BLOCK_SAMPLES];
        _core.render(wf, inc, pw, amp, tmp, AUDIO_BLOCK_SAMPLES);
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            int32_t v = int32_t(tmp[i] * 32767.0f);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            block->data[i] = int16_t(v);
        }
        transmit(block, 0);
        release(block);
    }

private:
    dsp::PolyBlepCore _core;
    uint16_t _waveform = dsp::WAVE_SAWTOOTH;
    float _inc = 440.0f / AUDIO_SAMPLE_RATE_EXACT;
    float _pw = 0.5f;
    float _amp = 1.0f;
    volatile bool _enabled = true;
};

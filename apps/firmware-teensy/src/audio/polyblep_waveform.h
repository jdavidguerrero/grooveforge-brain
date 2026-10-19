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
    /// Forma continua saw → pulse (OB-6): la mezcla la fija setShape().
    static constexpr uint16_t WAVEFORM_SHAPE = dsp::WAVE_SHAPE_MORPH;

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

    /// Mezcla de WAVEFORM_SHAPE: 0 = saw, 1 = pulse (con el ancho de pulseWidth()).
    void setShape(float m) {
        __disable_irq();
        _shape = m < 0.0f ? 0.0f : m > 1.0f ? 1.0f : m;
        __enable_irq();
    }

    /**
     * @brief Hard sync: reinicia la fase en cada vuelta de `master` (nullptr = libre).
     *
     * `master` tiene que estar construido antes que este objeto: la Teensy Audio Library llama a los update() en
     * orden de construcción, así las vueltas del bloque ya están anotadas cuando corre el esclavo (fidelidad §11.3).
     */
    void syncTo(const AudioSynthPolyBlep* master) {
        __disable_irq();
        _master = master;
        __enable_irq();
    }

    /// Vueltas del último bloque (las lee el esclavo de sync).
    const dsp::SyncEvents& syncEvents() const { return _events; }

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
        if (!_enabled || _amp <= 0.0f) {
            _events.n = 0;                 // sin vueltas: un esclavo de sync no repite las del bloque anterior
            return;
        }
        audio_block_t* block = allocate();
        if (!block) {
            _events.n = 0;
            return;
        }

        uint16_t wf;
        float inc, pw, amp, shape;
        const AudioSynthPolyBlep* master;
        __disable_irq();
        wf = _waveform;
        inc = _inc;
        pw = _pw;
        amp = _amp;
        shape = _shape;
        master = _master;
        __enable_irq();

        float tmp[AUDIO_BLOCK_SAMPLES];
        _core.render(wf, inc, pw, amp, tmp, AUDIO_BLOCK_SAMPLES, shape, &_events,
                     master ? &master->syncEvents() : nullptr);
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
    float _shape = 0.0f;
    const AudioSynthPolyBlep* _master = nullptr;
    dsp::SyncEvents _events;
    volatile bool _enabled = true;
};

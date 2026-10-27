#pragma once

#include <Arduino.h>
#include <Audio.h>

#include "../dsp/phaser_core.h"

/**
 * @file phaser_stage.h
 * @brief AudioStream del PHASER de la cadena del synth (dsp::PhaserCore, theory/fase3-fx.md §2). Código custom.
 *
 * Es el TYPE PHASER del lugar CHORUS: comparte DEPTH, RATE y MIX. Una copia por canal; la de R con el LFO desfasado.
 */
class AudioPhaserStage : public AudioStream {
public:
    AudioPhaserStage() : AudioStream(1, _queue) {}

    void begin(float lfo_phase) { _core.set_phase(lfo_phase); }
    void set(float depth, float rate_hz, float mix) {
        __disable_irq();
        _core.set_depth(depth);
        _core.set_rate(rate_hz);
        _mix = mix < 0.0f ? 0.0f : mix > 1.0f ? 1.0f : mix;
        __enable_irq();
    }
    void set_bypass(bool b) { _bypass = b; }

    void update() override {
        audio_block_t* in = receiveReadOnly(0);
        if (!in) return;
        if (_bypass) {
            transmit(in, 0);
            release(in);
            return;
        }
        audio_block_t* out = allocate();
        if (!out) {
            release(in);
            return;
        }
        float x[AUDIO_BLOCK_SAMPLES], y[AUDIO_BLOCK_SAMPLES];
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) x[i] = in->data[i] * (1.0f / 32768.0f);
        _core.render(x, y, AUDIO_BLOCK_SAMPLES, AUDIO_SAMPLE_RATE_EXACT);
        const float m = _mix;
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            float v = x[i] + m * (y[i] - x[i]);
            v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
            out->data[i] = int16_t(v * 32767.0f);
        }
        release(in);
        transmit(out, 0);
        release(out);
    }

private:
    audio_block_t* _queue[1];
    dsp::PhaserCore _core;
    float _mix = 0.5f;
    volatile bool _bypass = true;
};

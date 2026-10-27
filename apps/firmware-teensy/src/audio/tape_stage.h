#pragma once

#include <Arduino.h>
#include <Audio.h>

#include "../dsp/tape_core.h"

/**
 * @file tape_stage.h
 * @brief AudioStream del TAPE en coma flotante (dsp::TapeCore, theory/fase3-fx.md, TAPE rev. 2). Código custom.
 */
class AudioTapeStage : public AudioStream {
public:
    AudioTapeStage() : AudioStream(1, _queue) { _core.set_cutoff(20000.0f, AUDIO_SAMPLE_RATE_EXACT); }

    void set_drive(float d) {
        __disable_irq();
        _core.set_drive(d);
        __enable_irq();
    }
    void set_cutoff(float hz) {
        __disable_irq();
        _core.set_cutoff(hz, AUDIO_SAMPLE_RATE_EXACT);
        __enable_irq();
    }
    void set_mix(float m) {
        __disable_irq();
        _core.set_mix(m);
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
        _core.render(x, y, AUDIO_BLOCK_SAMPLES);
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            const float v = y[i] > 1.0f ? 1.0f : y[i] < -1.0f ? -1.0f : y[i];
            out->data[i] = int16_t(v * 32767.0f);
        }
        release(in);
        transmit(out, 0);
        release(out);
    }

private:
    audio_block_t* _queue[1];
    dsp::TapeCore _core;
    volatile bool _bypass = false;
};

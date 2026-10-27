#pragma once

#include <Arduino.h>
#include <Audio.h>

#include "../dsp/drive_core.h"

/**
 * @file drive_stage.h
 * @brief AudioStream del DRIVE de la cadena del synth (dsp::DriveCore, theory/fase3-fx.md §2). Código custom.
 */
class AudioDriveStage : public AudioStream {
public:
    AudioDriveStage() : AudioStream(1, _queue) {}

    void set(uint8_t type, float amount, float tone, float mix) {
        __disable_irq();
        _core.set_type(type);
        _core.set_amount(amount);
        _core.set_tone(tone, AUDIO_SAMPLE_RATE_EXACT);
        _core.set_mix(mix);
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
    dsp::DriveCore _core;
    volatile bool _bypass = true;
};

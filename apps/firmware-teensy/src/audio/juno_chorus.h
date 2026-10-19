#pragma once

#include "../dsp/juno_chorus_core.h"
#include <Arduino.h>
#include <Audio.h>

/**
 * @file juno_chorus.h
 * @brief AudioStream que envuelve a dsp::JunoChorusCore (HPF de 4 posiciones + Chorus BBD MN3009).
 */

class JunoBbdChorus : public AudioStream {
public:
    JunoBbdChorus() : AudioStream(1, _inputQueueArray) {
        _core.reset();
    }

    void set_mode(uint8_t mode) {
        __disable_irq();
        _core.setMode(mode);
        __enable_irq();
    }

    void set_hpf(uint8_t hpf) {
        __disable_irq();
        _core.setHpf(hpf);
        __enable_irq();
    }

    void set_enabled(bool on) {
        if (on == _enabled) return;
        if (on) {
            __disable_irq();
            _core.reset();
            _enabled = true;
            __enable_irq();
        } else {
            _enabled = false;
        }
    }

    void update() override {
        audio_block_t* in_block = receiveReadOnly(0);
        if (!in_block) return;
        if (!_enabled) {
            release(in_block);
            return;
        }
        audio_block_t* out_block = allocate();
        if (!out_block) {
            release(in_block);
            return;
        }

        float in_f[AUDIO_BLOCK_SAMPLES];
        float out_f[AUDIO_BLOCK_SAMPLES];
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            in_f[i] = float(in_block->data[i]) * (1.0f / 32768.0f);
        }
        _core.render(in_f, out_f, AUDIO_BLOCK_SAMPLES, AUDIO_SAMPLE_RATE_EXACT);
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            int32_t v = int32_t(out_f[i] * 32767.0f);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            out_block->data[i] = int16_t(v);
        }

        release(in_block);
        transmit(out_block, 0);
        release(out_block);
    }

private:
    audio_block_t* _inputQueueArray[1];
    dsp::JunoChorusCore _core;
    volatile bool _enabled = true;
};

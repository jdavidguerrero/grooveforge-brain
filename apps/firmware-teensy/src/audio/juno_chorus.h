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
        audio_block_t* out_l = allocate();
        audio_block_t* out_r = allocate();
        if (!out_l || !out_r) {
            if (out_l) release(out_l);
            if (out_r) release(out_r);
            release(in_block);
            return;
        }

        float in_f[AUDIO_BLOCK_SAMPLES], l_f[AUDIO_BLOCK_SAMPLES], r_f[AUDIO_BLOCK_SAMPLES];
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) in_f[i] = float(in_block->data[i]) * (1.0f / 32768.0f);
        _core.render_stereo(in_f, l_f, r_f, AUDIO_BLOCK_SAMPLES, AUDIO_SAMPLE_RATE_EXACT);
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            out_l->data[i] = to_i16(l_f[i]);
            out_r->data[i] = to_i16(r_f[i]);
        }

        release(in_block);
        transmit(out_l, 0);   // salida 0 = L
        transmit(out_r, 1);   // salida 1 = R (fidelidad §9.3)
        release(out_l);
        release(out_r);
    }

private:
    static int16_t to_i16(float x) {
        const int32_t v = int32_t(x * 32767.0f);
        return int16_t(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }

    audio_block_t* _inputQueueArray[1];
    dsp::JunoChorusCore _core;
    volatile bool _enabled = true;
};

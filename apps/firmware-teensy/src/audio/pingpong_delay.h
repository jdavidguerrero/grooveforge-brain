#pragma once

#include <Arduino.h>
#include <Audio.h>
#include <stdlib.h>

#include "../dsp/pingpong_delay_core.h"

/**
 * @file pingpong_delay.h
 * @brief AudioStream estéreo del DELAY ping-pong (dsp::PingPongDelayCore, theory/fase3-fx.md §2). Código custom.
 *
 * 2 entradas (L, R) y 2 salidas: lo comparten las dos cadenas del synth. Las líneas van en el heap (RAM2).
 */
class AudioPingPongDelay : public AudioStream {
public:
    AudioPingPongDelay() : AudioStream(2, _queue) {}

    /// Reserva las líneas (`seconds` por canal). @return false si no hay memoria (queda en bypass).
    bool begin(float seconds) {
        const uint32_t len = uint32_t(seconds * AUDIO_SAMPLE_RATE_EXACT) + 2;
        int16_t* l = static_cast<int16_t*>(malloc(len * sizeof(int16_t)));
        int16_t* r = static_cast<int16_t*>(malloc(len * sizeof(int16_t)));
        if (!l || !r) {
            free(l);
            free(r);
            return false;
        }
        __disable_irq();
        _core.init(l, r, len, AUDIO_SAMPLE_RATE_EXACT);
        _ready = true;
        __enable_irq();
        return true;
    }
    void set(float seconds, float feedback, float tone, float mix) {
        __disable_irq();
        _core.set_time(seconds);
        _core.set_feedback(feedback);
        _core.set_tone(tone);
        _core.set_mix(mix);
        __enable_irq();
    }
    void set_time(float seconds) {
        __disable_irq();
        _core.set_time(seconds);
        __enable_irq();
    }
    /// Al encender, las líneas arrancan vacías (no suena el eco viejo de la última vez).
    void set_bypass(bool b) {
        if (!b && _bypass && _ready) {
            __disable_irq();
            _core.clear();
            __enable_irq();
        }
        _bypass = b;
    }

    void update() override {
        audio_block_t* in[2] = {receiveReadOnly(0), receiveReadOnly(1)};
        if (_bypass || !_ready) {
            for (int c = 0; c < 2; ++c)
                if (in[c]) {
                    transmit(in[c], c);
                    release(in[c]);
                }
            return;
        }
        audio_block_t* out[2] = {allocate(), allocate()};
        if (!out[0] || !out[1]) {
            for (int c = 0; c < 2; ++c) {
                if (in[c]) release(in[c]);
                if (out[c]) release(out[c]);
            }
            return;
        }
        float xl[AUDIO_BLOCK_SAMPLES], xr[AUDIO_BLOCK_SAMPLES], yl[AUDIO_BLOCK_SAMPLES], yr[AUDIO_BLOCK_SAMPLES];
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            xl[i] = in[0] ? in[0]->data[i] * (1.0f / 32768.0f) : 0.0f;
            xr[i] = in[1] ? in[1]->data[i] * (1.0f / 32768.0f) : 0.0f;
        }
        _core.render(xl, xr, yl, yr, AUDIO_BLOCK_SAMPLES);
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            const float l = yl[i] > 1.0f ? 1.0f : yl[i] < -1.0f ? -1.0f : yl[i];
            const float r = yr[i] > 1.0f ? 1.0f : yr[i] < -1.0f ? -1.0f : yr[i];
            out[0]->data[i] = int16_t(l * 32767.0f);
            out[1]->data[i] = int16_t(r * 32767.0f);
        }
        for (int c = 0; c < 2; ++c) {
            if (in[c]) release(in[c]);
            transmit(out[c], c);
            release(out[c]);
        }
    }

private:
    audio_block_t* _queue[2];
    dsp::PingPongDelayCore _core;
    volatile bool _bypass = true, _ready = false;
};

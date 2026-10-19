#pragma once

#include <Arduino.h>
#include <Audio.h>

#include "../dsp/color_noise_core.h"

/**
 * @file color_noise.h
 * @brief Generador de ruido blanco → rosa como objeto de audio (código custom; reemplaza a AudioSynthNoiseWhite).
 */
class AudioSynthColorNoise : public AudioStream {
public:
    AudioSynthColorNoise() : AudioStream(0, nullptr) {
        static uint32_t s_seed = 0x7A3C5E91u;
        s_seed = s_seed * 1664525u + 1013904223u;
        _core.reset(s_seed);
    }

    void amplitude(float a) { _amp = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a; }
    /// 0 = blanco, 1 = rosa.
    void color(float c) {
        __disable_irq();
        _core.set_color(c);
        __enable_irq();
    }
    void enable(bool on) { _enabled = on; }

    void update() override {
        if (!_enabled || _amp <= 0.0f) return;
        audio_block_t* block = allocate();
        if (!block) return;
        float tmp[AUDIO_BLOCK_SAMPLES];
        _core.render(tmp, AUDIO_BLOCK_SAMPLES);
        const float g = _amp * 32767.0f;
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            const float v = tmp[i] * g;
            block->data[i] = int16_t(v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v);
        }
        transmit(block);
        release(block);
    }

private:
    dsp::ColorNoiseCore _core;
    float _amp = 1.0f;
    volatile bool _enabled = true;
};

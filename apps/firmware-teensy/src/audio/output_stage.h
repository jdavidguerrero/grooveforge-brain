#pragma once

#include <Arduino.h>
#include <Audio.h>

#include "../dsp/output_stage_core.h"

/**
 * @file output_stage.h
 * @brief Acople AC + saturación suave a la salida del bus de engines (código custom, subclase de AudioStream).
 */
class AudioOutputStage : public AudioStream {
public:
    AudioOutputStage() : AudioStream(1, _queue) { _core.set(8.0f, 1.2f, AUDIO_SAMPLE_RATE_EXACT); }

    void update() override {
        audio_block_t* in = receiveWritable(0);
        if (!in) return;
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            const float y = _core.tick(in->data[i] * (1.0f / 32768.0f)) * 32767.0f;
            in->data[i] = int16_t(y > 32767.0f ? 32767.0f : y < -32768.0f ? -32768.0f : y);
        }
        transmit(in);
        release(in);
    }

private:
    audio_block_t* _queue[1];
    dsp::OutputStageCore _core;
};

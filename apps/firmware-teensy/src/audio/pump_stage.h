#pragma once

#include <Arduino.h>
#include <Audio.h>

/**
 * @file pump_stage.h
 * @brief Ganancia del PUMP al final de la cadena de SYNTH FX (código custom, subclase de AudioStream).
 *
 * La curva (dsp/pump.h) se evalúa a 1 kHz en el loop con la posición del reloj común; acá solo se interpola por
 * muestra desde la ganancia del bloque anterior hasta la última pedida, para que no haya escalones (theory/09 §2).
 */
class AudioPumpStage : public AudioStream {
public:
    AudioPumpStage() : AudioStream(1, _queue) {}

    /// Ganancia objetivo 0–1 (se llama desde el loop; un float alineado se escribe atómico en el M7).
    void set_gain(float g) { _target = g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g; }

    void update() override {
        audio_block_t* in = receiveWritable(0);
        if (!in) return;
        const float target = _target;
        if (target >= 0.9999f && _gain >= 0.9999f) {   // PUMP apagado o recuperado: pasa tal cual
            _gain = 1.0f;
            transmit(in);
            release(in);
            return;
        }
        const float step = (target - _gain) * (1.0f / AUDIO_BLOCK_SAMPLES);
        float g = _gain;
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            g += step;
            in->data[i] = int16_t(in->data[i] * g);
        }
        _gain = target;
        transmit(in);
        release(in);
    }

private:
    audio_block_t* _queue[1];
    volatile float _target = 1.0f;
    float _gain = 1.0f;
};

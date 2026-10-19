#pragma once

#include <Arduino.h>
#include <Audio.h>

#include "../dsp/analog_env.h"

/**
 * @file analog_vca.h
 * @brief VCA con envolvente RC por muestra (código custom, subclase de AudioStream).
 *
 * Reemplaza a AudioEffectEnvelope (Teensy Audio Library, segmentos lineales) con la misma API: attack, decay,
 * sustain, release, noteOn, noteOff, isActive. La curva es la de dsp::AnalogEnv evaluada a 44.1 kHz
 * (theory/fidelidad-engines.md §9.2).
 */
class AnalogVca : public AudioStream {
public:
    AnalogVca() : AudioStream(1, _queue) { _env.set_rate(AUDIO_SAMPLE_RATE_EXACT); }

    void attack(float ms)   { __disable_irq(); _env.setAttack(ms); __enable_irq(); }
    void decay(float ms)    { __disable_irq(); _env.setDecay(ms); __enable_irq(); }
    void sustain(float lvl) { __disable_irq(); _env.setSustain(lvl); __enable_irq(); }
    void release(float ms)  { __disable_irq(); _env.setRelease(ms); __enable_irq(); }
    /// Sobrepaso del ataque (carácter por engine; 0.5 por defecto).
    void attackOvershoot(float os) { __disable_irq(); _env.setAttackOvershoot(os); __enable_irq(); }

    void noteOn()  { __disable_irq(); _env.noteOn(); __enable_irq(); }
    void noteOff() { __disable_irq(); _env.noteOff(); __enable_irq(); }
    bool isActive() const { return _env.active(); }

    void update() override {
        audio_block_t* in = receiveReadOnly(0);
        if (!in) return;
        if (!_env.active()) {   // en silencio no se transmite nada (como AudioEffectEnvelope)
            AudioStream::release(in);
            return;
        }
        audio_block_t* out = allocate();
        if (!out) {
            AudioStream::release(in);
            return;
        }
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) out->data[i] = int16_t(float(in->data[i]) * _env.step());
        AudioStream::release(in);
        transmit(out);
        AudioStream::release(out);
    }

private:
    audio_block_t* _queue[1];
    dsp::AnalogEnv _env;
};

/**
 * @file moog_ladder.cpp
 * @brief D'Angelo & Välimäki Moog ladder filter — implementación AudioStream.
 *
 * Ver moog_ladder.h para documentación completa y referencias.
 *
 * Código custom (no es Teensy Audio Library oficial).
 */

#include "moog_ladder.h"
#include <string.h>

#include "../dsp/fast_math.h"
#include <math.h>
#include <Arduino.h>

// ── Constructor ───────────────────────────────────────────────────────────────

MoogLadder4P::MoogLadder4P() : AudioStream(1, inputQueueArray) {
    resetState();
    frequency(1000.0f);
    resonance(0.707f);
}

// ── Parámetros públicos ───────────────────────────────────────────────────────

void MoogLadder4P::frequency(float hz) {
    _core.set_frequency(hz);
}

void MoogLadder4P::resonance(float q) {
    // Convención de AudioFilterStateVariable [0.707, 5.0] → k [0, 4.4]: el tope pasa apenas el umbral de
    // auto-oscilación del ZDF con límite de lazo (~4.05), así la resonancia máxima canta con amplitud ~0.5.
    _core.set_resonance(q);
}

void MoogLadder4P::setCharacter(dsp::LadderCharacter ch) {
    AudioNoInterrupts();
    _core.set_character(ch);
    AudioInterrupts();
}

void MoogLadder4P::set_enabled(bool on) {
    if (on == _enabled) return;
    if (on) {
        // Bajo AudioNoInterrupts para que la ISR no lea un estado a medio resetear.
        AudioNoInterrupts();
        resetState();
        _enabled = true;
        AudioInterrupts();
    } else {
        _enabled = false;
    }
}

// ── Procesamiento por muestra ─────────────────────────────────────────────────

void MoogLadder4P::set_drive(float drive) {
    const auto setting = dsp::LadderStageCore::prepare_drive(drive);
    AudioNoInterrupts();
    _core.set_drive(setting);
    AudioInterrupts();
}

void MoogLadder4P::resetState() {
    _core.reset();
}

float MoogLadder4P::processSample(float in) {
    return _core.process_sample(in);
}

// ── AudioStream::update() — procesa 128 muestras por llamada ─────────────────

void MoogLadder4P::update(void) {
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

    const int16_t* src = in_block->data;
    int16_t*       dst = out_block->data;

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        // Teensy Audio Library usa int16 internamente: normalizar a ±1.0 para el modelo
        float in_f  = (float)src[i] * (1.0f / 32768.0f);
        float out_f = processSample(in_f);

        // Convertir de vuelta a int16 con saturación
        int32_t out_i = (int32_t)(out_f * 32767.0f);
        if (out_i >  32767) out_i =  32767;
        if (out_i < -32767) out_i = -32767;
        dst[i] = (int16_t)out_i;
    }

    release(in_block);
    transmit(out_block, 0);
    release(out_block);
}

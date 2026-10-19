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
    _zdf.set_cutoff(hz, AUDIO_SAMPLE_RATE_EXACT);
}

void MoogLadder4P::resonance(float q) {
    // Convención de AudioFilterStateVariable [0.707, 5.0] → k [0, 4.4]: el tope pasa apenas el umbral de
    // auto-oscilación del ZDF con límite de lazo (~4.05), así la resonancia máxima canta con amplitud ~0.5.
    const float r = (q - 0.707f) / (5.0f - 0.707f);
    _zdf.set_k((r < 0.0f ? 0.0f : r > 1.0f ? 1.0f : r) * 4.4f);
}

void MoogLadder4P::setCharacter(dsp::LadderCharacter ch) {
    AudioNoInterrupts();
    _zdf.set_character(ch);
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
    if (drive < 0.5f) drive = 0.5f;
    if (drive > 4.0f) drive = 4.0f;
    const float comp = 1.0f / sqrtf(drive);
    AudioNoInterrupts();
    _drive = drive;
    _inv_drive_comp = comp;
    AudioInterrupts();
}

void MoogLadder4P::resetState() {
    _zdf.reset();
}

float MoogLadder4P::processSample(float in) {
    const float y = _zdf.tick(in * _drive);
    // Protección ante NaN/Inf en transitorios de parámetros: resetea en vez de propagar
    if (!isfinite(y)) {
        resetState();
        return 0.0f;
    }
    float out = y * _inv_drive_comp;
    if (out >  1.0f) out =  1.0f;
    if (out < -1.0f) out = -1.0f;
    return out;
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

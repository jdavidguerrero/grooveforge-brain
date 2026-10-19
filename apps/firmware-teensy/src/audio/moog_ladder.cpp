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
#include "../dsp/ladder_core.h"
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
    if (hz < 1.0f)                             hz = 1.0f;
    if (hz > AUDIO_SAMPLE_RATE_EXACT * 0.499f) hz = AUDIO_SAMPLE_RATE_EXACT * 0.499f;

    // Ecuación (20) del paper:
    //   A = π(fc/fs) × (1 - π(fc/fs)) / (1 + π(fc/fs))
    // Mapeo de la respuesta bilineal al eje de frecuencia analógico (pre-warping).
    float pi_fc = (float)M_PI * hz / AUDIO_SAMPLE_RATE_EXACT;
    float A = pi_fc * (1.0f - pi_fc) / (1.0f + pi_fc);

    // g = 4·fs·A·VT — coeficiente de las derivadas diferenciales (ecs. 13-14).
    // Absorbe fs y VT aquí para reducir operaciones en el loop de muestras.
    float g = 4.0f * AUDIO_SAMPLE_RATE_EXACT * A * VT;

    AudioNoInterrupts();
    _A = A;
    _g = g;
    AudioInterrupts();
}

void MoogLadder4P::resonance(float q) {
    // AudioFilterStateVariable usa q ∈ [0.707, 5.0].
    // El paper de D'Angelo usa k ∈ [0.0, 4.0], donde k=4 es auto-oscilación.
    // Mapeo lineal: q=0.707 → k=0.0, q=5.0 → k=4.0
    float k = (q - 0.707f) / (5.0f - 0.707f) * 4.0f;
    if (k < 0.0f) k = 0.0f;
    if (k > 4.0f) k = 4.0f;
    AudioNoInterrupts();
    _k = k;
    AudioInterrupts();
}

void MoogLadder4P::set_drive(float drive) {
    if (drive < 0.5f) drive = 0.5f;
    if (drive > 4.0f) drive = 4.0f;
    float comp = 1.0f / sqrtf(drive);
    AudioNoInterrupts();
    _drive = drive;
    _inv_drive_comp = comp;
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

void MoogLadder4P::resetState() {
    _s = dsp::LadderState{};
}

float MoogLadder4P::processSample(float in) {
    // Escalar entrada normalizada a dominio de voltaje del modelo BJT + drive
    float vin = in * (DRIVE * _drive);

    // Derivadas (ecs. 13–14) e integración trapezoidal (ec. 15) con tanh de Padé: theory/fidelidad-engines.md §1
    const float inv2fs = 1.0f / (2.0f * AUDIO_SAMPLE_RATE_EXACT);
    const float v3 = dsp::ladder_step<dsp::fast_tanh>(_s, vin, _g, _k, INV2VT, inv2fs);

    // Protección anti-divergencia: en transitorios de parámetros con alta
    // resonancia + señal grande, el feedback puede divergir. isfinite()
    // detecta NaN/Inf y resetea el estado limpiamente en lugar de propagar
    // valores indefinidos al bloque de audio.
    if (!isfinite(v3)) {
        resetState();
        return 0.0f;
    }

    // Escalar de vuelta a rango normalizado con compensación de ganancia por drive y soft-clip
    float out = v3 * INV_DRIVE * _inv_drive_comp;
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

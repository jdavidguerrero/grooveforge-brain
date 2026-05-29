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
    _A = pi_fc * (1.0f - pi_fc) / (1.0f + pi_fc);

    // g = 4·fs·A·VT — coeficiente de las derivadas diferenciales (ecs. 13-14).
    // Absorbe fs y VT aquí para reducir operaciones en el loop de muestras.
    _g = 4.0f * AUDIO_SAMPLE_RATE_EXACT * _A * VT;
}

void MoogLadder4P::resonance(float q) {
    // AudioFilterStateVariable usa q ∈ [0.707, 5.0].
    // El paper de D'Angelo usa k ∈ [0.0, 4.0], donde k=4 es auto-oscilación.
    // Mapeo lineal: q=0.707 → k=0.0, q=5.0 → k=4.0
    _k = (q - 0.707f) / (5.0f - 0.707f) * 4.0f;
    if (_k < 0.0f) _k = 0.0f;
    if (_k > 4.0f) _k = 4.0f;
}

// ── Procesamiento por muestra ─────────────────────────────────────────────────

void MoogLadder4P::resetState() {
    memset(_V,  0, sizeof(_V));
    memset(_dV, 0, sizeof(_dV));
}

float MoogLadder4P::processSample(float in) {
    // Escalar entrada normalizada a dominio de voltaje del modelo BJT
    float vin = in * DRIVE;

    // ── Derivadas (ecuaciones 13 y 14 del paper) ──────────────────────────────
    //
    // Stage 1 (ec. 13): incluye feedback de k·V4 (resonancia del ladder)
    // Stage 2-4 (ec. 14): cada stage integra la diferencia entre el stage previo y sí mismo
    float dV0 = -_g * (tanhf(_V[0] * INV2VT)
                     + tanhf((vin + _k * _V[3]) * INV2VT));
    float dV1 =  _g * (tanhf(_V[0] * INV2VT) - tanhf(_V[1] * INV2VT));
    float dV2 =  _g * (tanhf(_V[1] * INV2VT) - tanhf(_V[2] * INV2VT));
    float dV3 =  _g * (tanhf(_V[2] * INV2VT) - tanhf(_V[3] * INV2VT));

    // ── Integración bilineal (ecuación 15 del paper) ──────────────────────────
    // Vi[n] = Vi[n-1] + (dVi[n] + dVi[n-1]) / (2·fs)
    // El método trapezoidal (bilineal) es estable y tiene respuesta de fase
    // similar al integrador analógico, a diferencia del Euler forward que
    // puede divergir con alta resonancia.
    const float inv2fs = 1.0f / (2.0f * AUDIO_SAMPLE_RATE_EXACT);
    _V[0] += (dV0 + _dV[0]) * inv2fs;
    _V[1] += (dV1 + _dV[1]) * inv2fs;
    _V[2] += (dV2 + _dV[2]) * inv2fs;
    _V[3] += (dV3 + _dV[3]) * inv2fs;

    // Guardar derivadas para la siguiente muestra
    _dV[0] = dV0;
    _dV[1] = dV1;
    _dV[2] = dV2;
    _dV[3] = dV3;

    // Protección anti-divergencia: en transitorios de parámetros con alta
    // resonancia + señal grande, el feedback puede divergir. isfinite()
    // detecta NaN/Inf y resetea el estado limpiamente en lugar de propagar
    // valores indefinidos al bloque de audio.
    if (!isfinite(_V[3])) {
        resetState();
        return 0.0f;
    }

    // Escalar de vuelta a rango normalizado y limitar hard a ±1.0
    float out = _V[3] * INV_DRIVE;
    if (out >  1.0f) out =  1.0f;
    if (out < -1.0f) out = -1.0f;
    return out;
}

// ── AudioStream::update() — procesa 128 muestras por llamada ─────────────────

void MoogLadder4P::update(void) {
    audio_block_t* in_block = receiveReadOnly(0);
    if (!in_block) return;

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

#include "beat_sync.h"
#include <math.h>  // fminf

// Factores de subdivisión relativos al beat period (60000/BPM ms).
// El orden debe coincidir exactamente con el enum Subdivision.
static const float SUBDIVISION_FACTORS[] = {
    2.0f,   // HALF
    1.0f,   // QUARTER
    0.75f,  // DOTTED_EIGHTH — 3/4 de una negra: la negra con punto del "Edge delay"
    0.5f,   // EIGHTH
    0.25f,  // SIXTEENTH
};

float subdivision_ms(float bpm, Subdivision sub) {
    if (bpm < 1.0f) return 0.0f;
    float beat_ms = 60000.0f / bpm;
    float factor  = SUBDIVISION_FACTORS[static_cast<uint8_t>(sub)];
    float ms      = beat_ms * factor;
    // Clamp al límite del buffer de AudioEffectDelay (Teensy Audio Library oficial).
    // AudioEffectDelay tiene AUDIO_SAMPLE_RATE_EXACT * 1.5 muestras de buffer ≈ 1500ms.
    return fminf(ms, 1490.0f);
}

BeatSync::BeatSync() : _bpm_smooth(120.0f), _has_bpm(false) {}

void BeatSync::update(const BeatResult& br, float min_confidence) {
    if (!br.valid || br.confidence < min_confidence) return;

    if (!_has_bpm) {
        // Primera lectura válida: sin EMA para evitar ramp-up artificial
        // desde el valor inicial 120 BPM cuando el tempo real es diferente.
        _bpm_smooth = br.bpm;
        _has_bpm    = true;
    } else {
        // EMA con alpha=0.1: respuesta lenta para suavizar cambios de tempo.
        // Un salto de 120→180 BPM converge al 95% en ~30 beats (~15s a 120 BPM).
        _bpm_smooth = EMA_ALPHA * br.bpm + (1.0f - EMA_ALPHA) * _bpm_smooth;
    }
}

float BeatSync::delay_ms(Subdivision sub) const {
    if (!_has_bpm) return 0.0f;
    return subdivision_ms(_bpm_smooth, sub);
}

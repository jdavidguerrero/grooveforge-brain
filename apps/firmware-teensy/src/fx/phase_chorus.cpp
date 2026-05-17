// apps/firmware-teensy/src/fx/phase_chorus.cpp
// Sprint 2.4 — FX Phase Chorus
// Theory: apps/docs/sprints/09-phase-chorus.md
// Refs:   apps/docs/05-fx-architecture.md §1.8
//         Zölzer, "DAFX" (2011), §2.6 "Chorus"
//         BOSS CE-2 Service Notes (Roland, 1979)

#include "phase_chorus.h"
#include <math.h>

// ── Constructor ──────────────────────────────────────────────────────────────────
// Conexiones que no dependen de inputStream: se inicializan aquí como miembros
// con tiempo de vida igual al objeto.
// _cIn1 y _cIn2 dependen de inputStream (desconocido en compilación) → se crean en begin().
PhaseChorus::PhaseChorus()
    : _cPreSatChorus(_preSat,      0, _chorus,     0)   // pre-sat out → chorus in
    , _cChorusWet   (_chorus,      0, _dryWetMix,  1)   // chorus out  → wet channel
    , _cMixL        (_dryWetMix,   0, _out,        0)   // mix → I2S Left
    , _cMixR        (_dryWetMix,   0, _out,        1)   // mix → I2S Right
{}

// ── begin() ──────────────────────────────────────────────────────────────────────
void PhaseChorus::begin(AudioStream& inputStream, float volume) {
    // 3 oscs + chorus ocupa ~8 bloques activos. AudioMemory(20) da 2.5× headroom.
    // Ref: apps/docs/sprints/09-phase-chorus.md §"Bloques AudioMemory"
    AudioMemory(20);

    _codec.enable();
    _codec.volume(volume);

    // Pre-sat: drive=1.5 fijo — coloración de tercer armónico sin distorsión audible
    _buildPreSatTable(1.5f);

    // CRÍTICO: chorus.begin() DESPUÉS de AudioMemory().
    // Si se llama antes, los punteros internos del objeto apuntan a memoria no inicializada
    // → comportamiento indefinido (ruido o bloqueo).
    // Ref: apps/docs/sprints/09-phase-chorus.md §"Inicialización del chorus — orden obligatorio"
    _chorus.begin(_chorusBuf, CHORUS_BUF_LEN, _voices);

    _dryWetMix.gain(0, 1.0f - _mix);   // ch0 = dry
    _dryWetMix.gain(1, _mix);           // ch1 = wet
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    // Conexiones dinámicas: inputStream se conoce solo en begin()
    _cIn1 = new AudioConnection(inputStream, 0, _preSat,    0);  // → wet path (pre-sat → chorus)
    _cIn2 = new AudioConnection(inputStream, 0, _dryWetMix, 0);  // → dry path (sin coloración)
}

// ── setRate() ────────────────────────────────────────────────────────────────────
void PhaseChorus::setRate(float hz) {
    if (hz < 0.1f)  hz = 0.1f;
    if (hz > 10.0f) hz = 10.0f;
    _rate = hz;
}

// ── setDepth() ───────────────────────────────────────────────────────────────────
void PhaseChorus::setDepth(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    _depth = depth;

    // Solo aplica inmediatamente si no estamos en bypass; si bypass=true,
    // el gain wet está en 0.0f y update() lo restaurará al salir de bypass.
    if (!_bypass) {
        _dryWetMix.gain(1, _depth);
    }
}

// ── setDrift() ───────────────────────────────────────────────────────────────────
void PhaseChorus::setDrift(float drift) {
    if (drift < 0.0f) drift = 0.0f;
    if (drift > 1.0f) drift = 1.0f;
    _drift = drift;
}

// ── setVoices() ──────────────────────────────────────────────────────────────────
void PhaseChorus::setVoices(uint8_t v) {
    if (v < 1) v = 1;
    if (v > 4) v = 4;
    _voices = v;

    // Fade-out de 10ms antes de reorganizar taps internos del chorus.
    // AudioEffectChorus.voices() reposiciona punteros de tap en el buffer —
    // sin el fade produce un click audible por la discontinuidad de amplitud.
    // Ref: apps/docs/sprints/09-phase-chorus.md §"Cambio de número de voces en runtime"
    _dryWetMix.gain(1, 0.0f);
    delay(10);
    _chorus.voices(v);
    // El próximo ciclo de update() restaurará la ganancia wet con el LFO drift actual.
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
void PhaseChorus::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    if (!_bypass) {
        _dryWetMix.gain(0, 1.0f - _mix);
        _dryWetMix.gain(1, _mix);
    }
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void PhaseChorus::setBypass(bool bypass) {
    _bypass = bypass;
    if (_bypass) {
        _dryWetMix.gain(0, 1.0f);
        _dryWetMix.gain(1, 0.0f);
    } else {
        _dryWetMix.gain(0, 1.0f - _mix);
        _dryWetMix.gain(1, _mix);
    }
}

// ── update() ─────────────────────────────────────────────────────────────────────
void PhaseChorus::update(float dt_ms) {
    const float dt_s = dt_ms * 0.001f;

    // Avance de tres fases independientes (wrap en 1.0 para precisión numérica a largo plazo)
    _lfoPhase1 += _rate * dt_s;
    _lfoPhase2 += _rate * 0.743f * dt_s;   // ≈ razón áurea invertida: máxima irracionalidad
    _lfoPhase3 += _rate * 1.329f * dt_s;   // ajustado para "respiración" audible

    if (_lfoPhase1 > 1.0f) _lfoPhase1 -= 1.0f;
    if (_lfoPhase2 > 1.0f) _lfoPhase2 -= 1.0f;
    if (_lfoPhase3 > 1.0f) _lfoPhase3 -= 1.0f;

    // LFO compuesto: senoide principal + dos sub-senoides inarmónicas escaladas por drift.
    // A drift=0: solo primera senoide → LFO puro periódico.
    // A drift=1: tres componentes → patrón que no se repite en escalas musicales.
    // Ref: apps/docs/sprints/09-phase-chorus.md §"LFO drift caótico — rompiendo la periodicidad"
    float lfo = sinf(CHORUS_TWO_PI * _lfoPhase1)
              + _drift * 0.4f * sinf(CHORUS_TWO_PI * _lfoPhase2)
              + _drift * 0.2f * sinf(CHORUS_TWO_PI * _lfoPhase3);

    // Normalización: a drift=0, amplitud pico=1.0; a drift=1, amplitud pico≈1.6.
    // Dividir por (1 + drift*0.6) mantiene el rango efectivo en ≈[-1, +1].
    float norm = 1.0f + _drift * 0.6f;
    lfo /= norm;
    _lfoOut = lfo;

    if (!_bypass) {
        // Modulación de amplitud wet: el LFO varía el gain del canal wet ±30% del depth base.
        // El factor 0.3 es el headroom de modulación: suficiente para "breathing", sin
        // hacer desaparecer el efecto (wet_min = depth*0.7) ni saturar (wet_max = depth*1.3).
        // Ref: apps/docs/sprints/09-phase-chorus.md §"Cómo el LFO drift se aplica al chorus"
        float wetGain = _depth * (1.0f + 0.3f * lfo);
        if (wetGain < 0.0f) wetGain = 0.0f;
        if (wetGain > 1.0f) wetGain = 1.0f;
        _dryWetMix.gain(1, wetGain);
    }
}

// ── _buildPreSatTable() ──────────────────────────────────────────────────────────
void PhaseChorus::_buildPreSatTable(float drive) {
    // f(x) = tanh(drive * x) / tanh(drive), normalizada a [-1.0, 1.0].
    // drive=1.5: desviación de linealidad ~1.3 dB a amplitud típica de audio (x≈0.7).
    // Genera tercer armónico ~1-3% — coloración de "calidez", no distorsión audible.
    // Modelo del TL072 buffer stage del BOSS CE-2 pre-MN3007.
    // Ref: apps/docs/sprints/09-phase-chorus.md §"Saturación pre-chorus"
    float norm = tanhf(drive);
    if (norm < 0.001f) norm = 0.001f;   // guarda contra drive≈0 (no ocurre con drive=1.5)

    for (int i = 0; i < 257; i++) {
        float x = (float)i / 128.0f - 1.0f;   // mapea índice 0–256 → [-1.0, +1.0]
        _preSatTable[i] = tanhf(drive * x) / norm;
    }

    // AudioEffectWaveshaper aplica la tabla por lookup + interpolación lineal, O(1)/muestra
    _preSat.shape(_preSatTable, 257);
}

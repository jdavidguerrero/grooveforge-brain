// apps/firmware-teensy/src/engines/dx7.cpp
// Sprint 40 — DX7 Engine: 6-voice 2-operator FM synthesis.
// Theory: apps/docs/sprints/40-dx7-engine.md

#include "dx7.h"
#include <math.h>

// ── Constructor ───────────────────────────────────────────────────────────────
//
// AudioSynthWaveformModulated: cada instancia tiene un port de entrada de audio
// (port 0) que recibe la señal de modulación de frecuencia. El conexionado
// mod→car(port 0) habilita la FM: la señal del modulador escala el índice
// definido por frequencyModulation(octaves) para desviar la frecuencia de la portadora.
//
// Por qué frequencyModulation(2.0) en begin():
// Con sensibilidad de 2 octavas y modulador a amplitud _fmDepth=0.5, la
// desviación de frecuencia es 0.5 × 2^2 = 2× la frecuencia base → parciales
// adicionales a f ± 2f, ±4f, etc. (varía con el espectro de Bessel).
// Este rango cubre el espectro clásico del DX7 en modos piano/e.piano/bell.
//
// Layout de conexiones (26 totales):
//   _c0–_c2:   voz 0  (mod→car(port0), car→filter, filter(LP)→ampEnv)
//   _c3–_c5:   voz 1
//   _c6–_c8:   voz 2
//   _c9–_c11:  voz 3
//   _c12–_c14: voz 4
//   _c15–_c17: voz 5
//   _c18–_c21: ampEnv[0-3] → masterMixA ch0-3
//   _c22–_c23: ampEnv[4-5] → masterMixB ch0-1
//   _c24:      masterMixA  → finalMix ch0
//   _c25:      masterMixB  → finalMix ch1

DX7Engine::DX7Engine() :
    // Voz 0
    _c0 (_mod[0],     0, _car[0],       0),   // modulador → portadora port 0 (FM input)
    _c1 (_car[0],     0, _filter[0],    0),   // portadora → SVF
    _c2 (_filter[0],  0, _ampEnv[0],    0),   // SVF LP (port 0) → AmpEnv
    // Voz 1
    _c3 (_mod[1],     0, _car[1],       0),
    _c4 (_car[1],     0, _filter[1],    0),
    _c5 (_filter[1],  0, _ampEnv[1],    0),
    // Voz 2
    _c6 (_mod[2],     0, _car[2],       0),
    _c7 (_car[2],     0, _filter[2],    0),
    _c8 (_filter[2],  0, _ampEnv[2],    0),
    // Voz 3
    _c9 (_mod[3],     0, _car[3],       0),
    _c10(_car[3],     0, _filter[3],    0),
    _c11(_filter[3],  0, _ampEnv[3],    0),
    // Voz 4
    _c12(_mod[4],     0, _car[4],       0),
    _c13(_car[4],     0, _filter[4],    0),
    _c14(_filter[4],  0, _ampEnv[4],    0),
    // Voz 5
    _c15(_mod[5],     0, _car[5],       0),
    _c16(_car[5],     0, _filter[5],    0),
    _c17(_filter[5],  0, _ampEnv[5],    0),
    // ampEnv → master mixers
    _c18(_ampEnv[0],  0, _masterMixA,   0),
    _c19(_ampEnv[1],  0, _masterMixA,   1),
    _c20(_ampEnv[2],  0, _masterMixA,   2),
    _c21(_ampEnv[3],  0, _masterMixA,   3),
    _c22(_ampEnv[4],  0, _masterMixB,   0),
    _c23(_ampEnv[5],  0, _masterMixB,   1),
    // master path
    _c24(_masterMixA, 0, _finalMix,     0),
    _c25(_masterMixB, 0, _finalMix,     1)
{}

// ── begin() ───────────────────────────────────────────────────────────────────

void DX7Engine::begin(float volume) {
    (void)volume;  // volumen lo gestiona el codec compartido (audio_graph::init)

    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        // Portadora (carrier): sine wave — FM clásico requiere portadora sinusoidal
        // para que la descomposición de Bessel del índice de modulación sea exacta.
        // frequencyModulation(2.0): sensibilidad de 2 octavas — la señal del modulador
        // a amplitud 1.0 desvía ±2 octavas. Con _fmDepth=0.5, la desviación real es ±1 octava.
        _car[i].begin(WAVEFORM_SINE);
        _car[i].amplitude(1.0f);
        _car[i].frequency(440.0f);
        _car[i].frequencyModulation(2.0f);

        // Modulador: sine wave — alimenta el port 0 de la portadora.
        // Su amplitud = _fmDepth determina el índice I de modulación.
        // Frecuencia = freq_carrier × _fmRatio (se actualiza en _triggerVoice).
        _mod[i].begin(WAVEFORM_SINE);
        _mod[i].amplitude(_fmDepth);
        _mod[i].frequency(440.0f * _fmRatio);

        // SVF: cutoff alto (FM es brillante de por sí), resonancia suave
        _filter[i].frequency(_filterCutoff);
        _filter[i].resonance(_filterResonance);
        _filter[i].octaveControl(1.0f);

        // AmpEnv: defaults DX7-style — rápido ataque, sustain alto (e.piano vibes)
        _ampEnv[i].attack(_filterEnvAttackMs);
        _ampEnv[i].decay(_filterEnvDecayMs);
        _ampEnv[i].sustain(_filterEnvSustain);
        _ampEnv[i].release(_filterEnvReleaseMs);

        // Estado inicial del filter envelope
        _filterEnvStage[i]   = EnvStage::IDLE;
        _filterEnvStageMs[i] = 0;
        _filterEnvLevel[i]   = 0.0f;
    }

    // masterMixA: 4 voces × 0.25 = 1.0 nivel máximo
    _masterMixA.gain(0, 0.25f);
    _masterMixA.gain(1, 0.25f);
    _masterMixA.gain(2, 0.25f);
    _masterMixA.gain(3, 0.25f);

    // masterMixB: voces 4-5 en ch0-1, ch2-3 vacíos
    _masterMixB.gain(0, 0.25f);
    _masterMixB.gain(1, 0.25f);
    _masterMixB.gain(2, 0.0f);
    _masterMixB.gain(3, 0.0f);

    // finalMix: ch0=masterMixA, ch1=masterMixB, ch2-3 vacíos
    _finalMix.gain(0, 1.0f);
    _finalMix.gain(1, 1.0f);
    _finalMix.gain(2, 0.0f);
    _finalMix.gain(3, 0.0f);

    _lastUpdateMs = millis();
}

// ── Note control ──────────────────────────────────────────────────────────────

void DX7Engine::noteOn(uint8_t midi_note, float velocity) {
    (void)velocity;

    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();

    _triggerVoice((uint8_t)voice, midi_note);
}

void DX7Engine::noteOff(uint8_t midi_note) {
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

// ── FM params ─────────────────────────────────────────────────────────────────

void DX7Engine::setFmRatio(float ratio) {
    _fmRatio = ratio;
    // Actualizar frecuencia del modulador en voces activas para efecto inmediato
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        if (_voices[i].midi_note != 255) {
            float base = _midiToFreq(_voices[i].midi_note);
            _mod[i].frequency(base * _fmRatio);
        }
    }
}

void DX7Engine::setFmDepth(float depth) {
    _fmDepth = depth;
    // La amplitud del modulador controla directamente el índice FM
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _mod[i].amplitude(_fmDepth);
    }
}

void DX7Engine::setCarrierWaveform(uint16_t w) {
    _carrierWave = w;
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _car[i].begin(w);
        // begin() resetea la amplitud a 0 en AudioSynthWaveformModulated — restaurar
        _car[i].amplitude(1.0f);
    }
}

void DX7Engine::setModWaveform(uint16_t w) {
    _modWave = w;
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _mod[i].begin(w);
        _mod[i].amplitude(_fmDepth);
    }
}

// ── Filter params ─────────────────────────────────────────────────────────────

void DX7Engine::setFilterCutoff(float hz) {
    _filterCutoff = hz;
    // La frecuencia real se aplica en update() con la modulación del envelope sumada
}

void DX7Engine::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _filter[i].resonance(q);
    }
}

void DX7Engine::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

// ── Envelope params ───────────────────────────────────────────────────────────

void DX7Engine::setAttack(float ms) {
    _filterEnvAttackMs = ms;
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _ampEnv[i].attack(ms);
    }
}

void DX7Engine::setDecay(float ms) {
    _filterEnvDecayMs = ms;
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _ampEnv[i].decay(ms);
    }
}

void DX7Engine::setSustain(float level) {
    _filterEnvSustain = level;
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _ampEnv[i].sustain(level);
    }
}

void DX7Engine::setRelease(float ms) {
    _filterEnvReleaseMs = ms;
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _ampEnv[i].release(ms);
    }
}

// ── update() — llamar desde loop() ────────────────────────────────────────────

void DX7Engine::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _updateFilterEnv(i, dt);

        // Aplicar cutoff base + modulación del envelope al SVF de esta voz.
        // Rango de modulación 8000 Hz (más amplio que otros engines) porque el
        // FM puro ya tiene muchos parciales altos — el envelope barre un rango mayor
        // para que sea expresivo en el espectro brillante del FM.
        float modulated = _filterCutoff + _filterEnvAmt * _filterEnvLevel[i] * 8000.0f;
        if (modulated < 500.0f)   modulated = 500.0f;
        if (modulated > 20000.0f) modulated = 20000.0f;
        _filter[i].frequency(modulated);
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void DX7Engine::_triggerVoice(uint8_t v, uint8_t midi_note) {
    float freq = _midiToFreq(midi_note);

    _car[v].frequency(freq);
    _car[v].amplitude(1.0f);
    _mod[v].frequency(freq * _fmRatio);
    _mod[v].amplitude(_fmDepth);   // amplitud del mod = índice FM

    _ampEnv[v].noteOn();

    // Re-iniciar filter envelope desde cero
    _filterEnvStage[v]   = EnvStage::ATTACK;
    _filterEnvStageMs[v] = 0;
    _filterEnvLevel[v]   = 0.0f;

    _voices[v].midi_note = midi_note;
    _voices[v].active    = true;
    _voices[v].age       = ++_ageCounter;
}

void DX7Engine::_releaseVoice(uint8_t v) {
    _ampEnv[v].noteOff();

    _filterEnvStage[v]   = EnvStage::RELEASE;
    _filterEnvStageMs[v] = 0;

    _voices[v].midi_note = 255;
    _voices[v].active    = false;
}

int8_t DX7Engine::_findFreeVoice() {
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        if (_voices[i].midi_note == 255) return (int8_t)i;
    }
    return -1;
}

int8_t DX7Engine::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t DX7Engine::_stealOldestVoice() {
    uint8_t  oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;
    for (uint8_t i = 1; i < DX7_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

void DX7Engine::_updateFilterEnv(uint8_t v, uint32_t dt_ms) {
    // Misma FSM que Prophet5/_updateFilterEnv, adaptada a DX7_VOICES.
    // Ver apps/firmware-teensy/src/engines/prophet_5.cpp para la lógica base.
    _filterEnvStageMs[v] += dt_ms;

    switch (_filterEnvStage[v]) {
        case EnvStage::IDLE:
            _filterEnvLevel[v] = 0.0f;
            break;

        case EnvStage::ATTACK:
            if (_filterEnvAttackMs < 1.0f) {
                _filterEnvLevel[v] = 1.0f;
                _filterEnvStage[v] = EnvStage::DECAY;
                _filterEnvStageMs[v] = 0;
            } else {
                _filterEnvLevel[v] = (float)_filterEnvStageMs[v] / _filterEnvAttackMs;
                if (_filterEnvLevel[v] >= 1.0f) {
                    _filterEnvLevel[v] = 1.0f;
                    _filterEnvStage[v] = EnvStage::DECAY;
                    _filterEnvStageMs[v] = 0;
                }
            }
            break;

        case EnvStage::DECAY:
            if (_filterEnvDecayMs < 1.0f) {
                _filterEnvLevel[v] = _filterEnvSustain;
                _filterEnvStage[v] = EnvStage::SUSTAIN;
            } else {
                float t = (float)_filterEnvStageMs[v] / _filterEnvDecayMs;
                _filterEnvLevel[v] = 1.0f - t * (1.0f - _filterEnvSustain);
                if (t >= 1.0f) {
                    _filterEnvLevel[v] = _filterEnvSustain;
                    _filterEnvStage[v] = EnvStage::SUSTAIN;
                }
            }
            break;

        case EnvStage::SUSTAIN:
            _filterEnvLevel[v] = _filterEnvSustain;
            break;

        case EnvStage::RELEASE:
            if (_filterEnvReleaseMs < 1.0f) {
                _filterEnvLevel[v] = 0.0f;
                _filterEnvStage[v] = EnvStage::IDLE;
            } else {
                float t = (float)_filterEnvStageMs[v] / _filterEnvReleaseMs;
                _filterEnvLevel[v] = _filterEnvSustain * (1.0f - t);
                if (t >= 1.0f) {
                    _filterEnvLevel[v] = 0.0f;
                    _filterEnvStage[v] = EnvStage::IDLE;
                }
            }
            break;
    }
}

float DX7Engine::_midiToFreq(uint8_t midi_note) {
    // A4 (MIDI 69) = 440Hz, cada semitono = factor 2^(1/12)
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

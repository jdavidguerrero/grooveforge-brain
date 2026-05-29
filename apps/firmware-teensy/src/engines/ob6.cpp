// apps/firmware-teensy/src/engines/ob6.cpp
// Sprint 38 — OB-6 Engine: 6-voice polyphony con 2 VCOs + sub + noise por voz.
// Theory: apps/docs/sprints/38-ob6-engine.md

#include "ob6.h"
#include <math.h>

// ── Constructor ───────────────────────────────────────────────────────────────
//
// Cada AudioConnection se construye con una referencia al src y al dst.
// La lista de inicialización garantiza que los arrays (_vco1, _vco2, etc.)
// se construyen antes que las conexiones (orden de declaración en el header).
//
// Por qué conexiones individuales (no array de AudioConnection):
// AudioConnection registra src/dst en el grafo global durante la construcción.
// No tiene copy constructor — un array de AudioConnections requeriría llamar
// al copy ctor en la brace-initialization, lo que no compila.
//
// Layout de conexiones (44 totales):
//   _c0–_c5:   voz 0  (vco1→mix(0), vco2→mix(1), sub→mix(2), noise→mix(3), mix→filter, filter(LP,port0)→ampEnv)
//   _c6–_c11:  voz 1
//   _c12–_c17: voz 2
//   _c18–_c23: voz 3
//   _c24–_c29: voz 4
//   _c30–_c35: voz 5
//   _c36:      ampEnv[0] → masterMixA ch0
//   _c37:      ampEnv[1] → masterMixA ch1
//   _c38:      ampEnv[2] → masterMixA ch2
//   _c39:      ampEnv[3] → masterMixA ch3
//   _c40:      ampEnv[4] → masterMixB ch0
//   _c41:      ampEnv[5] → masterMixB ch1
//   _c42:      masterMixA → finalMix ch0
//   _c43:      masterMixB → finalMix ch1
//
// AudioFilterStateVariable: puerto 0 = LP, 1 = BP, 2 = HP.
// Conectamos siempre al puerto 0 (LP). setFilterMode() es un placeholder v1.

OB6::OB6() :
    // Voz 0
    _c0 (_vco1[0],      0, _voiceMix[0], 0),
    _c1 (_vco2[0],      0, _voiceMix[0], 1),
    _c2 (_sub[0],       0, _voiceMix[0], 2),
    _c3 (_noise[0],     0, _voiceMix[0], 3),
    _c4 (_voiceMix[0],  0, _filter[0],   0),
    _c5 (_filter[0],    0, _ampEnv[0],   0),   // port 0 = LP output
    // Voz 1
    _c6 (_vco1[1],      0, _voiceMix[1], 0),
    _c7 (_vco2[1],      0, _voiceMix[1], 1),
    _c8 (_sub[1],       0, _voiceMix[1], 2),
    _c9 (_noise[1],     0, _voiceMix[1], 3),
    _c10(_voiceMix[1],  0, _filter[1],   0),
    _c11(_filter[1],    0, _ampEnv[1],   0),
    // Voz 2
    _c12(_vco1[2],      0, _voiceMix[2], 0),
    _c13(_vco2[2],      0, _voiceMix[2], 1),
    _c14(_sub[2],       0, _voiceMix[2], 2),
    _c15(_noise[2],     0, _voiceMix[2], 3),
    _c16(_voiceMix[2],  0, _filter[2],   0),
    _c17(_filter[2],    0, _ampEnv[2],   0),
    // Voz 3
    _c18(_vco1[3],      0, _voiceMix[3], 0),
    _c19(_vco2[3],      0, _voiceMix[3], 1),
    _c20(_sub[3],       0, _voiceMix[3], 2),
    _c21(_noise[3],     0, _voiceMix[3], 3),
    _c22(_voiceMix[3],  0, _filter[3],   0),
    _c23(_filter[3],    0, _ampEnv[3],   0),
    // Voz 4
    _c24(_vco1[4],      0, _voiceMix[4], 0),
    _c25(_vco2[4],      0, _voiceMix[4], 1),
    _c26(_sub[4],       0, _voiceMix[4], 2),
    _c27(_noise[4],     0, _voiceMix[4], 3),
    _c28(_voiceMix[4],  0, _filter[4],   0),
    _c29(_filter[4],    0, _ampEnv[4],   0),
    // Voz 5
    _c30(_vco1[5],      0, _voiceMix[5], 0),
    _c31(_vco2[5],      0, _voiceMix[5], 1),
    _c32(_sub[5],       0, _voiceMix[5], 2),
    _c33(_noise[5],     0, _voiceMix[5], 3),
    _c34(_voiceMix[5],  0, _filter[5],   0),
    _c35(_filter[5],    0, _ampEnv[5],   0),
    // ampEnv → master mixers
    _c36(_ampEnv[0],    0, _masterMixA,  0),
    _c37(_ampEnv[1],    0, _masterMixA,  1),
    _c38(_ampEnv[2],    0, _masterMixA,  2),
    _c39(_ampEnv[3],    0, _masterMixA,  3),
    _c40(_ampEnv[4],    0, _masterMixB,  0),
    _c41(_ampEnv[5],    0, _masterMixB,  1),
    // master path
    _c42(_masterMixA,   0, _finalMix,    0),
    _c43(_masterMixB,   0, _finalMix,    1)
{}

// ── begin() ───────────────────────────────────────────────────────────────────

void OB6::begin(float volume) {
    (void)volume;  // volumen lo gestiona el codec compartido (audio_graph::init)

    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        // VCO1: sawtooth — carácter primario del OB-6 (Juno-style DCO rich harmonics)
        _vco1[i].begin(WAVEFORM_SAWTOOTH);
        _vco1[i].amplitude(1.0f);
        _vco1[i].frequency(440.0f);

        // VCO2: sawtooth — se puede detunar via setVco2Interval()
        _vco2[i].begin(WAVEFORM_SAWTOOTH);
        _vco2[i].amplitude(1.0f);
        _vco2[i].frequency(440.0f);

        // Sub oscillator: square, 1 octava por debajo de VCO1.
        // La frecuencia real se asigna en _triggerVoice() con freq * 0.5.
        // Inicializamos en square para que begin() no dependa de un noteOn previo.
        _sub[i].begin(WAVEFORM_SQUARE);
        _sub[i].amplitude(1.0f);
        _sub[i].frequency(220.0f);

        // Noise: amplitude=0 en init, controlado via voiceMix ch3
        _noise[i].amplitude(0.0f);

        // VoiceMix: defaults del OB-6 real — VCO1/VCO2 dominantes, sub sutil, noise off
        _voiceMix[i].gain(0, 0.45f);  // vco1
        _voiceMix[i].gain(1, 0.45f);  // vco2
        _voiceMix[i].gain(2, 0.10f);  // sub
        _voiceMix[i].gain(3, 0.0f);   // noise — off por defecto

        // VCF: AudioFilterStateVariable — placeholder digital para el CEM3320 del OB-6.
        // octaveControl(1.0f): 1 octava por unidad de modulación lineal (rango SVF).
        _filter[i].frequency(_filterCutoff);
        _filter[i].resonance(_filterResonance);
        _filter[i].octaveControl(1.0f);

        // VCA envelope: ADSR comparte el mismo tiempo que el filter envelope
        _ampEnv[i].attack(_filterEnvAttackMs);
        _ampEnv[i].decay(_filterEnvDecayMs);
        _ampEnv[i].sustain(_filterEnvSustain);
        _ampEnv[i].release(_filterEnvReleaseMs);

        // Estado inicial del filter envelope
        _filterEnvStage[i]   = EnvStage::IDLE;
        _filterEnvStageMs[i] = 0;
        _filterEnvLevel[i]   = 0.0f;
    }

    // masterMixA: 4 voces × 0.25 = 1.0 nivel máximo en bus
    _masterMixA.gain(0, 0.25f);
    _masterMixA.gain(1, 0.25f);
    _masterMixA.gain(2, 0.25f);
    _masterMixA.gain(3, 0.25f);

    // masterMixB: voces 4-5 en canales 0-1; canales 2-3 silenciados
    _masterMixB.gain(0, 0.25f);
    _masterMixB.gain(1, 0.25f);
    _masterMixB.gain(2, 0.0f);
    _masterMixB.gain(3, 0.0f);

    // finalMix: combina ambos submixers sin atenuación adicional
    _finalMix.gain(0, 1.0f);
    _finalMix.gain(1, 1.0f);
    _finalMix.gain(2, 0.0f);
    _finalMix.gain(3, 0.0f);

    _lastUpdateMs = millis();
}

// ── Note control ──────────────────────────────────────────────────────────────

void OB6::noteOn(uint8_t midi_note, float velocity) {
    (void)velocity;

    // Re-trigger si la nota ya suena — evita duplicar voces para la misma nota
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();

    _triggerVoice((uint8_t)voice, midi_note);
}

void OB6::noteOff(uint8_t midi_note) {
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

// ── VCO params ────────────────────────────────────────────────────────────────

void OB6::setVco1Waveform(uint16_t waveform) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _vco1[i].begin(waveform);
    }
}

void OB6::setVco2Waveform(uint16_t waveform) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _vco2[i].begin(waveform);
    }
}

void OB6::setVco2Interval(float semitones) {
    _vco2Semitones = semitones;
    // Actualizar frecuencias de VCO2 en voces activas
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        if (_voices[i].midi_note != 255) {
            float baseFreq = _midiToFreq(_voices[i].midi_note);
            _vco2[i].frequency(baseFreq * _semitonesToRatio(_vco2Semitones));
        }
    }
}

void OB6::setOscMix(float vco1, float vco2, float sub, float noise) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _voiceMix[i].gain(0, vco1);
        _voiceMix[i].gain(1, vco2);
        _voiceMix[i].gain(2, sub);
        _voiceMix[i].gain(3, noise);
    }
}

// ── Filter params ─────────────────────────────────────────────────────────────

void OB6::setFilterCutoff(float hz) {
    _filterCutoff = hz;
    // La frecuencia real se aplica en update() con la modulación del envelope sumada
}

void OB6::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filter[i].resonance(q);
    }
}

void OB6::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

void OB6::setFilterMode(uint8_t mode) {
    // v1: solo LP (puerto 0 hardcodeado en las AudioConnections del constructor).
    // Para soportar BP/HP en runtime se necesita un AudioMixer4 por voz que sume
    // las 3 salidas del SVF con gains proporcionales — pendiente sprint futuro.
    // El campo _filterMode se almacena para compatibilidad futura.
    _filterMode = mode;
    // No-op intencional. Ver Doxygen en ob6.h para la justificación.
}

// ── Envelope params ───────────────────────────────────────────────────────────

void OB6::setAttack(float ms) {
    _filterEnvAttackMs = ms;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _ampEnv[i].attack(ms);
    }
}

void OB6::setDecay(float ms) {
    _filterEnvDecayMs = ms;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _ampEnv[i].decay(ms);
    }
}

void OB6::setSustain(float level) {
    _filterEnvSustain = level;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _ampEnv[i].sustain(level);
    }
}

void OB6::setRelease(float ms) {
    _filterEnvReleaseMs = ms;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _ampEnv[i].release(ms);
    }
}

// ── update() — llamar desde loop() ────────────────────────────────────────────

void OB6::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        // 1. Filter envelope software (misma FSM que Prophet5)
        _updateFilterEnv(i, dt);

        // 2. Aplicar cutoff + modulación del envelope al VCF de esta voz.
        // El envelope agrega hasta _filterEnvAmt × 6000 Hz sobre el cutoff base.
        float modulated = _filterCutoff + _filterEnvAmt * _filterEnvLevel[i] * 6000.0f;
        if (modulated < 20.0f)    modulated = 20.0f;
        if (modulated > 20000.0f) modulated = 20000.0f;
        _filter[i].frequency(modulated);
        // El OB-6 no tiene FM cross-modulation entre VCOs (a diferencia del Prophet-5
        // Poly-Mod). Sin ningún mod adicional, update() es más simple.
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void OB6::_triggerVoice(uint8_t voice_idx, uint8_t midi_note) {
    float freq = _midiToFreq(midi_note);

    _vco1[voice_idx].frequency(freq);
    _vco2[voice_idx].frequency(freq * _semitonesToRatio(_vco2Semitones));
    // Sub osc: 1 octava por debajo de VCO1 — multiplicar por 0.5 es equivalente
    // a bajar exactamente 12 semitones (factor 2^(-1) = 0.5).
    _sub[voice_idx].frequency(freq * 0.5f);

    _ampEnv[voice_idx].noteOn();

    // Re-iniciar filter envelope desde cero (retrigger limpio)
    _filterEnvStage[voice_idx]   = EnvStage::ATTACK;
    _filterEnvStageMs[voice_idx] = 0;
    _filterEnvLevel[voice_idx]   = 0.0f;

    _voices[voice_idx].midi_note = midi_note;
    _voices[voice_idx].active    = true;
    _voices[voice_idx].age       = ++_ageCounter;
}

void OB6::_releaseVoice(uint8_t voice_idx) {
    _ampEnv[voice_idx].noteOff();

    _filterEnvStage[voice_idx]   = EnvStage::RELEASE;
    _filterEnvStageMs[voice_idx] = 0;

    // Marcar como libre — midi_note=255 es el centinela de "voz disponible"
    _voices[voice_idx].midi_note = 255;
    _voices[voice_idx].active    = false;
}

int8_t OB6::_findFreeVoice() {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        if (_voices[i].midi_note == 255) return (int8_t)i;
    }
    return -1;
}

int8_t OB6::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t OB6::_stealOldestVoice() {
    // Robar la voz con menor age (la que lleva más tiempo sonando)
    uint8_t  oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;

    for (uint8_t i = 1; i < OB6_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

void OB6::_updateFilterEnv(uint8_t voice_idx, uint32_t dt_ms) {
    // Misma FSM que Prophet5._updateFilterEnv.
    // Ver prophet_5.cpp para comentarios detallados del algoritmo.
    _filterEnvStageMs[voice_idx] += dt_ms;

    switch (_filterEnvStage[voice_idx]) {
        case EnvStage::IDLE:
            _filterEnvLevel[voice_idx] = 0.0f;
            break;

        case EnvStage::ATTACK:
            if (_filterEnvAttackMs < 1.0f) {
                _filterEnvLevel[voice_idx] = 1.0f;
                _filterEnvStage[voice_idx] = EnvStage::DECAY;
                _filterEnvStageMs[voice_idx] = 0;
            } else {
                _filterEnvLevel[voice_idx] =
                    (float)_filterEnvStageMs[voice_idx] / _filterEnvAttackMs;
                if (_filterEnvLevel[voice_idx] >= 1.0f) {
                    _filterEnvLevel[voice_idx] = 1.0f;
                    _filterEnvStage[voice_idx] = EnvStage::DECAY;
                    _filterEnvStageMs[voice_idx] = 0;
                }
            }
            break;

        case EnvStage::DECAY:
            if (_filterEnvDecayMs < 1.0f) {
                _filterEnvLevel[voice_idx] = _filterEnvSustain;
                _filterEnvStage[voice_idx] = EnvStage::SUSTAIN;
            } else {
                float t = (float)_filterEnvStageMs[voice_idx] / _filterEnvDecayMs;
                _filterEnvLevel[voice_idx] = 1.0f - t * (1.0f - _filterEnvSustain);
                if (t >= 1.0f) {
                    _filterEnvLevel[voice_idx] = _filterEnvSustain;
                    _filterEnvStage[voice_idx] = EnvStage::SUSTAIN;
                }
            }
            break;

        case EnvStage::SUSTAIN:
            _filterEnvLevel[voice_idx] = _filterEnvSustain;
            break;

        case EnvStage::RELEASE:
            if (_filterEnvReleaseMs < 1.0f) {
                _filterEnvLevel[voice_idx] = 0.0f;
                _filterEnvStage[voice_idx] = EnvStage::IDLE;
            } else {
                float t = (float)_filterEnvStageMs[voice_idx] / _filterEnvReleaseMs;
                _filterEnvLevel[voice_idx] = _filterEnvSustain * (1.0f - t);
                if (t >= 1.0f) {
                    _filterEnvLevel[voice_idx] = 0.0f;
                    _filterEnvStage[voice_idx] = EnvStage::IDLE;
                }
            }
            break;
    }
}

float OB6::_midiToFreq(uint8_t midi_note) {
    // A4 (MIDI 69) = 440Hz, cada semitono = factor 2^(1/12)
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

float OB6::_semitonesToRatio(float semitones) {
    // 12 semitones = 1 octava = factor 2.0
    return powf(2.0f, semitones / 12.0f);
}

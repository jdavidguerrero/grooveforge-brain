// apps/firmware-teensy/src/engines/arp2600.cpp
// Sprint 39 — ARP 2600 Engine: 4-voice polyphony con ring modulator + noise.
// Theory: apps/docs/sprints/39-arp2600-engine.md

#include "arp2600.h"
#include <math.h>

// ── Constructor ───────────────────────────────────────────────────────────────
//
// AudioConnection se registra en el grafo global en tiempo de construcción.
// No tiene copy constructor — los arrays brace-init llamarían al copy ctor
// y no compilan. Por eso cada conexión es un miembro nombrado.
//
// Layout de conexiones (33 totales):
//   _c0–_c6:   voz 0  (7 conexiones)
//   _c7–_c13:  voz 1
//   _c14–_c20: voz 2
//   _c21–_c27: voz 3
//   _c28–_c31: ampEnv[0-3] → masterMixA ch0-3
//   _c32:      masterMixA  → finalMix ch0
//
// Ring modulator (AudioEffectMultiply): requiere exactamente 2 inputs.
//   Input 0 (port 0): VCO1 → _ringMod
//   Input 1 (port 1): VCO2 → _ringMod
// La señal de salida es el producto punto a punto de las dos señales.
// Con VCO1=sawtooth y VCO2=sine: salida ≈ AM con portadora parcialmente suprimida
// (no es AM DSB-SC puro porque el sawtooth tiene DC offset).

ARP2600::ARP2600() :
    // Voz 0
    _c0 (_vco1[0],     0, _ringMod[0],    0),   // VCO1 → ring mod input A (port 0)
    _c1 (_vco2[0],     0, _ringMod[0],    1),   // VCO2 → ring mod input B (port 1)
    _c2 (_ringMod[0],  0, _voiceMix[0],   0),   // ring mod out → mix ch0
    _c3 (_vco1[0],     0, _voiceMix[0],   1),   // VCO1 dry → mix ch1
    _c4 (_noise[0],    0, _voiceMix[0],   2),   // noise → mix ch2
    _c5 (_voiceMix[0], 0, _filter[0],     0),   // mix → filter
    _c6 (_filter[0],   0, _ampEnv[0],     0),   // filter LP (port 0) → ampEnv
    // Voz 1
    _c7 (_vco1[1],     0, _ringMod[1],    0),
    _c8 (_vco2[1],     0, _ringMod[1],    1),
    _c9 (_ringMod[1],  0, _voiceMix[1],   0),
    _c10(_vco1[1],     0, _voiceMix[1],   1),
    _c11(_noise[1],    0, _voiceMix[1],   2),
    _c12(_voiceMix[1], 0, _filter[1],     0),
    _c13(_filter[1],   0, _ampEnv[1],     0),
    // Voz 2
    _c14(_vco1[2],     0, _ringMod[2],    0),
    _c15(_vco2[2],     0, _ringMod[2],    1),
    _c16(_ringMod[2],  0, _voiceMix[2],   0),
    _c17(_vco1[2],     0, _voiceMix[2],   1),
    _c18(_noise[2],    0, _voiceMix[2],   2),
    _c19(_voiceMix[2], 0, _filter[2],     0),
    _c20(_filter[2],   0, _ampEnv[2],     0),
    // Voz 3
    _c21(_vco1[3],     0, _ringMod[3],    0),
    _c22(_vco2[3],     0, _ringMod[3],    1),
    _c23(_ringMod[3],  0, _voiceMix[3],   0),
    _c24(_vco1[3],     0, _voiceMix[3],   1),
    _c25(_noise[3],    0, _voiceMix[3],   2),
    _c26(_voiceMix[3], 0, _filter[3],     0),
    _c27(_filter[3],   0, _ampEnv[3],     0),
    // ampEnv → masterMixA
    _c28(_ampEnv[0],   0, _masterMixA,    0),
    _c29(_ampEnv[1],   0, _masterMixA,    1),
    _c30(_ampEnv[2],   0, _masterMixA,    2),
    _c31(_ampEnv[3],   0, _masterMixA,    3),
    // master path
    _c32(_masterMixA,  0, _finalMix,      0)
{}

// ── begin() ───────────────────────────────────────────────────────────────────

void ARP2600::begin(float volume) {
    (void)volume;  // volumen lo gestiona el codec compartido (audio_graph::init)

    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        // VCO1: sawtooth — fuente principal, rico en armónicos.
        // También alimenta el ring mod como primer operando.
        _vco1[i].begin(WAVEFORM_SAWTOOTH);
        _vco1[i].amplitude(1.0f);
        _vco1[i].frequency(440.0f);

        // VCO2: sine — segundo operando del ring mod. Con sine puro y VCO1 sawtooth,
        // la salida del multiplicador tiene parciales a (f_vco1 ± f_vco2) para cada
        // armónico del sawtooth. Ratio 1.0 produce un carácter AM agresivo.
        _vco2[i].begin(WAVEFORM_SINE);
        _vco2[i].amplitude(1.0f);
        _vco2[i].frequency(440.0f);

        _noise[i].amplitude(1.0f);

        // VoiceMix: ch0=ring, ch1=VCO1 dry, ch2=noise, ch3=sin uso
        // _ringMixAmt default 0.5 → ring y dry a igual nivel
        _voiceMix[i].gain(0, _ringMixAmt);
        _voiceMix[i].gain(1, 1.0f - _ringMixAmt);
        _voiceMix[i].gain(2, 0.0f);   // noise off por defecto
        _voiceMix[i].gain(3, 0.0f);

        // SVF: LP (port 0) conectado, resonancia moderada
        _filter[i].frequency(_filterCutoff);
        _filter[i].resonance(_filterResonance);
        _filter[i].octaveControl(1.0f);

        // AmpEnv: defaults ARP 2600 — ataque suave, decay/release más largos
        _ampEnv[i].attack(_filterEnvAttackMs);
        _ampEnv[i].decay(_filterEnvDecayMs);
        _ampEnv[i].sustain(_filterEnvSustain);
        _ampEnv[i].release(_filterEnvReleaseMs);

        // Estado inicial del filter envelope
        _filterEnvStage[i]   = EnvStage::IDLE;
        _filterEnvStageMs[i] = 0;
        _filterEnvLevel[i]   = 0.0f;
    }

    // masterMixA: 4 voces × 0.25 = 1.0 nivel máximo en el bus
    _masterMixA.gain(0, 0.25f);
    _masterMixA.gain(1, 0.25f);
    _masterMixA.gain(2, 0.25f);
    _masterMixA.gain(3, 0.25f);

    // finalMix: solo ch0 activo (masterMixA)
    _finalMix.gain(0, 1.0f);
    _finalMix.gain(1, 0.0f);
    _finalMix.gain(2, 0.0f);
    _finalMix.gain(3, 0.0f);

    _lastUpdateMs = millis();
}

// ── Note control ──────────────────────────────────────────────────────────────

void ARP2600::noteOn(uint8_t midi_note, float velocity) {
    (void)velocity;

    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();

    _triggerVoice((uint8_t)voice, midi_note);
}

void ARP2600::noteOff(uint8_t midi_note) {
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

// ── OSC params ────────────────────────────────────────────────────────────────

void ARP2600::setVco1Waveform(uint16_t waveform) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _vco1[i].begin(waveform);
    }
}

void ARP2600::setVco2Waveform(uint16_t waveform) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _vco2[i].begin(waveform);
    }
}

void ARP2600::setVco2Ratio(float ratio) {
    _vco2Ratio = ratio;
    // Actualizar frecuencia de VCO2 en voces activas para que el cambio sea audible
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        if (_voices[i].midi_note != 255) {
            float base = _midiToFreq(_voices[i].midi_note);
            _vco2[i].frequency(base * _vco2Ratio);
        }
    }
}

void ARP2600::setRingMix(float mix) {
    _ringMixAmt = mix;
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _voiceMix[i].gain(0, mix);
        _voiceMix[i].gain(1, 1.0f - mix);
    }
}

void ARP2600::setNoiseLevel(float level) {
    _noiseLevel = level;
    // Escalar a 0.5 × level para no clipear el voiceMix cuando noise + ring + dry suman
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _voiceMix[i].gain(2, level * 0.5f);
    }
}

// ── Filter params ─────────────────────────────────────────────────────────────

void ARP2600::setFilterCutoff(float hz) {
    _filterCutoff = hz;
    // La frecuencia real se aplica en update() con la modulación del envelope sumada
}

void ARP2600::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _filter[i].resonance(q);
    }
}

void ARP2600::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

// ── Envelope params ───────────────────────────────────────────────────────────

void ARP2600::setAttack(float ms) {
    _filterEnvAttackMs = ms;
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _ampEnv[i].attack(ms);
    }
}

void ARP2600::setDecay(float ms) {
    _filterEnvDecayMs = ms;
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _ampEnv[i].decay(ms);
    }
}

void ARP2600::setSustain(float level) {
    _filterEnvSustain = level;
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _ampEnv[i].sustain(level);
    }
}

void ARP2600::setRelease(float ms) {
    _filterEnvReleaseMs = ms;
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _ampEnv[i].release(ms);
    }
}

// ── update() — llamar desde loop() ────────────────────────────────────────────

void ARP2600::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _updateFilterEnv(i, dt);

        // Aplicar cutoff base + modulación del envelope al SVF de esta voz.
        // El envelope agrega hasta _filterEnvAmt × 6000 Hz sobre el cutoff base.
        float modulated = _filterCutoff + _filterEnvAmt * _filterEnvLevel[i] * 6000.0f;
        if (modulated < 20.0f)    modulated = 20.0f;
        if (modulated > 20000.0f) modulated = 20000.0f;
        _filter[i].frequency(modulated);
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void ARP2600::_triggerVoice(uint8_t v, uint8_t midi_note) {
    float freq = _midiToFreq(midi_note);
    _vco1[v].frequency(freq);
    _vco2[v].frequency(freq * _vco2Ratio);   // VCO2 a ratio × base

    _ampEnv[v].noteOn();

    // Re-iniciar filter envelope desde cero (retrigger limpio)
    _filterEnvStage[v]   = EnvStage::ATTACK;
    _filterEnvStageMs[v] = 0;
    _filterEnvLevel[v]   = 0.0f;

    _voices[v].midi_note = midi_note;
    _voices[v].active    = true;
    _voices[v].age       = ++_ageCounter;
}

void ARP2600::_releaseVoice(uint8_t v) {
    _ampEnv[v].noteOff();

    _filterEnvStage[v]   = EnvStage::RELEASE;
    _filterEnvStageMs[v] = 0;

    _voices[v].midi_note = 255;
    _voices[v].active    = false;
}

int8_t ARP2600::_findFreeVoice() {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        if (_voices[i].midi_note == 255) return (int8_t)i;
    }
    return -1;
}

int8_t ARP2600::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t ARP2600::_stealOldestVoice() {
    uint8_t  oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;
    for (uint8_t i = 1; i < ARP_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

void ARP2600::_updateFilterEnv(uint8_t v, uint32_t dt_ms) {
    // Misma FSM que Prophet5/_updateFilterEnv, adaptada a ARP_VOICES.
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

float ARP2600::_midiToFreq(uint8_t midi_note) {
    // A4 (MIDI 69) = 440Hz, cada semitono = factor 2^(1/12)
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

// apps/firmware-teensy/src/engines/juno_106.cpp
// Sprint 41 — Juno-106 Engine: migración mono → 6 voces polifónicas.
// Theory, wiring y demo: apps/docs/sprints/06-juno-106.md

#include "juno_106.h"
#include <math.h>

// ── Constructor ───────────────────────────────────────────────────────────────
//
// Cada AudioConnection se construye con una referencia al src y al dst.
// La lista de inicialización garantiza que los arrays (_dco, _sub, etc.)
// se construyen antes que las conexiones (orden de declaración en el header).
//
// Por qué conexiones individuales (no array de AudioConnection):
// AudioConnection registra src/dst en el grafo global durante la construcción.
// No tiene copy constructor — un array de AudioConnections requeriría llamar
// al copy ctor en la brace-initialization, lo que no compila.
//
// Layout de conexiones (41 totales):
//   _c0–_c4:   voz 0  (dco→mix(0), sub→mix(1), noise→mix(2), mix→filter, filter→ampEnv)
//   _c5–_c9:   voz 1
//   _c10–_c14: voz 2
//   _c15–_c19: voz 3
//   _c20–_c24: voz 4
//   _c25–_c29: voz 5
//   _c30:      ampEnv[0] → masterMixA ch0
//   _c31:      ampEnv[1] → masterMixA ch1
//   _c32:      ampEnv[2] → masterMixA ch2
//   _c33:      ampEnv[3] → masterMixA ch3
//   _c34:      ampEnv[4] → masterMixB ch0
//   _c35:      ampEnv[5] → masterMixB ch1
//   _c36:      masterMixA → masterBus ch0
//   _c37:      masterMixB → masterBus ch1
//   _c38:      masterBus → chorus (wet send)
//   _c39:      chorus → dryWetMix ch1 (wet return)
//   _c40:      masterBus → dryWetMix ch0 (dry)
//
// El chorus opera sobre el bus completo (no por voz) — así funciona el Juno-106
// real que tenía un único chip MN3009 BBD para las 6 voces.

Juno106::Juno106() :
    // Voz 0
    _c0 (_dco[0],      0, _voiceMix[0], 0),
    _c1 (_sub[0],      0, _voiceMix[0], 1),
    _c2 (_noise[0],    0, _voiceMix[0], 2),
    _c3 (_voiceMix[0], 0, _filter[0],   0),
    _c4 (_filter[0],   0, _ampEnv[0],   0),
    // Voz 1
    _c5 (_dco[1],      0, _voiceMix[1], 0),
    _c6 (_sub[1],      0, _voiceMix[1], 1),
    _c7 (_noise[1],    0, _voiceMix[1], 2),
    _c8 (_voiceMix[1], 0, _filter[1],   0),
    _c9 (_filter[1],   0, _ampEnv[1],   0),
    // Voz 2
    _c10(_dco[2],      0, _voiceMix[2], 0),
    _c11(_sub[2],      0, _voiceMix[2], 1),
    _c12(_noise[2],    0, _voiceMix[2], 2),
    _c13(_voiceMix[2], 0, _filter[2],   0),
    _c14(_filter[2],   0, _ampEnv[2],   0),
    // Voz 3
    _c15(_dco[3],      0, _voiceMix[3], 0),
    _c16(_sub[3],      0, _voiceMix[3], 1),
    _c17(_noise[3],    0, _voiceMix[3], 2),
    _c18(_voiceMix[3], 0, _filter[3],   0),
    _c19(_filter[3],   0, _ampEnv[3],   0),
    // Voz 4
    _c20(_dco[4],      0, _voiceMix[4], 0),
    _c21(_sub[4],      0, _voiceMix[4], 1),
    _c22(_noise[4],    0, _voiceMix[4], 2),
    _c23(_voiceMix[4], 0, _filter[4],   0),
    _c24(_filter[4],   0, _ampEnv[4],   0),
    // Voz 5
    _c25(_dco[5],      0, _voiceMix[5], 0),
    _c26(_sub[5],      0, _voiceMix[5], 1),
    _c27(_noise[5],    0, _voiceMix[5], 2),
    _c28(_voiceMix[5], 0, _filter[5],   0),
    _c29(_filter[5],   0, _ampEnv[5],   0),
    // ampEnv → masterMixA (voces 0-3)
    _c30(_ampEnv[0],   0, _masterMixA,  0),
    _c31(_ampEnv[1],   0, _masterMixA,  1),
    _c32(_ampEnv[2],   0, _masterMixA,  2),
    _c33(_ampEnv[3],   0, _masterMixA,  3),
    // ampEnv → masterMixB (voces 4-5)
    _c34(_ampEnv[4],   0, _masterMixB,  0),
    _c35(_ampEnv[5],   0, _masterMixB,  1),
    // master bus
    _c36(_masterMixA,  0, _masterBus,   0),
    _c37(_masterMixB,  0, _masterBus,   1),
    // chorus path
    _c38(_masterBus,   0, _chorus,      0),
    _c39(_chorus,      0, _dryWetMix,   1),
    _c40(_masterBus,   0, _dryWetMix,   0)
{}

// ── begin() ───────────────────────────────────────────────────────────────────

void Juno106::begin(float volume) {
    (void)volume;  // volumen lo gestiona el codec compartido (audio_graph::init)

    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        // DCO principal — sawtooth por defecto (más rico en armónicos, base del Juno)
        _dco[i].begin(WAVEFORM_SAWTOOTH);
        _dco[i].amplitude(1.0f);
        _dco[i].frequency(440.0f);

        // Sub-oscillator — siempre square, siempre una octava abajo.
        // El sub-osc del Juno no tiene forma de onda seleccionable — es square fijo.
        _sub[i].begin(WAVEFORM_SQUARE);
        _sub[i].amplitude(1.0f);
        _sub[i].frequency(220.0f);   // 440/2 = 220Hz (A3), se actualiza en noteOn

        _noise[i].amplitude(1.0f);

        // voiceMix: DCO domina, sub engorda el grave, noise desactivado por defecto.
        // ch3 no se usa (el OB-6 tiene 4 fuentes; el Juno solo tiene 3).
        _voiceMix[i].gain(0, 0.6f);   // DCO
        _voiceMix[i].gain(1, 0.3f);   // sub
        _voiceMix[i].gain(2, 0.0f);   // noise — off por defecto
        _voiceMix[i].gain(3, 0.0f);   // no usado

        // VCF — la frecuencia real se actualiza en update() con la modulación del envelope
        _filter[i].frequency(_filterCutoff);
        _filter[i].resonance(0.7f);    // Butterworth flat — sin énfasis inicial
        _filter[i].octaveControl(1.0f);

        // VCA envelope — tiempos iniciales replican el preset "strings" del Juno
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

    // masterBus: combina ambos submixers sin atenuación adicional
    _masterBus.gain(0, 1.0f);
    _masterBus.gain(1, 1.0f);
    _masterBus.gain(2, 0.0f);
    _masterBus.gain(3, 0.0f);

    // Chorus BBD — begin() requiere el buffer externo + su longitud + número de voces.
    // n_voices=3 es el sweet spot para el sonido Juno sin CPU excesivo.
    _chorus.begin(_chorusBuf, JUNO_CHORUS_BUF_LEN, 3);

    // dryWetMix inicial: Modo I default (dry 60%, wet 40%)
    _dryWetMix.gain(0, 0.6f);   // dry
    _dryWetMix.gain(1, 0.4f);   // wet — chorus Modo I
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    _lastUpdateMs = millis();
}

// ── Note control ──────────────────────────────────────────────────────────────

void Juno106::noteOn(uint8_t midi_note, float velocity) {
    (void)velocity;

    // Re-trigger si la nota ya suena — evita duplicar voces para la misma nota
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();

    _triggerVoice((uint8_t)voice, midi_note);
}

void Juno106::noteOff(uint8_t midi_note) {
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

// ── DCO params ────────────────────────────────────────────────────────────────

void Juno106::setWaveform(uint16_t waveform) {
    // Solo cambia el DCO principal — el sub siempre es WAVEFORM_SQUARE (spec Juno)
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _dco[i].begin(waveform);
    }
}

void Juno106::setSubLevel(float level) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _voiceMix[i].gain(1, level);
    }
}

void Juno106::setNoiseLevel(float level) {
    // Atenuar ×0.5 para evitar clipping cuando noise + DCO suman
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _voiceMix[i].gain(2, level * 0.5f);
    }
}

// ── Filter params ─────────────────────────────────────────────────────────────

void Juno106::setFilterCutoff(float hz) {
    _filterCutoff = hz;
    // La frecuencia real se aplica en update() con la modulación del envelope sumada
}

void Juno106::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _filter[i].resonance(q);
    }
}

void Juno106::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

// ── ADSR compartido ───────────────────────────────────────────────────────────
// Cada setter actualiza la copia interna (filter envelope software) y el VCA
// envelope (AudioEffectEnvelope) de todas las voces. Un solo ADSR controla ambos
// — diferencia clave con el MoogModelD que tiene dos envelopes independientes.

void Juno106::setAttack(float ms) {
    _filterEnvAttackMs = ms;
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _ampEnv[i].attack(ms);
    }
}

void Juno106::setDecay(float ms) {
    _filterEnvDecayMs = ms;
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _ampEnv[i].decay(ms);
    }
}

void Juno106::setSustain(float level) {
    _filterEnvSustain = level;
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _ampEnv[i].sustain(level);
    }
}

void Juno106::setRelease(float ms) {
    _filterEnvReleaseMs = ms;
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _ampEnv[i].release(ms);
    }
}

// ── Chorus ────────────────────────────────────────────────────────────────────

void Juno106::setChorus(bool enabled, uint8_t mode) {
    _chorusEnabled = enabled;
    _chorusMode    = mode;

    if (!enabled) {
        // Bypass total: solo señal dry, chorus silenciado
        _dryWetMix.gain(0, 1.0f);
        _dryWetMix.gain(1, 0.0f);
        return;
    }

    if (mode == 2) {
        // Modo II: chorus pronunciado — más wet, más movimiento percibido
        _dryWetMix.gain(0, 0.4f);
        _dryWetMix.gain(1, 0.6f);
        _chorus.voices(4);   // más voces = más densidad de chorus
    } else {
        // Modo I (default): chorus sutil — strings, pads
        _dryWetMix.gain(0, 0.6f);
        _dryWetMix.gain(1, 0.4f);
        _chorus.voices(3);
    }
}

// ── update() — llamar desde loop() ────────────────────────────────────────────

void Juno106::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        // 1. Filter envelope software
        _updateFilterEnv(i, dt);

        // 2. Aplicar cutoff + modulación del envelope al VCF de esta voz.
        // El envelope agrega hasta _filterEnvAmt × 8000 Hz sobre el cutoff base.
        // 8000Hz es el headroom útil: cutoff 1200Hz + 8000Hz = 9200Hz (dentro del audible)
        float modulated = _filterCutoff + _filterEnvAmt * _filterEnvLevel[i] * 8000.0f;
        if (modulated < 20.0f)    modulated = 20.0f;
        if (modulated > 20000.0f) modulated = 20000.0f;
        _filter[i].frequency(modulated);
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Juno106::_triggerVoice(uint8_t voice_idx, uint8_t midi_note) {
    float freq = _midiToFreq(midi_note);

    _dco[voice_idx].frequency(freq);
    // Sub osc: 1 octava por debajo del DCO — multiplicar por 0.5 es equivalente
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

void Juno106::_releaseVoice(uint8_t voice_idx) {
    _ampEnv[voice_idx].noteOff();

    _filterEnvStage[voice_idx]   = EnvStage::RELEASE;
    _filterEnvStageMs[voice_idx] = 0;

    // Marcar como libre — midi_note=255 es el centinela de "voz disponible"
    _voices[voice_idx].midi_note = 255;
    _voices[voice_idx].active    = false;
}

int8_t Juno106::_findFreeVoice() {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        if (_voices[i].midi_note == 255) return (int8_t)i;
    }
    return -1;
}

int8_t Juno106::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t Juno106::_stealOldestVoice() {
    // Robar la voz con menor age (la que lleva más tiempo sonando)
    uint8_t  oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;

    for (uint8_t i = 1; i < JUNO_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

void Juno106::_updateFilterEnv(uint8_t voice_idx, uint32_t dt_ms) {
    // Misma FSM que OB6._updateFilterEnv.
    // Ver ob6.cpp para comentarios detallados del algoritmo.
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
                // Aproximación: release desde el nivel de sustain.
                // Fix correcto para release desde decay/attack: snapshot del nivel
                // en el momento del noteOff — pendiente sprint futuro.
                _filterEnvLevel[voice_idx] = _filterEnvSustain * (1.0f - t);
                if (t >= 1.0f) {
                    _filterEnvLevel[voice_idx] = 0.0f;
                    _filterEnvStage[voice_idx] = EnvStage::IDLE;
                }
            }
            break;
    }
}

float Juno106::_midiToFreq(uint8_t midi_note) {
    // A4 (MIDI 69) = 440Hz, cada semitono = factor 2^(1/12)
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

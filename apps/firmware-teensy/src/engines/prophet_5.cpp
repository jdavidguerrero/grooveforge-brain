// apps/firmware-teensy/src/engines/prophet_5.cpp
// Sprint 2.2 — Prophet-5 Engine: 5-voice polyphony con 2 VCOs por voz.
// Theory, voice allocation y demo: apps/docs/sprints/07-prophet-5.md

#include "prophet_5.h"
#include <math.h>

// ── Constructor ───────────────────────────────────────────────────────────────
//
// Las AudioConnections se construyen con referencias a los audio objects.
// La lista de inicialización garantiza que los arrays (_oscA, _oscB, etc.) se
// construyen primero (orden de declaración en el header), y las conexiones los
// referencian.
//
// Por qué conexiones individuales (no array de AudioConnection):
// AudioConnection registra src/dst en el grafo global en tiempo de construcción.
// No tiene copy constructor — los arrays brace-init llamarían al copy ctor,
// lo que no compila. Ver theory doc §"Implementation Notes".
//
// Layout de conexiones:
//   _c0–_c3:   voz 0  (oscA→mix, oscB→mix, mix→vcf, vcf→vcaEnv)
//   _c4–_c7:   voz 1
//   _c8–_c11:  voz 2
//   _c12–_c15: voz 3
//   _c16–_c19: voz 4
//   _c20–_c23: vcaEnv[0..3] → masterMixA (canales 0–3)
//   _c24:      vcaEnv[4]    → masterMixB (canal 0)
//   _c25:      masterMixA   → finalMix (canal 0)
//   _c26:      masterMixB   → finalMix (canal 1)
//   _c27:      finalMix     → out (canal L)
//   _c28:      finalMix     → out (canal R)

Prophet5::Prophet5() :
    // Voz 0
    _c0(_oscA[0],      0, _voiceMix[0], 0),
    _c1(_oscB[0],      0, _voiceMix[0], 1),
    _c2(_voiceMix[0],  0, _vcf[0],      0),
    _c3(_vcf[0],       0, _vcaEnv[0],   0),
    // Voz 1
    _c4(_oscA[1],      0, _voiceMix[1], 0),
    _c5(_oscB[1],      0, _voiceMix[1], 1),
    _c6(_voiceMix[1],  0, _vcf[1],      0),
    _c7(_vcf[1],       0, _vcaEnv[1],   0),
    // Voz 2
    _c8(_oscA[2],      0, _voiceMix[2], 0),
    _c9(_oscB[2],      0, _voiceMix[2], 1),
    _c10(_voiceMix[2], 0, _vcf[2],      0),
    _c11(_vcf[2],      0, _vcaEnv[2],   0),
    // Voz 3
    _c12(_oscA[3],     0, _voiceMix[3], 0),
    _c13(_oscB[3],     0, _voiceMix[3], 1),
    _c14(_voiceMix[3], 0, _vcf[3],      0),
    _c15(_vcf[3],      0, _vcaEnv[3],   0),
    // Voz 4
    _c16(_oscA[4],     0, _voiceMix[4], 0),
    _c17(_oscB[4],     0, _voiceMix[4], 1),
    _c18(_voiceMix[4], 0, _vcf[4],      0),
    _c19(_vcf[4],      0, _vcaEnv[4],   0),
    // vcaEnv → master mixers
    _c20(_vcaEnv[0],   0, _masterMixA,  0),
    _c21(_vcaEnv[1],   0, _masterMixA,  1),
    _c22(_vcaEnv[2],   0, _masterMixA,  2),
    _c23(_vcaEnv[3],   0, _masterMixA,  3),
    _c24(_vcaEnv[4],   0, _masterMixB,  0),
    // master path
    _c25(_masterMixA,  0, _finalMix,    0),
    _c26(_masterMixB,  0, _finalMix,    1),
    _c27(_finalMix,    0, _out,         0),   // canal L
    _c28(_finalMix,    0, _out,         1)    // canal R
{}

// ── begin() ───────────────────────────────────────────────────────────────────

void Prophet5::begin(float volume) {
    // 72 bloques × 256B = 18.4KB — factor 2.5× sobre los 29 bloques activos
    // Ver cálculo detallado en theory doc §"AudioMemory para 5 voces"
    AudioMemory(72);

    _codec.enable();
    _codec.volume(volume);

    // Inicializar cada voz
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        // VCO-A: sawtooth — onda más rica en armónicos (base del sonido Prophet)
        _oscA[i].begin(WAVEFORM_SAWTOOTH);
        _oscA[i].amplitude(1.0f);
        _oscA[i].frequency(440.0f);

        // VCO-B: sawtooth — mismo, intervalo se aplica en _triggerVoice
        _oscB[i].begin(WAVEFORM_SAWTOOTH);
        _oscB[i].amplitude(1.0f);
        _oscB[i].frequency(440.0f);

        // VoiceMixer: 50% A, 50% B por defecto (Prophet-5 default mix era igual)
        _voiceMix[i].gain(0, _oscAGain);
        _voiceMix[i].gain(1, _oscBGain);
        _voiceMix[i].gain(2, 0.0f);
        _voiceMix[i].gain(3, 0.0f);

        // VCF: AudioFilterStateVariable como placeholder digital
        // TODO Sprint 1.4: reemplazar con routing via SGTL5000 ADC cuando el
        //                  ladder analógico 2N3904 esté disponible.
        _vcf[i].frequency(_filterCutoff);
        _vcf[i].resonance(_filterResonance);
        _vcf[i].octaveControl(1.0f);

        // VCA envelope
        _vcaEnv[i].attack(_filterEnvAttackMs);
        _vcaEnv[i].decay(_filterEnvDecayMs);
        _vcaEnv[i].sustain(_filterEnvSustain);
        _vcaEnv[i].release(_filterEnvReleaseMs);

        // Estado inicial de filter envelope
        _filterEnvStage[i]   = EnvStage::IDLE;
        _filterEnvStageMs[i] = 0;
        _filterEnvLevel[i]   = 0.0f;
        _crossmodPhase[i]    = 0.0f;
    }

    // masterMixA: 4 voces → 25% cada una para no clipear al sumar
    // 4 voces × 0.25 = 1.0 nivel máximo en el bus (igual que el Moog monofónico)
    _masterMixA.gain(0, 0.25f);
    _masterMixA.gain(1, 0.25f);
    _masterMixA.gain(2, 0.25f);
    _masterMixA.gain(3, 0.25f);

    // masterMixB: solo la voz 4 en canal 0
    _masterMixB.gain(0, 0.25f);
    _masterMixB.gain(1, 0.0f);
    _masterMixB.gain(2, 0.0f);
    _masterMixB.gain(3, 0.0f);

    // finalMix: suma masterMixA + masterMixB a unidad completa
    _finalMix.gain(0, 1.0f);
    _finalMix.gain(1, 1.0f);
    _finalMix.gain(2, 0.0f);
    _finalMix.gain(3, 0.0f);

    _lastUpdateMs = millis();
}

// ── Note control ──────────────────────────────────────────────────────────────

void Prophet5::noteOn(uint8_t midi_note, float velocity) {
    (void)velocity;  // reservado para implementación futura

    // Re-trigger si la nota ya suena (evita duplicar voces para la misma nota)
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();

    _triggerVoice((uint8_t)voice, midi_note);
}

void Prophet5::noteOff(uint8_t midi_note) {
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

// ── VCO params ────────────────────────────────────────────────────────────────

void Prophet5::setOscAWaveform(uint16_t waveform) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _oscA[i].begin(waveform);
    }
}

void Prophet5::setOscBWaveform(uint16_t waveform) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _oscB[i].begin(waveform);
    }
}

void Prophet5::setOscBInterval(float semitones) {
    _oscBSemitones = semitones;
    // Actualizar frecuencias de VCO-B en voces activas
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        if (_voices[i].midi_note != 255) {
            float baseFreq = _midiToFreq(_voices[i].midi_note);
            _oscB[i].frequency(baseFreq * _semitonesToRatio(_oscBSemitones));
        }
    }
}

void Prophet5::setCrossmod(float amount) {
    _crossmodAmt = amount;
}

void Prophet5::setOscMix(float oscA, float oscB) {
    _oscAGain = oscA;
    _oscBGain = oscB;
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _voiceMix[i].gain(0, _oscAGain);
        _voiceMix[i].gain(1, _oscBGain);
    }
}

// ── Filter params ─────────────────────────────────────────────────────────────

void Prophet5::setFilterCutoff(float hz) {
    _filterCutoff = hz;
    // La frecuencia real se aplica en update() con la modulación del envelope sumada
}

void Prophet5::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _vcf[i].resonance(q);
    }
}

void Prophet5::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

// ── Envelope params (VCF + VCA compartido) ───────────────────────────────────

void Prophet5::setAttack(float ms) {
    _filterEnvAttackMs = ms;
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _vcaEnv[i].attack(ms);
    }
}

void Prophet5::setDecay(float ms) {
    _filterEnvDecayMs = ms;
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _vcaEnv[i].decay(ms);
    }
}

void Prophet5::setSustain(float level) {
    _filterEnvSustain = level;
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _vcaEnv[i].sustain(level);
    }
}

void Prophet5::setRelease(float ms) {
    _filterEnvReleaseMs = ms;
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _vcaEnv[i].release(ms);
    }
}

// ── update() — llamar desde loop() ────────────────────────────────────────────

void Prophet5::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        // 1. Filter envelope software
        _updateFilterEnv(i, dt);

        // 2. Aplicar cutoff + modulación del envelope al VCF de esta voz
        // El envelope agrega hasta _filterEnvAmt × 6000 Hz sobre el cutoff base.
        float modulated = _filterCutoff + _filterEnvAmt * _filterEnvLevel[i] * 6000.0f;
        if (modulated < 20.0f)    modulated = 20.0f;
        if (modulated > 20000.0f) modulated = 20000.0f;
        _vcf[i].frequency(modulated);

        // 3. Cross-modulation: aproximación FM analógico del Poly-Mod
        // Solo procesar si hay crossmod activo y la voz está sonando.
        // Aproximación: acumulamos la fase del oscilador-B y la usamos como
        // modulador de la frecuencia de VCO-A. No es FM exacto — ver theory doc.
        if (_crossmodAmt > 0.001f && _voices[i].midi_note != 255) {
            float baseFreq = _midiToFreq(_voices[i].midi_note);
            float oscBFreq = baseFreq * _semitonesToRatio(_oscBSemitones);

            // Acumular fase (dt_ms × 0.001 convierte ms → segundos)
            _crossmodPhase[i] += oscBFreq * (float)dt * 0.001f;
            // Normalizar a [0, 1) para evitar overflow en float a largo plazo
            if (_crossmodPhase[i] >= 1.0f) _crossmodPhase[i] -= 1.0f;

            float mod = _crossmodAmt * sinf(2.0f * M_PI * _crossmodPhase[i]);
            // Factor 0.5: sin esto, crossmod=1.0 desafinaría una octava completa.
            // 0.5 da hasta ±50% de la freq base — rango tímbrico útil sin romper afinación.
            _oscA[i].frequency(baseFreq * (1.0f + mod * 0.5f));
        }
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Prophet5::_triggerVoice(uint8_t voice_idx, uint8_t midi_note) {
    float baseFreq = _midiToFreq(midi_note);

    _oscA[voice_idx].frequency(baseFreq);
    _oscB[voice_idx].frequency(baseFreq * _semitonesToRatio(_oscBSemitones));

    _vcaEnv[voice_idx].noteOn();

    // Re-iniciar filter envelope desde cero (retrigger limpio)
    _filterEnvStage[voice_idx]   = EnvStage::ATTACK;
    _filterEnvStageMs[voice_idx] = 0;
    _filterEnvLevel[voice_idx]   = 0.0f;
    _crossmodPhase[voice_idx]    = 0.0f;

    _voices[voice_idx].midi_note = midi_note;
    _voices[voice_idx].active    = true;
    _voices[voice_idx].age       = ++_ageCounter;
}

void Prophet5::_releaseVoice(uint8_t voice_idx) {
    _vcaEnv[voice_idx].noteOff();

    _filterEnvStage[voice_idx]   = EnvStage::RELEASE;
    _filterEnvStageMs[voice_idx] = 0;

    // Marcar como libre — midi_note=255 es el centinela de "voz disponible"
    _voices[voice_idx].midi_note = 255;
    _voices[voice_idx].active    = false;
}

int8_t Prophet5::_findFreeVoice() {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        if (_voices[i].midi_note == 255) return (int8_t)i;
    }
    return -1;
}

int8_t Prophet5::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t Prophet5::_stealOldestVoice() {
    // Robar la voz con menor age (la que lleva más tiempo sonando)
    uint8_t oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;

    for (uint8_t i = 1; i < PROPHET_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

void Prophet5::_updateFilterEnv(uint8_t voice_idx, uint32_t dt_ms) {
    // Misma máquina de estados que MoogModelD._updateFilterEnv, pero por voz.
    // Ver theory doc §"Voice Allocation" y moog_model_d.cpp para la lógica base.
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

float Prophet5::_midiToFreq(uint8_t midi_note) {
    // A4 (MIDI 69) = 440Hz, cada semitono = factor 2^(1/12)
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

float Prophet5::_semitonesToRatio(float semitones) {
    // 12 semitones = 1 octava = factor 2.0
    return powf(2.0f, semitones / 12.0f);
}

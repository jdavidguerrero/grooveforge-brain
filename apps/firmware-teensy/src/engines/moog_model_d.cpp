// apps/firmware-teensy/src/engines/moog_model_d.cpp
// Sprint 1.5 — Moog Model D Skeleton
// Theory, wiring y demo: apps/docs/sprints/05-moog-model-d.md

#include "moog_model_d.h"
#include <math.h>
#include <Wire.h>

// ── Constructor ───────────────────────────────────────────────────────────────
// Las AudioConnections deben construirse con referencias a objetos que ya existen.
// La lista de inicialización garantiza que los audio objects se construyen primero
// (orden de declaración en el header), y luego las conexiones los referencian.

MoogModelD::MoogModelD() :
    _c1(_osc1,   0, _mixer,  0),
    _c2(_osc2,   0, _mixer,  1),
    _c3(_osc3,   0, _mixer,  2),
    _c4(_noise,  0, _mixer,  3),
    _c5(_mixer,  0, _filter, 0),
    _c6(_filter, 0, _vcaEnv, 0),
    _c7(_vcaEnv, 0, _out,    0),   // canal L
    _c8(_vcaEnv, 0, _out,    1)    // canal R
{}

// ── begin() ───────────────────────────────────────────────────────────────────

void MoogModelD::begin(float volume) {
    // AudioMemory() es responsabilidad del caller (sketch) — NO llamar aquí.
    // Razón: AudioMemory() es una macro que crea un static array + registra el pool.
    // Llamarla dos veces (sketch + begin) re-inicializa con el segundo tamaño, perdiendo
    // el primero. El sketch 28 llama AudioMemory(40) para dar margen al bridge+navegación.
    //
    // Wire.begin() explícito antes de enable(): USBHost_t36 corre constructores
    // globales antes de setup(), lo que puede dejar Wire sin inicializar cuando
    // AudioControlSGTL5000::enable() lo llama internamente. Llamarlo aquí garantiza
    // que el bus I2C está listo independientemente del orden de construcción global.
    Wire.begin();
    delay(10);   // margen para que el bus I2C estabilice tras begin()

    bool codec_ok = _codec.enable();
    Serial.print(F("[MoogModelD] SGTL5000 enable: "));
    Serial.println(codec_ok ? F("OK") : F("FAIL — revisar I2C (SDA=18, SCL=19) y 3.3V"));

    _codec.volume(volume);        // headphones (jack 3.5mm en el Teensy)
    _codec.lineOutLevel(13);      // line out — 3.16Vpp (nivel estándar -10dBV)
                                  // Sin esta llamada la salida 1/4" queda muda.

    // VCOs en sawtooth — onda más rica en armónicos, base del sonido Moog
    _osc1.begin(WAVEFORM_SAWTOOTH);
    _osc2.begin(WAVEFORM_SAWTOOTH);
    _osc3.begin(WAVEFORM_SAWTOOTH);
    _osc1.amplitude(1.0f);
    _osc2.amplitude(1.0f);
    _osc3.amplitude(1.0f);

    // Frecuencias iniciales — se actualizan en noteOn
    _updateOscFreqs();

    // Mixer: osc1+osc2 dominan la mezcla, sub-octave engrosa el grave,
    // noise desactivado por defecto (activable via setOscLevels)
    _mixer.gain(0, 0.45f);
    _mixer.gain(1, 0.45f);
    _mixer.gain(2, 0.30f);
    _mixer.gain(3, 0.00f);

    // Filter placeholder — SVF en modo low-pass
    // TODO Sprint 1.4: reemplazar con routing via SGTL5000 ADC cuando el ladder
    //                  analógico 2N3904 esté disponible.
    _filter.frequency(_filterCutoff);
    _filter.resonance(0.7f);    // Butterworth flat — sin énfasis en la resonancia
    _filter.octaveControl(1.0f);

    // VCA envelope: attack rápido, decay medio, sustain alto — sonido tipo lead Moog
    _vcaEnv.attack(10.0f);
    _vcaEnv.decay(200.0f);
    _vcaEnv.sustain(0.8f);
    _vcaEnv.release(400.0f);

    _lastUpdateMs = millis();
}

// ── Note control ──────────────────────────────────────────────────────────────

void MoogModelD::noteOn(uint8_t midi_note, float velocity) {
    (void)velocity;  // reservado para implementación futura

    _targetFreq = _midiToFreq(midi_note);

    if (_glideMs < 1.0f) {
        // Sin glide: salto inmediato a la frecuencia objetivo
        _currentFreq = _targetFreq;
    }
    // Con glide: _currentFreq se interpola en update() hacia _targetFreq

    _updateOscFreqs();

    // Dispara el filter envelope desde cero (re-trigger en legato)
    _filterEnvStage   = EnvStage::ATTACK;
    _filterEnvStageMs = 0;
    _filterEnvLevel   = 0.0f;

    _vcaEnv.noteOn();
    _noteActive = true;
}

void MoogModelD::noteOff() {
    _filterEnvStage   = EnvStage::RELEASE;
    _filterEnvStageMs = 0;

    _vcaEnv.noteOff();
    _noteActive = false;
}

// ── VCO params ────────────────────────────────────────────────────────────────

void MoogModelD::setWaveform(uint8_t osc_idx, uint16_t waveform) {
    switch (osc_idx) {
        case 0: _osc1.begin(waveform); break;
        case 1: _osc2.begin(waveform); break;
        case 2: _osc3.begin(waveform); break;
    }
}

void MoogModelD::setDetune(float cents) {
    _detuneCents = cents;
    _updateOscFreqs();
}

void MoogModelD::setOscLevels(float osc1, float osc2, float osc3, float noise) {
    _mixer.gain(0, osc1);
    _mixer.gain(1, osc2);
    _mixer.gain(2, osc3);
    _mixer.gain(3, noise);
}

// ── Filter params ─────────────────────────────────────────────────────────────

void MoogModelD::setFilterCutoff(float hz) {
    _filterCutoff = hz;
    // La frecuencia real se aplica en update() con la modulación del envelope sumada
}

void MoogModelD::setFilterResonance(float q) {
    _filter.resonance(q);
}

void MoogModelD::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

void MoogModelD::setFilterAttack(float ms)        { _filterEnvAttackMs  = ms; }
void MoogModelD::setFilterDecay(float ms)         { _filterEnvDecayMs   = ms; }
void MoogModelD::setFilterSustain(float level)    { _filterEnvSustain   = level; }
void MoogModelD::setFilterRelease(float ms)       { _filterEnvReleaseMs = ms; }

// ── VCA envelope params ───────────────────────────────────────────────────────

void MoogModelD::setVCAAttack(float ms)    { _vcaEnv.attack(ms); }
void MoogModelD::setVCADecay(float ms)     { _vcaEnv.decay(ms); }
void MoogModelD::setVCASustain(float level){ _vcaEnv.sustain(level); }
void MoogModelD::setVCARelease(float ms)   { _vcaEnv.release(ms); }

// ── Glide ─────────────────────────────────────────────────────────────────────

void MoogModelD::setGlide(float ms) {
    _glideMs = ms;
}

// ── update() — llamar desde loop() ────────────────────────────────────────────

void MoogModelD::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    // 1. Glide: interpolación lineal en Hz hacia el target
    if (_glideMs >= 1.0f && _currentFreq != _targetFreq) {
        float step = (_targetFreq - _currentFreq) * (float)dt / _glideMs;
        _currentFreq += step;

        // Clamp: evitar overshooting por dt variable
        if ((_targetFreq > _currentFreq + step && step > 0.0f) ||
            (_targetFreq < _currentFreq + step && step < 0.0f)) {
            _currentFreq = _targetFreq;
        }

        _updateOscFreqs();
    }

    // 2. Filter envelope software
    _updateFilterEnv(dt);

    // 3. Aplicar cutoff + modulación del envelope al filtro
    // El envelope agrega hasta _filterEnvAmt × 8000 Hz sobre el cutoff base.
    // 8000Hz es el headroom útil: cutoff 800Hz + 8000Hz = 8800Hz (audible)
    float modulated = _filterCutoff + _filterEnvAmt * _filterEnvLevel * 8000.0f;

    // Clamp al rango soportado por AudioFilterStateVariable
    if (modulated < 20.0f)    modulated = 20.0f;
    if (modulated > 20000.0f) modulated = 20000.0f;

    _filter.frequency(modulated);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void MoogModelD::_updateOscFreqs() {
    _osc1.frequency(_currentFreq);
    _osc2.frequency(_currentFreq * _centsToRatio(_detuneCents));
    _osc3.frequency(_currentFreq / 2.0f);   // sub-octave fija (ver theory doc)
}

void MoogModelD::_updateFilterEnv(uint32_t dt_ms) {
    _filterEnvStageMs += dt_ms;

    switch (_filterEnvStage) {
        case EnvStage::IDLE:
            _filterEnvLevel = 0.0f;
            break;

        case EnvStage::ATTACK:
            if (_filterEnvAttackMs < 1.0f) {
                _filterEnvLevel = 1.0f;
                _filterEnvStage = EnvStage::DECAY;
                _filterEnvStageMs = 0;
            } else {
                _filterEnvLevel = (float)_filterEnvStageMs / _filterEnvAttackMs;
                if (_filterEnvLevel >= 1.0f) {
                    _filterEnvLevel = 1.0f;
                    _filterEnvStage = EnvStage::DECAY;
                    _filterEnvStageMs = 0;
                }
            }
            break;

        case EnvStage::DECAY:
            if (_filterEnvDecayMs < 1.0f) {
                _filterEnvLevel = _filterEnvSustain;
                _filterEnvStage = EnvStage::SUSTAIN;
            } else {
                // Interpolación lineal de 1.0 a sustain durante decay
                float t = (float)_filterEnvStageMs / _filterEnvDecayMs;
                _filterEnvLevel = 1.0f - t * (1.0f - _filterEnvSustain);
                if (t >= 1.0f) {
                    _filterEnvLevel = _filterEnvSustain;
                    _filterEnvStage = EnvStage::SUSTAIN;
                }
            }
            break;

        case EnvStage::SUSTAIN:
            // El nivel se mantiene hasta noteOff (que dispara RELEASE)
            _filterEnvLevel = _filterEnvSustain;
            break;

        case EnvStage::RELEASE:
            if (_filterEnvReleaseMs < 1.0f) {
                _filterEnvLevel = 0.0f;
                _filterEnvStage = EnvStage::IDLE;
            } else {
                // Interpolación lineal desde el nivel actual a 0.
                // Usamos el nivel en que estaba al entrar a release (puede ser
                // cualquier valor si noteOff se dispara durante attack/decay).
                float t = (float)_filterEnvStageMs / _filterEnvReleaseMs;
                // Guardamos el nivel inicial de release en _filterEnvSustain solo si
                // entramos desde SUSTAIN; si entramos desde otra etapa usamos el nivel actual.
                // Simplificación: tomamos el nivel en t=0 como 1.0 * (1-t).
                // Suficiente para skeleton — en producción se necesita snapshot del nivel.
                _filterEnvLevel = _filterEnvSustain * (1.0f - t);
                if (t >= 1.0f) {
                    _filterEnvLevel = 0.0f;
                    _filterEnvStage = EnvStage::IDLE;
                }
            }
            break;
    }
}

float MoogModelD::_midiToFreq(uint8_t midi_note) {
    // A4 (MIDI 69) = 440Hz, cada semitono = factor 2^(1/12)
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

float MoogModelD::_centsToRatio(float cents) {
    // 1200 cents = 1 octava = factor 2.0
    return powf(2.0f, cents / 1200.0f);
}

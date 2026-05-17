// apps/firmware-teensy/src/engines/juno_106.cpp
// Sprint 2.1 — Roland Juno-106 Engine
// Theory, wiring y demo: apps/docs/sprints/06-juno-106.md

#include "juno_106.h"
#include <math.h>

// ── Constructor ───────────────────────────────────────────────────────────────
// Las AudioConnections se construyen en la lista de inicialización para garantizar
// que los audio objects ya existen cuando se registran las conexiones.
// Orden de declaración en el header = orden de construcción.
//
// Grafo:
//   _dco/_sub/_noise → _mixer → _vcf → _vcaEnv → _dryWetMix(ch0, dry)
//                                                → _chorus → _dryWetMix(ch1, wet)
//                                                            _dryWetMix → _out L+R

Juno106::Juno106() :
    _c1(_dco,        0, _mixer,      0),
    _c2(_sub,        0, _mixer,      1),
    _c3(_noise,      0, _mixer,      2),
    _c4(_mixer,      0, _vcf,        0),
    _c5(_vcf,        0, _vcaEnv,     0),
    _c6(_vcaEnv,     0, _dryWetMix,  0),   // path dry — bypass del chorus
    _c7(_vcaEnv,     0, _chorus,     0),   // send al chorus
    _c8(_chorus,     0, _dryWetMix,  1),   // return del chorus (wet)
    _c9(_dryWetMix,  0, _out,        0),   // canal L
    _c10(_dryWetMix, 0, _out,        1)    // canal R — mismo mix, mono en esta etapa
{}

// ── begin() ───────────────────────────────────────────────────────────────────

void Juno106::begin(float volume) {
    // 24 bloques × 256 bytes = 6.144 KB
    // Calculado: 10 conexiones activas + chorus doble-buffer + seguridad
    // Ver desglose en apps/docs/sprints/06-juno-106.md §Cálculo de AudioMemory
    AudioMemory(24);

    _codec.enable();
    _codec.volume(volume);

    // DCO principal — sawtooth por defecto (más rico en armónicos, base del Juno)
    _dco.begin(WAVEFORM_SAWTOOTH);
    _dco.amplitude(1.0f);
    _dco.frequency(440.0f);

    // Sub-oscillator — siempre square, siempre una octava abajo
    // El sub-osc del Juno no tiene forma de onda seleccionable — es square fijo
    _sub.begin(WAVEFORM_SQUARE);
    _sub.amplitude(1.0f);
    _sub.frequency(220.0f);   // 440/2 = 220Hz (A3)

    // Mixer: DCO domina, sub engorda el grave, noise desactivado por defecto
    _mixer.gain(0, 0.6f);   // DCO
    _mixer.gain(1, 0.3f);   // sub
    _mixer.gain(2, 0.0f);   // noise
    _mixer.gain(3, 0.0f);   // no usado

    // VCF placeholder — SVF en modo low-pass, 2-polo (12dB/oct como el IR3109)
    // La frecuencia real se actualiza en update() con la modulación del envelope
    _vcf.frequency(_filterCutoff);
    _vcf.resonance(0.7f);    // Butterworth flat — sin énfasis inicial
    _vcf.octaveControl(1.0f);

    // VCA envelope — los tiempos iniciales replican el preset "strings" del Juno
    _vcaEnv.attack(10.0f);
    _vcaEnv.decay(200.0f);
    _vcaEnv.sustain(0.7f);
    _vcaEnv.release(300.0f);

    // Chorus BBD — begin() requiere el buffer externo + su longitud + número de voces
    // n_voices=3 es el sweet spot para el sonido Juno sin CPU excesivo
    _chorus.begin(_chorusBuf, JUNO_CHORUS_BUF_LEN, 3);

    // dry/wet mix inicial: 50/50 (Modo I default)
    // El canal 0 del dryWetMix recibe la señal dry (post-VCA)
    // El canal 1 recibe el output del chorus (wet)
    _dryWetMix.gain(0, 0.6f);   // dry levemente más alto para claridad tonal
    _dryWetMix.gain(1, 0.4f);   // wet — chorus Modo I
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    _lastUpdateMs = millis();
}

// ── Note control ──────────────────────────────────────────────────────────────

void Juno106::noteOn(uint8_t midi_note, float velocity) {
    (void)velocity;  // reservado para velocity sensitivity en sprint futuro

    float freq = _midiToFreq(midi_note);
    _updateOscFreqs(freq);

    // Re-trigger del filter envelope desde cero (comportamiento legato)
    _filterEnvStage   = EnvStage::ATTACK;
    _filterEnvStageMs = 0;
    _filterEnvLevel   = 0.0f;

    _vcaEnv.noteOn();
}

void Juno106::noteOff() {
    _filterEnvStage   = EnvStage::RELEASE;
    _filterEnvStageMs = 0;

    _vcaEnv.noteOff();
}

// ── DCO params ────────────────────────────────────────────────────────────────

void Juno106::setWaveform(uint16_t waveform) {
    // Solo cambia el DCO principal — el sub siempre es WAVEFORM_SQUARE (spec Juno)
    _dco.begin(waveform);
}

void Juno106::setSubLevel(float level) {
    _mixer.gain(1, level);
}

void Juno106::setNoiseLevel(float level) {
    _mixer.gain(2, level);
}

// ── Filter params ─────────────────────────────────────────────────────────────

void Juno106::setFilterCutoff(float hz) {
    _filterCutoff = hz;
    // La frecuencia real se aplica en update() con la modulación del envelope sumada
}

void Juno106::setFilterResonance(float q) {
    _vcf.resonance(q);
}

void Juno106::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

// ── ADSR compartido ───────────────────────────────────────────────────────────
// Cada setter actualiza tanto la copia interna (filter envelope software) como el
// VCA envelope (AudioEffectEnvelope). Un solo ADSR controla ambos — diferencia
// clave con el MoogModelD que tiene dos envelopes independientes.

void Juno106::setAttack(float ms) {
    _filterEnvAttackMs = ms;
    _vcaEnv.attack(ms);
}

void Juno106::setDecay(float ms) {
    _filterEnvDecayMs = ms;
    _vcaEnv.decay(ms);
}

void Juno106::setSustain(float level) {
    _filterEnvSustain = level;
    _vcaEnv.sustain(level);
}

void Juno106::setRelease(float ms) {
    _filterEnvReleaseMs = ms;
    _vcaEnv.release(ms);
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

    // 1. Filter envelope software
    _updateFilterEnv(dt);

    // 2. Aplicar cutoff + modulación del envelope al VCF
    // El envelope agrega hasta _filterEnvAmt × 8000 Hz sobre el cutoff base.
    // 8000Hz es el headroom útil: cutoff 1200Hz + 8000Hz = 9200Hz (dentro del rango audible)
    float modulated = _filterCutoff + _filterEnvAmt * _filterEnvLevel * 8000.0f;

    // Clamp al rango soportado por AudioFilterStateVariable
    if (modulated < 20.0f)    modulated = 20.0f;
    if (modulated > 20000.0f) modulated = 20000.0f;

    _vcf.frequency(modulated);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Juno106::_updateOscFreqs(float freq) {
    _dco.frequency(freq);
    // El sub-oscillator sigue al DCO, siempre una octava abajo (freq/2)
    // No hay control de octava relativa en el Juno original — es fijo
    _sub.frequency(freq / 2.0f);
}

void Juno106::_updateFilterEnv(uint32_t dt_ms) {
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
                float t = (float)_filterEnvStageMs / _filterEnvDecayMs;
                _filterEnvLevel = 1.0f - t * (1.0f - _filterEnvSustain);
                if (t >= 1.0f) {
                    _filterEnvLevel = _filterEnvSustain;
                    _filterEnvStage = EnvStage::SUSTAIN;
                }
            }
            break;

        case EnvStage::SUSTAIN:
            _filterEnvLevel = _filterEnvSustain;
            break;

        case EnvStage::RELEASE:
            if (_filterEnvReleaseMs < 1.0f) {
                _filterEnvLevel = 0.0f;
                _filterEnvStage = EnvStage::IDLE;
            } else {
                float t = (float)_filterEnvStageMs / _filterEnvReleaseMs;
                // Aproximación: release desde el nivel de sustain. Si noteOff ocurrió
                // durante attack o decay, hay un glitch menor — aceptable en skeleton.
                // Fix correcto: snapshot del nivel en el momento del noteOff.
                _filterEnvLevel = _filterEnvSustain * (1.0f - t);
                if (t >= 1.0f) {
                    _filterEnvLevel = 0.0f;
                    _filterEnvStage = EnvStage::IDLE;
                }
            }
            break;
    }
}

float Juno106::_midiToFreq(uint8_t midi_note) {
    // A4 (MIDI 69) = 440Hz, cada semitono = factor 2^(1/12)
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013) y AnalogEnv RC
#include "../audio/moog_ladder.h"
#include "../dsp/analog_env.h"
#include "../audio/analog_vca.h"
#include "../dsp/voice_spread.h"
#include "lfo_mod.h"
#include "prophet_osc_core.h"

static constexpr uint8_t PROPHET_VOICES = 5;

/**
 * @brief Osciladores de una voz del Prophet-5 como objeto de audio (código custom, subclase de AudioStream).
 *
 * Envuelve a Prophet5OscCore: A, B, ruido, POLY-MOD a velocidad de audio y sync (theory/prophet-5-completo.md §2).
 * Los parámetros los fija el engine a tasa de control con set(); el objeto los lee en la ISR de audio.
 */
class Prophet5Osc : public AudioStream {
public:
    Prophet5Osc() : AudioStream(0, nullptr) {}

    /// Parámetros del próximo bloque (copia protegida contra la ISR de audio).
    void set(const Prophet5OscParams& p) {
        __disable_irq();
        _p = p;
        __enable_irq();
    }

    /// false = no genera nada (engine inactivo: no gasta CPU).
    void enable(bool on) { _on = on; }

    /// Fase actual [0,1) del OSC B para modulación de POLY-MOD → FILTER.
    float phase_b() const { return _core.phase_b(); }

    void update() override;

private:
    Prophet5OscCore _core;
    Prophet5OscParams _p;
    volatile bool _on = true;
};

/**
 * @brief Engine Prophet-5: 5 voces, osciladores propios (POLY-MOD, sync, saw + pulse),
 *        MoogLadder4P con Drive, AnalogEnv RC sin salto en release y VCA por voz.
 */
class Prophet5 {
public:
    Prophet5();

    void begin(float volume = 0.5f);

    void noteOn(uint8_t midi_note, float velocity = 1.0f);
    void noteOff(uint8_t midi_note);

    // ── OSC A ──────────────────────────────────────────────────────────────────
    void setOscAFreq(float semitones) { _oscASemitones = semitones; }
    void setOscAShape(bool saw, bool pulse) { _aSaw = saw; _aPulse = pulse; }
    void setOscAPw(float pw) { _pwA = pw; }
    void setSync(bool on) { _sync = on; }

    // ── OSC B ──────────────────────────────────────────────────────────────────
    void setOscBInterval(float semitones) { _oscBSemitones = semitones; }
    void setOscBFine(float cents) { _oscBFine = cents; }
    void setOscBShape(bool saw, bool tri, bool pulse) { _bSaw = saw; _bTri = tri; _bPulse = pulse; }
    void setOscBPw(float pw) { _pwB = pw; }
    void setOscBKeyboard(bool on) { _bKeyboard = on; }
    void setOscBLoFreq(bool on) { _bLoFreq = on; }

    // ── MIXER, GLIDE y filtro ──────────────────────────────────────────────────
    void setMixer(float a, float b, float noise) { _lvlA = a; _lvlB = b; _lvlN = noise; }
    void setGlide(float ms) { _glideMs = ms; }
    void setFilterKeyboard(float amount) { _filterKbd = amount; }
    void setFilterCutoff(float hz) { _filterCutoff = hz; }
    void setFilterResonance(float q);
    void setFilterEnvAmount(float amount) { _filterEnvAmt = amount; }
    void setDrive(float drive);

    // ── POLY-MOD y WHEEL-MOD ───────────────────────────────────────────────────
    void setPolyMod(float filt_env, float osc_b, bool freq_a, bool pw_a, bool filter) {
        _pmEnv = filt_env; _pmB = osc_b; _pmFreqA = freq_a; _pmPwA = pw_a; _pmFilter = filter;
    }
    void setWheelMod(float source_mix, bool freq_a, bool freq_b, bool pw_a, bool pw_b, bool filter) {
        _wmMix = source_mix; _wmFreqA = freq_a; _wmFreqB = freq_b; _wmPwA = pw_a; _wmPwB = pw_b; _wmFilter = filter;
    }
    void setWheel(float amount) { _wheel = amount; }

    // ── Compatibilidad con el sketch 28 ────────────────────────────────────────
    void setOscAWaveform(uint16_t waveform);
    void setOscBWaveform(uint16_t waveform);
    void setCrossmod(float amount) { _pmB = amount; _pmFreqA = amount > 0.0f || _pmFreqA; }
    void setOscMix(float oscA, float oscB) { _lvlA = oscA; _lvlB = oscB; }

    // ── Envelopes ──────────────────────────────────────────────────────────────
    void setAttack(float ms);
    void setDecay(float ms);
    void setSustain(float level);
    void setRelease(float ms);

    void setLfoMod(const LfoMod& m) { _lfo = m; }

    void setFilterEnv(float a_ms, float d_ms, float s, float r_ms) {
        for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
            _filterEnv[i].setAttack(a_ms);
            _filterEnv[i].setDecay(d_ms);
            _filterEnv[i].setSustain(s);
            _filterEnv[i].setRelease(r_ms);
        }
    }

    void setAmpEnv(float a_ms, float d_ms, float s, float r_ms) {
        for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
            _vcaEnv[i].attack(a_ms);
            _vcaEnv[i].decay(d_ms);
            _vcaEnv[i].sustain(s);
            _vcaEnv[i].release(r_ms);
        }
    }

    void update();
    void set_dsp_enabled(bool on);

    AudioStream& getOutput() { return _finalMix; }

private:
    dsp::VoiceSpread _spread[PROPHET_VOICES];   ///< voice cards: desafinación y cutoff fijos por voz (fidelidad §10)
    uint8_t _rr = 0;                         ///< asignación rotativa: próxima voz a probar
    LfoMod _lfo;

    // ── Audio objects ───────────────────────────────────────────────────────────
    Prophet5Osc         _osc[PROPHET_VOICES];
    MoogLadder4P        _vcf[PROPHET_VOICES];
    AnalogVca _vcaEnv[PROPHET_VOICES];
    AudioMixer4         _masterMixA;
    AudioMixer4         _masterMixB;
    AudioMixer4         _finalMix;

    AudioConnection _c0, _c1, _c2, _c3, _c4, _c5, _c6, _c7, _c8, _c9;
    AudioConnection _c10, _c11, _c12, _c13, _c14;
    AudioConnection _c15, _c16;

    // ── Voces ──────────────────────────────────────────────────────────────────
    struct VoiceState {
        uint8_t  midi_note = 255;
        bool     active    = false;
        uint32_t age       = 0;
        float    velocity  = 1.0f;
    };
    VoiceState _voices[PROPHET_VOICES];
    uint32_t   _ageCounter = 0;
    float      _targetHz[PROPHET_VOICES] = {};
    float      _currentHz[PROPHET_VOICES] = {};

    // ── Panel ──────────────────────────────────────────────────────────────────
    float _oscASemitones = 0.0f, _oscBSemitones = 0.0f, _oscBFine = 0.0f;
    bool  _aSaw = true, _aPulse = false, _sync = false;
    bool  _bSaw = true, _bTri = false, _bPulse = false, _bKeyboard = true, _bLoFreq = false;
    float _pwA = 0.5f, _pwB = 0.5f;
    float _lvlA = 0.6f, _lvlB = 0.4f, _lvlN = 0.0f;
    float _glideMs = 0.0f, _filterKbd = 0.0f;
    float _pmEnv = 0.0f, _pmB = 0.0f;
    bool  _pmFreqA = false, _pmPwA = false, _pmFilter = false;
    float _wmMix = 0.0f, _wheel = 0.0f, _wmNoise = 0.0f;
    bool  _wmFreqA = true, _wmFreqB = true, _wmPwA = false, _wmPwB = false, _wmFilter = false;
    uint32_t _rng = 0x9E3779B9u;

    float _filterCutoff    = 2000.0f;
    float _filterEnvAmt    = 0.4f;
    float _filterResonance = 0.7f;

    dsp::AnalogEnv _filterEnv[PROPHET_VOICES];
    uint32_t       _lastUpdateMs = 0;

    int8_t _findFreeVoice();
    int8_t _findVoiceForNote(uint8_t midi_note);
    int8_t _stealOldestVoice();
    void   _triggerVoice(uint8_t voice_idx, uint8_t midi_note, float velocity);
    void   _releaseVoice(uint8_t voice_idx);
    float  _midiToFreq(uint8_t midi_note);
};

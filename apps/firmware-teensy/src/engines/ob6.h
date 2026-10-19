#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — PolyBLEP VCO, filtro Oberheim SEM SVF 12 dB/oct con Morph LP↔Notch↔HP y AnalogEnv RC
#include "../audio/polyblep_waveform.h"
#include "../audio/sem_svf.h"
#include "../dsp/analog_env.h"
#include "../audio/analog_vca.h"
#include "../dsp/voice_spread.h"
#include "lfo_mod.h"

static constexpr uint8_t OB6_VOICES = 6;

/**
 * @brief Motor de síntesis Sequential / Oberheim OB-6 para GrooveForge Brain.
 *
 * 6 voces polifónicas, cada una con:
 *   - 2 VCOs PolyBLEP con detune fino en cents + sub oscillator + noise
 *   - Filtro Oberheim SEM State-Variable 2-polos (12 dB/oct) con Morph continuo LP ↔ Notch ↔ HP y modo BP
 *   - Matriz X-MOD (Filter Env + VCO2 → VCO1 Freq, Filter Mode Morph, VCO1 PW)
 *   - Envelopes con curvas RC analógicas (dsp::AnalogEnv) sin salto en release
 */
class OB6 {
public:
    OB6();

    void begin(float volume = 0.5f);

    void noteOn(uint8_t midi_note, float velocity = 1.0f);
    void noteOff(uint8_t midi_note);

    void setVco1Waveform(uint16_t waveform);
    void setVco2Waveform(uint16_t waveform);
    void setVco2Interval(float semitones);
    void setVco2Detune(float cents);
    void setPulseWidth(float pw1, float pw2);
    void setOscMix(float vco1, float vco2, float sub, float noise);

    void setFilterCutoff(float hz);
    void setFilterResonance(float q);
    void setFilterEnvAmount(float amount);

    /**
     * @brief Morph continuo del filtro SEM 12 dB/oct: 0.0 = LP, 0.5 = Notch, 1.0 = HP.
     */
    void setFilterMorph(float morph);

    /** @brief Activa el modo Band-Pass (BP) dedicado del filtro SEM. */
    void setFilterBpMode(bool bp);

    /**
     * @brief Compatibilidad discretizada: 0=LP, 1=BP, 2=HP, 3=Notch.
     */
    void setFilterMode(uint8_t mode);

    void setDrive(float drive);
    void setGlide(float ms) { _glideMs = ms; }

    /**
     * @brief X-MOD del OB-6: Filter Env y VCO2 modulan Freq VCO1, Morph del filtro SEM y PW VCO1.
     */
    void setXMod(float filt_env, float vco2, bool freq1, bool morph, bool pw1) {
        _xmEnv   = filt_env;
        _xmVco2  = vco2;
        _xmFreq1 = freq1;
        _xmMorph = morph;
        _xmPw1   = pw1;
    }

    float filterMorph() const { return _filterMorph; }
    bool  filterBpMode() const { return _filterBp; }

    void setAttack(float ms);
    void setDecay(float ms);
    void setSustain(float level);
    void setRelease(float ms);

    void setLfoMod(const LfoMod& m) { _lfo = m; }

    void setFilterEnv(float a_ms, float d_ms, float s, float r_ms) {
        for (uint8_t i = 0; i < OB6_VOICES; i++) {
            _filterEnv[i].setAttack(a_ms);
            _filterEnv[i].setDecay(d_ms);
            _filterEnv[i].setSustain(s);
            _filterEnv[i].setRelease(r_ms);
        }
    }

    void setAmpEnv(float a_ms, float d_ms, float s, float r_ms) {
        for (uint8_t i = 0; i < OB6_VOICES; i++) {
            _ampEnv[i].attack(a_ms);
            _ampEnv[i].decay(d_ms);
            _ampEnv[i].sustain(s);
            _ampEnv[i].release(r_ms);
        }
    }

    void update();
    void set_dsp_enabled(bool on);

    AudioStream& getOutput() { return _finalMix; }

private:
    dsp::VoiceSpread _spread[OB6_VOICES];   ///< voice cards: desafinación y cutoff fijos por voz (fidelidad §10)
    uint8_t _rr = 0;                         ///< asignación rotativa: próxima voz a probar
    LfoMod _lfo;

    // ── Audio objects — 6 voces estáticas ────────────────────────────────────
    AudioSynthPolyBlep       _vco1[OB6_VOICES];
    AudioSynthPolyBlep       _vco2[OB6_VOICES];
    AudioSynthPolyBlep       _sub[OB6_VOICES];
    AudioSynthNoiseWhite     _noise[OB6_VOICES];
    AudioMixer4              _voiceMix[OB6_VOICES];
    OberheimSemSvf2P         _filter[OB6_VOICES];
    AnalogVca      _ampEnv[OB6_VOICES];

    AudioMixer4              _masterMixA;
    AudioMixer4              _masterMixB;
    AudioMixer4              _finalMix;

    AudioConnection _c0,  _c1,  _c2,  _c3,  _c4,  _c5;
    AudioConnection _c6,  _c7,  _c8,  _c9,  _c10, _c11;
    AudioConnection _c12, _c13, _c14, _c15, _c16, _c17;
    AudioConnection _c18, _c19, _c20, _c21, _c22, _c23;
    AudioConnection _c24, _c25, _c26, _c27, _c28, _c29;
    AudioConnection _c30, _c31, _c32, _c33, _c34, _c35;
    AudioConnection _c36, _c37, _c38, _c39;
    AudioConnection _c40, _c41;
    AudioConnection _c42, _c43;

    struct VoiceState {
        uint8_t  midi_note = 255;
        bool     active    = false;
        uint32_t age       = 0;
        float    targetHz  = 440.0f;
        float    currentHz = 440.0f;
        float    velocity  = 1.0f;
    };
    VoiceState _voices[OB6_VOICES];
    uint32_t   _ageCounter = 0;

    // ── Parámetros globales ──────────────────────────────────────────────────
    float   _filterCutoff    = 2000.0f;
    float   _filterEnvAmt    = 0.4f;
    float   _filterResonance = 1.5f;
    float   _filterMorph     = 0.0f;
    bool    _filterBp        = false;
    uint8_t _filterMode      = 0;

    float   _vco2Semitones   = 0.0f;
    float   _vco2DetuneCents = 7.0f;   // detune analógico rico por defecto del OB-6
    float   _pw1             = 0.5f;
    float   _pw2             = 0.5f;
    float   _lvl1            = 0.45f;
    float   _lvl2            = 0.45f;
    float   _lvlSub          = 0.10f;
    float   _lvlNoise        = 0.0f;
    float   _glideMs         = 0.0f;

    // X-MOD
    float   _xmEnv           = 0.0f;
    float   _xmVco2          = 0.0f;
    bool    _xmFreq1         = false;
    bool    _xmMorph         = false;
    bool    _xmPw1           = false;

    dsp::AnalogEnv _filterEnv[OB6_VOICES];
    uint32_t       _lastUpdateMs = 0;

    int8_t _findFreeVoice();
    int8_t _findVoiceForNote(uint8_t midi_note);
    int8_t _stealOldestVoice();
    void   _triggerVoice(uint8_t voice_idx, uint8_t midi_note, float velocity);
    void   _releaseVoice(uint8_t voice_idx);
    float  _midiToFreq(uint8_t midi_note);
    float  _semitonesToRatio(float semitones);
};

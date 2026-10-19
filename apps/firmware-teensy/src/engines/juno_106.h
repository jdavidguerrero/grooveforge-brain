#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — PolyBLEP DCO, filtro MoogLadder4P, AnalogEnv RC y Chorus BBD MN3009 + HPF
#include "../audio/juno_chorus.h"
#include "../audio/moog_ladder.h"
#include "../audio/polyblep_waveform.h"
#include "../dsp/analog_env.h"
#include "../audio/analog_vca.h"
#include "../dsp/voice_spread.h"
#include "lfo_mod.h"

static constexpr uint8_t JUNO_VOICES = 6;

/**
 * @brief Motor Roland Juno-106 — 6 voces polifónicas, DCO PolyBLEP (Saw+Pulse simultáneos),
 *        HPF de 4 posiciones y Chorus BBD MN3009 interpolado (OFF / I / II / I+II).
 *
 * Signal flow por voz:
 *   DCO[v] (PolyBLEP Saw/Pulse/Saw+Pulse) ──ch0──┐
 *   sub[v] (PolyBLEP Square -1 oct)       ──ch1──┤──→ voiceMix[v] ──→ filter[v] (IR3109 24dB) ──→ ampEnv[v]
 *   noise[v]                              ──ch2──┘
 *
 * Master path:
 *   ampEnv[0-3] ──→ masterMixA ──ch0──┐
 *                                      ├──→ masterBus ──→ JunoBbdChorus (HPF 0..3 + MN3009 BBD) ──→ Output
 *   ampEnv[4-5] ──→ masterMixB ──ch1──┘
 */
class Juno106 {
public:
    Juno106();

    void begin(float volume = 0.5f);

    void noteOn(uint8_t midi_note, float velocity = 1.0f);
    void noteOff(uint8_t midi_note);

    /**
     * @brief Selecciona la forma de onda del DCO principal de todas las voces.
     * Soporta WAVEFORM_SAWTOOTH, WAVEFORM_SQUARE, WAVEFORM_PULSE, WAVEFORM_TRIANGLE
     * y AudioSynthPolyBlep::WAVEFORM_SAW_PULSE (100) para Saw + Pulse simultáneos del Juno-106.
     */
    void setWaveform(uint16_t waveform);

    /** @brief Ancho de pulso manual del DCO [0.05, 0.95]. */
    void setPulseWidth(float pw);

    void setSubLevel(float level);
    void setNoiseLevel(float level);

    void setFilterCutoff(float hz);
    void setFilterResonance(float q);
    void setFilterEnvAmount(float amount);
    void setDrive(float drive);
    void setGlide(float ms) { _glideMs = ms; }

    /** @brief Selector HPF de 4 posiciones del Juno-106: 0=Bass Boost, 1=Flat, 2=225Hz, 3=720Hz. */
    void setHpf(uint8_t pos);

    /** @brief Modo de Chorus BBD MN3009: 0=OFF, 1=Chorus I, 2=Chorus II, 3=Chorus I+II. */
    void setChorusMode(uint8_t mode);

    /** @brief Compatibilidad legacy con setChorus(bool, uint8_t). */
    void setChorus(bool enabled, uint8_t mode = 1);

    void setAttack(float ms);
    void setDecay(float ms);
    void setSustain(float level);
    void setRelease(float ms);

    void setLfoMod(const LfoMod& m) { _lfo = m; }

    void setFilterEnv(float a_ms, float d_ms, float s, float r_ms) {
        for (uint8_t i = 0; i < JUNO_VOICES; i++) {
            _filterEnv[i].setAttack(a_ms);
            _filterEnv[i].setDecay(d_ms);
            _filterEnv[i].setSustain(s);
            _filterEnv[i].setRelease(r_ms);
        }
    }

    void setAmpEnv(float a_ms, float d_ms, float s, float r_ms) {
        _amp[0] = a_ms; _amp[1] = d_ms; _amp[2] = s; _amp[3] = r_ms;
        // VCA en GATE (fidelidad §11.5): abre y cierra con la tecla; 1 ms / 8 ms para no hacer click
        const bool g = _vcaGate;
        for (uint8_t i = 0; i < JUNO_VOICES; i++) {
            _ampEnv[i].attack(g ? 1.0f : a_ms);
            _ampEnv[i].decay(g ? 1.0f : d_ms);
            _ampEnv[i].sustain(g ? 1.0f : s);
            _ampEnv[i].release(g ? 8.0f : r_ms);
        }
    }

    /// VCA ENV (false) / GATE (true).
    void setVcaGate(bool gate) {
        _vcaGate = gate;
        setAmpEnv(_amp[0], _amp[1], _amp[2], _amp[3]);
    }
    /// RANGE del DCO: 0 = 16' · 1 = 8' · 2 = 4'.
    void setRange(uint8_t range) { _rangeRatio = range == 0 ? 0.5f : range >= 2 ? 2.0f : 1.0f; }

    void update();
    void set_dsp_enabled(bool on);

    AudioStream& getOutput() { return _chorus; }
    /// Salida estéreo del chorus: puerto 0 = L (getOutput), puerto 1 = R (fidelidad §9.3).
    static constexpr uint8_t OUTPUT_PORT_R = 1;

private:
    float _amp[4] = {5.0f, 300.0f, 0.7f, 300.0f};
    bool  _vcaGate = false;
    float _rangeRatio = 1.0f;
    dsp::VoiceSpread _spread[JUNO_VOICES];   ///< voice cards: desafinación y cutoff fijos por voz (fidelidad §10)
    uint8_t _rr = 0;                         ///< asignación rotativa: próxima voz a probar
    LfoMod _lfo;

    // ── Per-voice audio objects ───────────────────────────────────────────────
    AudioSynthPolyBlep   _dco[JUNO_VOICES];
    AudioSynthPolyBlep   _sub[JUNO_VOICES];
    AudioSynthNoiseWhite _noise[JUNO_VOICES];
    AudioMixer4          _voiceMix[JUNO_VOICES];
    MoogLadder4P         _filter[JUNO_VOICES];
    AnalogVca  _ampEnv[JUNO_VOICES];

    // ── Master path ───────────────────────────────────────────────────────────
    AudioMixer4          _masterMixA;
    AudioMixer4          _masterMixB;
    AudioMixer4          _masterBus;
    JunoBbdChorus        _chorus;

    // ── Conexiones (39): por voz (5×6=30) + master (9) ───────────────────────
    AudioConnection _c0,  _c1,  _c2,  _c3,  _c4;
    AudioConnection _c5,  _c6,  _c7,  _c8,  _c9;
    AudioConnection _c10, _c11, _c12, _c13, _c14;
    AudioConnection _c15, _c16, _c17, _c18, _c19;
    AudioConnection _c20, _c21, _c22, _c23, _c24;
    AudioConnection _c25, _c26, _c27, _c28, _c29;
    AudioConnection _c30, _c31, _c32, _c33;
    AudioConnection _c34, _c35;
    AudioConnection _c36, _c37;
    AudioConnection _c38;

    // ── Voice state (FIFO stealing) ───────────────────────────────────────────
    struct VoiceState {
        uint8_t  midi_note  = 255;
        bool     active     = false;
        uint32_t age        = 0;
        float    targetHz   = 440.0f;
        float    currentHz  = 440.0f;
        float    velocity   = 1.0f;
    };
    VoiceState _voices[JUNO_VOICES];
    uint32_t   _ageCounter = 0;

    // ── Parámetros globales ───────────────────────────────────────────────────
    float   _filterCutoff    = 1200.0f;
    float   _filterEnvAmt    = 0.5f;
    float   _filterResonance = 1.5f;
    float   _pulseWidth      = 0.5f;
    float   _subLevel        = 0.3f;
    float   _noiseLevel      = 0.0f;
    float   _glideMs         = 0.0f;
    uint8_t _chorusMode      = 1;
    uint8_t _hpfPos          = 1;

    dsp::AnalogEnv _filterEnv[JUNO_VOICES];
    uint32_t       _lastUpdateMs = 0;

    int8_t _findFreeVoice();
    int8_t _findVoiceForNote(uint8_t midi_note);
    int8_t _stealOldestVoice();
    void   _triggerVoice(uint8_t voice_idx, uint8_t midi_note, float velocity);
    void   _releaseVoice(uint8_t voice_idx);
    float  _midiToFreq(uint8_t midi_note);
};

#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — PolyBLEP VCO, filtro digital Moog/ARP ladder 4012 y AnalogEnv RC
#include "../audio/moog_ladder.h"
#include "../audio/polyblep_waveform.h"
#include "../dsp/analog_env.h"
#include "../audio/analog_vca.h"
#include "../dsp/voice_spread.h"
#include "lfo_mod.h"

static constexpr uint8_t ARP_VOICES = 4;

/**
 * @brief Motor de síntesis ARP 2600 para GrooveForge Brain.
 *
 * 4 voces polifónicas con:
 *   - 2 VCOs PolyBLEP por voz + Ring Modulator (VCO1 × VCO2) + White Noise
 *   - Mezclador pre-filtro de 4 canales reales:
 *       ch0 = Ring Mod (VCO1 × VCO2)
 *       ch1 = VCO 1 directo
 *       ch2 = Noise
 *       ch3 = VCO 2 directo (permite usar el ARP 2600 como sinte de 2 VCOs además de Ring Mod)
 *   - Filtro Ladder 24 dB/oct (ARP 4012 / 4072) con saturación Drive
 *   - Envelopes RC analógicos (dsp::AnalogEnv) sin salto en release
 */
class ARP2600 {
public:
    ARP2600();

    void begin(float volume = 0.5f);

    void noteOn(uint8_t midi_note, float velocity = 1.0f);
    void noteOff(uint8_t midi_note);

    void setVco1Waveform(uint16_t waveform);
    void setVco2Waveform(uint16_t waveform);
    void setVco2Ratio(float ratio);
    void setPulseWidth(float pw);

    /**
     * @brief Nivel de Ring Modulator y balance clásico con VCO1 dry.
     */
    void setRingMix(float mix);

    /** @brief Nivel directo de VCO1 en el mezclador pre-filtro [0.0, 1.0]. */
    void setVco1Level(float level);

    /** @brief Nivel directo de VCO2 en el mezclador pre-filtro [0.0, 1.0]. */
    void setVco2Level(float level);

    void setNoiseLevel(float level);

    void setFilterCutoff(float hz);
    void setFilterResonance(float q);
    void setFilterEnvAmount(float amount);
    void setDrive(float drive);
    void setGlide(float ms) { _glideMs = ms; }

    void setAttack(float ms);
    void setDecay(float ms);
    void setSustain(float level);
    void setRelease(float ms);

    void setLfoMod(const LfoMod& m) { _lfo = m; }

    void setFilterEnv(float a_ms, float d_ms, float s, float r_ms) {
        for (uint8_t i = 0; i < ARP_VOICES; i++) {
            _filterEnv[i].setAttack(a_ms);
            _filterEnv[i].setDecay(d_ms);
            _filterEnv[i].setSustain(s);
            _filterEnv[i].setRelease(r_ms);
        }
    }

    void setAmpEnv(float a_ms, float d_ms, float s, float r_ms) {
        for (uint8_t i = 0; i < ARP_VOICES; i++) {
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
    dsp::VoiceSpread _spread[ARP_VOICES];   ///< voice cards: desafinación y cutoff fijos por voz (fidelidad §10)
    uint8_t _rr = 0;                         ///< asignación rotativa: próxima voz a probar
    LfoMod _lfo;

    // ── Audio objects — 4 voces estáticas ────────────────────────────────────
    AudioSynthPolyBlep       _vco1[ARP_VOICES];
    AudioSynthPolyBlep       _vco2[ARP_VOICES];
    AudioSynthNoiseWhite     _noise[ARP_VOICES];
    AudioEffectMultiply      _ringMod[ARP_VOICES];
    AudioMixer4              _voiceMix[ARP_VOICES];   // ch0=ringMod, ch1=VCO1 dry, ch2=noise, ch3=VCO2 dry
    MoogLadder4P             _filter[ARP_VOICES];
    AnalogVca      _ampEnv[ARP_VOICES];

    AudioMixer4 _masterMixA;
    AudioMixer4 _finalMix;

    // ── Conexiones — 37 individuales (8 por voz × 4 = 32 + 5 master) ─────────
    AudioConnection _c0,  _c1,  _c2,  _c3,  _c4,  _c5,  _c6,  _c0b;   // voz 0
    AudioConnection _c7,  _c8,  _c9,  _c10, _c11, _c12, _c13, _c1b;   // voz 1
    AudioConnection _c14, _c15, _c16, _c17, _c18, _c19, _c20, _c2b;   // voz 2
    AudioConnection _c21, _c22, _c23, _c24, _c25, _c26, _c27, _c3b;   // voz 3
    AudioConnection _c28, _c29, _c30, _c31;
    AudioConnection _c32;

    struct VoiceState {
        uint8_t  midi_note = 255;
        bool     active    = false;
        uint32_t age       = 0;
        float    targetHz  = 440.0f;
        float    currentHz = 440.0f;
        float    velocity  = 1.0f;
    };
    VoiceState _voices[ARP_VOICES];
    uint32_t   _ageCounter = 0;

    // ── Parámetros globales ──────────────────────────────────────────────────
    float _vco2Ratio       = 1.0f;
    float _ringMixAmt      = 0.5f;
    float _vco1Level       = 0.5f;
    float _vco2Level       = 0.35f;
    float _noiseLevel      = 0.0f;
    float _pulseWidth      = 0.5f;
    float _filterCutoff    = 2000.0f;
    float _filterResonance = 1.5f;
    float _filterEnvAmt    = 0.5f;
    float _glideMs         = 0.0f;

    dsp::AnalogEnv _filterEnv[ARP_VOICES];
    uint32_t       _lastUpdateMs = 0;

    int8_t _findFreeVoice();
    int8_t _findVoiceForNote(uint8_t midi_note);
    int8_t _stealOldestVoice();
    void   _triggerVoice(uint8_t voice_idx, uint8_t midi_note, float velocity);
    void   _releaseVoice(uint8_t voice_idx);
    void   _applyVoiceMix(uint8_t v);
    float  _midiToFreq(uint8_t midi_note);
};

#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — oscilador PolyBLEP y filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013)
#include "../audio/moog_ladder.h"
#include "../audio/polyblep_waveform.h"
#include "../dsp/analog_env.h"
#include "../audio/analog_vca.h"
#include "lfo_mod.h"

/**
 * @brief Motor de síntesis Moog Minimoog Model D para GrooveForge Brain.
 *
 * Implementa la arquitectura substractiva del Minimoog 1970:
 *   3 VCO (PolyBLEP band-limited + organic drift) → Mixer (soft-clip + drive) → VCF (MoogLadder4P) → VCA → I2S out
 *
 * Dos envelopes independientes:
 *   - Filter envelope: modula el cutoff del VCF con curvas RC analógicas (dsp::AnalogEnv)
 *   - VCA envelope:    controla la amplitud via AnalogVca (envolvente RC por muestra)
 *
 * Incluye:
 *   - Pila monofónica de 8 notas con prioridad a la última nota y single-trigger legato
 *   - Glide exponencial estable (sin overshoot por dt variable)
 *   - Saturación (Drive) en la entrada del ladder transistorizado
 *   - Sensibilidad a la velocidad en amplitud y brillo del filtro
 */
class MoogModelD {
public:
    MoogModelD();

    void begin(float volume = 0.5f);

    /**
     * @brief Inicia una nota en la pila monofónica.
     * En modo legato (default true), si ya hay una nota presionada, desliza la frecuencia
     * sin re-disparar los envelopes desde cero (single-trigger auténtico del Minimoog).
     */
    void noteOn(uint8_t midi_note, float velocity = 1.0f);

    /**
     * @brief Suelta una nota específica de la pila monofónica.
     * Si quedan otras teclas presionadas en la pila, regresa a la nota previa sin cortar.
     */
    void noteOff(uint8_t midi_note);

    /**
     * @brief Suelta todas las notas activas (compatibilidad con sketches legacy).
     */
    void noteOff();

    void setWaveform(uint8_t osc_idx, uint16_t waveform);
    void setDetune(float cents);
    void setVco3Interval(float semitones);
    void setVco3Detune(float cents);
    void setPulseWidth(float pw);
    void setOscLevels(float osc1, float osc2, float osc3, float noise);

    void setFilterCutoff(float hz);
    void setFilterResonance(float q);
    void setFilterEnvAmount(float amount);
    void setDrive(float drive);
    void setLegato(bool on) { _legato = on; }

    void setFilterAttack(float ms);
    void setFilterDecay(float ms);
    void setFilterSustain(float level);
    void setFilterRelease(float ms);

    void setVCAAttack(float ms);
    void setVCADecay(float ms);
    void setVCASustain(float level);
    void setVCARelease(float ms);

    void setGlide(float ms);
    void setLfoMod(const LfoMod& m) { _lfo = m; }

    void update();
    void set_dsp_enabled(bool on);

    AudioStream& getOutput() { return _vcaEnv; }

private:
    LfoMod _lfo;
    float  _appliedPitchRatio = 1.0f;

    // ── Audio objects ─────────────────────────────────────────────────────────
    AudioSynthPolyBlep       _osc1;
    AudioSynthPolyBlep       _osc2;
    AudioSynthPolyBlep       _osc3;
    AudioSynthNoiseWhite     _noise;
    AudioMixer4              _mixer;
    MoogLadder4P             _filter;
    AnalogVca      _vcaEnv;

    AudioConnection _c1, _c2, _c3, _c4, _c5, _c6;

    // ── Estado de pitch, glide y pila mono ────────────────────────────────────
    float    _baseFreq      = 440.0f;
    float    _targetFreq    = 440.0f;
    float    _currentFreq   = 440.0f;
    float    _glideMs       = 0.0f;
    float    _detuneCents   = 5.0f;
    float    _vco3Semitones = -12.0f;
    float    _vco3Detune    = -3.0f;
    float    _pulseWidth    = 0.5f;

    float    _lvl1          = 0.45f;
    float    _lvl2          = 0.45f;
    float    _lvl3          = 0.30f;
    float    _lvlNoise      = 0.00f;
    float    _velGain       = 1.0f;
    float    _velBrightness = 1.0f;

    static constexpr uint8_t MONO_STACK_MAX = 8;
    uint8_t  _noteStack[MONO_STACK_MAX] = {};
    uint8_t  _noteStackLen  = 0;
    bool     _legato        = true;

    // ── Estado del filtro ─────────────────────────────────────────────────────
    float    _filterCutoff = 800.0f;
    float    _filterEnvAmt = 0.6f;

    // ── Estado general ────────────────────────────────────────────────────────
    bool     _noteActive   = false;
    uint32_t _lastUpdateMs = 0;

    // ── Filter envelope con curvas RC y sin salto en release ──────────────────
    dsp::AnalogEnv _filterEnv;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void  _updateOscFreqs();
    void  _applyMixerGains();
    float _midiToFreq(uint8_t midi_note);
    float _centsToRatio(float cents);
    float _semitonesToRatio(float semitones);
};

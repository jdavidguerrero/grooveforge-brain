#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>

// Longitud del delay line del chorus en shorts.
// 1200 shorts × 2 bytes = 2400 bytes. Suficiente para ~25ms @ 44.1kHz.
// El Juno-106 original usaba ~15ms de delay máximo en el MN3009.
#define JUNO_CHORUS_BUF_LEN 1200

/**
 * @brief Motor de síntesis Roland Juno-106 para GrooveForge Brain.
 *
 * Arquitectura: 1 DCO (sawtooth/square) + sub-oscillator (square, /2) +
 * noise + BBD chorus + VCF (LP 2-polo placeholder) + VCA.
 *
 * Un solo envelope ADSR modula simultáneamente:
 *   - VCF: desplaza el cutoff en software (resolución de loop(), ~1-2ms)
 *   - VCA: controla amplitud sample-accurate via AudioEffectEnvelope
 *
 * Chorus: emulación BBD digital via AudioEffectChorus (Teensy Audio Library oficial).
 * Dos modos de operación:
 *   - Modo I:  wet 40%, sutil — strings, pads
 *   - Modo II: wet 60%, pronunciado — bass, lead
 *
 * Diferencia clave con MoogModelD:
 *   - 1 DCO vs 3 VCOs (sin detune analógico, sin beating involuntario)
 *   - 1 ADSR compartido vs 2 envelopes independientes
 *   - Chorus BBD integrado como parte del engine (no FX separado)
 *
 * Theory completa: apps/docs/sprints/06-juno-106.md
 *
 * Refs: apps/docs/01-architecture.md §4.1 — polyphony targets
 *       apps/docs/05-fx-architecture.md §1.8 — Phase Chorus (AudioEffectChorus usage)
 */
class Juno106 {
public:
    Juno106();

    /**
     * @brief Inicializa el codec SGTL5000 y el grafo de audio.
     *
     * Debe llamarse una vez en setup(), antes de cualquier noteOn.
     * Llama AudioMemory(24) internamente — no llamar AudioMemory() desde el sketch.
     *
     * @param volume Volumen del codec, 0.0–1.0 (default 0.5)
     */
    void begin(float volume = 0.5f);

    /**
     * @brief Inicia una nota.
     *
     * Calcula la frecuencia desde midi_note, actualiza DCO + sub, dispara el
     * filter envelope software y el VCA envelope sample-accurate.
     *
     * @param midi_note Nota MIDI (0–127). A4 = 69.
     * @param velocity  Velocidad normalizada 0.0–1.0 (reservado — no implementado aún).
     */
    void noteOn(uint8_t midi_note, float velocity = 1.0f);

    /**
     * @brief Suelta la nota activa. Dispara la etapa release de ambos envelopes.
     */
    void noteOff();

    /**
     * @brief Selecciona la forma de onda del DCO principal.
     *
     * @param waveform Constante de la Teensy Audio Library: WAVEFORM_SAWTOOTH,
     *                 WAVEFORM_SQUARE, WAVEFORM_SINE, etc.
     *                 El sub-oscillator es siempre WAVEFORM_SQUARE (fijo por spec).
     */
    void setWaveform(uint16_t waveform);

    /**
     * @brief Nivel del sub-oscillator en el mixer (0.0–1.0).
     *
     * El sub siempre sigue la frecuencia del DCO / 2 (una octava abajo).
     */
    void setSubLevel(float level);

    /**
     * @brief Nivel del ruido blanco en el mixer (0.0–1.0).
     *
     * Desactivado por defecto. Útil para breath noise en leads.
     */
    void setNoiseLevel(float level);

    /**
     * @brief Frecuencia de corte del VCF en Hz (20–20000).
     *
     * El valor real aplicado al filtro incluye la modulación del envelope:
     *   cutoff_real = _filterCutoff + _filterEnvAmt * _filterEnvLevel * 8000 Hz
     */
    void setFilterCutoff(float hz);

    /**
     * @brief Resonancia del VCF (0.7 = flat/Butterworth, 5.0 = énfasis máximo).
     *
     * AudioFilterStateVariable acepta valores positivos; por encima de ~10 el
     * comportamiento es inestable en el placeholder digital.
     */
    void setFilterResonance(float q);

    /**
     * @brief Cantidad de modulación del envelope sobre el cutoff (0.0–1.0).
     *
     * 0.0 = el envelope no mueve el filtro (VCA-only envelope).
     * 1.0 = el envelope agrega hasta 8000 Hz sobre el cutoff base.
     */
    void setFilterEnvAmount(float amount);

    /** @brief Attack del ADSR compartido en ms (afecta VCF + VCA). */
    void setAttack(float ms);
    /** @brief Decay del ADSR compartido en ms. */
    void setDecay(float ms);
    /** @brief Sustain del ADSR compartido, 0.0–1.0. */
    void setSustain(float level);
    /** @brief Release del ADSR compartido en ms. */
    void setRelease(float ms);

    /**
     * @brief Configura el chorus BBD y selecciona el modo.
     *
     * El chorus es la firma sonora del Juno-106. Modos de operación:
     *   - Modo I  (mode=1): wet ratio 40%, LFO suave (~0.5Hz) — pads, strings
     *   - Modo II (mode=2): wet ratio 60%, LFO más rápido (~1.0Hz) — bass, lead
     *   - Off     (enabled=false): bypass completo, señal 100% dry
     *
     * @param enabled true = chorus activo
     * @param mode    1 = Modo I (sutil), 2 = Modo II (pronunciado)
     */
    void setChorus(bool enabled, uint8_t mode = 1);

    /**
     * @brief Actualiza el estado del engine — debe llamarse desde loop().
     *
     * Procesa la máquina de estados del filter envelope y aplica la modulación
     * resultante al AudioFilterStateVariable. Resolución temporal = granularidad
     * de loop() (~0.5-2ms) — suficiente para modulación de filtro (>5ms audible).
     */
    void update();

private:
    // ── Audio objects (Teensy Audio Library oficial) ──────────────────────────
    AudioSynthWaveform       _dco;
    AudioSynthWaveform       _sub;        // square fijo, octava abajo (freq/2)
    AudioSynthNoiseWhite     _noise;
    AudioMixer4              _mixer;
    // Placeholder digital 2-polo LP. TODO: reemplazar con routing analógico
    // cuando el ladder 2N3904 esté disponible (ver 01-architecture.md §3.3 pin 25).
    AudioFilterStateVariable _vcf;
    AudioEffectEnvelope      _vcaEnv;
    AudioEffectChorus        _chorus;
    AudioMixer4              _dryWetMix;  // blend dry/chorus para el bypass
    AudioOutputI2S           _out;
    AudioControlSGTL5000     _codec;

    // Buffer de delay para el chorus. Debe vivir mientras el objeto exista.
    // AudioEffectChorus no gestiona memoria propia — el owner provee el buffer.
    short _chorusBuf[JUNO_CHORUS_BUF_LEN];

    // ── Conexiones del grafo (deben existir mientras el engine viva) ──────────
    AudioConnection _c1, _c2, _c3, _c4, _c5, _c6, _c7, _c8, _c9, _c10;

    // ── Estado del filtro ─────────────────────────────────────────────────────
    float _filterCutoff  = 1200.0f;  // Hz, base sin modulación
    float _filterEnvAmt  = 0.5f;     // 0.0–1.0

    // ── Filter envelope (software, no sample-accurate) ────────────────────────
    float    _filterEnvLevel      = 0.0f;
    float    _filterEnvAttackMs   = 10.0f;
    float    _filterEnvDecayMs    = 200.0f;
    float    _filterEnvSustain    = 0.5f;
    float    _filterEnvReleaseMs  = 300.0f;

    enum class EnvStage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    EnvStage _filterEnvStage   = EnvStage::IDLE;
    uint32_t _filterEnvStageMs = 0;
    uint32_t _lastUpdateMs     = 0;

    // ── Estado del chorus ─────────────────────────────────────────────────────
    bool    _chorusEnabled = true;
    uint8_t _chorusMode    = 1;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void  _updateOscFreqs(float freq);
    void  _updateFilterEnv(uint32_t dt_ms);
    float _midiToFreq(uint8_t midi_note);
};

#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013)
#include "../audio/moog_ladder.h"

/**
 * @brief Motor de síntesis Moog Minimoog Model D para GrooveForge Brain.
 *
 * Implementa la arquitectura substractiva del Minimoog 1970:
 *   3 VCO (sawtooth) → Mixer → VCF → VCA → I2S out
 *
 * Dos envelopes independientes:
 *   - Filter envelope: modula el cutoff del VCF en software (ver update())
 *   - VCA envelope:    controla la amplitud via AudioEffectEnvelope (sample-accurate)
 *
 * El VCF usa AudioFilterStateVariable como placeholder digital (2-polo, 12dB/oct).
 * TODO Sprint 1.4: reemplazar con routing via SGTL5000 ADC cuando el ladder
 *                  analógico 2N3904 esté disponible.
 *
 * Theory completa y diagrama de signal flow:
 *   apps/docs/sprints/05-moog-model-d.md
 *
 * Refs: apps/docs/01-architecture.md §4.1 — polyphony targets
 *       apps/docs/05-fx-architecture.md §2 — CPU budget
 */
class MoogModelD {
public:
    MoogModelD();

    /**
     * @brief Inicializa el codec SGTL5000 y el grafo de audio.
     *
     * Debe llamarse una vez en setup(), DESPUÉS de AudioMemory() del sketch.
     * No llama AudioMemory() internamente — el sketch es responsable de llamarla
     * con el tamaño correcto antes de begin() (mínimo 20 bloques para el engine
     * solo; 40+ si hay bridge, navegación y otros objetos de audio activos).
     *
     * @param volume Volumen del codec, 0.0–1.0 (default 0.5)
     */
    void begin(float volume = 0.5f);

    /**
     * @brief Inicia una nota.
     *
     * Calcula la frecuencia objetivo desde midi_note, activa el glide si corresponde,
     * dispara el filter envelope y el VCA envelope.
     *
     * @param midi_note Nota MIDI (0–127). A4 = 69.
     * @param velocity  Velocidad normalizada 0.0–1.0 (reservado — no implementado aún).
     */
    void noteOn(uint8_t midi_note, float velocity = 1.0f);

    /**
     * @brief Suelta la nota activa. Dispara las etapas release de ambos envelopes.
     */
    void noteOff();

    /**
     * @brief Selecciona la forma de onda de un oscilador.
     *
     * @param osc_idx  Índice del oscilador: 0=osc1, 1=osc2, 2=osc3
     * @param waveform Constante de la Teensy Audio Library: WAVEFORM_SAWTOOTH,
     *                 WAVEFORM_SQUARE, WAVEFORM_SINE, etc.
     */
    void setWaveform(uint8_t osc_idx, uint16_t waveform);

    /**
     * @brief Ajusta el detune entre osc1 y osc2 en cents.
     *
     * El detune crea beating (interferencia periódica) que da el "warmth" del Moog.
     *
     * @param cents Detune en cents. 100 = 1 semitono. Rango útil: 0–50.
     */
    void setDetune(float cents);

    /**
     * @brief Intervalo de VCO3 relativo a VCO1 en semitones.
     *
     * El Minimoog real tiene VCO3 como oscilador completamente independiente con
     * switch de octava (32', 16', 8', 4', 2') y ajuste fino. Aquí se expone como
     * intervalo continuo en semitones para control digital preciso.
     *
     * Valores de referencia del hardware original:
     *   -24 = 32' (dos octavas bajo VCO1) — grave pesado
     *   -12 = 16' (sub-octava)           — default, "bass foundation"
     *     0 = 8'  (unísono con VCO1)     — engrosamiento de unísono
     *   +12 = 4'  (octava arriba)        — brillante
     *   +24 = 2'  (dos octavas arriba)   — muy brillante
     *
     * @param semitones Intervalo en semitones (−24 a +24). Default: −12.
     */
    void setVco3Interval(float semitones);

    /**
     * @brief Ajusta los gains del mixer de osciladores.
     *
     * La suma de gains debería ser ≤ 1.0 para evitar clipping digital antes del filter.
     *
     * @param osc1  Gain del VCO1 (0.0–1.0)
     * @param osc2  Gain del VCO2 (0.0–1.0)
     * @param osc3  Gain del VCO3 sub-octave (0.0–1.0)
     * @param noise Gain del generador de ruido blanco (0.0–1.0)
     */
    void setOscLevels(float osc1, float osc2, float osc3, float noise);

    /**
     * @brief Ajusta la frecuencia de corte del filtro placeholder.
     *
     * TODO Sprint 1.4: cuando el ladder analógico esté disponible, esta función
     * controlará el exponential converter que genera el CV de cutoff.
     *
     * @param hz Frecuencia de corte en Hz (20–20000).
     */
    void setFilterCutoff(float hz);

    /**
     * @brief Ajusta la resonancia del filtro.
     *
     * @param q Resonancia. 0.7 = Butterworth (flat), 5.0 = cerca de auto-oscilación.
     *          El AudioFilterStateVariable acepta valores positivos sin límite superior
     *          definido — por encima de ~10 el comportamiento es inestable.
     */
    void setFilterResonance(float q);

    /**
     * @brief Ajusta cuánto modula el filter envelope al cutoff.
     *
     * El envelope agrega hasta filterEnvAmount × 8000 Hz al cutoff base.
     *
     * @param amount Cantidad de modulación, 0.0–1.0.
     */
    void setFilterEnvAmount(float amount);

    /** @brief Attack del filter envelope en ms. */
    void setFilterAttack(float ms);
    /** @brief Decay del filter envelope en ms. */
    void setFilterDecay(float ms);
    /** @brief Sustain del filter envelope, 0.0–1.0. */
    void setFilterSustain(float level);
    /** @brief Release del filter envelope en ms. */
    void setFilterRelease(float ms);

    /** @brief Attack del VCA envelope en ms (rango AudioEffectEnvelope: 0–11880ms). */
    void setVCAAttack(float ms);
    /** @brief Decay del VCA envelope en ms. */
    void setVCADecay(float ms);
    /** @brief Sustain del VCA envelope, 0.0–1.0. */
    void setVCASustain(float level);
    /** @brief Release del VCA envelope en ms. */
    void setVCARelease(float ms);

    /**
     * @brief Ajusta el tiempo de glide (portamento) entre notas.
     *
     * El glide interpola linealmente en Hz entre la frecuencia actual y la objetivo.
     * Ver theory en apps/docs/sprints/05-moog-model-d.md §Glide.
     *
     * @param ms Tiempo de deslizamiento en ms. 0 = instantáneo (sin glide).
     */
    void setGlide(float ms);

    /**
     * @brief Actualiza el estado del engine — debe llamarse desde loop().
     *
     * Procesa:
     *  1. Glide: interpolación de frecuencia entre _currentFreq y _targetFreq
     *  2. Filter envelope software: máquina de estados ATTACK/DECAY/SUSTAIN/RELEASE
     *  3. Escritura del cutoff al AudioFilterStateVariable con modulación aplicada
     *
     * Resolución temporal: granularidad de loop() (~0.5-2ms). Suficiente para
     * envelopes de filtro — el oído no distingue por debajo de ~5ms de resolución
     * en modulación de filtro.
     */
    void update();

    /**
     * @brief Retorna el último nodo del grafo del engine — fuente para mezcla externa.
     *
     * Sprint 36: extraer AudioOutputI2S/AudioControlSGTL5000 a `src/audio/audio_graph.cpp`
     * para permitir multi-engine. El sketch (o main.cpp) conecta esta salida a un
     * AudioMixer4 compartido que enruta al singleton I2S.
     *
     * @return Referencia al _vcaEnv — output mono del engine (mismo en L y R).
     */
    AudioStream& getOutput() { return _vcaEnv; }

private:
    // ── Audio objects (Teensy Audio Library oficial) ──────────────────────────
    AudioSynthWaveform       _osc1;
    AudioSynthWaveform       _osc2;
    AudioSynthWaveform       _osc3;
    AudioSynthNoiseWhite     _noise;
    AudioMixer4              _mixer;
    // Filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013) — código custom.
    // Reemplaza AudioFilterStateVariable placeholder. API compatible: frequency(),
    // resonance(), octaveControl(). El filtro analógico 2N3904 sigue operando sobre
    // la mezcla final (apps/docs/03-filter-design.md §0).
    MoogLadder4P _filter;
    AudioEffectEnvelope      _vcaEnv;
    // AudioOutputI2S y AudioControlSGTL5000 movidos a src/audio/audio_graph.cpp
    // (Sprint 36) — son singletons de hardware, no pueden estar duplicados entre
    // los 3 engines. Cada engine expone su _vcaEnv vía getOutput() y el audio_graph
    // los mezcla en un AudioMixer4 compartido antes del singleton I2S.

    // ── Conexiones del grafo (deben existir mientras el engine viva) ──────────
    // _c7/_c8 (_vcaEnv → _out L+R) eliminados en Sprint 36 — la conexión al I2S
    // vive ahora en src/audio/audio_graph.cpp via getOutput().
    AudioConnection _c1, _c2, _c3, _c4, _c5, _c6;

    // ── Estado de pitch y glide ───────────────────────────────────────────────
    float    _baseFreq     = 440.0f;   // frecuencia de la nota activa
    float    _targetFreq   = 440.0f;   // destino del glide
    float    _currentFreq  = 440.0f;   // frecuencia actual (interpola hacia target)
    float    _glideMs      = 0.0f;
    float    _detuneCents   = 5.0f;
    float    _vco3Semitones = -12.0f;  ///< intervalo VCO3 vs VCO1 [semitones]; default sub-octava

    // ── Estado del filtro ─────────────────────────────────────────────────────
    float    _filterCutoff = 800.0f;   // Hz, base sin modulación
    float    _filterEnvAmt = 0.6f;     // 0.0–1.0, cuánto agrega el envelope al cutoff

    // ── Estado general ────────────────────────────────────────────────────────
    bool     _noteActive      = false;
    uint32_t _lastUpdateMs    = 0;

    // ── Filter envelope (software, no sample-accurate) ────────────────────────
    float    _filterEnvLevel      = 0.0f;   // nivel actual, 0.0–1.0
    float    _filterEnvAttackMs   = 20.0f;
    float    _filterEnvDecayMs    = 300.0f;
    float    _filterEnvSustain    = 0.4f;
    float    _filterEnvReleaseMs  = 400.0f;

    enum class EnvStage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    EnvStage _filterEnvStage   = EnvStage::IDLE;
    uint32_t _filterEnvStageMs = 0;  // tiempo acumulado en la etapa actual

    // ── Helpers ───────────────────────────────────────────────────────────────
    void  _updateOscFreqs();
    void  _updateFilterEnv(uint32_t dt_ms);
    float _midiToFreq(uint8_t midi_note);
    float _centsToRatio(float cents);
    float _semitonesToRatio(float semitones);  ///< 2^(semitones/12)
};

#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013)
#include "../audio/moog_ladder.h"

static constexpr uint8_t OB6_VOICES = 6;

/**
 * @brief Motor de síntesis Sequential OB-6 para GrooveForge Brain.
 *
 * 6 voces polifónicas, cada una con 2 VCOs + sub oscillator + noise + VCF + VCA envelope.
 * Signal flow por voz:
 *   VCO1  ──ch0──┐
 *   VCO2  ──ch1──┤
 *                ├──→ VoiceMixer ──→ VCF (SVF LP) ──→ AmpEnv ──→ MasterMixer ──→ Output
 *   Sub   ──ch2──┤
 *   Noise ──ch3──┘
 *
 * El OB-6 real usa un filtro State Variable Filter (Curtis CEM3320 clone) con
 * salidas simultáneas LP/BP/HP. Aquí usamos AudioFilterStateVariable de la Teensy
 * Audio Library que provee los tres modos en puertos separados (0=LP, 1=BP, 2=HP).
 * El puerto de salida está hardcodeado en la AudioConnection — setFilterMode() es
 * un placeholder para implementación futura con bus mixer por voz.
 *
 * Voice stealing: FIFO — se roba la voz con menor age counter cuando todas están activas.
 *
 * Master mix: AudioMixer4 tiene 4 canales max → 2 mixers:
 *   _masterMixA: voces 0-3 (canales 0-3)
 *   _masterMixB: voces 4-5 (canales 0-1)
 *   _finalMix:   _masterMixA (ch0) + _masterMixB (ch1)
 *
 * CPU estimado: ~4-5% con 6 voces activas (01-architecture.md §4.1 budget: ~30%).
 * AudioMemory: presupuestado en audio_graph::init() — OB6 NO llama AudioMemory().
 *
 * Theory: apps/docs/sprints/38-ob6-engine.md
 * Refs:   apps/docs/01-architecture.md §4.1
 *         apps/docs/05-fx-architecture.md §2
 */
class OB6 {
public:
    OB6();

    /**
     * @brief Inicializa el grafo de audio de 6 voces.
     *
     * Debe llamarse una vez en setup(), después de audio_graph::init().
     * No llama AudioMemory() ni g_codec.enable() — ambos son responsabilidad
     * de audio_graph::init() (singleton compartido).
     *
     * @param volume Reservado — no usado. El volumen lo gestiona el codec compartido.
     */
    void begin(float volume = 0.5f);

    /**
     * @brief Dispara una nota polifónica.
     *
     * Busca una voz libre. Si no hay voces libres, roba la voz con menor age (FIFO).
     * Si la nota ya está sonando, hace re-trigger en la misma voz.
     *
     * @param midi_note Nota MIDI (0–127). A4 = 69.
     * @param velocity  Velocidad normalizada 0.0–1.0 (reservado).
     */
    void noteOn(uint8_t midi_note, float velocity = 1.0f);

    /**
     * @brief Suelta una nota. Solo afecta la voz con esa nota MIDI asignada.
     *
     * @param midi_note Nota MIDI a soltar (0–127).
     */
    void noteOff(uint8_t midi_note);

    /**
     * @brief Waveform para VCO1 de todas las voces.
     *
     * @param waveform Constante Teensy Audio Library: WAVEFORM_SAWTOOTH,
     *                 WAVEFORM_SQUARE, WAVEFORM_SINE, etc.
     */
    void setVco1Waveform(uint16_t waveform);

    /**
     * @brief Waveform para VCO2 de todas las voces.
     *
     * @param waveform Constante Teensy Audio Library.
     */
    void setVco2Waveform(uint16_t waveform);

    /**
     * @brief Intervalo de VCO2 relativo a VCO1 en semitones.
     *
     * 0 = unísono, 12 = octava arriba, -12 = octava abajo.
     * Se aplica a todas las voces simultáneamente.
     *
     * @param semitones Intervalo en semitones (-24 a +24).
     */
    void setVco2Interval(float semitones);

    /**
     * @brief Mix de 4 fuentes en el VoiceMixer de cada voz.
     *
     * @param vco1  Gain de VCO1 (0.0–1.0), conectado a voiceMix ch0.
     * @param vco2  Gain de VCO2 (0.0–1.0), conectado a voiceMix ch1.
     * @param sub   Gain del sub oscillator (0.0–1.0), conectado a voiceMix ch2.
     * @param noise Gain del noise (0.0–1.0), conectado a voiceMix ch3.
     */
    void setOscMix(float vco1, float vco2, float sub, float noise);

    /**
     * @brief Frecuencia de corte del VCF de todas las voces.
     *
     * @param hz Frecuencia de corte en Hz (20–20000).
     */
    void setFilterCutoff(float hz);

    /**
     * @brief Resonancia del VCF de todas las voces.
     *
     * @param q Resonancia: 0.7 = Butterworth flat, 5.0 = énfasis máximo.
     */
    void setFilterResonance(float q);

    /**
     * @brief Cuánto modula el envelope el cutoff del VCF.
     *
     * El envelope agrega hasta filterEnvAmount × 6000 Hz al cutoff base.
     *
     * @param amount Modulación 0.0–1.0.
     */
    void setFilterEnvAmount(float amount);

    /**
     * @brief Selecciona el modo del VCF (LP/BP/HP).
     *
     * NOTA: El AudioFilterStateVariable tiene 3 puertos de salida simultáneos
     * (0=LP, 1=BP, 2=HP), pero las AudioConnections son estáticas — no se
     * puede redirigir el destino de un puerto en runtime.
     *
     * Esta implementación v1 siempre usa LP (puerto 0 conectado en constructor).
     * El campo _filterMode se guarda pero no tiene efecto en audio.
     * Implementación completa requiere un AudioMixer4 por voz para combinar
     * las 3 salidas del SVF — pendiente sprint futuro.
     *
     * @param mode 0=LP (activo), 1=BP (sin efecto), 2=HP (sin efecto).
     */
    void setFilterMode(uint8_t mode);

    /** @brief Attack del VCA envelope en ms. */
    void setAttack(float ms);
    /** @brief Decay del VCA envelope en ms. */
    void setDecay(float ms);
    /** @brief Sustain del VCA envelope, 0.0–1.0. */
    void setSustain(float level);
    /** @brief Release del VCA envelope en ms. */
    void setRelease(float ms);

    /**
     * @brief Actualiza el estado del engine — debe llamarse desde loop().
     *
     * Procesa por cada voz activa:
     *  1. Filter envelope software (ATTACK/DECAY/SUSTAIN/RELEASE)
     *  2. Cutoff del VCF con modulación de envelope aplicada
     *
     * Resolución temporal: granularidad de loop() (~0.5–2ms).
     */
    void update();

    /**
     * @brief Retorna el nodo de salida del engine — fuente para mezcla externa.
     *
     * _finalMix combina las 6 voces (vía _masterMixA + _masterMixB).
     * El sketch conecta este nodo a g_engine_mix_a ch3.
     *
     * @return Referencia al _finalMix (output port 0 = mono mix de 6 voces).
     */
    AudioStream& getOutput() { return _finalMix; }

private:
    // ── Audio objects — 6 voces estáticas (Teensy Audio Library oficial) ─────────
    AudioSynthWaveform       _vco1[OB6_VOICES];
    AudioSynthWaveform       _vco2[OB6_VOICES];
    AudioSynthWaveform       _sub[OB6_VOICES];         // sub osc: square, 1 oct below VCO1
    AudioSynthNoiseWhite     _noise[OB6_VOICES];
    AudioMixer4              _voiceMix[OB6_VOICES];    // vco1(ch0) + vco2(ch1) + sub(ch2) + noise(ch3)
    // Filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013) — código custom.
    // 6 instancias independientes (una por voz). API compatible con AudioFilterStateVariable.
    MoogLadder4P             _filter[OB6_VOICES];
    AudioEffectEnvelope      _ampEnv[OB6_VOICES];

    // Master mix: 2 AudioMixer4 para 6 voces
    // _masterMixA: voces 0–3 (canales 0–3)
    // _masterMixB: voces 4–5 (canales 0–1, canales 2–3 vacíos)
    // _finalMix:   _masterMixA (ch0) + _masterMixB (ch1)
    AudioMixer4              _masterMixA;
    AudioMixer4              _masterMixB;
    AudioMixer4              _finalMix;

    // ── Conexiones — 44 individuales (AudioConnection no tiene copy ctor) ─────────
    // Por voz (6 conexiones × 6 voces = 36):
    //   vco1→voiceMix(ch0), vco2→voiceMix(ch1), sub→voiceMix(ch2),
    //   noise→voiceMix(ch3), voiceMix→filter, filter(port0=LP)→ampEnv
    AudioConnection _c0,  _c1,  _c2,  _c3,  _c4,  _c5;   // voz 0
    AudioConnection _c6,  _c7,  _c8,  _c9,  _c10, _c11;  // voz 1
    AudioConnection _c12, _c13, _c14, _c15, _c16, _c17;  // voz 2
    AudioConnection _c18, _c19, _c20, _c21, _c22, _c23;  // voz 3
    AudioConnection _c24, _c25, _c26, _c27, _c28, _c29;  // voz 4
    AudioConnection _c30, _c31, _c32, _c33, _c34, _c35;  // voz 5
    // ampEnv → master mixers (8 conexiones):
    AudioConnection _c36, _c37, _c38, _c39;  // ampEnv[0-3] → masterMixA ch0-3
    AudioConnection _c40, _c41;               // ampEnv[4-5] → masterMixB ch0-1
    AudioConnection _c42, _c43;               // masterMixA→finalMix, masterMixB→finalMix

    // ── Voice state ───────────────────────────────────────────────────────────────
    struct VoiceState {
        uint8_t  midi_note = 255;   // 255 = libre (valor inválido en MIDI)
        bool     active    = false;
        uint32_t age       = 0;     // para voice stealing (menor age = más antigua)
    };
    VoiceState _voices[OB6_VOICES];
    uint32_t   _ageCounter = 0;

    // ── Parámetros globales ───────────────────────────────────────────────────────
    float _filterCutoff    = 2000.0f;
    float _filterEnvAmt    = 0.4f;
    float _filterResonance = 1.5f;
    float _vco2Semitones   = 0.0f;
    uint8_t _filterMode    = 0;     // 0=LP — reservado, sin efecto en v1 (ver setFilterMode())

    // ── Filter envelope por voz (software FSM) ────────────────────────────────────
    float    _filterEnvLevel[OB6_VOICES]    = {};
    float    _filterEnvAttackMs  = 10.0f;
    float    _filterEnvDecayMs   = 300.0f;
    float    _filterEnvSustain   = 0.6f;
    float    _filterEnvReleaseMs = 500.0f;

    enum class EnvStage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    EnvStage _filterEnvStage[OB6_VOICES]    = {};
    uint32_t _filterEnvStageMs[OB6_VOICES]  = {};

    uint32_t _lastUpdateMs = 0;

    // ── Helpers ───────────────────────────────────────────────────────────────────
    /** @brief Retorna índice de la primera voz libre (midi_note==255), o -1 si ninguna. */
    int8_t _findFreeVoice();
    /** @brief Retorna índice de la voz que tiene asignada midi_note, o -1 si no existe. */
    int8_t _findVoiceForNote(uint8_t midi_note);
    /** @brief Retorna índice de la voz con menor age (más antigua). Nunca retorna -1. */
    int8_t _stealOldestVoice();
    /** @brief Inicializa una voz con la nota MIDI dada y la dispara. */
    void   _triggerVoice(uint8_t voice_idx, uint8_t midi_note);
    /** @brief Dispara el release de una voz y la marca como libre. */
    void   _releaseVoice(uint8_t voice_idx);
    /** @brief Avanza la máquina de estados del filter envelope de una voz. */
    void   _updateFilterEnv(uint8_t voice_idx, uint32_t dt_ms);
    /** @brief Convierte nota MIDI a frecuencia Hz. A4 (69) = 440Hz. */
    float  _midiToFreq(uint8_t midi_note);
    /** @brief Convierte semitones a ratio de frecuencia. 12 semitones = 2.0. */
    float  _semitonesToRatio(float semitones);
};

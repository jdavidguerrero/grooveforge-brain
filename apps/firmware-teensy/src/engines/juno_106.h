#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013)
#include "../audio/moog_ladder.h"

static constexpr uint8_t  JUNO_VOICES         = 6;
static constexpr uint16_t JUNO_CHORUS_BUF_LEN = 1200;  // ~25ms @ 44.1kHz

/**
 * @brief Motor Roland Juno-106 — 6 voces polifónicas, BBD chorus en master bus.
 *
 * Migración mono → poly (Sprint 41).
 *
 * Arquitectura históricamente correcta:
 *   - 6 DCOs independientes (uno por voz) — el Juno real tenía 6 DCO chips
 *   - 1 chorus BBD sobre el bus de mezcla — el hardware tenía 1 BBD para todas las voces
 *   - Filter MoogLadder4P por voz (SVF digital ladder D'Angelo 2013)
 *   - Voice stealing FIFO — igual que OB-6/Prophet-5
 *
 * Signal flow por voz:
 *   DCO[v] ──ch0──┐
 *   sub[v] ──ch1──┤──→ voiceMix[v] ──→ filter[v] (MoogLadder4P) ──→ ampEnv[v]
 *   noise[v]─ch2──┘
 *
 * Master path (chorus en el bus — históricamente correcto):
 *   ampEnv[0-3] ──→ masterMixA ──ch0──┐
 *                                       ├──→ masterBus ──→ chorus ──→ dryWetMix(ch1, wet)
 *   ampEnv[4-5] ──→ masterMixB ──ch1──┘              └──────────────→ dryWetMix(ch0, dry)
 *
 * El "ancho" del Juno viene del chorus sobre 6 voces sumadas — no del detune por voz.
 * Diferencia sonora clave vs Prophet: el Prophet tiene 2 VCOs con detune por voz.
 *
 * CPU estimado: ~5-6% con 6 voces activas + chorus (01-architecture.md §4.1 budget: ~30%).
 *
 * Theory: apps/docs/sprints/06-juno-106.md
 * Refs:   apps/docs/01-architecture.md §4.1
 *         apps/docs/05-fx-architecture.md §1.8
 */
class Juno106 {
public:
    Juno106();

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
     * @brief Suelta la nota con ese MIDI note asignada.
     *
     * API cambiada de noteOff() sin argumento a noteOff(note) para consistencia
     * con el resto de engines polifónicos. noteOff con nota no asignada es no-op.
     *
     * @param midi_note Nota MIDI a soltar (0–127).
     */
    void noteOff(uint8_t midi_note);

    /**
     * @brief Selecciona la forma de onda del DCO principal de todas las voces.
     *
     * @param waveform Constante Teensy Audio Library: WAVEFORM_SAWTOOTH,
     *                 WAVEFORM_SQUARE, WAVEFORM_SINE, etc.
     *                 El sub-oscillator es siempre WAVEFORM_SQUARE (fijo por spec).
     */
    void setWaveform(uint16_t waveform);

    /**
     * @brief Nivel del sub-oscillator en el voiceMix de cada voz (0.0–1.0).
     *
     * El sub siempre sigue la frecuencia del DCO / 2 (una octava abajo).
     */
    void setSubLevel(float level);

    /**
     * @brief Nivel del ruido blanco en el voiceMix de cada voz (0.0–1.0).
     *
     * Desactivado por defecto. Se atenúa internamente ×0.5 para evitar clipping.
     */
    void setNoiseLevel(float level);

    /**
     * @brief Frecuencia de corte del VCF en Hz (20–20000).
     *
     * El valor real aplicado incluye la modulación del envelope:
     *   cutoff_real = _filterCutoff + _filterEnvAmt × _filterEnvLevel[v] × 8000 Hz
     */
    void setFilterCutoff(float hz);

    /**
     * @brief Resonancia del VCF de todas las voces (0.7 = flat/Butterworth).
     */
    void setFilterResonance(float q);

    /**
     * @brief Cantidad de modulación del envelope sobre el cutoff (0.0–1.0).
     *
     * 1.0 = el envelope agrega hasta 8000 Hz sobre el cutoff base.
     */
    void setFilterEnvAmount(float amount);

    /** @brief Attack del ADSR compartido en ms (afecta VCF + VCA por voz). */
    void setAttack(float ms);
    /** @brief Decay del ADSR compartido en ms. */
    void setDecay(float ms);
    /** @brief Sustain del ADSR compartido, 0.0–1.0. */
    void setSustain(float level);
    /** @brief Release del ADSR compartido en ms. */
    void setRelease(float ms);

    /**
     * @brief Configura el chorus BBD en el master bus.
     *
     * El chorus es la firma sonora del Juno-106. Opera sobre el bus de mezcla
     * completo (no por voz) — así funciona el hardware Juno-106 real con el MN3009.
     *
     * Modos:
     *   - Off     (enabled=false): bypass completo, señal 100% dry
     *   - Modo I  (mode=1): wet 40%, LFO suave — pads, strings
     *   - Modo II (mode=2): wet 60%, más movimiento — bass, lead
     *
     * @param enabled true = chorus activo
     * @param mode    1 = Modo I (sutil), 2 = Modo II (pronunciado)
     */
    void setChorus(bool enabled, uint8_t mode = 1);

    /**
     * @brief Actualiza el estado del engine — debe llamarse desde loop().
     *
     * Procesa por cada voz activa:
     *  1. Filter envelope software (ATTACK/DECAY/SUSTAIN/RELEASE)
     *  2. Cutoff del VCF con modulación de envelope aplicada
     */
    void update();

    /**
     * @brief Retorna el nodo de salida del engine — fuente para mezcla externa.
     *
     * _dryWetMix combina dry + chorus wet. El sketch lo conecta a g_engine_mix_a ch1.
     *
     * @return Referencia al _dryWetMix (output port 0).
     */
    AudioStream& getOutput() { return _dryWetMix; }

private:
    // ── Per-voice audio objects (Teensy Audio Library oficial) ───────────────
    AudioSynthWaveform   _dco[JUNO_VOICES];
    AudioSynthWaveform   _sub[JUNO_VOICES];        // square fijo, octava abajo (freq/2)
    AudioSynthNoiseWhite _noise[JUNO_VOICES];
    AudioMixer4          _voiceMix[JUNO_VOICES];   // dco(ch0) + sub(ch1) + noise(ch2)
    // Filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013) — código custom.
    // 6 instancias independientes (una por voz). API compatible con AudioFilterStateVariable.
    MoogLadder4P         _filter[JUNO_VOICES];
    AudioEffectEnvelope  _ampEnv[JUNO_VOICES];

    // ── Master path ───────────────────────────────────────────────────────────
    AudioMixer4       _masterMixA;   // voces 0-3 (canales 0-3)
    AudioMixer4       _masterMixB;   // voces 4-5 (canales 0-1)
    AudioMixer4       _masterBus;    // suma A+B → chorus y dry
    // 1 chorus BBD sobre el bus completo (el hardware Juno-106 real usaba 1 MN3009)
    AudioEffectChorus _chorus;
    AudioMixer4       _dryWetMix;    // ch0=dry, ch1=chorus wet → output

    // Buffer de delay para el chorus. AudioEffectChorus no gestiona memoria propia.
    short _chorusBuf[JUNO_CHORUS_BUF_LEN];

    // ── Conexiones (41): por voz (5×6=30) + master (11) ─────────────────────
    AudioConnection _c0,  _c1,  _c2,  _c3,  _c4;   // voz 0
    AudioConnection _c5,  _c6,  _c7,  _c8,  _c9;   // voz 1
    AudioConnection _c10, _c11, _c12, _c13, _c14;  // voz 2
    AudioConnection _c15, _c16, _c17, _c18, _c19;  // voz 3
    AudioConnection _c20, _c21, _c22, _c23, _c24;  // voz 4
    AudioConnection _c25, _c26, _c27, _c28, _c29;  // voz 5
    AudioConnection _c30, _c31, _c32, _c33;         // ampEnv[0-3] → masterMixA
    AudioConnection _c34, _c35;                     // ampEnv[4-5] → masterMixB
    AudioConnection _c36, _c37;                     // masterMixA/B → masterBus
    AudioConnection _c38;                           // masterBus → chorus
    AudioConnection _c39;                           // chorus → dryWetMix ch1 (wet)
    AudioConnection _c40;                           // masterBus → dryWetMix ch0 (dry)

    // ── Voice state (FIFO stealing) ───────────────────────────────────────────
    struct VoiceState {
        uint8_t  midi_note = 255;   // 255 = libre (valor inválido en MIDI)
        bool     active    = false;
        uint32_t age       = 0;     // para voice stealing (menor age = más antigua)
    };
    VoiceState _voices[JUNO_VOICES];
    uint32_t   _ageCounter = 0;

    // ── Parámetros globales ───────────────────────────────────────────────────
    float _filterCutoff    = 1200.0f;
    float _filterEnvAmt    = 0.5f;
    float _filterResonance = 1.5f;

    // ── Filter envelope por voz (software FSM) ────────────────────────────────
    float    _filterEnvLevel[JUNO_VOICES]    = {};
    float    _filterEnvAttackMs  = 10.0f;
    float    _filterEnvDecayMs   = 200.0f;
    float    _filterEnvSustain   = 0.5f;
    float    _filterEnvReleaseMs = 300.0f;

    enum class EnvStage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    EnvStage _filterEnvStage[JUNO_VOICES]   = {};
    uint32_t _filterEnvStageMs[JUNO_VOICES] = {};

    uint32_t _lastUpdateMs = 0;

    bool    _chorusEnabled = true;
    uint8_t _chorusMode    = 1;

    // ── Helpers ───────────────────────────────────────────────────────────────
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
};

#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013)
#include "../audio/moog_ladder.h"

static constexpr uint8_t ARP_VOICES = 4;

/**
 * @brief Motor de síntesis ARP 2600 para GrooveForge Brain.
 *
 * El ARP 2600 (1971) es un sintetizador semi-modular. Su carácter distintivo
 * viene del ring modulator (VCO1 × VCO2), el filtro de escalera derivado del
 * diseño Moog, y la capacidad de parcheo. Esta implementación:
 *   - 2 VCOs + ring modulator + noise por voz
 *   - SVF como filtro (AudioFilterStateVariable, LP port 0)
 *   - Un envelope compartido VCF+VCA (software filter env + hardware amp env)
 *   - 4 voces polifónicas
 *
 * Signal flow por voz:
 *   VCO1 ──ch1 (dry)──┐
 *   VCO1×VCO2 ──ch0───┤── VoiceMixer ──→ SVF ──→ AmpEnv ──→ MasterMix ──→ Output
 *   Noise ─────ch2────┘
 *
 * Voice stealing: FIFO — menor age counter se roba.
 *
 * CPU estimado: ~3-4% con 4 voces activas (01-architecture.md §4.1 budget: ~30%).
 * AudioMemory: presupuestado en audio_graph::init() — ARP2600 NO llama AudioMemory().
 *
 * Theory: apps/docs/sprints/39-arp2600-engine.md
 * Refs:   apps/docs/01-architecture.md §4.1
 *         apps/docs/05-fx-architecture.md §2
 */
class ARP2600 {
public:
    ARP2600();

    /**
     * @brief Inicializa el grafo de audio de 4 voces.
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
     * VCO1 es la fuente principal y también un operando del ring modulator.
     *
     * @param waveform Constante Teensy Audio Library: WAVEFORM_SAWTOOTH,
     *                 WAVEFORM_SINE, WAVEFORM_SQUARE, etc.
     */
    void setVco1Waveform(uint16_t waveform);

    /**
     * @brief Waveform para VCO2 de todas las voces.
     *
     * VCO2 es el segundo operando del ring modulator. WAVEFORM_SINE es el
     * default — da ring mod clásico (AM con portadora eliminada).
     *
     * @param waveform Constante Teensy Audio Library.
     */
    void setVco2Waveform(uint16_t waveform);

    /**
     * @brief Ratio de frecuencia de VCO2 respecto a VCO1.
     *
     * freq_vco2 = freq_vco1 × ratio. Ratio 1.0 = unísono (ring mod en banda
     * base). Ratio 2.0 = VCO2 una octava arriba (parciales pares en la salida
     * del ring mod). Ratio irracional = parciales inarmónicos — carácter metálico.
     *
     * @param ratio Factor de frecuencia [0.5, 4.0].
     */
    void setVco2Ratio(float ratio);

    /**
     * @brief Mix entre ring mod y VCO1 dry.
     *
     * 0.0 = solo VCO1 dry (sin ring mod).
     * 1.0 = solo ring mod (VCO1×VCO2 puro).
     * 0.5 = blend 50/50 — mezcla de carácter agresivo con cuerpo fundamental.
     *
     * @param mix Factor ring mod [0.0, 1.0].
     */
    void setRingMix(float mix);

    /**
     * @brief Nivel de ruido blanco en la mezcla de voz.
     *
     * Añade textura de ruido antes del filtro. 0.0 = sin ruido.
     * El gain interno se escala a 0.5 × level para no clipear el mixeador.
     *
     * @param level Nivel de ruido [0.0, 1.0].
     */
    void setNoiseLevel(float level);

    /**
     * @brief Frecuencia de corte del SVF de todas las voces.
     *
     * La frecuencia real se aplica en update() con la modulación del envelope sumada.
     *
     * @param hz Frecuencia de corte en Hz [20, 20000].
     */
    void setFilterCutoff(float hz);

    /**
     * @brief Resonancia del SVF de todas las voces.
     *
     * @param q Resonancia: 0.7 = Butterworth flat, 5.0 = énfasis máximo.
     */
    void setFilterResonance(float q);

    /**
     * @brief Cantidad de modulación del envelope sobre el cutoff.
     *
     * El envelope agrega hasta filterEnvAmount × 6000 Hz al cutoff base.
     *
     * @param amount Modulación [0.0, 1.0].
     */
    void setFilterEnvAmount(float amount);

    /** @brief Attack del envelope (VCF + VCA compartido) en ms. */
    void setAttack(float ms);
    /** @brief Decay del envelope en ms. */
    void setDecay(float ms);
    /** @brief Sustain del envelope, 0.0–1.0. */
    void setSustain(float level);
    /** @brief Release del envelope en ms. */
    void setRelease(float ms);

    /**
     * @brief Avanza el filter envelope software — debe llamarse desde loop().
     *
     * Procesa por cada voz activa:
     *   1. Filter envelope FSM (ATTACK/DECAY/SUSTAIN/RELEASE)
     *   2. Cutoff del SVF con modulación aplicada
     *
     * Resolución temporal: granularidad de loop() (~0.5–2ms).
     */
    void update();

    /**
     * @brief Retorna el nodo de salida del engine — fuente para mezcla externa.
     *
     * _finalMix combina las 4 voces (vía _masterMixA).
     * El sketch conecta este nodo a g_engine_mix_b ch1.
     *
     * @return Referencia al _finalMix (output port 0 = mono mix de 4 voces).
     */
    AudioStream& getOutput() { return _finalMix; }

private:
    // ── Audio objects — 4 voces estáticas (Teensy Audio Library oficial) ─────────
    AudioSynthWaveform       _vco1[ARP_VOICES];       // VCO principal + ring mod input A
    AudioSynthWaveform       _vco2[ARP_VOICES];       // VCO secundario (sine por defecto) = ring mod input B
    AudioSynthNoiseWhite     _noise[ARP_VOICES];      // ruido blanco para textura
    AudioEffectMultiply      _ringMod[ARP_VOICES];    // ring mod: VCO1 × VCO2 (multiplicación de señales)
    AudioMixer4              _voiceMix[ARP_VOICES];   // ch0=ringMod, ch1=VCO1 dry, ch2=noise
    // Filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013) — código custom.
    // 4 instancias independientes (una por voz). API compatible con AudioFilterStateVariable.
    MoogLadder4P             _filter[ARP_VOICES];
    AudioEffectEnvelope      _ampEnv[ARP_VOICES];

    // Master mix: 4 voces caben en un solo AudioMixer4
    AudioMixer4 _masterMixA;   // ch0=voz0, ch1=voz1, ch2=voz2, ch3=voz3
    AudioMixer4 _finalMix;     // ch0=masterMixA

    // ── Conexiones — 33 individuales (AudioConnection no tiene copy ctor) ─────────
    // Por voz (7 conexiones × 4 voces = 28):
    //   vco1→ringMod(A,in0), vco2→ringMod(B,in1), ringMod→voiceMix(ch0),
    //   vco1→voiceMix(ch1)[dry], noise→voiceMix(ch2), voiceMix→filter, filter(LP,port0)→ampEnv
    AudioConnection _c0,  _c1,  _c2,  _c3,  _c4,  _c5,  _c6;   // voz 0
    AudioConnection _c7,  _c8,  _c9,  _c10, _c11, _c12, _c13;  // voz 1
    AudioConnection _c14, _c15, _c16, _c17, _c18, _c19, _c20;  // voz 2
    AudioConnection _c21, _c22, _c23, _c24, _c25, _c26, _c27;  // voz 3
    // ampEnv → masterMixA (4 conexiones)
    AudioConnection _c28, _c29, _c30, _c31;  // ampEnv[0-3] → masterMixA ch0-3
    // master path (1 conexión)
    AudioConnection _c32;                    // masterMixA → finalMix ch0

    // ── Voice state ───────────────────────────────────────────────────────────────
    struct VoiceState {
        uint8_t  midi_note = 255;   // 255 = libre (valor inválido en MIDI)
        bool     active    = false;
        uint32_t age       = 0;     // para voice stealing (menor age = más antigua)
    };
    VoiceState _voices[ARP_VOICES];
    uint32_t   _ageCounter = 0;

    // ── Parámetros globales ───────────────────────────────────────────────────────
    float _vco2Ratio       = 1.0f;    // VCO2 en unísono por defecto
    float _ringMixAmt      = 0.5f;    // 50% ring + 50% dry
    float _noiseLevel      = 0.0f;
    float _filterCutoff    = 2000.0f;
    float _filterResonance = 1.5f;
    float _filterEnvAmt    = 0.5f;

    // ── Filter envelope por voz (software FSM — misma arquitectura que OB-6) ─────
    float    _filterEnvLevel[ARP_VOICES]    = {};
    float    _filterEnvAttackMs   = 10.0f;
    float    _filterEnvDecayMs    = 300.0f;
    float    _filterEnvSustain    = 0.6f;
    float    _filterEnvReleaseMs  = 500.0f;

    enum class EnvStage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    EnvStage _filterEnvStage[ARP_VOICES]   = {};
    uint32_t _filterEnvStageMs[ARP_VOICES] = {};

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
};

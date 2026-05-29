#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013)
#include "../audio/moog_ladder.h"

static constexpr uint8_t DX7_VOICES = 6;

/**
 * @brief Motor de síntesis DX7 (2-operator FM) para GrooveForge Brain.
 *
 * El Yamaha DX7 (1983) usa síntesis FM con 6 operadores por voz en 32 algoritmos.
 * Esta implementación simplificada usa 2 operadores (modulador → portadora):
 *
 *   FM formula: out = sin(ω_carrier × t + I × sin(ω_mod × t))
 *   donde I = "FM index" (profundidad de modulación) = amplitud del modulador.
 *
 * AudioSynthWaveformModulated recibe una señal de modulación de frecuencia
 * en su port 0 y aplica modulación de fase exacta (no FM analógica aproximada).
 * `frequencyModulation(octaves)` define la sensibilidad: con 2.0 octaves,
 * una señal de amplitud 1.0 desvía la frecuencia ±2 octavas.
 *
 * Signal flow por voz:
 *   Modulator ──port0──→ Carrier (FM) ──→ SVF ──→ AmpEnv ──→ MasterMix ──→ Output
 *
 * El SVF agrega calidez analógica al timbre FM puro (que es inherentemente digital/brillante).
 *
 * 6 voces polifónicas, voice stealing FIFO (menor age se roba).
 *
 * NOTA: La clase se llama DX7Engine (no DX7) para evitar colisión con macros
 * o símbolos globales que puedan usar ese nombre.
 *
 * CPU estimado: ~4-5% con 6 voces activas (01-architecture.md §4.1 budget: ~30%).
 * AudioMemory: presupuestado en audio_graph::init() — DX7Engine NO llama AudioMemory().
 *
 * Theory: apps/docs/sprints/40-dx7-engine.md
 * Refs:   apps/docs/01-architecture.md §4.1
 *         apps/docs/05-fx-architecture.md §2
 */
class DX7Engine {
public:
    DX7Engine();

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
     * @brief Ratio de frecuencia del modulador respecto a la portadora.
     *
     * freq_mod = freq_carrier × ratio.
     *
     * Ratios enteros producen timbres armónicos (DX7 "bell" = ratio 2.0,
     * "brass" = ratio 1.0). Ratios irracionales producen parciales inarmónicas
     * (metálicos, percusivos).
     *
     * @param ratio Factor de frecuencia [0.5, 8.0].
     */
    void setFmRatio(float ratio);

    /**
     * @brief Índice de modulación FM (FM depth).
     *
     * Controla la amplitud del modulador que alimenta la portadora.
     * 0.0 = portadora pura (sine wave), sin modulación.
     * 0.5 = FM moderada — timbre brillante con pocos parciales adicionales.
     * 2.0 = FM intensa — muchos parciales, timbre complejo y agresivo.
     *
     * En FM estricta: I (índice) determina el número de parciales significativos.
     * Por la regla de Bessel: I=1 → ~3 pares de parciales significativos.
     *
     * @param depth Índice FM [0.0, 2.0].
     */
    void setFmDepth(float depth);

    /**
     * @brief Waveform de la portadora.
     *
     * AudioSynthWaveformModulated soporta WAVEFORM_SINE (para FM clásico),
     * WAVEFORM_SAWTOOTH, WAVEFORM_SQUARE, WAVEFORM_TRIANGLE.
     * El FM más "correcto" usa WAVEFORM_SINE para portadora y modulador.
     *
     * @param w Constante Teensy Audio Library.
     */
    void setCarrierWaveform(uint16_t w);

    /**
     * @brief Waveform del modulador.
     *
     * @param w Constante Teensy Audio Library.
     */
    void setModWaveform(uint16_t w);

    /**
     * @brief Frecuencia de corte del SVF de todas las voces.
     *
     * @param hz Frecuencia de corte en Hz [500, 20000].
     */
    void setFilterCutoff(float hz);

    /**
     * @brief Resonancia del SVF de todas las voces.
     *
     * @param q Resonancia: 0.7 = Butterworth flat, 3.0 = énfasis notable.
     */
    void setFilterResonance(float q);

    /**
     * @brief Cantidad de modulación del envelope sobre el cutoff.
     *
     * El envelope agrega hasta filterEnvAmount × 8000 Hz al cutoff base.
     * Rango más amplio que otros engines porque el FM ya es brillante por naturaleza.
     *
     * @param amount Modulación [0.0, 1.0].
     */
    void setFilterEnvAmount(float amount);

    /** @brief Attack del envelope (VCF + VCA) en ms. */
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
     * _finalMix combina las 6 voces (vía _masterMixA + _masterMixB).
     * El sketch conecta este nodo a g_engine_mix_b ch0.
     *
     * @return Referencia al _finalMix (output port 0 = mono mix de 6 voces).
     */
    AudioStream& getOutput() { return _finalMix; }

private:
    // ── Audio objects — 6 voces estáticas (Teensy Audio Library oficial) ─────────
    // AudioSynthWaveformModulated: oscilador con entrada de modulación de frecuencia.
    // Port 0 = audio input para FM. frequencyModulation() define la sensibilidad en octaves.
    AudioSynthWaveformModulated _mod[DX7_VOICES];   // modulador FM (alimenta la portadora)
    AudioSynthWaveformModulated _car[DX7_VOICES];   // portadora (recibe FM del modulador)
    // Filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013) — código custom.
    // 6 instancias independientes (una por voz). API compatible con AudioFilterStateVariable.
    MoogLadder4P                _filter[DX7_VOICES];
    AudioEffectEnvelope         _ampEnv[DX7_VOICES];

    // Master mix: 6 voces → 2 AudioMixer4 (mismo patrón que OB-6)
    // _masterMixA: voces 0–3 (canales 0–3)
    // _masterMixB: voces 4–5 (canales 0–1, canales 2–3 vacíos)
    // _finalMix:   _masterMixA (ch0) + _masterMixB (ch1)
    AudioMixer4 _masterMixA;
    AudioMixer4 _masterMixB;
    AudioMixer4 _finalMix;

    // ── Conexiones — 26 individuales (AudioConnection no tiene copy ctor) ─────────
    // Por voz (3 conexiones × 6 voces = 18):
    //   mod→car(FM port 0), car→filter, filter(LP port 0)→ampEnv
    // Master (8 conexiones):
    //   ampEnv[0-3]→masterMixA, ampEnv[4-5]→masterMixB, mixA→final, mixB→final
    AudioConnection _c0,  _c1,  _c2;   // voz 0
    AudioConnection _c3,  _c4,  _c5;   // voz 1
    AudioConnection _c6,  _c7,  _c8;   // voz 2
    AudioConnection _c9,  _c10, _c11;  // voz 3
    AudioConnection _c12, _c13, _c14;  // voz 4
    AudioConnection _c15, _c16, _c17;  // voz 5
    AudioConnection _c18, _c19, _c20, _c21;  // ampEnv[0-3] → masterMixA
    AudioConnection _c22, _c23;              // ampEnv[4-5] → masterMixB
    AudioConnection _c24, _c25;             // mixA→final, mixB→final

    // ── Voice state ───────────────────────────────────────────────────────────────
    struct VoiceState {
        uint8_t  midi_note = 255;   // 255 = libre (valor inválido en MIDI)
        bool     active    = false;
        uint32_t age       = 0;     // para voice stealing (menor age = más antigua)
    };
    VoiceState _voices[DX7_VOICES];
    uint32_t   _ageCounter = 0;

    // ── FM params ─────────────────────────────────────────────────────────────────
    float    _fmRatio       = 2.0f;    // ratio 2.0 = octava arriba → timbre "bell" clásico DX7
    float    _fmDepth       = 0.5f;    // índice FM moderado
    uint16_t _carrierWave   = WAVEFORM_SINE;
    uint16_t _modWave       = WAVEFORM_SINE;

    // ── Filter params ─────────────────────────────────────────────────────────────
    float _filterCutoff    = 15000.0f;  // alto por defecto: FM ya es brillante, SVF add calidez suave
    float _filterResonance = 0.7f;
    float _filterEnvAmt    = 0.2f;

    // ── Filter envelope por voz (software FSM — misma arquitectura que OB-6) ─────
    float    _filterEnvLevel[DX7_VOICES]    = {};
    float    _filterEnvAttackMs   = 5.0f;
    float    _filterEnvDecayMs    = 200.0f;
    float    _filterEnvSustain    = 0.8f;
    float    _filterEnvReleaseMs  = 300.0f;

    enum class EnvStage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    EnvStage _filterEnvStage[DX7_VOICES]   = {};
    uint32_t _filterEnvStageMs[DX7_VOICES] = {};

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

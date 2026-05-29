#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013)
#include "../audio/moog_ladder.h"

static constexpr uint8_t PROPHET_VOICES = 5;

/**
 * @brief Motor de síntesis Sequential Circuits Prophet-5 para GrooveForge Brain.
 *
 * 5 voces polifónicas, cada una con 2 VCOs + VCF + VCA envelope.
 * Signal flow por voz:
 *   VCO-A ──┐
 *           ├──→ VoiceMixer ──→ VCF ──→ VCA ──→ MasterMixer ──→ Output
 *   VCO-B ──┘
 *
 * Voice stealing: FIFO — se roba la voz con menor age counter cuando todas están
 * activas. Ver theory doc §"Voice Allocation".
 *
 * Cross-modulation (Poly-Mod): VCO-B modula la frecuencia de VCO-A en software.
 * Aproximación FM analógico — no FM exacto. Ver theory doc §"Cross-Modulation".
 *
 * CPU estimado: ~3% con 5 voces activas (01-architecture.md §4.1 budget: ~30%).
 * AudioMemory: 72 bloques = 18.4KB (budget: 400KB).
 *
 * Theory completa: apps/docs/sprints/07-prophet-5.md
 * Refs: apps/docs/01-architecture.md §4.1
 *       apps/docs/05-fx-architecture.md §2
 */
class Prophet5 {
public:
    Prophet5();

    /**
     * @brief Inicializa el codec SGTL5000 y el grafo de audio de 5 voces.
     *
     * Debe llamarse una vez en setup(), antes de cualquier noteOn.
     * Llama AudioMemory(72) internamente — no llamar AudioMemory() desde el sketch.
     *
     * @param volume Volumen del codec, 0.0–1.0 (default 0.5)
     */
    void begin(float volume = 0.5f);

    /**
     * @brief Dispara una nota polifónica.
     *
     * Busca una voz libre. Si no hay voces libres, roba la voz con menor age
     * (FIFO stealing). Si la nota ya está sonando, hace re-trigger en la misma voz.
     *
     * @param midi_note Nota MIDI (0–127). A4 = 69.
     * @param velocity  Velocidad normalizada 0.0–1.0 (reservado — no implementado).
     */
    void noteOn(uint8_t midi_note, float velocity = 1.0f);

    /**
     * @brief Suelta una nota. Solo afecta la voz que tiene asignada esa nota MIDI.
     *
     * @param midi_note Nota MIDI a soltar (0–127).
     */
    void noteOff(uint8_t midi_note);

    /**
     * @brief Waveform para VCO-A de todas las voces.
     *
     * @param waveform Constante Teensy Audio Library: WAVEFORM_SAWTOOTH,
     *                 WAVEFORM_SQUARE, WAVEFORM_SINE, etc.
     */
    void setOscAWaveform(uint16_t waveform);

    /**
     * @brief Waveform para VCO-B de todas las voces.
     *
     * @param waveform Constante Teensy Audio Library.
     */
    void setOscBWaveform(uint16_t waveform);

    /**
     * @brief Intervalo de VCO-B relativo a VCO-A en semitones.
     *
     * 0 = unísono, 12 = octava arriba, 7 = quinta justa, -12 = octava abajo.
     * El intervalo se aplica a todas las voces simultáneamente.
     *
     * @param semitones Intervalo en semitones (-24 a +24).
     */
    void setOscBInterval(float semitones);

    /**
     * @brief Cantidad de cross-modulation de VCO-B sobre VCO-A (Poly-Mod).
     *
     * 0.0 = ninguna modulación (sonido puro dos VCOs).
     * 0.3 = modulación moderada (carácter FM sutil).
     * 1.0 = modulación máxima (sonidos metálicos/campana inarmónicos).
     *
     * Aproximación FM analógico — ver theory doc §"Cross-Modulation".
     *
     * @param amount Profundidad de crossmod, 0.0–1.0.
     */
    void setCrossmod(float amount);

    /**
     * @brief Mix entre VCO-A y VCO-B en cada voz.
     *
     * @param oscA Gain de VCO-A (0.0–1.0)
     * @param oscB Gain de VCO-B (0.0–1.0)
     */
    void setOscMix(float oscA, float oscB);

    /**
     * @brief Frecuencia de corte del VCF digital de todas las voces.
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

    /** @brief Attack del envelope (VCF + VCA comparten el mismo envelope) en ms. */
    void setAttack(float ms);
    /** @brief Decay del envelope en ms. */
    void setDecay(float ms);
    /** @brief Sustain del envelope, 0.0–1.0. */
    void setSustain(float level);
    /** @brief Release del envelope en ms. */
    void setRelease(float ms);

    /**
     * @brief Actualiza el estado del engine — debe llamarse desde loop().
     *
     * Procesa por cada voz activa:
     *  1. Filter envelope software (ATTACK/DECAY/SUSTAIN/RELEASE)
     *  2. Cutoff del VCF con modulación de envelope aplicada
     *  3. Cross-modulation: fase de VCO-B → modulación de frecuencia de VCO-A
     *
     * Resolución temporal: granularidad de loop() (~0.5–2ms).
     */
    void update();

    /**
     * @brief Retorna el último nodo del grafo del engine — fuente para mezcla externa.
     *
     * El `_finalMix` ya combina las 5 voces (vía masterMixA + masterMixB). Sprint 36:
     * main.cpp lo conecta a un AudioMixer4 compartido que enruta al singleton I2S.
     *
     * @return Referencia al `_finalMix` (output port 0 = mono mix de 5 voces).
     */
    AudioStream& getOutput() { return _finalMix; }

private:
    // ── Audio objects — 5 voces estáticas (Teensy Audio Library oficial) ─────────
    AudioSynthWaveform       _oscA[PROPHET_VOICES];
    AudioSynthWaveform       _oscB[PROPHET_VOICES];
    AudioMixer4              _voiceMix[PROPHET_VOICES];    // oscA (ch0) + oscB (ch1)
    // Filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013) — código custom.
    // 5 instancias independientes (una por voz). API compatible con AudioFilterStateVariable.
    MoogLadder4P             _vcf[PROPHET_VOICES];
    AudioEffectEnvelope      _vcaEnv[PROPHET_VOICES];

    // Master mix: AudioMixer4 tiene 4 entradas → 2 mixers para 5 voces
    // masterMixA: voces 0–3 (canales 0–3)
    // masterMixB: voz 4 (canal 0), canales 1–3 vacíos
    // finalMix:   masterMixA (ch0) + masterMixB (ch1) — SIN salida propia
    AudioMixer4              _masterMixA;
    AudioMixer4              _masterMixB;
    AudioMixer4              _finalMix;
    // AudioOutputI2S y AudioControlSGTL5000 eliminados — son singleton de hardware.
    // Sprint 32 conectará _finalMix al mixer compartido del sketch.

    // ── Conexiones — 27 individuales (AudioConnection no tiene copy ctor) ─────────
    // Por voz: oscA→voiceMix(0), oscB→voiceMix(1), voiceMix→vcf, vcf→vcaEnv (×5 = 20)
    AudioConnection _c0,  _c1,  _c2,  _c3;   // voz 0
    AudioConnection _c4,  _c5,  _c6,  _c7;   // voz 1
    AudioConnection _c8,  _c9,  _c10, _c11;  // voz 2
    AudioConnection _c12, _c13, _c14, _c15;  // voz 3
    AudioConnection _c16, _c17, _c18, _c19;  // voz 4
    // vcaEnv → masterMix (5)
    AudioConnection _c20, _c21, _c22, _c23;  // voces 0–3 → masterMixA
    AudioConnection _c24;                     // voz 4 → masterMixB
    // master path (2) — _c27/_c28 eliminados (no hay _out propio)
    AudioConnection _c25, _c26;              // masterMixA→finalMix, masterMixB→finalMix

    // ── Voice state ───────────────────────────────────────────────────────────────
    struct VoiceState {
        uint8_t  midi_note = 255;   // 255 = libre (valor inválido en MIDI, rango 0–127)
        bool     active    = false;
        uint32_t age       = 0;     // para voice stealing (menor age = voz más antigua)
    };
    VoiceState _voices[PROPHET_VOICES];
    uint32_t   _ageCounter = 0;

    // ── Parámetros globales ───────────────────────────────────────────────────────
    float _filterCutoff   = 2000.0f;
    float _filterEnvAmt   = 0.4f;
    float _filterResonance = 0.7f;
    float _oscBSemitones  = 0.0f;
    float _crossmodAmt    = 0.0f;
    float _oscAGain       = 0.5f;
    float _oscBGain       = 0.5f;

    // ── Filter envelope por voz (software — misma arquitectura que MoogModelD) ───
    float    _filterEnvLevel[PROPHET_VOICES]    = {};
    float    _filterEnvAttackMs  = 10.0f;
    float    _filterEnvDecayMs   = 300.0f;
    float    _filterEnvSustain   = 0.7f;
    float    _filterEnvReleaseMs = 500.0f;

    enum class EnvStage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    EnvStage _filterEnvStage[PROPHET_VOICES]    = {};
    uint32_t _filterEnvStageMs[PROPHET_VOICES]  = {};

    // ── Cross-modulation — fase acumulada por voz ─────────────────────────────────
    // Aproximación FM: acumulamos la fase del oscilador-B y la usamos como modulador.
    // No es FM exacto — ver theory doc §"Cross-Modulation" para la justificación.
    float    _crossmodPhase[PROPHET_VOICES] = {};

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

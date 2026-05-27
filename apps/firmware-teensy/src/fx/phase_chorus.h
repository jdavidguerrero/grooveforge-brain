#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>

/**
 * @brief FX Phase Chorus — Sprint 2.4.
 *
 * Modela chorus analógico estilo BOSS CE-2/Roland Juno-106 con:
 *   - Saturación pre-chorus (AudioEffectWaveshaper, tanh drive=1.5 fijo)
 *   - Núcleo de chorus con 1–4 voces (AudioEffectChorus, buffer 1200 samples)
 *   - LFO caótico compuesto de tres senoides inarmónicas que modulan el nivel wet
 *   - Dry/wet mixer paralelo
 *
 * Signal flow:
 *   inputStream ──┬──→ [AudioEffectWaveshaper pre-sat] → [AudioEffectChorus] → dryWetMix(ch1)
 *                 └──────────────────────────────────────────────────────────→ dryWetMix(ch0)
 *                                                                                    ↓
 *                                                                            AudioOutputI2S L+R
 *
 * El dry path toma la señal ANTES del pre-sat: depth=0 reproduce el engine sin coloración.
 *
 * CPU estimado: ~4.7% adicional sobre el engine (05-fx-architecture.md §1.8)
 * AudioMemory: 20 bloques recomendados — el sketch debe llamar AudioMemory() ANTES de begin().
 *
 * Theory completa: apps/docs/sprints/09-phase-chorus.md
 * Refs: apps/docs/05-fx-architecture.md §1.8
 *       apps/docs/01-architecture.md §5.2
 *       Zölzer, "DAFX" (2011), §2.6 "Chorus"
 */
class PhaseChorus {
public:
    PhaseChorus();

    /**
     * @brief Conecta el stream de entrada. El sketch maneja AudioMemory y el codec.
     *
     * Crea las AudioConnections dinámicas hacia inputStream (la fuente no se conoce
     * en tiempo de compilación — viene del engine o mixer activo).
     * NO llama AudioMemory() ni codec.enable() — responsabilidad del sketch.
     *
     * IMPORTANTE: AudioEffectChorus.begin() se llama aquí. El sketch debe haber
     * llamado AudioMemory() ANTES de llamar a este método — si AudioMemory() no fue
     * llamado, los punteros internos del chorus apuntan a memoria no inicializada.
     *
     * @param inputStream Stream de audio de entrada (ej: AudioMixer4 del engine).
     */
    void begin(AudioStream& inputStream);

    /**
     * @brief Retorna el último nodo del grafo de audio — canal izquierdo.
     *
     * @return Referencia al AudioMixer4 de salida (output port 0).
     */
    AudioStream& getOutputL() { return _dryWetMix; }

    /**
     * @brief Retorna el último nodo del grafo de audio — canal derecho.
     *
     * PhaseChorus es mono-interno: ambos canales salen del mismo nodo (port 0).
     *
     * @return Referencia al AudioMixer4 de salida (output port 0).
     */
    AudioStream& getOutputR() { return _dryWetMix; }

    /**
     * @brief Velocidad del LFO de chorus.
     *
     * 0.1 Hz = movimiento muy lento (pad de larga duración).
     * 0.5 Hz = chorus Juno-106 clásico.
     * 5.0 Hz = quasi-vibrato rápido (flutter-chorus).
     * 10.0 Hz = límite superior — arriba de 5 Hz empieza a sonar como trémolo.
     *
     * @param hz Rate del LFO en Hz [0.1, 10.0].
     */
    void setRate(float hz);

    /**
     * @brief Profundidad del efecto chorus (nivel base del canal wet).
     *
     * 0.0 = sin efecto (señal seca completa).
     * 0.7 = valor nominal — chorus claramente audible con mezcla natural.
     * 1.0 = wet máximo.
     *
     * @param depth Profundidad [0.0, 1.0].
     */
    void setDepth(float depth);

    /**
     * @brief Caos del LFO — desvía la regularidad perfecta del seno.
     *
     * Activa dos senoides secundarias con frecuencias inarmónicas (×0.743 y ×1.329)
     * que suman al LFO principal. Los ratios irracionales hacen que el patrón combinado
     * no se repita en escalas de tiempo musicalmente relevantes.
     * El resultado perceptual es el "breathing" orgánico del CE-2 real.
     *
     * 0.0 = LFO sinusoidal puro (mecánico, periódico).
     * 0.3 = valor nominal — respiración sutil, no obvia.
     * 1.0 = drift máximo — claramente irregular, muy orgánico.
     *
     * Ref: apps/docs/sprints/09-phase-chorus.md §"LFO drift caótico"
     * Ref: Zölzer, "DAFX" (2011), §2.6.2 — LFO shapes y su impacto perceptual.
     *
     * @param drift Caos [0.0, 1.0].
     */
    void setDrift(float drift);

    /**
     * @brief Número de voces del chorus.
     *
     * Cada voz es un tap independiente del buffer de delay con su propio sub-LFO.
     * 1 = vibrato sutil (Juno-60).
     * 2 = BOSS CE-2 estándar.
     * 3 = Roland Dimension D Small.
     * 4 = Roland Dimension D Large — ensemble denso.
     *
     * El cambio aplica un fade de 10ms en el canal wet antes de reorganizar
     * los taps internos del AudioEffectChorus, para evitar clicks audibles.
     *
     * @param v Voces [1, 4].
     */
    void setVoices(uint8_t v);

    /**
     * @brief Nivel wet adicional global (multiplica sobre el depth base).
     *
     * Permite escalar la mezcla total dry/wet independientemente del depth.
     * 0.0 = solo señal seca.
     * 1.0 = mezcla completa según depth.
     *
     * @param wet Nivel de mezcla [0.0, 1.0].
     */
    void setMix(float wet);

    /**
     * @brief Activa o desactiva el bypass del FX.
     *
     * bypass=true: dry al 100%, wet silenciado. Preserva _mix para restaurar.
     * bypass=false: restaura gain(0)=1.0-_mix, gain(1)=_mix.
     *
     * @param bypass true = bypass activo (señal limpia), false = FX activo.
     */
    void setBypass(bool bypass);

    /**
     * @brief Avanza el LFO drift y actualiza la ganancia wet. Llamar desde loop().
     *
     * Calcula el LFO compuesto (1–3 senoides según drift) y aplica la modulación
     * de nivel wet en dryWetMix. No corre en el ISR de audio — la granularidad
     * de loop() es suficiente para un LFO sub-audio de 0.1–10 Hz.
     *
     * @param dt_ms Tiempo transcurrido desde la última llamada, en milisegundos.
     */
    void update(float dt_ms);

private:
    // ── Audio objects internos (Teensy Audio Library oficial) ─────────────────────
    AudioEffectWaveshaper _preSat;      // saturación suave pre-chorus (código custom: tabla tanh)
    AudioEffectChorus     _chorus;      // núcleo del efecto — código oficial PJRC
    AudioMixer4           _dryWetMix;   // último nodo — expuesto via getOutputL/R()

    // ── Buffer de delay del chorus (RAM estática, no usa AudioMemory pool) ─────────
    // 1200 samples × 2 bytes = 2400 bytes ≈ 2.3 KB
    // 1200 = 25ms @ 48kHz: headroom suficiente para LFO modulando entre 3–20ms.
    // No es potencia de 2 porque AudioEffectChorus no usa módulo que se beneficie de ello.
    // Ref: apps/docs/sprints/09-phase-chorus.md §"AudioEffectChorus — API y config del buffer"
    static constexpr int CHORUS_BUF_LEN = 1200;
    short _chorusBuf[CHORUS_BUF_LEN];

    // ── Conexiones estáticas (inicializadas en lista del constructor) ──────────────
    AudioConnection _cPreSatChorus;  // _preSat    → _chorus     (wet path)
    AudioConnection _cChorusWet;     // _chorus    → _dryWetMix(ch1)

    // ── Conexiones dinámicas (creadas en begin(), dependen de inputStream) ─────────
    AudioConnection* _cIn1 = nullptr;   // inputStream → _preSat (wet path)
    AudioConnection* _cIn2 = nullptr;   // inputStream → _dryWetMix ch0 (dry path)

    // ── Tabla del waveshaper pre-sat (código custom) ──────────────────────────────
    // Drive fijo 1.5: prácticamente lineal, solo colorea con 3er armónico ~1-3%.
    // Ref: apps/docs/sprints/09-phase-chorus.md §"Saturación pre-chorus"
    float _preSatTable[257];

    // ── Estado LFO drift (tres fases para las tres senoides inarmónicas) ──────────
    float _lfoPhase1 = 0.0f;   // senoide principal a _rate Hz
    float _lfoPhase2 = 0.0f;   // senoide a _rate × 0.743 Hz (≈ razón áurea invertida)
    float _lfoPhase3 = 0.0f;   // senoide a _rate × 1.329 Hz (ajustado para "respiración")
    float _lfoOut    = 0.0f;   // valor actual del LFO combinado (informativo)

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    float   _rate   = 0.5f;
    float   _depth  = 0.7f;
    float   _drift  = 0.3f;
    float   _mix    = 0.7f;
    bool    _bypass = false;
    uint8_t _voices = 2;

    // TWO_PI está definido como macro en wiring.h de Teensyduino — se usa CHORUS_TWO_PI
    // para evitar la redefinición de símbolo que causaría error de compilación.
    static constexpr float CHORUS_TWO_PI = 6.28318530f;

    /**
     * @brief Genera la tabla tanh del waveshaper pre-sat con el drive dado.
     *
     * f(x) = tanh(drive * x) / tanh(drive), normalizada a [-1.0, 1.0].
     * Con drive=1.5, la desviación de linealidad a x=0.7 es ~1.3 dB —
     * suficiente para colorear con tercer armónico, insuficiente para distorsionar.
     *
     * Ref: Smith, "Intro to Digital Filters" (W3K, 2007), Cap. 8 "Nonlinear Filters".
     *
     * @param drive Factor de compresión. Para pre-sat: 1.5 fijo.
     */
    void _buildPreSatTable(float drive);
};

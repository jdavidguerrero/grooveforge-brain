#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>

/**
 * @brief FX Bit Sculpt — Sprint 2.5.
 *
 * Bitcrusher con dither configurable: convierte la distorsión armónica de la
 * cuantización en ruido estadísticamente decorrelacionado (Sculpt=0 → RPDF,
 * Sculpt=0.5 → TPDF, Sculpt=1.0 → TPDF + noise shaping de primer orden).
 *
 * Signal flow:
 *   inputStream ──┬──→ ditherMix(ch0)
 *                 │         ↑
 *                 │    noise(ch1, nivel = 1 LSB × sculpt_amount)
 *                 │         ↓
 *                 │   [AudioEffectBitcrusher] ← bits + sampleRate
 *                 │         ↓
 *                 │    dryWetMix(ch1) ← wet
 *                 └──→ dryWetMix(ch0) ← dry (sin dither, sin crusher)
 *                           ↓
 *                    AudioOutputI2S L+R
 *
 * El dry toma la señal ANTES de ditherMix: Mix=0.0 reproduce el engine sin artefactos.
 *
 * CPU estimado: ~3.6% adicional sobre el engine (apps/docs/sprints/10-bit-sculpt.md §CPU)
 * AudioMemory: begin() llama AudioMemory(20) — suficiente para 3 oscs + bit sculpt.
 *
 * Theory completa: apps/docs/sprints/10-bit-sculpt.md
 * Refs: apps/docs/05-fx-architecture.md §1.6
 *       apps/docs/01-architecture.md §5.2
 *       Pohlmann, "Principles of Digital Audio" (6th ed.), Cap. 4
 *       Zölzer, "DAFX" (2011), §7.1–7.2
 */
class BitSculpt {
public:
    BitSculpt();

    /**
     * @brief Conecta el stream de entrada e inicializa el codec SGTL5000.
     *
     * Crea las AudioConnections dinámicas hacia inputStream. Debe llamarse
     * una vez en setup(), después de configurar la fuente de audio.
     * Llama AudioMemory(20) internamente — no llamar AudioMemory() desde el sketch.
     *
     * @param inputStream Stream de audio de entrada (ej: AudioMixer4 del engine).
     * @param volume      Volumen del codec SGTL5000, 0.0–1.0 (default 0.5).
     */
    void begin(AudioStream& inputStream, float volume = 0.5f);

    /**
     * @brief Resolución de cuantización en bits.
     *
     * 16 = resolución CD, sin distorsión perceptible.
     * 12 = SP-1200/Akai S612 — "warm lo-fi".
     * 8  = Sound Blaster / Game Boy con sample rate reducido.
     * 4  = crunch agresivo, distorsión armónica muy audible.
     * 1  = onda cuadrada total — solo dos niveles (+1 / -1).
     *
     * Recalcula el nivel de dither (1 LSB depende de bits) vía update().
     *
     * @param bits Resolución [1, 16].
     */
    void setBits(uint8_t bits);

    /**
     * @brief Frecuencia de sample-and-hold (reducción de sample rate).
     *
     * 48000 Hz = sin reducción (solo bitcrusher actúa).
     * 26000 Hz ≈ E-mu SP-1200.
     * 22050 Hz = Sound Blaster 1.0.
     * 8000  Hz = Game Boy DMG — aliasing inarmónico muy audible.
     * 1000  Hz = filtro de teléfono + aliasing extremo.
     *
     * @param hz Sample rate objetivo [1000.0, 48000.0] Hz.
     */
    void setSampleRate(float hz);

    /**
     * @brief Carácter del dither — selecciona tipo y cantidad de ruido pre-crusher.
     *
     * 0.0 → sin dither (RPDF mínimo): distorsión armónica pura. "Digital feo".
     * 0.5 → TPDF: decorrelación completa de segundo orden. Estándar de mastering CD.
     * 1.0 → TPDF + noise shaping primer orden: ruido concentrado en altas frecuencias.
     *        Más "musical" a baja resolución — oído menos sensible a >8kHz.
     *
     * Ref: Pohlmann, "Principles of Digital Audio" (6th ed.), Cap. 4, pp. 115–130.
     *
     * @param sculpt Carácter de dither [0.0, 1.0].
     */
    void setSculpt(float sculpt);

    /**
     * @brief Mezcla dry/wet del bitcrusher.
     *
     * 0.0 = señal 100% seca (sin crusher).
     * 0.7 = 70% wet — carácter lo-fi con 30% de señal limpia preservando transientes.
     * 1.0 = 100% bitcrushed — sin punch del dry.
     *
     * @param wet Nivel wet [0.0, 1.0].
     */
    void setMix(float wet);

    /**
     * @brief Activa o desactiva el bypass del FX.
     *
     * bypass=true: dry al 100%, wet y dither silenciados.
     * bypass=false: restaura ganancia seca/húmeda según _mix y actualiza dither.
     *
     * @param bypass true = bypass activo (señal limpia), false = FX activo.
     */
    void setBypass(bool bypass);

    /**
     * @brief Actualiza el nivel de dither noise y el noise shaping. Llamar desde loop().
     *
     * Calcula el nivel de ruido en ditherMix(ch1) basado en Sculpt y bits actuales:
     *   - Sculpt ∈ [0.0, 0.5]: blend RPDF → TPDF (nivel sube de 0 a 1 LSB)
     *   - Sculpt ∈ [0.5, 1.0]: TPDF + noise shaping de primer orden (aproximación
     *     de baja velocidad — no sample-accurate, suficiente para carácter perceptual)
     *
     * No corre en el ISR de audio — la granularidad de loop() (~1ms) es suficiente
     * para actualizar el nivel de dither, que varía en escala de tiempo de parámetro.
     *
     * Ref: apps/docs/sprints/10-bit-sculpt.md §"Actualización del nivel de dither en loop()"
     */
    void update();

private:
    // ── Audio objects internos (Teensy Audio Library oficial) ─────────────────────
    AudioSynthNoiseWhite  _noise;       // fuente de dither (RPDF blanco)
    AudioMixer4           _ditherMix;   // ch0=señal, ch1=noise → bitcrusher
    AudioEffectBitcrusher _crusher;     // cuantización de amplitud + sample-and-hold
    AudioMixer4           _dryWetMix;   // ch0=dry, ch1=wet
    AudioOutputI2S        _out;
    AudioControlSGTL5000  _codec;

    // ── Conexiones estáticas (inicializadas en lista del constructor) ──────────────
    AudioConnection _cNoiseDither;  // _noise     → _ditherMix(ch1)
    AudioConnection _cDitherCrush;  // _ditherMix → _crusher
    AudioConnection _cCrushWet;     // _crusher   → _dryWetMix(ch1)
    AudioConnection _cMixL;         // _dryWetMix → _out(ch0, L)
    AudioConnection _cMixR;         // _dryWetMix → _out(ch1, R)

    // ── Conexiones dinámicas (creadas en begin(), dependen de inputStream) ─────────
    AudioConnection* _cIn1 = nullptr;   // inputStream → _ditherMix(ch0) — wet path
    AudioConnection* _cIn2 = nullptr;   // inputStream → _dryWetMix(ch0) — dry path

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    uint8_t _bits       = 8;
    float   _sampleRate = 22050.0f;
    float   _sculpt     = 0.3f;
    float   _mix        = 0.7f;
    bool    _bypass     = false;

    // Nivel de ruido de dither calculado en update().
    // Aproximación del error para noise shaping de primer orden (no sample-accurate).
    float _ditherLevel      = 0.0f;
    float _lastErrorApprox  = 0.0f;   // feedback del error para Sculpt > 0.5
};

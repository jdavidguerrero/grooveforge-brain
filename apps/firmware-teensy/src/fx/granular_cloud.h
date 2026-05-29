#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
#include <stdint.h>

/**
 * @brief FX Granular Cloud — Sprint 2.9.
 *
 * Granulizador basado en AudioEffectGranular (Teensy Audio Library oficial).
 * Dos modos de operación:
 *
 *   Passthrough (freeze=false): el audio se procesa en tiempo real como granos.
 *     Cada grano es una ventana corta del input, repetida y solapada para crear
 *     la nube de sonido. La duración y pitch de los granos son controlables.
 *
 *   Freeze (freeze=true): captura el audio en el buffer y lo convierte en nube
 *     estática — texturas ambient que se sostienen indefinidamente sin input nuevo.
 *     La nota física de Chion: "el tiempo se detiene, el sonido se convierte en
 *     espacio" — Granular Cloud en freeze mode.
 *
 * Buffer interno: 12800 int16_t ≈ 25600 bytes ≈ 267ms a 48kHz.
 * El buffer es static (definido en .cpp) — vive en RAM de Teensy (1MB disponible).
 *
 * Signal flow:
 *   inputStream ──→ _granular (AudioEffectGranular) ──→ _dryWetMix(ch1) [wet]
 *   inputStream ──────────────────────────────────────→ _dryWetMix(ch0) [dry]
 *                        _dryWetMix ──→ _out L+R
 *
 * CPU estimado: ~12% (apps/docs/05-fx-architecture.md §1.2)
 * AudioMemory: begin() llama AudioMemory(30).
 *
 * Theory: apps/docs/sprints/14-granular-cloud.md (pendiente)
 * Refs: apps/docs/05-fx-architecture.md §1.2
 *       Roads, "Microsound" (MIT Press, 2001) — teoría de síntesis granular
 *       Truax, "Composing with Time-Shifted and Transposed Sound" (1988) — freeze granular
 *       Xenakis, "Formalized Music" (Pendragon Press, 1992) — densidad de nube de granos
 */
class GranularCloud {
public:

    /// Tamaño del buffer granular en muestras int16_t.
    /// 12800 muestras × 2 bytes = 25600 bytes ≈ 267ms a AUDIO_SAMPLE_RATE_EXACT (48kHz).
    /// Elegido para caber en DMAMEM de Teensy 4.1 sin comprometer el tensor arena de TinyML.
    /// Ref: apps/docs/04-ai-architecture.md §1.2 — budget de RAM
    static constexpr int GRANULAR_MEMORY_SIZE = 12800;

    GranularCloud();
    ~GranularCloud();

    /**
     * @brief Conecta el stream de entrada. El sketch maneja AudioMemory y el codec.
     *
     * Llama _granular.begin() con el buffer estático y GRANULAR_MEMORY_SIZE.
     * Crea 2 AudioConnections dinámicas: inputStream→_granular e inputStream→dry.
     * NO llama AudioMemory() ni codec.enable() — responsabilidad del sketch.
     *
     * @param inputStream Stream de audio de entrada.
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
     * GranularCloud es mono-interno: ambos canales salen del mismo nodo (port 0).
     *
     * @return Referencia al AudioMixer4 de salida (output port 0).
     */
    AudioStream& getOutputR() { return _dryWetMix; }

    /**
     * @brief Duración de cada grano en milisegundos.
     *
     * Granos cortos (5–30ms): textura densa, pitch definido.
     * Granos medios (50–100ms): nube característica, pitch difuso.
     * Granos largos (150–250ms): texturas lentas, casi sin pitch aparente.
     *
     * Ref: Roads "Microsound" (2001) Cap. 2 §"Grain Duration"
     *
     * @param ms Duración del grano [5.0, 250.0] ms.
     */
    void setGrainSize(float ms);

    /**
     * @brief Ratio de velocidad/pitch de reproducción de granos.
     *
     * 1.0 → pitch original.
     * 0.5 → una octava abajo (reproducción lenta).
     * 2.0 → una octava arriba (reproducción rápida).
     * Valores < 0.25 o > 4.0 producen artefactos de granularización severos —
     * clampeados en el setter.
     *
     * Solo activo en modo passthrough (freeze=false).
     *
     * @param ratio Speed/pitch ratio [0.25, 4.0].
     */
    void setSpeed(float ratio);

    /**
     * @brief Activa o desactiva el modo freeze.
     *
     * freeze=true:  captura el audio actual en el buffer y crea la nube estática.
     *               El input ya no afecta el output — textura ambient sostenida.
     * freeze=false: vuelve al modo passthrough — granulización en tiempo real.
     *
     * @param f true = freeze activo.
     */
    void setFreeze(bool f);

    /**
     * @brief Balance dry/wet.
     *
     * @param wet Nivel wet [0.0, 1.0].
     */
    void setMix(float wet);

    /**
     * @brief Activa o desactiva el bypass del FX.
     *
     * @param bypass true = bypass activo.
     */
    void setBypass(bool bypass);

    /**
     * @brief No-op en v1.0. Reservado para análisis espectral en v2.0.
     *
     * En v2.0 (Fase 4) se integrará con TinyML para ajustar density/pitch de granos
     * según el contenido espectral del input (apps/docs/04-ai-architecture.md §"FX AI").
     */
    void update();

private:
    // ── Audio objects (Teensy Audio Library oficial) ──────────────────────────────
    AudioEffectGranular  _granular;    ///< granulizador built-in de Teensy Audio Library
    AudioMixer4          _dryWetMix;   ///< último nodo — expuesto via getOutputL/R()

    // ── Conexiones estáticas ──────────────────────────────────────────────────────
    AudioConnection _cGranWet;   ///< _granular → _dryWetMix(ch1)

    // ── Conexiones dinámicas ──────────────────────────────────────────────────────
    AudioConnection* _cIn;    ///< inputStream → _granular
    AudioConnection* _cDry;   ///< inputStream → _dryWetMix(ch0)

    // ── Buffer granular estático ──────────────────────────────────────────────────
    // Declarado en .cpp como miembro estático de la clase para que el linker lo
    // ubique en DMAMEM (memoria de acceso directo por DMA en Teensy 4.1).
    // DMAMEM no reduce la RAM general de 1MB — es una sección DMA-accessible separada.
    static int16_t _granularBuffer[GRANULAR_MEMORY_SIZE];

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    float _grainSize = 100.0f;   ///< duración del grano [5.0, 250.0] ms
    float _speed     = 1.0f;     ///< speed/pitch ratio [0.25, 4.0]
    bool  _freeze    = false;    ///< true = freeze mode
    float _mix       = 0.5f;     ///< wet level [0.0, 1.0]
    bool  _bypass    = false;
};

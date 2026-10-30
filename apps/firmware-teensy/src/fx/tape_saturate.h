#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>

#include "../audio/tape_stage.h"
#include "../dsp/tape_modulation.h"

/**
 * @brief FX Tape Saturate — Master Layer, Sprint 2.3.
 *
 * Modela saturación de cinta magnética con (rev. 2026-10-27, theory/fase3-fx.md, TAPE):
 *   - tanh(d·x)/√d en coma flotante (dsp::TapeCore): sin la tabla int16 ni los SVF en punto fijo, que metían ruido
 *     con cada nota
 *   - bloqueador de DC (~20 Hz) y pasabajos de un polo para la edad de la cinta, con el corte suavizado (sin zipper)
 *   - mezcla seco / mojado dentro del mismo objeto
 *   - LFOs de Wow y Flutter que modulan pitch (calculados en loop(), no en ISR)
 *
 * Signal flow:
 *   input → [AudioTapeStage: tanh → DC block → LP edad · seco / mojado] → salida
 *
 * CPU estimado: ~3.7% adicional sobre el engine (05-fx-architecture.md §1.5)
 * AudioMemory: 80 bloques recomendados con Prophet-5 — el sketch es responsable de llamar AudioMemory().
 *
 * Theory completa: apps/docs/sprints/08-tape-saturate.md
 * Refs: apps/docs/05-fx-architecture.md §1.5
 *       apps/docs/01-architecture.md §5.2
 */
class TapeSaturate {
public:
    TapeSaturate();
    ~TapeSaturate();

    /**
     * @brief Conecta el stream de entrada. El sketch maneja AudioMemory y el codec.
     *
     * Crea las AudioConnections dinámicas hacia inputStream (el FX no puede
     * conocer la fuente en tiempo de compilación — viene del engine activo).
     * Debe llamarse una vez en setup(), después de configurar la fuente de audio.
     * NO llama AudioMemory() ni codec.enable() — responsabilidad del sketch.
     *
     * @param inputStream Stream de audio de entrada (ej: AudioMixer4 del engine).
     */
    void begin(AudioStream& inputStream);

    /**
     * @brief Retorna el último nodo del grafo de audio — canal izquierdo.
     *
     * @return Referencia al AudioMixer4 de salida (output port 0).
     */
    AudioStream& getOutputL() { return _stage; }

    /**
     * @brief Retorna el último nodo del grafo de audio — canal derecho.
     *
     * TapeSaturate es mono-interno: ambos canales salen del mismo nodo (port 0).
     *
     * @return Referencia al AudioMixer4 de salida (output port 0).
     */
    AudioStream& getOutputR() { return _stage; }

    /**
     * @brief Drive de saturación — controla la compresión de la curva tanh.
     *
     * 1.0 = saturación mínima (casi lineal).
     * 10.0 = saturación agresiva (casi hard-clip).
     * Regenera la tabla del waveshaper en cada llamada — safe desde loop().
     *
     * @param drive Valor en rango [1.0, 10.0].
     */
    void setDrive(float drive);

    /**
     * @brief Profundidad del LFO de Wow (variaciones lentas de pitch, ~0.5 Hz).
     *
     * Modela irregularidades del motor y excentricidad del eje de la casetera.
     * El factor resultante se aplica vía getWowFlutterMod() a los osciladores del engine.
     *
     * @param depth Profundidad normalizada [0.0, 1.0]. depth=1.0 → ±5 cents de variación.
     */
    void setWow(float depth);

    /**
     * @brief Profundidad del LFO de Flutter (variaciones rápidas de pitch, ~6 Hz).
     *
     * Modela resonancias mecánicas del capstan y del pinch roller.
     *
     * @param depth Profundidad normalizada [0.0, 1.0]. depth=1.0 → ±3 cents de variación.
     */
    void setFlutter(float depth);

    /**
     * @brief Grado de envejecimiento de la cinta — controla el LP rolloff de alta frecuencia.
     *
     * 0.0 = cinta nueva (f_corte ≈ 20 kHz, sin rolloff audible).
     * 1.0 = cinta muy desgastada (f_corte ≈ 6 kHz, carácter vintage marcado).
     * Escala logarítmica: f_corte = 20000 × 0.3^amount.
     *
     * @param amount Envejecimiento [0.0, 1.0].
     */
    void setAge(float amount);

    /**
     * @brief Mezcla dry/wet del FX.
     *
     * 0.0 = señal 100% seca (sin saturación).
     * 1.0 = señal 100% saturada (solo wet).
     * Valores intermedios hacen parallel compression de saturación.
     *
     * @param wet Nivel wet [0.0, 1.0].
     */
    void setMix(float wet);

    /**
     * @brief Activa o desactiva el bypass del FX.
     *
     * En bypass=true, la señal dry pasa directamente al output sin saturación.
     * Preserva el estado de _mix para restaurar al desactivar.
     *
     * @param bypass true = bypass activo (señal limpia), false = FX activo.
     */
    void setBypass(bool bypass);

    /**
     * @brief Avanza los LFOs de Wow y Flutter. Llamar desde loop().
     *
     * Actualiza las fases de los osciladores de Wow (0.5 Hz) y Flutter (6 Hz)
     * con dt_ms como delta de tiempo. No corre en el ISR de audio — la granularidad
     * de loop() (~1-5ms) es suficiente para LFOs sub-audio.
     *
     * Suma dos senoides ligeramente desincronizadas por LFO para romper la
     * periodicidad perfecta y aproximar la naturaleza irregular de la cinta real.
     *
     * @param dt_ms Tiempo transcurrido desde la última llamada, en milisegundos.
     */
    void update(float dt_ms);

    /**
     * @brief Factor de modulación de pitch actual (Wow + Flutter combinados).
     *
     * Aplicar a las frecuencias de los osciladores del engine:
     *   f_modulada = f_nota * (1.0 + getWowFlutterMod())
     *
     * Rango típico: ±0.003 (≈ ±5 cents con wow=1.0 y flutter=0.0).
     *
     * @return Factor multiplicativo de desviación de pitch (dimensionless).
     */
    float getWowFlutterMod() const;

private:
    AudioTapeStage _stage;               // último nodo — expuesto via getOutputL/R()
    AudioConnection* _cIn = nullptr;     // inputStream → _stage (creada en begin())
    float _lpApplied = -1.0f;            ///< último corte enviado (el stage lo suaviza por muestra)

    // ── Estado LFO Wow/Flutter ────────────────────────────────────────────────────
    dsp::TapeModulation _modulation;
    float _wowDepth      = 0.0f;
    float _flutterDepth  = 0.0f;
    float _currentMod    = 0.0f;    // factor combinado Wow+Flutter, leído por getWowFlutterMod()

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    float _mix       = 0.7f;
    float _ageAmount = 0.0f;
    bool  _bypass    = false;

};

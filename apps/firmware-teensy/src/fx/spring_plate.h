#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>

/**
 * @brief FX Spring + Plate — Sprint 2.10.
 *
 * Dual reverb con dos algoritmos modelados en base a AudioEffectFreeverb (Schroeder):
 *
 *   Spring tank (algorithm=0): decay corto (~1.5s), damping agresivo (HF rolloff
 *     rápido), carácter "wobbly" que caracteriza los spring reverbs de Hammond y Fender.
 *     roomsize=0.4, damping=0.7.
 *
 *   Plate reverb (algorithm=1): decay largo (~3.5s), damping suave, difusión homogénea
 *     sin el carácter tonal del spring. Characteristic del EMT 140 plate vintage.
 *     roomsize=0.75, damping=0.3.
 *
 *   Blend (algorithm=2): crossfade continuo entre spring y plate. El usuario puede
 *     definir roomsize y damping propios, aplicados a ambos algoritmos simultáneamente.
 *
 * Nota sobre AudioEffectFreeverb: implementa el algoritmo de Schroeder (1962) con
 * mejoras de Moorer — 8 filtros comb paralelos + 4 all-pass en serie. Es una aproximación
 * estadística de reverb (no modal), pero da excelente relación CPU/calidad.
 * Ref: Freeverb por Jezar (1999) — open source, base de la implementación de Teensy.
 *      Parker, "Efficient Simulation of Spring Reverb" (DAFX 2011).
 *
 * Signal flow:
 *   inputStream ──┬──→ _spring (Freeverb, spring tuning) ──→ _blendMix(ch0)
 *                 └──→ _plate  (Freeverb, plate tuning)  ──→ _blendMix(ch1)
 *                              _blendMix ─────────────────→ _dryWetMix(ch1) [wet]
 *   inputStream ──────────────────────────────────────────→ _dryWetMix(ch0) [dry]
 *                         _dryWetMix ──→ _out L+R
 *
 * CPU estimado: ~10% (apps/docs/05-fx-architecture.md §1.10)
 * AudioMemory: begin() llama AudioMemory(30).
 *
 * Theory: apps/docs/sprints/15-spring-plate.md (pendiente)
 * Refs: apps/docs/05-fx-architecture.md §1.10
 *       Parker, "Efficient Simulation of Spring Reverb" (DAFX 2011, Paris)
 *       Schroeder, "Natural Sounding Artificial Reverberation" (JAES, 1962)
 *       Freeverb source by Jezar at Dreampoint (1999) — base del AudioEffectFreeverb
 */
class SpringPlate {
public:

    SpringPlate();

    /**
     * @brief Conecta el stream de entrada. El sketch maneja AudioMemory y el codec.
     *
     * Crea 3 AudioConnections dinámicas: inputStream→_spring, inputStream→_plate,
     * inputStream→_dryWetMix(dry). NO llama AudioMemory() ni codec.enable().
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
     * SpringPlate es mono-interno: ambos canales salen del mismo nodo (port 0).
     *
     * @return Referencia al AudioMixer4 de salida (output port 0).
     */
    AudioStream& getOutputR() { return _dryWetMix; }

    /**
     * @brief Selecciona el algoritmo de reverb.
     *
     * 0 = Spring: roomsize=0.4, damping=0.7 — vintage spring tank.
     * 1 = Plate:  roomsize=0.75, damping=0.3 — EMT 140 plate style.
     * 2 = Blend:  crossfade entre spring y plate con los parámetros de _roomSize y
     *             _damping definidos por setRoomSize() y setDamping(). El blend entre
     *             ambos lo controla setBlend().
     *
     * @param alg Algoritmo [0, 2].
     */
    void setAlgorithm(uint8_t alg);

    /**
     * @brief Tamaño de sala (solo activo cuando algorithm=2 blend).
     *
     * Aplica a ambos procesadores Freeverb cuando algorithm=2.
     * Valores presets en algorithm=0/1 los sobreescribe setAlgorithm().
     *
     * @param size Room size [0.0, 1.0].
     */
    void setRoomSize(float size);

    /**
     * @brief Damping de altas frecuencias (solo activo cuando algorithm=2 blend).
     *
     * 0.0 → sin damping, HF se sostiene como el input.
     * 1.0 → máximo damping, HF muere rápido (sonido oscuro/pantanoso).
     *
     * @param damp Damping [0.0, 1.0].
     */
    void setDamping(float damp);

    /**
     * @brief Crossfade entre spring y plate (solo activo cuando algorithm=2).
     *
     * 0.0 → solo spring (gain(0)=1.0, gain(1)=0.0).
     * 0.5 → blend 50/50 — suma de ambas reverbs.
     * 1.0 → solo plate (gain(0)=0.0, gain(1)=1.0).
     *
     * @param blend Blend ratio [0.0, 1.0].
     */
    void setBlend(float blend);

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
     * @brief No-op en v1.0. Reservado para pre-delay en v2.0.
     */
    void update();

private:
    // ── Audio objects (Teensy Audio Library oficial) ──────────────────────────────
    AudioEffectFreeverb  _spring;     ///< reverb spring: roomsize=0.4, damping=0.7
    AudioEffectFreeverb  _plate;      ///< reverb plate: roomsize=0.75, damping=0.3
    AudioMixer4          _blendMix;   ///< crossfade spring/plate
    AudioMixer4          _dryWetMix;  ///< último nodo — expuesto via getOutputL/R()

    // ── Conexiones estáticas ──────────────────────────────────────────────────────
    AudioConnection _cSpringBlend;   ///< _spring → _blendMix(ch0)
    AudioConnection _cPlateBlend;    ///< _plate  → _blendMix(ch1)
    AudioConnection _cBlendWet;      ///< _blendMix → _dryWetMix(ch1)

    // ── Conexiones dinámicas ──────────────────────────────────────────────────────
    AudioConnection* _cSpring;   ///< inputStream → _spring
    AudioConnection* _cPlate;    ///< inputStream → _plate
    AudioConnection* _cDry;      ///< inputStream → _dryWetMix(ch0)

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    uint8_t _algorithm = 0;     ///< 0=spring, 1=plate, 2=blend
    float   _roomSize  = 0.5f;  ///< [0.0, 1.0] — usado en blend mode
    float   _damping   = 0.5f;  ///< [0.0, 1.0] — usado en blend mode
    float   _blend     = 0.5f;  ///< [0.0, 1.0] — 0=spring, 1=plate (blend mode)
    float   _mix       = 0.5f;  ///< wet level [0.0, 1.0]
    bool    _bypass    = false;

    /**
     * @brief Aplica los parámetros actuales de blend a ambos Freeverb y actualiza gains.
     */
    void _applyBlend();
};

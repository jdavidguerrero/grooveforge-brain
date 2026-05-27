#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
#include <math.h>

/**
 * @brief FX Cymatic Resonator — Sprint 2.8.
 *
 * Cuatro filtros bandpass en paralelo, tuneados a ratios 1:2:3:5 sobre BASE_FREQ.
 * Un LFO senoidal lento modula el tune de todos los filtros en ±5% para dar
 * el movimiento "cristalino" que caracteriza al sonido cimático.
 *
 * El nombre viene de "cymatics" — el estudio de los patrones de Chladni, figuras
 * que emergen en superficies vibrantes cubiertas de arena. Los ratios 1:2:3:5
 * son pseudo-armónicos (no enteros exactos como 1:2:4:8) que producen interferencia
 * espectral interesante — distintos de una simple serie de Fourier.
 *
 * Arquitectura:
 *   - 4× AudioFilterBiquad en paralelo (BP resonante, Q = _resonance)
 *   - AudioMixer4 suma los 4 modos con control de densidad
 *   - LFO software en update() modula tune ±5% via setBandpass()
 *
 * Signal flow:
 *   inputStream ──┬──→ _mode[0] (BP, 1.0×_tune×BASE_FREQ, Q) → _modeMix(ch0)
 *                 ├──→ _mode[1] (BP, 2.0×_tune×BASE_FREQ, Q) → _modeMix(ch1)
 *                 ├──→ _mode[2] (BP, 3.0×_tune×BASE_FREQ, Q) → _modeMix(ch2)
 *                 └──→ _mode[3] (BP, 5.0×_tune×BASE_FREQ, Q) → _modeMix(ch3)
 *                          _modeMix ──────────────────────────→ _dryWetMix(ch1) [wet]
 *   inputStream ────────────────────────────────────────────→ _dryWetMix(ch0) [dry]
 *                          _dryWetMix ──→ _out L+R
 *
 * CPU estimado: ~8% (apps/docs/05-fx-architecture.md §1.1)
 * AudioMemory: begin() llama AudioMemory(30).
 *
 * Theory: apps/docs/sprints/13-cymatic-resonator.md (pendiente de creación)
 * Refs: apps/docs/05-fx-architecture.md §1.1
 *       apps/docs/01-architecture.md §5.2
 *       Chladni, "Entdeckungen über die Theorie des Klanges" (1787) — patrones modales
 *       Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011), §3.4 (bandpass resonators)
 */
class CymaticResonator {
public:

    CymaticResonator();

    /**
     * @brief Conecta el stream de entrada. El sketch maneja AudioMemory y el codec.
     *
     * Crea 5 AudioConnections dinámicas: 4 fan-out a los modos BP + 1 al dry path.
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
     * CymaticResonator es mono-interno: ambos canales salen del mismo nodo (port 0).
     *
     * @return Referencia al AudioMixer4 de salida (output port 0).
     */
    AudioStream& getOutputR() { return _dryWetMix; }

    /**
     * @brief Ratio de sintonía global sobre BASE_FREQ.
     *
     * Escala todas las frecuencias de modo proporcionalmente.
     * 1.0 → frecuencias en La4 (440 Hz base).
     * 0.5 → una octava abajo (220 Hz base).
     * 2.0 → una octava arriba (880 Hz base).
     *
     * @param tune Ratio de tune [0.5, 2.0].
     */
    void setTune(float tune);

    /**
     * @brief Número de modos resonantes activos.
     *
     * Los modos 0..(density-1) están activos. Los modos restantes se silencian.
     * density=1 → solo la resonancia fundamental (440 Hz × tune).
     * density=4 → todos los modos activos — máxima densidad cromática.
     *
     * @param density Modos activos [1, 4].
     */
    void setDensity(uint8_t density);

    /**
     * @brief Factor Q de todos los filtros bandpass.
     *
     * Q determina el ancho de banda: BW = freq / Q.
     * Q=1   → BW=261Hz @261Hz — muy ancho, coloración suave como caja de resonancia.
     * Q=5   → BW=52Hz @261Hz  — default: resonancia audible sin efecto "filtro".
     * Q=15  → BW=17Hz @261Hz  — estrecho, picos tónicos, carácter cristalino.
     * Q=30  → BW=9Hz @261Hz   — muy estrecho, cuasi-tonal, peligro de inestabilidad.
     *
     * Nota: AudioFilterBiquad en float32 puede ser inestable con Q>100 a frecuencias
     * bajas. Clampeado a 30 como máximo práctico.
     * Ref: Zölzer "DAFX" (2011), §3.2 — estabilidad numérica de biquad resonante.
     *
     * @param q Factor Q [1.0, 30.0].
     */
    void setResonance(float q);

    /**
     * @brief Frecuencia del LFO de modulación de tune.
     *
     * El LFO modula tune en ±5% sinusoidalmente para dar movimiento "cristalino".
     * 0.1 Hz → movimiento muy lento, casi estático — ideal para drone largo.
     * 0.5 Hz → ciclo de 2 segundos — respiración sutil (default).
     * 3.0 Hz → modulación rápida, efecto vibrante más obvio.
     *
     * @param hz Frecuencia del LFO [0.1, 3.0] Hz.
     */
    void setLfoRate(float hz);

    /**
     * @brief Balance dry/wet.
     *
     * 0.0 → solo dry (sin efecto).
     * 0.7 → dominancia del resonador — sweet spot (default).
     * 1.0 → solo wet (resonancias puras, sin dry).
     *
     * @param wet Nivel wet [0.0, 1.0].
     */
    void setMix(float wet);

    /**
     * @brief Activa o desactiva el bypass del FX.
     *
     * bypass=true:  silencia el wet, dry pasa limpio.
     * bypass=false: restaura _mix en el canal wet.
     *
     * @param bypass true = bypass activo.
     */
    void setBypass(bool bypass);

    /**
     * @brief Actualiza el LFO y aplica la modulación de tune. Llamar desde loop().
     *
     * Avanza _lfoPhase según _lfoRate y el tiempo transcurrido desde la última llamada.
     * Calcula lfoMod = 1.0 + 0.05 × sin(_lfoPhase) y actualiza setBandpass() en cada
     * modo activo. Thread-safe: setBandpass() en AudioFilterBiquad toma efecto al
     * inicio del próximo bloque de audio (128 muestras).
     */
    void update();

private:
    // ── Audio objects (Teensy Audio Library oficial) ──────────────────────────────
    AudioFilterBiquad    _mode[4];      ///< 4 filtros BP en paralelo
    AudioMixer4          _modeMix;      ///< suma de los 4 modos
    AudioMixer4          _dryWetMix;    ///< último nodo — expuesto via getOutputL/R()

    // ── Conexiones estáticas (inicializadas en lista del constructor) ─────────────
    // _mode[0..3] → _modeMix(ch0..3)  — 4 conexiones
    // _modeMix → _dryWetMix(ch1)      — wet path
    // Total: 5 conexiones estáticas
    AudioConnection _cMode0Mix;   ///< _mode[0] → _modeMix(ch0)
    AudioConnection _cMode1Mix;   ///< _mode[1] → _modeMix(ch1)
    AudioConnection _cMode2Mix;   ///< _mode[2] → _modeMix(ch2)
    AudioConnection _cMode3Mix;   ///< _mode[3] → _modeMix(ch3)
    AudioConnection _cMixWet;     ///< _modeMix → _dryWetMix(ch1)

    // ── Conexiones dinámicas (creadas en begin()) ─────────────────────────────────
    // Fan-out desde inputStream: una conexión por modo + una al dry.
    AudioConnection* _cMode[4];   ///< inputStream → _mode[n]
    AudioConnection* _cDry;       ///< inputStream → _dryWetMix(ch0)

    // ── Constantes de ratios modales ──────────────────────────────────────────────
    // Ratios 1:2:3:5 — pseudo-armónicos que generan interferencia espectral compleja.
    // No son la serie de Fourier (1:2:4:8) ni la serie de Fibonacci estricta (1:1:2:3:5).
    // El 5 en lugar del 4 introduce una inharmonicidad que da el carácter "cristalino".
    //
    // BASE_FREQ = C4 (261.63 Hz) — alineado con el chord de demo (C4+E4+G4).
    // Con ratios 1:2:3:5: modos en 261, 523, 784, 1308 Hz → coincide con fundamentales
    // y armónicos del acorde C mayor. tune=1.0 = default útil, no arbitrario.
    static constexpr float BASE_FREQ    = 261.63f;
    static constexpr float RATIOS[4]    = {1.0f, 2.0f, 3.0f, 5.0f};

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    float    _tune         = 1.0f;    ///< ratio de tune [0.5, 2.0]
    uint8_t  _density      = 4;       ///< modos activos [1, 4]
    float    _resonance    = 30.0f;   ///< Q de los filtros BP [10.0, 80.0]
    float    _lfoRate      = 0.5f;    ///< frecuencia LFO [0.1, 3.0] Hz
    float    _lfoPhase     = 0.0f;    ///< fase LFO acumulada [0, 2π]
    float    _mix          = 0.7f;    ///< wet level [0.0, 1.0]
    bool     _bypass       = false;
    uint32_t _lastUpdateMs = 0;

    /**
     * @brief Aplica setBandpass() con tune×lfoMod a todos los filtros activos.
     *
     * @param lfoMod Modificador del LFO, nominal 1.0 ± 0.05.
     */
    void _applyFilters(float lfoMod = 1.0f);

    /**
     * @brief Ajusta las ganancias del _modeMix según _density.
     *
     * Modos 0..(density-1): gain = 1.0.
     * Modos density..3:     gain = 0.0.
     * La normalización 1/density en _modeMix ch0 se hace aquí para evitar
     * clipping cuando hay 4 modos activos sumándose.
     */
    void _updateMixerGains();
};

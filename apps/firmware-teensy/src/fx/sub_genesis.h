#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>

/**
 * @brief FX Sub Genesis — Sprint 2.6.
 *
 * Generador de sub-octava independiente: sintetiza una senoide (o cuadrada) una
 * o dos octavas por debajo de la frecuencia raíz del patch activo, la satura con
 * un waveshaper tanh para generar armónicos perceptibles (tono virtual), la filtra
 * con un LP para enfocar el rango de sub-bass, y la mezcla SUMADA al dry path.
 *
 * A diferencia de un dry/wet tradicional, el dry siempre permanece al 100%.
 * Mix controla cuánto sub se suma sobre el chord original — nunca se pierde la
 * señal del engine.
 *
 * Signal flow:
 *   [AudioSynthWaveform _subOsc]  ← freq = rootFreq / 2^octaves
 *            ↓
 *   [AudioEffectWaveshaper _subSat]  ← tanh, drive 1.0–8.0
 *            ↓
 *   [AudioFilterStateVariable _subLP]  ← LP cutoff 40–200Hz
 *            ↓
 *   [AudioMixer4 _subMix]  ← sub a nivel _subLevel en ch0
 *            ↓
 *   [AudioMixer4 _dryWetMix]
 *     ch0 ← inputStream (dry, siempre 1.0)
 *     ch1 ← _subMix output (wet = sub, nivel = _mix)
 *            ↓
 *   AudioOutputI2S L+R
 *
 * CPU estimado: ~3.5% adicional sobre el engine (apps/docs/sprints/11-sub-genesis.md §CPU)
 * AudioMemory: 20 bloques recomendados — el sketch es responsable de llamar AudioMemory().
 *
 * Decisión de diseño: el sub-oscilador es independiente del input stream.
 * No se hace pitch tracking de audio (latencia inaceptable en live). La frecuencia
 * raíz se fija desde el sketch en Fase 2; en Fase 4 la provee el noteOn MIDI.
 * Ref: apps/docs/sprints/11-sub-genesis.md §"Por qué el Sub Genesis NO implementa
 *       pitch tracking de audio en v1.0"
 *
 * Theory completa: apps/docs/sprints/11-sub-genesis.md
 * Refs: apps/docs/05-fx-architecture.md §1.12
 *       apps/docs/01-architecture.md §5.2
 *       Zölzer, "DAFX" (2011), §5.4 (Harmonic Exciter)
 *       Boss OC-2 Service Manual (Roland, 1982)
 *       Fastl & Zwicker, "Psychoacoustics" (3rd ed., Springer, 2007), Cap. 8
 */
class SubGenesis {
public:
    SubGenesis();

    /**
     * @brief Conecta el stream de entrada. El sketch maneja AudioMemory y el codec.
     *
     * Crea la AudioConnection dinámica desde inputStream hacia el dry path.
     * Debe llamarse una vez en setup(), después de configurar la fuente de audio.
     * NO llama AudioMemory() ni codec.enable() — responsabilidad del sketch.
     *
     * @param inputStream Stream de audio de entrada (ej: AudioMixer4 del engine chord).
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
     * SubGenesis es mono-interno: ambos canales salen del mismo nodo (port 0).
     *
     * @return Referencia al AudioMixer4 de salida (output port 0).
     */
    AudioStream& getOutputR() { return _dryWetMix; }

    /**
     * @brief Frecuencia raíz del patch activo.
     *
     * El sub-oscilador corre a rootFreq / 2^octaves. En Fase 2 se fija estáticamente
     * desde el sketch. En Fase 4 (Sprint 4.1) se llama desde noteOn MIDI.
     *
     * @param hz Frecuencia raíz [20.0, 1000.0] Hz.
     */
    void setRootFreq(float hz);

    /**
     * @brief Número de octavas hacia abajo.
     *
     * 1 → f_sub = rootFreq / 2   (una octava abajo — rango 65–500 Hz típico)
     * 2 → f_sub = rootFreq / 4   (dos octavas abajo — sub-bass profundo, 32–250 Hz)
     *
     * Ref: apps/docs/sprints/11-sub-genesis.md §"Sub-octave synthesis"
     *
     * @param oct Octavas [1, 2].
     */
    void setOctaves(uint8_t oct);

    /**
     * @brief Ganancia del sub-oscilador antes del waveshaper.
     *
     * Controla la amplitud de entrada al waveshaper — influye directamente en la
     * cantidad de saturación (a mayor nivel, más armónicos generados con el mismo drive).
     * En UI: ENC R en FX mode per Panel spec §3.4.
     *
     * @param level Nivel [0.0, 1.0].
     */
    void setSubLevel(float level);

    /**
     * @brief Drive del waveshaper tanh del sub.
     *
     * 1.0 → prácticamente lineal (<1% THD): sub casi sinusoidal puro.
     * 2.0 → saturación suave (~3-5% THD): armónicos de 3er orden audibles.
     * 4.0 → saturación marcada (~15-20% THD): sub presente en cualquier sistema.
     * 8.0 → saturación agresiva (>40% THD): sub próximo a onda cuadrada.
     *
     * Regenera la tabla tanh de 257 puntos (~50-100 µs en Teensy 4.1 @ 600 MHz).
     * Ref: Zölzer, "DAFX" (2011), §5.1–5.2; apps/docs/sprints/11-sub-genesis.md §saturación
     *
     * @param drive Drive [1.0, 8.0].
     */
    void setDrive(float drive);

    /**
     * @brief Cutoff del filtro LP post-saturación del sub.
     *
     * Limita los armónicos generados por la saturación y define el carácter del sub:
     *   40–80 Hz  → sub puro (fundamental casi sola), dependiente de subwoofer físico.
     *   80–120 Hz → balance entre fundamental y primeros armónicos — sweet spot típico.
     *  120–200 Hz → más armónicos pasan: sub "gordo", funciona bien en cualquier sistema.
     *
     * @param hz Cutoff [40.0, 200.0] Hz.
     */
    void setCutoff(float hz);

    /**
     * @brief Nivel del sub sumado sobre el dry.
     *
     * Dry siempre permanece al 100%. Mix controla cuánto sub se añade encima del
     * chord original — no reemplaza el dry (comportamiento aditivo, no sustitutivo).
     *
     * 0.0 → solo chord, sin sub.
     * 0.5 → sub sumado al chord — sweet spot para dub/techno.
     * 1.0 → sub al nivel máximo sobre el chord completo.
     *
     * @param wet Nivel del sub [0.0, 1.0].
     */
    void setMix(float wet);

    /**
     * @brief Activa o desactiva el bypass del FX.
     *
     * bypass=true:  silencia el sub (gain(1)=0.0), dry al 100% — señal limpia.
     * bypass=false: restaura _mix en el canal wet del sub.
     *
     * @param bypass true = bypass activo, false = FX activo.
     */
    void setBypass(bool bypass);

    /**
     * @brief Forma de onda del sub-oscilador.
     *
     * WAVEFORM_SINE   → sub puro, armónicos controlados solo por el drive.
     *                   Ideal para dub, sub-bass limpio (default).
     * WAVEFORM_SQUARE → sub con armónicos impares propios (3f, 5f, 7f...).
     *                   Más agresivo y presente en medios-bajos — útil para techno.
     *
     * Ref: apps/docs/sprints/11-sub-genesis.md §"Waveform del sub"
     *
     * @param wave WAVEFORM_SINE o WAVEFORM_SQUARE (constantes de Teensy Audio Library).
     */
    void setWaveform(uint16_t wave);

private:
    // ── Audio objects internos (Teensy Audio Library oficial) ─────────────────────
    AudioSynthWaveform        _subOsc;     // oscilador sub independiente del input
    AudioEffectWaveshaper     _subSat;     // saturación tanh — añade armónicos al sub
    AudioFilterStateVariable  _subLP;      // LP post-saturación: enfoca el sub-bass
    AudioMixer4               _subMix;     // nivel del sub antes del blend (ch0 activo)
    AudioMixer4               _dryWetMix;  // último nodo — expuesto via getOutputL/R()

    // ── Conexiones estáticas (inicializadas en lista del constructor) ──────────────
    AudioConnection _cSubOscSat;   // _subOsc    → _subSat
    AudioConnection _cSubSatLP;    // _subSat    → _subLP
    AudioConnection _cSubLPMix;    // _subLP(LP) → _subMix(ch0)
    AudioConnection _cSubMixWet;   // _subMix    → _dryWetMix(ch1)

    // ── Conexión dinámica (creada en begin(), depende de inputStream) ──────────────
    // Solo una: inputStream → _dryWetMix(ch0) — dry path directo al blend.
    AudioConnection* _cIn = nullptr;

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    float    _rootFreq = 130.81f;    // C3 (frecuencia raíz del patch, fijada desde sketch)
    uint8_t  _octaves  = 1;          // octavas abajo: f_sub = rootFreq / 2^octaves
    float    _subLevel = 0.8f;       // ganancia del sub antes del waveshaper
    float    _drive    = 2.0f;       // drive del waveshaper tanh [1.0, 8.0]
    float    _cutoff   = 100.0f;     // cutoff del LP post-saturación [40.0, 200.0] Hz
    float    _mix      = 0.5f;       // nivel del sub sobre el dry [0.0, 1.0]
    bool     _bypass   = false;
    uint16_t _waveform = WAVEFORM_SINE;

    // Tabla de 257 puntos para el waveshaper tanh — regenerada en begin() y setDrive().
    // Mapea entrada [-1.0, 1.0] a salida normalizada por tanhf(drive).
    // Ref: apps/docs/sprints/11-sub-genesis.md §"AudioEffectWaveshaper"
    float _subSatTable[257];

    /**
     * @brief Genera la tabla de lookup tanh normalizada para el waveshaper.
     *
     * satTable[i] = tanhf(drive * x) / tanhf(drive), x ∈ [-1.0, 1.0]
     * La normalización garantiza que la ganancia máxima de salida sea ≈ 1.0
     * independientemente del drive. Se aplica con _subSat.shape() al terminar.
     *
     * @param drive Drive del waveshaper [1.0, 8.0].
     */
    void _buildSubSatTable(float drive);
};

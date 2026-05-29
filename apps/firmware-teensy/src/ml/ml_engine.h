#pragma once
// ml_engine.h — Orquestador de inferencia ML Layer 1 (Sprint 32 Batch B)
//
// Ejecuta KeyDetector, ChordRecognizer y BeatFollower secuencialmente
// sobre un pitch class histogram acumulativo. Cadencia: 500ms.
//
// Diseño de cadencia:
//   tick() se llama en cada iteración de loop(). Solo corre inferencia cuando
//   han transcurrido INFERENCE_INTERVAL_MS desde la última ejecución.
//   El histograma de pitch classes se acumula indefinidamente — no se resetea
//   entre inferencias. Esto provee continuidad tonal a lo largo de la sesión
//   y evita oscilaciones por silencios breves.
//
// Cambios detectados:
//   tick() retorna un bitmask (KEY_CHANGED | CHORD_CHANGED | BEAT_CHANGED).
//   Solo se emite un flag cuando el resultado difiere del anterior, reduciendo
//   el tráfico en el bridge UART.
//
// Uso en sketch 28:
//   MLEngine g_ml;
//   g_ml.init();                          // setup()
//   g_ml.on_onset(millis());              // midi_note_on
//   g_ml.update_pitch(note);             // midi_note_on (nota original, pre-snap)
//   uint8_t ch = g_ml.tick(now);          // loop() cuando g_in_ai_mode
//   if (ch & MLEngine::KEY_CHANGED)   bridge_send_ml_key(g_ml.last_key());
//   if (ch & MLEngine::CHORD_CHANGED) bridge_send_ml_chord(g_ml.last_chord());
//   if (ch & MLEngine::BEAT_CHANGED)  bridge_send_ml_beat(g_ml.last_beat());

#include <stdint.h>
#include "key_detector.h"
#include "chord_recognizer.h"
#include "beat_follower.h"

/**
 * @brief Orquestador de inferencia ML Layer 1.
 *
 * Agrega los tres modelos (Key, Chord, Beat) bajo una sola cadencia de 500ms.
 * Maneja el histograma de pitch classes y la detección de cambios entre ciclos.
 */
class MLEngine {
public:
    /** Bitmask de cambios retornado por tick(). */
    static constexpr uint8_t KEY_CHANGED   = 0x01;
    static constexpr uint8_t CHORD_CHANGED = 0x02;
    static constexpr uint8_t BEAT_CHANGED  = 0x04;

    /** Intervalo entre inferencias completas (ms). */
    static constexpr uint32_t INFERENCE_INTERVAL_MS = 500;

    MLEngine();

    /**
     * @brief Inicializa los tres modelos TFLite Micro.
     *
     * Llama init() en KeyDetector, ChordRecognizer y BeatFollower.
     * Loguea los bytes de arena usados por cada modelo.
     *
     * @return true solo si los tres modelos inicializaron correctamente.
     */
    bool init();

    /**
     * @brief Registra un onset para el BeatFollower.
     *
     * Llamar en cada midi_note_on con el timestamp de millis().
     *
     * @param now_ms  Timestamp actual en milisegundos (millis()).
     */
    void on_onset(uint32_t now_ms);

    /**
     * @brief Acumula una nota en el histograma de pitch classes y en el EMA de actividad.
     *
     * Usa la nota original (pre-snap, pre-transpose) para que el histograma
     * refleje lo que el músico intenta tocar, no lo que el engine produce.
     * La velocidad incrementa _pitch_activity[pc] saturado a 1024 antes del EMA.
     *
     * @param midi_note  Nota MIDI [0-127].
     * @param velocity   Velocidad MIDI [0-127]. Default 80 si no se provee.
     */
    void update_pitch(uint8_t midi_note, uint8_t velocity = 80);

    /**
     * @brief Retorna la actividad de un pitch class como uint8 [0-255].
     *
     * La actividad interna es un float con EMA tau=2s. Esta función la mapea
     * linealmente desde [0, 1024] a [0, 255] con clamp.
     *
     * @param pc  Pitch class [0-11]: C=0, C#=1, ..., B=11.
     * @return    Actividad normalizada [0-255].
     */
    uint8_t get_pitch_activity_u8(uint8_t pc) const;

    /**
     * @brief Corre el ciclo de inferencia si ha transcurrido INFERENCE_INTERVAL_MS.
     *
     * Secuencia interna:
     *   1. Normalizar _pitch_count[] → _pitch_hist[]
     *   2. KeyDetector::inference(_pitch_hist, ...)
     *   3. ChordRecognizer::inference(_pitch_hist, ...)
     *   4. BeatFollower::infer()
     *
     * Retorna 0 inmediatamente si el intervalo no ha vencido o si !_initialized.
     *
     * @param now_ms  Timestamp actual en milisegundos (millis()).
     * @return Bitmask de KEY_CHANGED | CHORD_CHANGED | BEAT_CHANGED.
     */
    uint8_t tick(uint32_t now_ms);

    /** @brief Último resultado de tonalidad. Válido solo si tick() retornó KEY_CHANGED. */
    const KeyResult&   last_key()   const { return _key_result; }

    /** @brief Último resultado de acorde. Válido solo si tick() retornó CHORD_CHANGED. */
    const ChordResult& last_chord() const { return _chord_result; }

    /** @brief Último resultado de tempo. Válido solo si tick() retornó BEAT_CHANGED. */
    const BeatResult&  last_beat()  const { return _beat_result; }

    /** @brief Retorna true si init() completó con éxito. */
    bool is_ready() const { return _initialized; }

private:
    KeyDetector    _key;
    ChordRecognizer _chord;
    BeatFollower   _beat;

    /** Contador de apariciones por pitch class [0-11]. Nunca se resetea. */
    uint32_t _pitch_count[12];

    /** Histograma normalizado para inferencia. Recomputado en cada tick(). */
    float _pitch_hist[12];

    /**
     * Actividad EMA por pitch class [0-11].
     * Unidades: acumulado de velocity / decay. Rango efectivo 0 a ~1024.
     * Decay aplicado en tick() con tau=2s.
     * Referencia: apps/docs/sprints/33-ai-hero-viz.md §Theory/EMA.
     */
    float _pitch_activity[12];

    /** Timestamp del último decay EMA aplicado sobre _pitch_activity[]. */
    uint32_t _last_decay_ms;

    uint32_t   _last_inference_ms;

    KeyResult   _key_result;
    ChordResult _chord_result;
    BeatResult  _beat_result;

    /** Índice de tonalidad del ciclo anterior; 0xFF = sin resultado previo. */
    uint8_t  _prev_key_index;

    /** Índice de acorde del ciclo anterior; 0xFF = sin resultado previo. */
    uint8_t  _prev_chord_index;

    /** BPM redondeado del ciclo anterior; 0 = sin resultado previo. */
    uint16_t _prev_bpm_int;

    bool _initialized;

    /**
     * @brief Normaliza _pitch_count[] en _pitch_hist[].
     *
     * @return false si la suma de _pitch_count[] es 0 (sin notas acumuladas).
     */
    bool _compute_histogram();
};

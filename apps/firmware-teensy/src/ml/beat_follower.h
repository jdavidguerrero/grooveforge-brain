#pragma once
// beat_follower.h — Detección de tempo (BPM) en Teensy 4.1
//
// Arquitectura híbrida: algoritmo determinístico + modelo ML
//
//   Algoritmo (on_onset + _compute_ioi_histogram):
//     - Buffer circular de los últimos 32 onsets (timestamps de NoteOn)
//     - Calcula 31 IOIs (Inter-Onset Intervals) en milisegundos
//     - Convierte a histograma de 32 bins sobre el rango 50-2000ms
//
//   Modelo ML (infer):
//     - Dense 3-layer INT8 (32→64→32→32)
//     - Input: histograma IOI normalizado (32 floats)
//     - Output: distribución de probabilidad sobre 32 clases de BPM
//     - Clases: 60, 63, 66, ..., 240 BPM (espaciadas logarítmicamente)
//
// Por qué híbrido y no solo ML:
//   La ML sola necesita secuencias de varios segundos para funcionar bien —
//   latencia inaceptable en tiempo real. El histograma de IOIs comprime la
//   historia temporal de 32 onsets en un vector de 32 valores, eliminando
//   la latencia de ventanado. El modelo desambigua double-time/half-time
//   usando la forma de la distribución de IOIs.
//
// Flujo de uso típico:
//   BeatFollower follower;
//   follower.init();  // en setup()
//
//   // En callback MIDI on_note_on():
//   follower.on_onset(millis());
//
//   // Cada 500ms en loop():
//   BeatResult result = follower.infer();
//   if (result.valid && result.confidence > 0.5f) {
//       display.show_bpm(result.bpm);
//   }
//
// Latencia esperada: <6ms en Teensy 4.1 @ 600MHz
// Tensor arena:      ~16KB (verificar con arena_used_bytes() después de init())
//
// Cross-ref: apps/docs/04-ai-architecture.md §1 (Beat Follower feature)
//            apps/docs/sprints/25-beat-follower.md

#include <stdint.h>
#include <stddef.h>

/** Número de clases BPM: 60, 63, 66, ..., 240 BPM. */
#define BEAT_FOLLOWER_N_BPM_CLASSES  32U

/**
 * Tamaño del buffer circular de onsets.
 *
 * 32 onsets → 31 IOIs. Es el mínimo práctico para que el histograma tenga
 * suficiente información estadística (al menos 10 beats en el rango más lento).
 *
 * En 60 BPM, 32 onsets equivalen a ~30 segundos de música.
 * En 120 BPM, equivalen a ~15 segundos.
 * En 240 BPM, equivalen a ~7.5 segundos.
 */
#define BEAT_FOLLOWER_ONSET_BUFFER_SIZE  32U

/**
 * Número de bins del histograma de IOIs.
 * Debe coincidir con la dimensión de input del modelo entrenado.
 */
#define BEAT_FOLLOWER_N_IOI_BINS  32U

/** IOI mínimo capturado (ms) = 50ms → 1200 BPM máximo. */
#define BEAT_FOLLOWER_IOI_MIN_MS  50.0f

/** IOI máximo capturado (ms) = 2000ms → 30 BPM mínimo. */
#define BEAT_FOLLOWER_IOI_MAX_MS  2000.0f

/**
 * Tamaño del tensor arena en bytes.
 *
 * Para este modelo (5248 parámetros, Dense INT8 sin BatchNorm):
 *   - Activaciones: 64 + 32 + 32 bytes = 128 bytes
 *   - Overhead TFLite Micro (op tables, metadata): ~8KB
 *   - Buffer temporal MatMul INT8: ~4KB
 *   - Padding de alineación: ~4KB
 *   Total estimado: ~16KB
 *
 * Si AllocateTensors() falla, incrementar este valor.
 * Reportar arena_used_bytes() en setup() para ajustar al mínimo real.
 *
 * Budget total de tensor arena: 200KB (04-ai-architecture.md §1.2).
 * KeyDetector ~16KB + ChordRecognizer ~24KB + BeatFollower ~16KB = ~56KB de 200KB.
 */
#define BEAT_FOLLOWER_ARENA_SIZE  (16U * 1024U)

/**
 * Número mínimo de onsets para inferencia válida.
 *
 * Con menos de 4 onsets (3 IOIs), el histograma es demasiado escaso para
 * ser significativo. infer() retorna valid=false hasta alcanzar este mínimo.
 */
#define BEAT_FOLLOWER_MIN_ONSETS  4U

// ---------------------------------------------------------------------------
// BPM_CLASSES — debe coincidir exactamente con dataset.py de training
// ---------------------------------------------------------------------------

/**
 * Array de BPM discretos que el modelo puede predecir.
 *
 * label=0 → 60 BPM, label=16 → 120 BPM, label=31 → 240 BPM.
 * Definido como extern para evitar múltiples definiciones en compilación.
 */
extern const uint16_t kBeatFollowerBpmClasses[BEAT_FOLLOWER_N_BPM_CLASSES];

// ---------------------------------------------------------------------------
// BeatResult
// ---------------------------------------------------------------------------

/**
 * Resultado de la detección de tempo.
 *
 * Usar result.valid antes de leer los otros campos.
 * Usar result.confidence para decidir si mostrar el BPM en display:
 *   confidence > 0.5f → mostrar con certeza
 *   confidence < 0.5f → mostrar con "?" o no mostrar
 */
struct BeatResult {
    float    bpm;             ///< BPM detectado (de kBeatFollowerBpmClasses[bpm_index])
    uint8_t  bpm_index;       ///< Índice en kBeatFollowerBpmClasses [0, 31]
    float    confidence;      ///< Probabilidad de la clase ganadora [0.0, 1.0]
    uint32_t beat_period_ms;  ///< Período del beat en ms (= 60000 / bpm, redondeado)
    bool     valid;           ///< false si hay <BEAT_FOLLOWER_MIN_ONSETS en el buffer
};

// ---------------------------------------------------------------------------
// BeatFollower
// ---------------------------------------------------------------------------

/**
 * Detector de tempo híbrido: algoritmo IOI + modelo TFLite Micro.
 *
 * Encapsula:
 *   1. Buffer circular de timestamps de onsets
 *   2. Cálculo de histograma de IOIs
 *   3. Inferencia TFLite Micro INT8
 *
 * Thread safety: on_onset() puede llamarse desde el callback MIDI (ISR-like).
 * infer() y reset() deben llamarse solo desde loop() — no desde ISR.
 *
 * En Teensy 4.1, el loop() corre a baja prioridad y los callbacks MIDI
 * son procesados en el contexto del USB host — es seguro si on_onset()
 * solo escribe en el buffer sin hacer allocaciones.
 */
class BeatFollower {
public:
    BeatFollower();
    ~BeatFollower();

    /**
     * Inicializa TFLite Micro y verifica el modelo.
     *
     * Debe llamarse en setup() antes de cualquier otro método.
     * Si retorna false, el modelo está corrupto o el arena es insuficiente.
     *
     * @return true si la inicialización fue exitosa.
     */
    bool init();

    /**
     * Registra un onset (evento NoteOn).
     *
     * Almacena el timestamp en el buffer circular.
     * Seguro de llamar desde el callback MIDI (no bloquea, no alloca).
     *
     * @param timestamp_ms  millis() en el momento del NoteOn.
     *                      Debe ser consistente con el sistema de reloj del Teensy.
     */
    void on_onset(uint32_t timestamp_ms);

    /**
     * Ejecuta inferencia sobre los onsets acumulados.
     *
     * Llamar periódicamente (cada 500ms) desde loop().
     * Calcula el histograma de IOIs y corre el modelo INT8.
     *
     * @return BeatResult con el BPM detectado.
     *         result.valid == false si hay menos de BEAT_FOLLOWER_MIN_ONSETS onsets.
     */
    BeatResult infer();

    /**
     * Resetea el buffer de onsets.
     *
     * Útil cuando cambia el instrumento o hay una pausa larga en la música.
     * El siguiente infer() retornará valid=false hasta acumular nuevos onsets.
     */
    void reset();

    /**
     * Bytes del tensor arena usados después de AllocateTensors().
     *
     * Llamar después de init() para verificar el arena real vs BEAT_FOLLOWER_ARENA_SIZE.
     * Retorna 0 si init() no fue llamado.
     */
    size_t arena_used_bytes() const;

private:
    // Buffer circular de timestamps de onsets.
    // _onset_head apunta al slot donde se escribirá el PRÓXIMO onset (escritura).
    // Los onsets válidos están en [0, _onset_count-1] si _onset_count < BUFFER_SIZE,
    // o en todo el buffer si _onset_count == BUFFER_SIZE (modo circular).
    uint32_t _onset_buffer[BEAT_FOLLOWER_ONSET_BUFFER_SIZE];
    uint8_t  _onset_head;   ///< Índice de escritura circular [0, BUFFER_SIZE-1]
    uint8_t  _onset_count;  ///< Número de onsets acumulados, máx BUFFER_SIZE

    // Arena de activaciones — en RAM del Teensy.
    // alignas(16): alineación para SIMD en Cortex-M7 + requerimiento de TFLite Micro.
    alignas(16) uint8_t _arena[BEAT_FOLLOWER_ARENA_SIZE];

    // TFLite Micro internals — void* para no exponer headers de TFLite en .h público.
    void* _interpreter;   // tflite::MicroInterpreter*
    void* _op_resolver;   // tflite::MicroMutableOpResolver<3>*
    bool  _initialized;

    // Parámetros de quantización del input tensor.
    // Necesarios para cuantizar float input → int8 antes de Invoke().
    float   _input_scale;
    int32_t _input_zero_point;

    // Parámetros de quantización del output tensor.
    float   _output_scale;
    int32_t _output_zero_point;

    /**
     * Calcula el histograma de IOIs del buffer circular.
     *
     * Lee los onsets del buffer en orden cronológico, calcula las diferencias,
     * y llena histogram_out con la distribución normalizada.
     *
     * @param histogram_out  Buffer de BEAT_FOLLOWER_N_IOI_BINS floats.
     *                       Inicializado a ceros por el llamador.
     */
    void _compute_ioi_histogram(float histogram_out[BEAT_FOLLOWER_N_IOI_BINS]) const;
};

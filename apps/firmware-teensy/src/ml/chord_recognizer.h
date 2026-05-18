#pragma once
// chord_recognizer.h — Inferencia de acordes musicales en Teensy 4.1
//
// Modelo: Dense 3-layer classifier con BatchNorm (12→128→64→61, INT8)
// Input:  Pitch class histogram de 12 valores (float, suma ≈ 1.0)
// Output: 61 clases — 60 acordes (5 tipos × 12 roots) + N (no chord)
//
// Esquema de labels:
//   0-11  → maj:  C_maj=0,  C#_maj=1,  ..., B_maj=11
//   12-23 → min:  C_min=12, C#_min=13, ..., B_min=23
//   24-35 → 7:    C_7=24,   C#_7=25,   ..., B_7=35
//   36-47 → m7:   C_m7=36,  C#_m7=37,  ..., B_m7=47
//   48-59 → maj7: C_maj7=48,C#_maj7=49,..., B_maj7=59
//   60    → N     (no chord / silencio / indeterminado)
//
// Uso típico:
//   ChordRecognizer recognizer;
//   recognizer.init();
//
//   float hist[12] = { /* pitch class histogram */ };
//   ChordResult result;
//   if (recognizer.inference(hist, result)) {
//       if (result.confidence > 0.6f) {
//           Serial.print(result.name);  // "C maj7", "F# min", "N", etc.
//       } else {
//           Serial.print(result.name);
//           Serial.print("?");          // baja confianza → mostrar con interrogación
//       }
//   }
//
// Integración con KeyDetector:
//   KeyDetector y ChordRecognizer comparten el mismo input (pitch class histogram).
//   Se corren secuencialmente — el arena no se comparte en paralelo.
//   Usar el arena más grande (este modelo: ~24KB) para ambos si se intercalan.
//
// Latencia esperada: <8ms en Teensy 4.1 @ 600MHz
// Tensor arena:      ~24KB (ajustar post-training con arena_used_bytes())
//
// Cross-ref: apps/docs/04-ai-architecture.md §1 (Chord Recognition feature)
//            Sprint 4.4 — apps/docs/sprints/24-chord-recognizer.md

#include <stdint.h>
#include <stddef.h>

/** Número de clases: 60 acordes + 1 clase N. */
#define CHORD_RECOGNIZER_NUM_CLASSES 61U

/**
 * Tamaño del tensor arena en bytes.
 *
 * Para este modelo (14782 parámetros, INT8 con BatchNorm fusionado):
 *   - Activaciones: 128 + 64 + 61 bytes ≈ 253 bytes
 *   - Overhead TFLite Micro (op tables, metadata): ~8KB
 *   - Buffer temporal MatMul INT8: ~4KB
 *   - Padding de alineación: ~2KB
 *   Total estimado post-training: ~14-18KB
 *   Con margen de seguridad: 24KB
 *
 * Si AllocateTensors() falla, incrementar este valor.
 * Reportar arena_used_bytes() en setup() para ajustar al mínimo real.
 *
 * Budget total de tensor arena: 200KB (04-ai-architecture.md §1.2).
 * KeyDetector usa ~16KB + ChordRecognizer ~24KB = ~40KB de los 200KB.
 */
#define CHORD_RECOGNIZER_ARENA_SIZE (24U * 1024U)

/**
 * Resultado de la inferencia de chord recognition.
 *
 * chord_index sigue el esquema del training (ver esquema de labels arriba).
 *
 * Para mostrar en display:
 *   if (result.confidence > 0.6f) → mostrar name con certeza
 *   if (result.confidence <= 0.6f) → mostrar name con "?" (incertidumbre)
 *   if (result.chord_type == 5) → es clase N, no mostrar acorde
 */
struct ChordResult {
    uint8_t chord_index;  ///< Índice de clase [0, 60]. 60 = N (no chord).
    uint8_t root_note;    ///< Nota raíz [0-11]: C=0, C#=1, ..., B=11. 255 si N.
    uint8_t chord_type;   ///< 0=maj, 1=min, 2=7, 3=m7, 4=maj7, 5=N.
    float   confidence;   ///< Probabilidad de la clase ganadora [0.0, 1.0].
    char    name[12];     ///< Nombre legible: "C maj7", "F# min", "N", etc. (null-terminated)
};

/**
 * Inferencia de acordes musicales con TFLite Micro.
 *
 * Encapsula el ciclo TFLite Micro:
 *   GetModel() → MicroMutableOpResolver → MicroInterpreter
 *   → AllocateTensors() → Invoke() → argmax de output
 *
 * El modelo vive en flash (g_chord_recognizer_model[]).
 * Solo las activaciones intermedias van al tensor arena (RAM).
 *
 * Ops registradas: AddFullyConnected, AddSoftmax, AddRelu.
 * BatchNorm está fusionado en FullyConnected post-quantización — no requiere
 * op adicional en TFLite Micro.
 *
 * Diferencia respecto a KeyDetector: este modelo tiene BatchNorm en el
 * grafo Keras, pero el TFLiteConverter lo fusiona con Dense durante la
 * conversión INT8. El resultado en TFLite es el mismo conjunto de ops:
 * FullyConnected + ReLU + Softmax. La fusión es transparente para el runtime.
 */
class ChordRecognizer {
public:
    ChordRecognizer();
    ~ChordRecognizer();

    /**
     * Inicializa TFLite Micro y verifica el modelo.
     *
     * Debe llamarse en setup() antes de inference().
     * Si retorna false, el modelo está corrupto o el arena es insuficiente.
     *
     * @return true si la inicialización fue exitosa.
     */
    bool init();

    /**
     * Corre inferencia sobre un pitch class histogram.
     *
     * El histogram debe ser float[12] con la distribución de pitch classes
     * de las notas activas. Idealmente suma 1.0 (normalizado).
     *
     * @param histogram  Pointer a 12 floats [C, C#, D, ..., B]
     * @param result     Output: chord_index, root_note, chord_type, confidence, name
     * @return true si la inferencia fue exitosa.
     */
    bool inference(const float* histogram, ChordResult& result);

    /**
     * Bytes del tensor arena usados después de AllocateTensors().
     * Llamar después de init() para verificar el arena real vs CHORD_RECOGNIZER_ARENA_SIZE.
     * Retorna 0 si init() no fue llamado.
     */
    size_t arena_used_bytes() const;

private:
    // Arena de activaciones — en RAM del Teensy.
    // alignas(16): alineación para SIMD en Cortex-M7.
    alignas(16) uint8_t _arena[CHORD_RECOGNIZER_ARENA_SIZE];

    // TFLite Micro internals — void* para no exponer headers de TFLite en .h público.
    void* _interpreter;   // tflite::MicroInterpreter*
    void* _op_resolver;   // tflite::MicroMutableOpResolver<3>*
    bool  _initialized;

    // Parámetros de quantización del input tensor (calibrados durante training).
    // Necesarios para cuantizar float input → int8 antes de Invoke().
    float   _input_scale;
    int32_t _input_zero_point;

    // Parámetros de quantización del output tensor (para descuantizar a float).
    float   _output_scale;
    int32_t _output_zero_point;

    /**
     * Formatea el nombre del acorde en result.name.
     * Ejemplos: "C maj7", "F# min", "A# 7", "N".
     */
    void _format_chord_name(uint8_t chord_index, char* out_name, size_t max_len) const;

    /**
     * Extrae root_note y chord_type de un chord_index.
     * Para chord_index == 60 (N): root_note=255, chord_type=5.
     */
    void _decode_chord_index(uint8_t chord_index, uint8_t& root_note, uint8_t& chord_type) const;
};

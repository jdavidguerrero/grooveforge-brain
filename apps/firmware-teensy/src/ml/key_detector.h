#pragma once
// key_detector.h — Inferencia de tonalidad musical en Teensy 4.1
//
// Modelo: Dense 3-layer classifier (12→64→32→24, INT8)
// Input:  Pitch class histogram de 12 valores (float, suma ≈ 1.0)
// Output: 24 clases — C_maj=0..B_maj=11, C_min=12..B_min=23
//
// Uso típico:
//   KeyDetector detector;
//   detector.init();
//
//   float hist[12] = { /* pitch class histogram */ };
//   KeyResult result;
//   if (detector.inference(hist, result)) {
//       Serial.print(result.name);  // "C maj", "F# min", etc.
//   }
//
// Latencia esperada: <5ms en Teensy 4.1 @ 600MHz (bien debajo del target 20ms)
// Tensor arena:      ~16KB (compartida secuencialmente con otros modelos)
//
// Cross-ref: apps/docs/04-ai-architecture.md §1 (Scale Lock feature)
//            Sprint 4.3 — apps/docs/sprints/23-key-detector.md

#include <stdint.h>
#include <stddef.h>

/** Número de clases de tonalidad: 12 mayores + 12 menores. */
#define KEY_DETECTOR_NUM_CLASSES 24U

/**
 * Tamaño del tensor arena en bytes.
 *
 * Para este modelo (3704 parámetros INT8), el arena es dominado por el
 * overhead de TFLite Micro (~8KB) más los buffers de activación intermedios.
 * 16KB da suficiente margen. Si AllocateTensors() falla, incrementar.
 *
 * Nota: el tensor arena se comparte secuencialmente entre todos los modelos
 * Layer 1 — el budget total es 200KB (04-ai-architecture.md §1.2).
 * Este modelo usa ~16KB de los 200KB disponibles.
 */
#define KEY_DETECTOR_ARENA_SIZE (16U * 1024U)

/**
 * Resultado de la inferencia de tonalidad.
 *
 * key_index sigue el esquema del training:
 *   0-11  → tonalidades mayores (C_maj=0, C#_maj=1, ..., B_maj=11)
 *   12-23 → tonalidades menores (C_min=12, C#_min=13, ..., B_min=23)
 */
struct KeyResult {
    uint8_t key_index;    ///< Índice de clase [0, 23]
    uint8_t root_note;    ///< Nota raíz [0-11]: C=0, C#=1, D=2, ..., B=11
    bool    is_minor;     ///< false = mayor, true = menor
    float   confidence;   ///< Probabilidad de la clase ganadora [0.0, 1.0]
    char    name[8];      ///< Nombre legible: "C maj", "F# min", etc. (null-terminated)
};

/**
 * Inferencia de tonalidad musical con TFLite Micro.
 *
 * Encapsula el ciclo completo de TFLite Micro:
 *   GetModel() → MicroMutableOpResolver → MicroInterpreter
 *   → AllocateTensors() → Invoke() → argmax de output
 *
 * El modelo vive en flash (g_key_detector_model[]).
 * Solo las activaciones intermedias van al tensor arena (RAM).
 *
 * Ops registradas: AddFullyConnected, AddSoftmax, AddRelu6.
 * Solo estas ops — no registrar ops adicionales (overhead de flash innecesario).
 */
class KeyDetector {
public:
    KeyDetector();
    ~KeyDetector();

    /**
     * Inicializa TFLite Micro y verifica el modelo.
     *
     * Debe llamarse en setup() antes de inference().
     * Configura: MicroMutableOpResolver, MicroInterpreter, AllocateTensors().
     *
     * @return true si la inicialización fue exitosa.
     *         false si el modelo está corrupto o el arena es insuficiente.
     */
    bool init();

    /**
     * Corre la inferencia sobre un pitch class histogram.
     *
     * El histogram debe ser un array de 12 floats que representan la
     * distribución de pitch classes de las notas activas. Idealmente suma 1.0
     * (normalizado), pero el modelo tolera histogramas no normalizados.
     *
     * El input float se cuantiza a int8 internamente usando el scale/zero_point
     * calibrado durante el training (convert_to_tflite_int8).
     *
     * @param histogram  Pointer a 12 floats [C, C#, D, D#, E, F, F#, G, G#, A, A#, B]
     * @param result     Struct de salida con key_index, root_note, is_minor, confidence, name
     * @return true si la inferencia fue exitosa, false si el modelo no está inicializado.
     */
    bool inference(const float* histogram, KeyResult& result);

    /**
     * Bytes del tensor arena realmente usados post-AllocateTensors().
     * Útil para ajustar KEY_DETECTOR_ARENA_SIZE al mínimo real.
     * Retorna 0 si init() no fue llamado.
     */
    size_t arena_used_bytes() const;

private:
    // Arena de memoria para tensores de activación — en RAM del Teensy.
    // alignas(16): TFLite Micro requiere alineación para operaciones SIMD en M7.
    alignas(16) uint8_t _arena[KEY_DETECTOR_ARENA_SIZE];

    // Internals de TFLite Micro — void* para no exponer los headers de TFLite
    // en la interfaz pública del .h. Se castean en el .cpp donde TFLite está disponible.
    void* _interpreter;   // MicroInterpreter*
    void* _op_resolver;   // MicroMutableOpResolver<3>*
    bool  _initialized;

    // Copia interna de los parámetros de quantización del input tensor.
    // Necesarios para cuantizar el float input a int8 antes de Invoke().
    float   _input_scale;
    int32_t _input_zero_point;

    // Parámetros de quantización del output tensor (para descuantizar a float).
    float   _output_scale;
    int32_t _output_zero_point;

    /** Formatea el nombre de tonalidad en result.name (ej: "C maj", "F# min"). */
    void _format_key_name(uint8_t key_index, char* out_name, size_t max_len) const;
};

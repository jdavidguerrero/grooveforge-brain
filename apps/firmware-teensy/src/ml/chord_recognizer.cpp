// chord_recognizer.cpp — Implementación de inferencia Chord Recognizer con TFLite Micro
//
// Requiere TFLite Micro para Arduino (ver lib_deps en platformio.ini).
// En env:native (tests sin hardware) este archivo es excluido del build via
// el guard NATIVE_TEST — los tests del dataset/model no necesitan TFLite en host.
//
// El modelo en flash está en chord_recognizer_model.h (generado por el training).
// Correr training antes de compilar en modo producción:
//   cd apps/training && uv run python models/chord_recognizer/train.py
//
// Nota sobre BatchNorm y TFLite Micro:
//   El modelo Keras usa BatchNorm, pero el TFLiteConverter INT8 fusiona BatchNorm
//   con Dense durante la conversión. El grafo TFLite resultante contiene solo:
//   FullyConnected + ReLU + Softmax — los mismos ops que KeyDetector.
//   Por eso registramos los mismos 3 ops en el MicroMutableOpResolver.
//
// Cross-ref: apps/docs/04-ai-architecture.md §1, Sprint 4.4

// Guard: excluir este archivo en el env native (host tests sin TFLite)
#ifndef NATIVE_TEST

#include "chord_recognizer.h"
#include "chord_recognizer_model.h"  // g_chord_recognizer_model[], g_chord_recognizer_model_len

// TFLite Micro headers
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <string.h>  // memset, snprintf

// ---------------------------------------------------------------------------
// Tablas de nombres — alineadas con el esquema de labels del training
// ---------------------------------------------------------------------------

// Notas raíz — mismo orden que NOTE_NAMES en dataset.py
static const char* const kRootNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Tipos de acorde — mismo orden que CHORD_TYPES en dataset.py
// Índice: 0=maj, 1=min, 2=7, 3=m7, 4=maj7, 5=N (no chord)
static const char* const kChordTypeNames[6] = {
    "maj", "min", "7", "m7", "maj7", "N"
};

// Número de roots por tipo de acorde
static constexpr uint8_t kNumRoots = 12U;

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

ChordRecognizer::ChordRecognizer()
    : _interpreter(nullptr)
    , _op_resolver(nullptr)
    , _initialized(false)
    , _input_scale(1.0f)
    , _input_zero_point(0)
    , _output_scale(1.0f)
    , _output_zero_point(0)
{
    memset(_arena, 0, sizeof(_arena));
}

ChordRecognizer::~ChordRecognizer() {
    // MicroInterpreter y MicroMutableOpResolver están en buffers estáticos.
    // TFLite Micro no usa heap — no hay delete.
}

// ---------------------------------------------------------------------------
// init()
// ---------------------------------------------------------------------------

bool ChordRecognizer::init() {
    if (_initialized) {
        return true;
    }

    // Verificar versión del schema del flatbuffer
    const tflite::Model* model = tflite::GetModel(g_chord_recognizer_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println("[ChordRecognizer] ERROR: schema version mismatch");
        Serial.println("[ChordRecognizer] Regenerar con la version actual de TFLite");
        return false;
    }

    // ── Op resolver: registrar los ops que el modelo necesita.
    //
    // Aunque el modelo Keras tiene BatchNorm, el TFLiteConverter INT8 fusiona
    // BN con los pesos de Dense durante la conversión. El grafo TFLite final
    // contiene solo FullyConnected + ReLU + Softmax — igual que KeyDetector.
    //
    // La fusión funciona así: BN aplica gamma/beta sobre las activaciones de
    // Dense. El converter absorbe gamma/beta en los pesos y bias de Dense:
    //   new_weight = gamma * weight / sqrt(variance + epsilon)
    //   new_bias   = gamma * (bias - mean) / sqrt(variance + epsilon) + beta
    // El resultado es un FullyConnected con pesos modificados — sin capa BN separada.
    static tflite::MicroMutableOpResolver<3> op_resolver;
    op_resolver.AddFullyConnected();
    op_resolver.AddSoftmax();
    op_resolver.AddRelu();  // ReLU (no Relu6) — el modelo usa ReLU explícito

    _op_resolver = &op_resolver;

    // ── Intérprete en buffer estático — no heap allocation en Teensy
    static tflite::MicroInterpreter interpreter(
        model,
        op_resolver,
        _arena,
        CHORD_RECOGNIZER_ARENA_SIZE
    );

    _interpreter = &interpreter;

    // ── Asignar tensores de activación en _arena
    TfLiteStatus alloc_status =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter)->AllocateTensors();

    if (alloc_status != kTfLiteOk) {
        Serial.println("[ChordRecognizer] ERROR: AllocateTensors fallo");
        Serial.print("[ChordRecognizer] Arena actual: ");
        Serial.print(CHORD_RECOGNIZER_ARENA_SIZE);
        Serial.println(" bytes — incrementar CHORD_RECOGNIZER_ARENA_SIZE");
        return false;
    }

    // ── Leer parámetros de quantización del input tensor
    tflite::MicroInterpreter* interp =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter);

    TfLiteTensor* input_tensor = interp->input(0);
    _input_scale      = input_tensor->params.scale;
    _input_zero_point = input_tensor->params.zero_point;

    // ── Leer parámetros de quantización del output tensor
    TfLiteTensor* output_tensor = interp->output(0);
    _output_scale      = output_tensor->params.scale;
    _output_zero_point = output_tensor->params.zero_point;

    _initialized = true;

    Serial.print("[ChordRecognizer] OK — arena usada: ");
    Serial.print(arena_used_bytes());
    Serial.println(" bytes");

    return true;
}

// ---------------------------------------------------------------------------
// inference()
// ---------------------------------------------------------------------------

bool ChordRecognizer::inference(const float* histogram, ChordResult& result) {
    if (!_initialized) {
        Serial.println("[ChordRecognizer] ERROR: llamar init() primero");
        return false;
    }

    tflite::MicroInterpreter* interp =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter);

    TfLiteTensor* input_tensor = interp->input(0);

    // ── Cuantizar el float input a int8
    // Fórmula: int8_val = clamp(round(float_val / scale + zero_point), -128, 127)
    int8_t* input_data = input_tensor->data.int8;
    for (int i = 0; i < 12; ++i) {
        float quantized = histogram[i] / _input_scale
                          + static_cast<float>(_input_zero_point);
        if (quantized >  127.0f) quantized =  127.0f;
        if (quantized < -128.0f) quantized = -128.0f;
        input_data[i] = static_cast<int8_t>(quantized);
    }

    // ── Ejecutar inferencia
    TfLiteStatus invoke_status = interp->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.println("[ChordRecognizer] ERROR: Invoke() fallo");
        return false;
    }

    // ── Leer output INT8 y encontrar la clase con mayor score
    TfLiteTensor* output_tensor = interp->output(0);
    const int8_t* output_data = output_tensor->data.int8;

    int8_t  best_score = output_data[0];
    uint8_t best_class = 0;

    for (uint8_t c = 1; c < CHORD_RECOGNIZER_NUM_CLASSES; ++c) {
        if (output_data[c] > best_score) {
            best_score = output_data[c];
            best_class = c;
        }
    }

    // ── Descuantizar la probabilidad de la clase ganadora
    float confidence = (static_cast<float>(best_score)
                        - static_cast<float>(_output_zero_point))
                       * _output_scale;
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;

    // ── Llenar el resultado
    result.chord_index = best_class;
    result.confidence  = confidence;

    _decode_chord_index(best_class, result.root_note, result.chord_type);
    _format_chord_name(best_class, result.name, sizeof(result.name));

    return true;
}

// ---------------------------------------------------------------------------
// arena_used_bytes()
// ---------------------------------------------------------------------------

size_t ChordRecognizer::arena_used_bytes() const {
    if (!_initialized) {
        return 0;
    }
    return reinterpret_cast<const tflite::MicroInterpreter*>(_interpreter)
        ->arena_used_bytes();
}

// ---------------------------------------------------------------------------
// _decode_chord_index() — interno
// ---------------------------------------------------------------------------

void ChordRecognizer::_decode_chord_index(
    uint8_t chord_index,
    uint8_t& root_note,
    uint8_t& chord_type
) const {
    if (chord_index >= CHORD_RECOGNIZER_NUM_CLASSES) {
        // Índice fuera de rango — tratar como N
        root_note  = 255U;
        chord_type = 5U;  // N
        return;
    }

    if (chord_index == 60U) {
        // Clase N (no chord)
        root_note  = 255U;
        chord_type = 5U;
        return;
    }

    // Clases 0-59: chord_type_idx * 12 + root_idx
    chord_type = chord_index / kNumRoots;   // 0=maj, 1=min, 2=7, 3=m7, 4=maj7
    root_note  = chord_index % kNumRoots;   // 0=C, 1=C#, ..., 11=B
}

// ---------------------------------------------------------------------------
// _format_chord_name() — interno
// ---------------------------------------------------------------------------

void ChordRecognizer::_format_chord_name(
    uint8_t chord_index,
    char*   out_name,
    size_t  max_len
) const {
    if (chord_index == 60U) {
        snprintf(out_name, max_len, "N");
        return;
    }

    uint8_t root, ctype;
    _decode_chord_index(chord_index, root, ctype);

    if (root >= 12U || ctype >= 5U) {
        snprintf(out_name, max_len, "?");
        return;
    }

    // Formato: "C maj7", "F# min", "A# 7", etc.
    // kRootNames[root] puede ser "C#" (2 chars) — snprintf maneja el espacio
    snprintf(out_name, max_len, "%s %s", kRootNames[root], kChordTypeNames[ctype]);
}

#endif  // !NATIVE_TEST

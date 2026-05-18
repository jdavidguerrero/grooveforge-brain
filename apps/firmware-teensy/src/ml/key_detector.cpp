// key_detector.cpp — Implementación de inferencia Key Detector con TFLite Micro
//
// Requiere TFLite Micro para Arduino (ver lib_deps en platformio.ini).
// En env:native (tests sin hardware) este archivo es excluido del build via
// el guard NATIVE_TEST — los tests del dataset/model no necesitan TFLite en host.
//
// El modelo en flash está en key_detector_model.h (generado por el training).
// Correr training antes de compilar en modo producción:
//   cd apps/training && uv run python models/key_detector/train.py
//
// Cross-ref: apps/docs/04-ai-architecture.md §1, Sprint 4.3

// Guard: excluir este archivo en el env native (host tests sin TFLite)
#ifndef NATIVE_TEST

#include <Arduino.h>  // Serial — necesario al compilar como unidad independiente
#include "key_detector.h"
#include "key_detector_model.h"  // g_key_detector_model[], g_key_detector_model_len

// TFLite Micro headers — requiere la librería TFLite Micro para Arduino
// (ver lib_deps en platformio.ini)
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <string.h>  // memset, snprintf

// ---------------------------------------------------------------------------
// Nombres de notas raíz — alineados con el esquema de labels del training
// KEY_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
// ---------------------------------------------------------------------------

static const char* const kRootNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

KeyDetector::KeyDetector()
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

KeyDetector::~KeyDetector() {
    // MicroInterpreter y MicroMutableOpResolver se colocan con placement new
    // en _arena o en buffers estáticos — no se hace delete explícito.
    // TFLite Micro está diseñado para embedded: no hay heap allocation.
}

// ---------------------------------------------------------------------------
// init()
// ---------------------------------------------------------------------------

bool KeyDetector::init() {
    if (_initialized) {
        return true;  // ya inicializado
    }

    // Verificar que el flatbuffer del modelo es válido
    const tflite::Model* model = tflite::GetModel(g_key_detector_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        // El modelo fue compilado con una versión de schema diferente a la
        // del runtime. Regenerar con la versión actual de TFLite.
        Serial.println("[KeyDetector] ERROR: schema version mismatch");
        return false;
    }

    // ── Op resolver: registrar SOLO los ops que el modelo usa.
    // Dense(relu) → FullyConnected + Relu6 (TFLite fusiona Relu en FC)
    // Dense(softmax) → FullyConnected + Softmax
    // No registrar ops adicionales: cada op ocupa flash del op table.
    //
    // Nota: usamos un buffer estático para el op resolver porque
    // MicroMutableOpResolver<N> no puede ir en el heap en Teensy.
    static tflite::MicroMutableOpResolver<3> op_resolver;
    op_resolver.AddFullyConnected();
    op_resolver.AddSoftmax();
    op_resolver.AddRelu6();  // TFLite Micro funde ReLU en FullyConnected como Relu6

    _op_resolver = &op_resolver;

    // ── Intérprete: placement en buffer estático, usa _arena para tensores
    static tflite::MicroInterpreter interpreter(
        model,
        op_resolver,
        _arena,
        KEY_DETECTOR_ARENA_SIZE
    );

    _interpreter = &interpreter;

    // ── Asignar tensores de activación en _arena
    TfLiteStatus alloc_status =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter)->AllocateTensors();

    if (alloc_status != kTfLiteOk) {
        Serial.println("[KeyDetector] ERROR: AllocateTensors fallo");
        Serial.print("[KeyDetector] Aumentar KEY_DETECTOR_ARENA_SIZE (actual: ");
        Serial.print(KEY_DETECTOR_ARENA_SIZE);
        Serial.println(" bytes)");
        return false;
    }

    // ── Leer parámetros de quantización del input tensor
    // El modelo INT8 espera que convirtamos el float input a int8:
    //   int8_val = float_val / input_scale + input_zero_point
    tflite::MicroInterpreter* interp =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter);

    TfLiteTensor* input_tensor = interp->input(0);
    _input_scale      = input_tensor->params.scale;
    _input_zero_point = input_tensor->params.zero_point;

    // ── Leer parámetros de quantización del output tensor
    // Para descuantizar la salida int8 a float probability:
    //   float_val = (int8_val - output_zero_point) * output_scale
    TfLiteTensor* output_tensor = interp->output(0);
    _output_scale      = output_tensor->params.scale;
    _output_zero_point = output_tensor->params.zero_point;

    _initialized = true;

    Serial.print("[KeyDetector] OK — arena usada: ");
    Serial.print(arena_used_bytes());
    Serial.println(" bytes");

    return true;
}

// ---------------------------------------------------------------------------
// inference()
// ---------------------------------------------------------------------------

bool KeyDetector::inference(const float* histogram, KeyResult& result) {
    if (!_initialized) {
        Serial.println("[KeyDetector] ERROR: llamar init() primero");
        return false;
    }

    tflite::MicroInterpreter* interp =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter);

    TfLiteTensor* input_tensor = interp->input(0);

    // ── Cuantizar el float input a int8
    // El modelo fue entrenado con histogramas float32 [0.0, 1.0].
    // El converter calibró scale/zero_point para el rango [0.0, 1.0].
    // Fórmula: int8_val = clamp(round(float_val / scale + zero_point), -128, 127)
    int8_t* input_data = input_tensor->data.int8;
    for (int i = 0; i < 12; ++i) {
        float quantized = histogram[i] / _input_scale + static_cast<float>(_input_zero_point);
        // Clampear a rango int8
        if (quantized > 127.0f)  quantized = 127.0f;
        if (quantized < -128.0f) quantized = -128.0f;
        input_data[i] = static_cast<int8_t>(quantized);
    }

    // ── Correr inferencia
    TfLiteStatus invoke_status = interp->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.println("[KeyDetector] ERROR: Invoke() fallo");
        return false;
    }

    // ── Leer output INT8 y encontrar la clase con mayor score
    TfLiteTensor* output_tensor = interp->output(0);
    const int8_t* output_data = output_tensor->data.int8;

    int8_t best_score = output_data[0];
    uint8_t best_class = 0;

    for (uint8_t c = 1; c < KEY_DETECTOR_NUM_CLASSES; ++c) {
        if (output_data[c] > best_score) {
            best_score = output_data[c];
            best_class = c;
        }
    }

    // ── Descuantizar la probabilidad de la clase ganadora
    // La confianza como float [0.0, 1.0] es útil para el display y para
    // decidir si "snapear" notas (Scale Lock solo actúa con confidence > 0.7)
    float confidence = (static_cast<float>(best_score) - static_cast<float>(_output_zero_point))
                       * _output_scale;
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;

    // ── Llenar el resultado
    result.key_index  = best_class;
    result.is_minor   = (best_class >= 12);
    result.root_note  = best_class % 12;
    result.confidence = confidence;

    _format_key_name(best_class, result.name, sizeof(result.name));

    return true;
}

// ---------------------------------------------------------------------------
// arena_used_bytes()
// ---------------------------------------------------------------------------

size_t KeyDetector::arena_used_bytes() const {
    if (!_initialized) {
        return 0;
    }
    return reinterpret_cast<const tflite::MicroInterpreter*>(_interpreter)
        ->arena_used_bytes();
}

// ---------------------------------------------------------------------------
// _format_key_name() — interno
// ---------------------------------------------------------------------------

void KeyDetector::_format_key_name(uint8_t key_index, char* out_name, size_t max_len) const {
    // key_index 0-11  → mayores: root_note = key_index,      mode = "maj"
    // key_index 12-23 → menores: root_note = key_index - 12, mode = "min"
    uint8_t root = key_index % 12;
    const char* mode = (key_index < 12) ? "maj" : "min";
    const char* root_str = kRootNames[root];

    // snprintf es seguro y disponible en Teensyduino (no usar sprintf)
    snprintf(out_name, max_len, "%s %s", root_str, mode);
}

#endif  // !NATIVE_TEST

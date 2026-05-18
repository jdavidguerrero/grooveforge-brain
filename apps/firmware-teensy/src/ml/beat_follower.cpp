// beat_follower.cpp — Implementación del Beat Follower híbrido con TFLite Micro
//
// Requiere TFLite Micro para Arduino (ver lib_deps en platformio.ini).
// En env:native (tests sin hardware) este archivo es excluido del build via
// el guard NATIVE_TEST — los tests del dataset/model no necesitan TFLite en host.
//
// El modelo en flash está en beat_follower_model.h (generado por el training).
// Correr training antes de compilar en modo producción:
//   cd apps/training && uv run python models/beat_follower/train.py
//
// Arquitectura del histograma de IOIs:
//   - 32 bins sobre [50ms, 2000ms] → bin width ≈ 60.9ms
//   - Bin i cubre el rango [50 + i*60.9, 50 + (i+1)*60.9) ms
//   - Para 120 BPM (IOI=500ms): bin = (500-50)/60.9 ≈ 7 → bin 7
//   - Para 60 BPM (IOI=1000ms): bin = (1000-50)/60.9 ≈ 15 → bin 15
//
// Cross-ref: apps/docs/04-ai-architecture.md §1, Sprint 4.5

// Guard: excluir este archivo en el env native (host tests sin TFLite)
#ifndef NATIVE_TEST

#include <Arduino.h>  // Serial, delay — necesario al compilar como unidad independiente
#include "beat_follower.h"
#include "beat_follower_model.h"  // g_beat_follower_model[], g_beat_follower_model_len

// TFLite Micro headers
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <string.h>  // memset

// ---------------------------------------------------------------------------
// BPM_CLASSES — debe coincidir exactamente con dataset.py de training
// ---------------------------------------------------------------------------

const uint16_t kBeatFollowerBpmClasses[BEAT_FOLLOWER_N_BPM_CLASSES] = {
     60,  63,  66,  69,  72,  76,  80,  84,
     88,  92,  96, 100, 104, 108, 112, 116,
    120, 126, 132, 138, 144, 152, 160, 168,
    176, 184, 192, 200, 208, 216, 224, 240,
};

// Ancho de bin del histograma (ms).
// (IOI_MAX_MS - IOI_MIN_MS) / N_BINS = (2000 - 50) / 32 ≈ 60.9375ms
static constexpr float kBinWidth =
    (BEAT_FOLLOWER_IOI_MAX_MS - BEAT_FOLLOWER_IOI_MIN_MS) / BEAT_FOLLOWER_N_IOI_BINS;

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

BeatFollower::BeatFollower()
    : _onset_head(0)
    , _onset_count(0)
    , _interpreter(nullptr)
    , _op_resolver(nullptr)
    , _initialized(false)
    , _input_scale(1.0f)
    , _input_zero_point(0)
    , _output_scale(1.0f)
    , _output_zero_point(0)
{
    memset(_onset_buffer, 0, sizeof(_onset_buffer));
    memset(_arena, 0, sizeof(_arena));
}

BeatFollower::~BeatFollower() {
    // TFLite Micro no usa heap — no hay delete.
}

// ---------------------------------------------------------------------------
// init()
// ---------------------------------------------------------------------------

bool BeatFollower::init() {
    if (_initialized) {
        return true;
    }

    // Verificar versión del schema del flatbuffer.
    // Si hay mismatch, el modelo fue generado con una versión diferente de TFLite.
    const tflite::Model* model = tflite::GetModel(g_beat_follower_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println("[BeatFollower] ERROR: schema version mismatch");
        Serial.println("[BeatFollower] Regenerar con la version actual de TFLite");
        return false;
    }

    // ── Op resolver: registrar los ops que el modelo necesita.
    //
    // Beat Follower usa una arquitectura Dense pura sin BatchNorm:
    //   Input(32) → Dense(64, ReLU) → Dropout(train-only) → Dense(32, ReLU) → Dense(32, Softmax)
    //
    // Dropout no existe en TFLite Micro — solo se aplica durante training.
    // En inferencia el grafo es: FullyConnected + ReLU + FullyConnected + ReLU + FullyConnected + Softmax
    //
    // Los ops necesarios son los mismos 3 que KeyDetector y ChordRecognizer:
    //   AddFullyConnected, AddRelu, AddSoftmax
    static tflite::MicroMutableOpResolver<3> op_resolver;
    op_resolver.AddFullyConnected();
    op_resolver.AddRelu();
    op_resolver.AddSoftmax();

    _op_resolver = &op_resolver;

    // ── Intérprete en buffer estático — no heap allocation en Teensy.
    static tflite::MicroInterpreter interpreter(
        model,
        op_resolver,
        _arena,
        BEAT_FOLLOWER_ARENA_SIZE
    );

    _interpreter = &interpreter;

    // ── Asignar tensores de activación en _arena.
    TfLiteStatus alloc_status =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter)->AllocateTensors();

    if (alloc_status != kTfLiteOk) {
        Serial.println("[BeatFollower] ERROR: AllocateTensors fallo");
        Serial.print("[BeatFollower] Arena actual: ");
        Serial.print(BEAT_FOLLOWER_ARENA_SIZE);
        Serial.println(" bytes — incrementar BEAT_FOLLOWER_ARENA_SIZE si el problema persiste");
        return false;
    }

    // ── Leer parámetros de quantización del input tensor.
    tflite::MicroInterpreter* interp =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter);

    TfLiteTensor* input_tensor = interp->input(0);
    _input_scale      = input_tensor->params.scale;
    _input_zero_point = input_tensor->params.zero_point;

    // ── Leer parámetros de quantización del output tensor.
    TfLiteTensor* output_tensor = interp->output(0);
    _output_scale      = output_tensor->params.scale;
    _output_zero_point = output_tensor->params.zero_point;

    _initialized = true;

    Serial.print("[BeatFollower] OK — arena usada: ");
    Serial.print(arena_used_bytes());
    Serial.println(" bytes");

    return true;
}

// ---------------------------------------------------------------------------
// on_onset()
// ---------------------------------------------------------------------------

void BeatFollower::on_onset(uint32_t timestamp_ms) {
    // Escribir en el buffer circular sin allocar ni bloquear.
    // _onset_head apunta al slot de escritura actual.
    _onset_buffer[_onset_head] = timestamp_ms;

    // Avanzar el head circularmente
    _onset_head = (_onset_head + 1) % BEAT_FOLLOWER_ONSET_BUFFER_SIZE;

    // Incrementar el contador hasta el máximo del buffer
    if (_onset_count < BEAT_FOLLOWER_ONSET_BUFFER_SIZE) {
        _onset_count++;
    }
    // Cuando _onset_count == BEAT_FOLLOWER_ONSET_BUFFER_SIZE, el buffer está lleno
    // y las escrituras futuras sobrescriben el onset más antiguo (FIFO circular).
}

// ---------------------------------------------------------------------------
// infer()
// ---------------------------------------------------------------------------

BeatResult BeatFollower::infer() {
    BeatResult result;
    result.valid       = false;
    result.bpm         = 0.0f;
    result.bpm_index   = 0;
    result.confidence  = 0.0f;
    result.beat_period_ms = 0;

    if (!_initialized) {
        Serial.println("[BeatFollower] ERROR: llamar init() primero");
        return result;
    }

    // Verificar que hay suficientes onsets para inferencia significativa.
    // Con menos de 4 onsets (3 IOIs), el histograma es demasiado escaso.
    if (_onset_count < BEAT_FOLLOWER_MIN_ONSETS) {
        return result;  // valid = false, sin error (condición normal al inicio)
    }

    // ── Calcular histograma de IOIs del buffer
    float histogram[BEAT_FOLLOWER_N_IOI_BINS];
    memset(histogram, 0, sizeof(histogram));
    _compute_ioi_histogram(histogram);

    // ── Cuantizar el histograma float → int8 y cargar en el tensor de input
    tflite::MicroInterpreter* interp =
        reinterpret_cast<tflite::MicroInterpreter*>(_interpreter);

    TfLiteTensor* input_tensor = interp->input(0);
    int8_t* input_data = input_tensor->data.int8;

    for (int i = 0; i < BEAT_FOLLOWER_N_IOI_BINS; ++i) {
        // Fórmula de quantización: int8 = clamp(round(float / scale + zero_point), -128, 127)
        float q = histogram[i] / _input_scale + static_cast<float>(_input_zero_point);
        if (q >  127.0f) q =  127.0f;
        if (q < -128.0f) q = -128.0f;
        input_data[i] = static_cast<int8_t>(q);
    }

    // ── Ejecutar inferencia
    TfLiteStatus invoke_status = interp->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.println("[BeatFollower] ERROR: Invoke() fallo");
        return result;
    }

    // ── Leer output INT8 y encontrar la clase con mayor score (argmax)
    TfLiteTensor* output_tensor = interp->output(0);
    const int8_t* output_data   = output_tensor->data.int8;

    int8_t  best_score = output_data[0];
    uint8_t best_class = 0;

    for (uint8_t c = 1; c < BEAT_FOLLOWER_N_BPM_CLASSES; ++c) {
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
    result.valid           = true;
    result.bpm_index       = best_class;
    result.bpm             = static_cast<float>(kBeatFollowerBpmClasses[best_class]);
    result.confidence      = confidence;
    result.beat_period_ms  = static_cast<uint32_t>(60000.0f / result.bpm + 0.5f);

    return result;
}

// ---------------------------------------------------------------------------
// reset()
// ---------------------------------------------------------------------------

void BeatFollower::reset() {
    _onset_head  = 0;
    _onset_count = 0;
    memset(_onset_buffer, 0, sizeof(_onset_buffer));
}

// ---------------------------------------------------------------------------
// arena_used_bytes()
// ---------------------------------------------------------------------------

size_t BeatFollower::arena_used_bytes() const {
    if (!_initialized) {
        return 0;
    }
    return reinterpret_cast<const tflite::MicroInterpreter*>(_interpreter)
        ->arena_used_bytes();
}

// ---------------------------------------------------------------------------
// _compute_ioi_histogram() — interno
// ---------------------------------------------------------------------------

void BeatFollower::_compute_ioi_histogram(float histogram_out[BEAT_FOLLOWER_N_IOI_BINS]) const {
    // El buffer circular almacena timestamps en orden de inserción:
    //   Si _onset_count < BUFFER_SIZE: los onsets válidos están en
    //     _onset_buffer[0 .. _onset_count-1] en orden cronológico.
    //   Si _onset_count == BUFFER_SIZE (buffer lleno): el onset más antiguo
    //     está en _onset_buffer[_onset_head] (el próximo slot de escritura).
    //
    // Estrategia: reconstruir el orden cronológico usando aritmética circular.

    float sum = 0.0f;

    if (_onset_count < 2) {
        // Menos de 2 onsets → no hay ningún IOI que calcular
        return;
    }

    // El onset más antiguo está en índice:
    //   - Si buffer no está lleno: índice 0
    //   - Si buffer está lleno: índice _onset_head (que es el próximo slot de escritura,
    //     es decir, el slot que contiene el onset más antiguo en el buffer lleno)
    uint8_t oldest;
    uint8_t n;

    if (_onset_count < BEAT_FOLLOWER_ONSET_BUFFER_SIZE) {
        oldest = 0;
        n      = _onset_count;
    } else {
        oldest = _onset_head;  // el slot de escritura es el más antiguo en buffer lleno
        n      = BEAT_FOLLOWER_ONSET_BUFFER_SIZE;
    }

    // Calcular IOIs iterando sobre los onsets en orden cronológico
    for (uint8_t i = 1; i < n; ++i) {
        uint8_t prev_idx = (oldest + i - 1) % BEAT_FOLLOWER_ONSET_BUFFER_SIZE;
        uint8_t curr_idx = (oldest + i)     % BEAT_FOLLOWER_ONSET_BUFFER_SIZE;

        uint32_t prev_ts = _onset_buffer[prev_idx];
        uint32_t curr_ts = _onset_buffer[curr_idx];

        // Protección contra overflow de millis() (rollover cada ~49 días)
        // y contra timestamps fuera de orden (no deberían ocurrir, pero por robustez)
        if (curr_ts <= prev_ts) {
            continue;  // skip IOI inválido
        }

        float ioi_ms = static_cast<float>(curr_ts - prev_ts);

        // Mapear IOI al bin del histograma
        // bin = clamp((ioi_ms - IOI_MIN_MS) / bin_width, 0, N_BINS-1)
        float raw_bin = (ioi_ms - BEAT_FOLLOWER_IOI_MIN_MS) / kBinWidth;
        int bin_idx;
        if (raw_bin < 0.0f) {
            bin_idx = 0;
        } else if (raw_bin >= BEAT_FOLLOWER_N_IOI_BINS) {
            bin_idx = BEAT_FOLLOWER_N_IOI_BINS - 1;
        } else {
            bin_idx = static_cast<int>(raw_bin);
        }

        histogram_out[bin_idx] += 1.0f;
        sum += 1.0f;
    }

    // Normalizar el histograma para que sume 1.0
    // (convierte a distribución de probabilidad, invariante al número de onsets)
    if (sum > 0.0f) {
        for (int i = 0; i < BEAT_FOLLOWER_N_IOI_BINS; ++i) {
            histogram_out[i] /= sum;
        }
    }
}

#endif  // !NATIVE_TEST

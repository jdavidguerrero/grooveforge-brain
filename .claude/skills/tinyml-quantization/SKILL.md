# Skill: TinyML Quantization

Pipeline completo para llevar modelos de música/MIDI desde training en Python hasta
inferencia INT8 en Teensy 4.1 con TFLite Micro. Cross-ref: `apps/docs/04-ai-architecture.md`.

---

## Pipeline completo

```
Dataset (Lakh MIDI / Maestro / Million Song)
    ↓ 1. Preprocessing
Feature extraction (librosa, mido) → numpy arrays
    ↓ 2. Training
TensorFlow/Keras model (Float32)
    ↓ 3. Quantización INT8
TFLiteConverter con representative dataset
    ↓ 4. Validación
Accuracy delta + memory footprint + latency bench
    ↓ 5. Embedding
xxd → C array header → bundle en firmware
    ↓ 6. Runtime
TFLite Micro + CMSIS-NN en Teensy 4.1
```

---

## 1. Preprocessing

```python
# apps/training/utils/midi_features.py
import mido
import numpy as np
import librosa

def extract_pitch_class_distribution(midi_path: str) -> np.ndarray:
    """Krumhansl-Schmuckler key-finding feature vector."""
    mid = mido.MidiFile(midi_path)
    pitch_counts = np.zeros(12)
    for track in mid.tracks:
        for msg in track:
            if msg.type == 'note_on' and msg.velocity > 0:
                pitch_counts[msg.note % 12] += 1
    total = pitch_counts.sum()
    return pitch_counts / total if total > 0 else pitch_counts

def extract_chroma(audio_path: str, sr: int = 22050) -> np.ndarray:
    """12-dim chroma feature para chord recognition."""
    y, sr = librosa.load(audio_path, sr=sr)
    chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
    return chroma.mean(axis=1)  # (12,) feature vector
```

---

## 2. Training (TensorFlow/Keras)

```python
# apps/training/models/key_detector/train.py
import tensorflow as tf
import numpy as np

def build_model(input_dim: int = 12, num_classes: int = 24) -> tf.keras.Model:
    """Modelo pequeño para key detection — cabe en 12KB INT8."""
    return tf.keras.Sequential([
        tf.keras.layers.Input(shape=(input_dim,)),
        tf.keras.layers.Dense(32, activation='relu'),
        tf.keras.layers.Dense(16, activation='relu'),
        tf.keras.layers.Dense(num_classes, activation='softmax'),
    ])

model = build_model()
model.compile(
    optimizer='adam',
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)
model.fit(X_train, y_train, epochs=50, validation_data=(X_val, y_val))

# Verificar accuracy antes de quantizar
_, accuracy = model.evaluate(X_test, y_test)
print(f"Float32 accuracy: {accuracy:.3f}")  # Target: >0.85
```

---

## 3. Quantización INT8

```python
# apps/training/utils/quantize.py
import tensorflow as tf

def quantize_model(
    model: tf.keras.Model,
    representative_data: np.ndarray,
    output_path: str
) -> bytes:
    """
    Cuantiza a INT8 con calibración por representative dataset.
    La calibración es CRÍTICA — sin ella la accuracy cae mucho más.
    """
    converter = tf.lite.TFLiteConverter.from_keras_model(model)

    # Flags obligatorios para INT8 full
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    # Representative dataset: muestras del training set (100-500 samples)
    def representative_dataset_gen():
        for sample in representative_data[:500]:
            yield [sample.reshape(1, -1).astype(np.float32)]

    converter.representative_dataset = representative_dataset_gen

    tflite_model = converter.convert()

    with open(output_path, 'wb') as f:
        f.write(tflite_model)

    print(f"Model size: {len(tflite_model) / 1024:.1f} KB")
    return tflite_model
```

---

## 4. Validación

```python
# apps/training/utils/validate.py

def validate_quantized_model(
    float_model: tf.keras.Model,
    tflite_path: str,
    X_test: np.ndarray,
    y_test: np.ndarray
) -> dict:
    """Valida accuracy delta y memory footprint."""

    # Accuracy Float32
    _, float_acc = float_model.evaluate(X_test, y_test, verbose=0)

    # Accuracy INT8 (via TFLite interpreter)
    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    correct = 0
    scale, zero_point = input_details[0]['quantization']

    for x, y in zip(X_test, y_test):
        x_q = (x / scale + zero_point).astype(np.int8).reshape(1, -1)
        interpreter.set_tensor(input_details[0]['index'], x_q)
        interpreter.invoke()
        pred = interpreter.get_tensor(output_details[0]['index']).argmax()
        if pred == y:
            correct += 1

    int8_acc = correct / len(y_test)
    delta = float_acc - int8_acc
    model_size_kb = os.path.getsize(tflite_path) / 1024

    results = {
        'float32_accuracy': float_acc,
        'int8_accuracy': int8_acc,
        'accuracy_delta': delta,
        'model_size_kb': model_size_kb,
    }

    # Targets de 04-ai-architecture.md §8
    assert delta < 0.05, f"Accuracy delta {delta:.3f} excede límite 5%"
    assert int8_acc > 0.85, f"INT8 accuracy {int8_acc:.3f} no alcanza target 85%"
    assert model_size_kb < 512, f"Modelo {model_size_kb:.1f}KB excede budget"

    return results
```

---

## 5. Embedding en firmware

```bash
# Desde apps/training/
python -m training.export key_detector  # genera el .tflite

# Generar C array
xxd -i models/key_detector/model.tflite > \
    ../firmware-teensy/src/ml/key_detector_model.h

# El header generado tiene esta forma:
# unsigned char models_key_detector_model_tflite[] = { 0x1c, 0x00, ... };
# unsigned int models_key_detector_model_tflite_len = 12288;
```

En el header del firmware, siempre agregar `const` y `PROGMEM` (para flash):
```cpp
// apps/firmware-teensy/src/ml/key_detector_model.h
// Editar el xxd output para agregar:
const unsigned char key_detector_model[] PROGMEM = { 0x1c, 0x00, ... };
const unsigned int key_detector_model_len = 12288;
```

---

## 6. Runtime en Teensy (TFLite Micro)

```cpp
// apps/firmware-teensy/src/ml/key_detector.cpp
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "key_detector_model.h"

// Tensor arena: compartida entre todos los modelos (run secuencialmente)
constexpr int kTensorArenaSize = 200 * 1024;  // 200KB — 04-ai-architecture §1.2
alignas(16) uint8_t tensor_arena[kTensorArenaSize];

class KeyDetector {
public:
    bool init() {
        model_ = tflite::GetModel(key_detector_model);
        resolver_.AddFullyConnected();
        resolver_.AddSoftmax();
        interpreter_ = new tflite::MicroInterpreter(
            model_, resolver_, tensor_arena, kTensorArenaSize);
        return interpreter_->AllocateTensors() == kTfLiteOk;
    }

    int8_t predict(const float* pitch_class_dist, uint8_t& confidence) {
        // Quantizar input
        auto* input = interpreter_->input(0);
        float scale = input->params.scale;
        int zero_point = input->params.zero_point;
        for (int i = 0; i < 12; i++) {
            input->data.int8[i] = (int8_t)(pitch_class_dist[i] / scale + zero_point);
        }
        interpreter_->Invoke();
        auto* output = interpreter_->output(0);
        int8_t max_val = output->data.int8[0];
        int8_t predicted_key = 0;
        for (int i = 1; i < 24; i++) {
            if (output->data.int8[i] > max_val) {
                max_val = output->data.int8[i];
                predicted_key = i;
            }
        }
        confidence = (uint8_t)(max_val + 128);  // dequantize a 0-255
        return predicted_key;
    }

private:
    const tflite::Model* model_;
    tflite::MicroMutableOpResolver<4> resolver_;
    tflite::MicroInterpreter* interpreter_;
};
```

---

## Checklist de validación antes de merge

- [ ] Float32 accuracy > 85%
- [ ] INT8 accuracy > 85%
- [ ] Accuracy delta < 5%
- [ ] Model size ≤ budget de 04-ai-architecture.md §1.3 (escala por modelo)
- [ ] Tensor arena ≤ 200KB total para todos los modelos activos
- [ ] `xxd` header generado con `const` + `PROGMEM`
- [ ] `interpreter_->AllocateTensors()` retorna `kTfLiteOk` en Teensy físico
- [ ] Inference time medido on-device con `micros()` < 20ms p99

---

## Referencias

- TFLite Micro docs: https://www.tensorflow.org/lite/microcontrollers
- CMSIS-NN: https://arm-software.github.io/CMSIS_5/NN/html/index.html
- Lakh MIDI Dataset: https://colinraffel.com/projects/lmd/
- Maestro Dataset: https://magenta.tensorflow.org/datasets/maestro
- `apps/docs/04-ai-architecture.md` — spec completo de modelos y budgets

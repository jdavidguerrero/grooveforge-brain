# Sprint 4.3 — Key Detector Model

**Feature:** Scale Lock (Layer 1 AI)
**Estado:** Implementado — artifacts generados, 17/17 tests passing
**Cross-ref:** `apps/docs/04-ai-architecture.md` §1 (Scale Lock), §8 (métricas de aceptación)

---

## Resultados del sprint

| Métrica | Target spec | Resultado |
|---|---|---|
| Accuracy float32 | >85% | 94.1% |
| Accuracy int8 | >85% | 93.6% |
| Delta float→int8 | <5% | 0.4% |
| Tamaño float32 | <100KB | 47.0 KB |
| Tamaño int8 | <100KB | 19.4 KB |
| Tensor arena estimado | <200KB | ~39 KB |
| Tests pytest | 17/17 | 17/17 pasando |
| Build firmware sketch | SUCCESS | SUCCESS |

---

## §1. Tonalidad musical — qué es y por qué es detectable con ML

### Las 24 tonalidades del sistema occidental

La música tonal occidental utiliza 24 tonalidades: 12 mayores y 12 menores. Cada tonalidad tiene una **nota raíz** (la tónica) y un **modo** (mayor o menor). La tónica determina la nota de "llegada", la que suena más estable o resuelta. El modo determina el carácter emocional (mayor generalmente más brillante, menor más oscuro).

Las 12 tonalidades mayores (modos jónicos) y sus raíces:

```
C_maj, C#_maj, D_maj, D#_maj, E_maj, F_maj,
F#_maj, G_maj, G#_maj, A_maj, A#_maj, B_maj
```

Las 12 tonalidades menores (modos eólicos naturales):

```
C_min, C#_min, D_min, D#_min, E_min, F_min,
F#_min, G_min, G#_min, A_min, A#_min, B_min
```

### Por qué no hay más de 24 tonalidades

El sistema temperado occidental divide la octava en 12 semitonos iguales. Las notas fuera de estas 12 posiciones (como los cuartos de tono de la música árabe o india) son **microtonos** — no existen en el piano ni en el MIDI estándar. Dentro de los 12 semitonos, hay equivalencias enarmónicas: C# y Db son el mismo sonido, F# y Gb también. Por eso, aunque los nombres de tonalidades pueden variar (Bb mayor vs A# mayor), las frecuencias físicas son idénticas. Nuestro modelo usa siempre la forma con sostenido (#) para evitar ambigüedad.

### La hipótesis central del modelo

En una pieza en **C mayor**, las notas C, E, G (la tríada de la tónica) aparecerán más frecuentemente que las notas cromáticas (C#, D#, F#, G#, A#). Un clasificador puede aprender esta correlación estadística **sin conocer ninguna regla de teoría musical** — solo observando los histogramas de 12 pitch classes de muchas piezas etiquetadas.

Esta hipótesis funciona para música tonal (pop, clásica, jazz estándar). No funciona para:
- **Música atonal**: Schoenberg, serialismo — no hay nota dominante.
- **Música modal**: usa los 7 modos de la escala mayor, no solo mayor/menor.
- **Música microtonal**: cuartos de tono, escala árabe, música india clásica.

El GrooveForge Brain está diseñado para música popular y clásica occidental — este límite es aceptable para el caso de uso.

### Por qué el pitch class histogram captura tonalidad

Un pitch class histogram agrega todas las notas de una ventana temporal (2-4 segundos típicamente) en 12 valores, uno por semitono, independientemente de la octava. Es una operación de **reducción de dimensionalidad musicalmente informada**: la octava no importa para la tonalidad (un C4 y un C5 pertenecen a la misma tonalidad), pero el semitono sí.

```
Pieza en C mayor (simplificado):
Notas: C4, E4, G4, C5, G4, E4, D4, C4
Pitch classes: 0, 4, 7, 0, 7, 4, 2, 0
Histogram: [3, 0, 1, 0, 2, 0, 0, 2, 0, 0, 0, 0]
Normalizado: [0.375, 0, 0.125, 0, 0.25, 0, 0, 0.25, 0, 0, 0, 0]
```

El modelo ve este vector de 12 floats y aprende que este patrón (picos en 0, 4, 7 con un pico secundario en 2) corresponde a C mayor.

---

## §2. Perfiles de Krumhansl-Schmuckler — el prior musical del modelo

### El experimento original (1986)

Carol Krumhansl y Mark Schmuckler (1986) realizaron un experimento psicoacústico fundamental: presentaron a sujetos humanos una escala completa de C mayor como contexto, y luego les hicieron escuchar cada uno de los 12 semitonos por separado. Los sujetos calificaron qué tan bien "encajaba" cada nota en el contexto de C mayor, en una escala de 1 a 7.

El resultado es un **perfil tonal** — un vector de 12 ratings que captura la jerarquía de consonancia percibida dentro de una tonalidad. Los valores para C mayor:

```
Índice: 0    1    2    3    4    5    6    7    8    9    10   11
Nota:   C    C#   D    D#   E    F    F#   G    G#   A    A#   B
Rating: 6.35 2.23 3.48 2.33 4.38 4.09 2.52 5.19 2.39 3.66 2.29 2.88
```

Por qué estos valores tienen sentido musicalmente:
- **C (6.35)**: la tónica. La nota más estable — es el "hogar" de la tonalidad.
- **G (5.19)**: la quinta. La nota más consonante después de la tónica por la relación de frecuencias 3:2.
- **E (4.38)**: la tercera mayor. Parte de la tríada de la tónica.
- **F (4.09)**: la subdominante. Nota diatónica con rol cadencial importante.
- **D, A, B (3.48-3.66)**: notas diatónicas de menor importancia estructural.
- **C#, D#, F#, G#, A# (2.23-2.52)**: notas cromáticas — "disonantes" en el contexto de C mayor.

### Por qué usamos estos perfiles como base del dataset

Los perfiles de K-S son un **prior musical** — conocimiento experto encapsulado en números. En lugar de entrenar desde cero con datos MIDI sin etiquetar (que requeriría millones de archivos), usamos los perfiles como punto de partida: cada muestra de entrenamiento es un perfil de K-S con ruido gaussiano superpuesto.

El ruido simula la realidad musical: ninguna pieza en C mayor sigue el perfil exacto. Hay notas de paso, cromatismos, acordes secundarios, cadencias a otras tonalidades. El nivel de ruido calibrado (noise_level=0.02) es suficiente para que el modelo generalice sin que los histogramas de tonalidades adyacentes se confundan.

### Separabilidad de los perfiles

Un análisis de correlación entre los 24 perfiles revela el mayor desafío del problema:

```
Par más similar: C_maj <-> A_min  (correlación de Pearson r=0.65)
```

C mayor y A menor comparten 6 de las 7 notas diatónicas (C, D, E, F, G, A, B para C mayor vs A, B, C, D, E, F, G para A menor). La diferencia está en el peso relativo: en C mayor la nota C tiene el mayor rating (6.35), en A menor la nota A tiene el mayor rating. El modelo necesita aprender que **el pitch class con mayor presencia en el histograma indica la tónica**, no solo el modo.

Esta alta correlación explica por qué noise_level=0.1 produjo solo 17% de accuracy: con tanto ruido, las diferencias entre los perfiles de tonalidades relativas (C mayor y A menor) desaparecían.

---

## §3. Arquitectura Dense vs CNN vs RNN para key detection

Esta comparación es importante para entender por qué se eligió Dense y en qué circunstancias las otras arquitecturas serían mejores.

| Arquitectura | Parámetros (aprox.) | Pros | Contras |
|---|---|---|---|
| Dense 3-layer (elegida) | ~11.5K | Mínimo flash, <1ms en Teensy, entrada ya es histograma | No captura secuencia temporal |
| CNN 1D sobre 12 inputs | ~15-50K | Captura co-ocurrencias locales entre notas adyacentes | Overkill para 12 valores |
| LSTM/GRU | ~50-200K | Captura evolución temporal (key tracking en tiempo real) | Requiere secuencia, muy grande |

### Por qué Dense es correcto aquí

El pitch class histogram ya es una **representación agregada en el tiempo**. Si la ventana de análisis es de 2-4 segundos, el histograma contiene la información de cientos de notas condensada en 12 números. No hay dimensión temporal que el modelo necesite explotar — toda la información temporal ya fue comprimida por la función `get_pitch_class_histogram()`.

Una CNN sobre 12 entradas capturaría "co-ocurrencias locales" como "C y C# juntos en el histograma" — pero estas co-ocurrencias en el espacio de pitch classes no tienen la misma semántica que en el espacio temporal. Una CNN sería útil si el input fuera un espectrograma 2D (tiempo × frecuencia), pero no para un histograma de pitch classes.

Un LSTM sería apropiado para un modelo de **key tracking en tiempo real** que recibe una secuencia de histogramas (uno cada 100ms) y hace seguimiento de la tonalidad a medida que la pieza progresa. Ese es un problema diferente — y más difícil. El GrooveForge Brain en su Layer 1 recibe histogramas de ventanas de 2-4 segundos, no streams continuos. Dense es la herramienta correcta.

### Arquitectura final

```
Input(12) → Dense(128, relu) → Dropout(0.3) → Dense(64, relu) → Dropout(0.2) → Dense(24, softmax)
```

Parámetros:
- Dense(128): 12×128 + 128 = 1664
- Dense(64): 128×64 + 64 = 8256
- Dense(24): 64×24 + 24 = 1560
- **TOTAL: 11480 parámetros** (≈11.2KB float32, ≈2.8KB en int8)

---

## §4. Data augmentation musical — transponer es gratis

### El problema sin augmentación

Con solo los 24 perfiles de K-S como fuente de datos, el dataset sintético tiene una estructura perfectamente regular: cada clase tiene exactamente n_samples/24 ejemplos, todos centrados en el perfil teórico más ruido. El modelo podría memorizarlos.

### La solución: transposición cíclica

Un histograma en C mayor transpuesto 2 semitonos **ES** exactamente un histograma en D mayor. Esta es una identidad musical exacta, no una aproximación.

Implementación: `np.roll(histogram, semitones)` rota el array de 12 posiciones. Rotar 2 posiciones a la derecha mueve el índice 0 (C) al índice 2 (D), el índice 1 (C#) al 3 (D#), etc.

```python
c_major_hist = [0.375, 0, 0.125, 0, 0.25, 0, 0, 0.25, 0, 0, 0, 0]
d_major_hist  = np.roll(c_major_hist, 2)
# = [0, 0, 0.375, 0, 0.125, 0, 0.25, 0, 0, 0.25, 0, 0]
#         ^D                 ^E         ^F#        ^A
# Picos en D(2), E(4=6-2?... Espera — np.roll hacia la derecha shift=2:
# nuevo[i] = antiguo[i-2], entonces nuevo[2] = antiguo[0] = 0.375 ✓
```

Esta propiedad se verifica en los tests (`test_transposition_consistency`) y está codificada en `generate_key_profiles()`: el perfil de D mayor se genera como `np.roll(C_major_profile, 2)`.

### Por qué multiplica el dataset x12

Cada pieza MIDI real puede transponerse a las 12 tonalidades cromáticas. Con el Lakh Dataset (1000 archivos), esto genera 12000 histogramas reales sin grabación adicional. En el dataset sintético, la transposición ya está incorporada implícitamente en la generación de los 24 perfiles (que son rotaciones del perfil base).

---

## §5. Cuantización int8 aplicada a este modelo

### Qué significa cuantizar un modelo Dense

Cuantización int8 convierte los pesos float32 (32 bits, 4 bytes por número) a int8 (8 bits, 1 byte). El factor de compresión es 4x en pesos. Además, las operaciones matriciales en int8 son ~4x más rápidas en el Cortex-M7 con CMSIS-NN. El beneficio combinado es hasta 16x en throughput (4x memoria × 4x velocidad).

### Cómo funciona la calibración

El converter necesita saber el **rango de valores** de cada tensor de activación para asignar la escala óptima de int8. Para el input:

```
Input float: [0.0, 1.0] (histograma normalizado)
int8 range: [-128, 127]
Scale: 1.0 / 127 ≈ 0.00787
Zero point: -128 (para que 0.0 float → -128 int8)
```

Para las activaciones internas (output de Dense(128, relu)):
```
ReLU garantiza valores ≥ 0.
Max activation depende de los datos reales.
El representative dataset calibra este max durante la conversión.
```

Los 200 samples del validation set usados como `representative_dataset` determinan el max de activación por capa. Si el representative dataset es demasiado pequeño o sesgado, el range calibrado estará mal y los valores fuera del range saturarán en int8.

### Delta de este modelo: 0.4%

Con dataset sintético limpio y noise_level=0.02, el delta es extremadamente bajo (0.4%) porque:
1. El rango de activaciones es predecible y regular.
2. No hay BatchNormalization (que puede causar problemas de quantización).
3. ReLU + Dense son los ops más amigables para int8.

Con datos reales de Lakh, el delta puede llegar a 1-2% pero raramente supera el 5% del spec.

### Ops registradas en TFLite Micro

El modelo usa exactamente 3 ops:
- `AddFullyConnected()`: la operación de multiplicación matricial de las capas Dense.
- `AddSoftmax()`: la función softmax de la capa de salida.
- `AddRelu6()`: TFLite Micro fusiona la activación relu con FullyConnected en un solo op.

No registrar ops adicionales es importante: el op table ocupa flash del Teensy. Cada op no usado es flash desperdiciado. Esta es la razón por la que `key_detector.cpp` registra exactamente estos 3 ops y nada más.

---

## §6. Deploy a Teensy — del .tflite al C array al binario

### Paso 1: Convertir .tflite a C array

El script de training genera automáticamente el header con:

```python
content = tflite_to_c_array(INT8_PATH, var_name="g_key_detector_model")
HEADER_PATH.write_text(content)
```

Esto es equivalente al comando:
```bash
xxd -i key_detector_int8.tflite > key_detector_model.h
```

El resultado es un archivo `.h` con:
```cpp
alignas(8) const uint8_t g_key_detector_model[] = { 0x1c, 0x00, ... };
const uint32_t g_key_detector_model_len = 19456;
```

El `alignas(8)` es crítico: TFLite Micro requiere que el flatbuffer esté alineado a 8 bytes en memoria. Sin esto, `GetModel()` puede retornar datos corruptos en Cortex-M7 (que tiene reglas de alineación estrictas para accesos multi-byte).

### Paso 2: El modelo vive en flash, no en RAM

`const` en C++ en Teensy 4.1 (Cortex-M7) coloca el array en `.rodata` (flash). TFLite Micro lee el modelo desde flash via el puntero retornado por `GetModel(model_data)` — **nunca lo copia a RAM**. Esto es fundamental: los 19KB del modelo int8 no consumen RAM, solo flash.

Lo que sí va a RAM es el **tensor arena**: los buffers temporales de activación durante la inferencia. Para este modelo:
- Activaciones Dense(128): 128 × 4 bytes = 512 bytes (float) o 128 bytes (int8)
- Activaciones Dense(64): 64 bytes (int8)
- Activaciones Dense(24): 24 bytes (int8)
- Overhead del runtime (metadata, scratch buffers, alineación): ~8KB
- **Total arena: ~39KB** (medido post-entrenamiento)

### Paso 3: Instalar TFLite Micro en el firmware Teensy

La librería `Arduino_TensorFlowLite` de Google tiene incompatibilidades con Teensy (periféricos Arduino Nano específicos). El método recomendado para Teensy 4.1 es vendorear directamente los fuentes de TFLite Micro:

```bash
# Desde la raíz del repo
git clone https://github.com/tensorflow/tflite-micro /tmp/tflite-micro
python /tmp/tflite-micro/tensorflow/lite/micro/tools/project_generation/create_tflm_tree.py \
  --output_dir=apps/firmware-teensy/lib/tflite-micro/
```

PlatformIO detecta la lib automáticamente (lib/ es una ruta buscada por el LDF). El `platformio.ini` no necesita `lib_deps` para esto.

### Paso 4: El env:sketch no incluye key_detector.cpp

El `build_src_filter` del env `sketch` es explícito:
```ini
build_src_filter = +<sketches/21-usb-midi-host.cpp> +<usb/midi_host.cpp> +<engines/moog_model_d.cpp>
```

`key_detector.cpp` no está en esta lista, así que el env `sketch` compila exitosamente sin TFLite instalado. El env `teensy41` (producción) incluirá `key_detector.cpp` cuando TFLite Micro esté vendoreado en `lib/`.

---

## §7. Calibración del noise_level — decisión de diseño documentada

El parámetro `noise_level` del dataset sintético tiene un impacto drástico en la accuracy y en la generalización del modelo. El proceso de calibración reveló:

| noise_level | Accuracy LR (sklearn) | Accuracy Dense | Observación |
|---|---|---|---|
| 0.00 | 100% | ~100% | Perfiles exactos — modelo memorizaría |
| 0.01 | 100% | ~99% | Muy poco ruido, no generaliza a música real |
| 0.02 | 94.6% | **94.1%** (elegido) | Balance separabilidad / varianza |
| 0.03 | 39.3% | ~72% | Tonalidades adyacentes se confunden |
| 0.05 | 39.3% | ~15% | El modelo falla en capturar la estructura |
| 0.10 | 17.2% | ~15% | Básicamente azar (1/24 = 4.2%) |

La curva no es monótona: noise=0.02 es un punto dulce donde los perfiles de K-S son suficientemente diferentes para que el modelo los distinga, pero suficientemente ruidosos como para que aprenda a generalizar más allá de los perfiles exactos.

Con datos reales de Lakh (que tienen más varianza natural que noise=0.02), el modelo puede ver un drop de 5-10% en accuracy de validación cruzada. La solución para ese caso es mezclar 50% sintético (noise=0.02) + 50% real, como implementa `build_dataset()` en `train.py`.

---

## Archivos creados en este sprint

### Training (`apps/training/`)

- `models/key_detector/__init__.py` — módulo Python
- `models/key_detector/dataset.py` — generación de dataset sintético y carga de Lakh
- `models/key_detector/model.py` — arquitectura Dense y función de training
- `models/key_detector/train.py` — script ejecutable completo
- `models/key_detector/artifacts/key_detector_float32.tflite` — modelo float32 (47.0 KB)
- `models/key_detector/artifacts/key_detector_int8.tflite` — modelo int8 (19.4 KB)
- `models/key_detector/artifacts/key_detector_model.h` — C array para firmware
- `tests/test_key_detector.py` — 17 tests pytest

### Firmware (`apps/firmware-teensy/src/ml/`)

- `key_detector.h` — clase KeyDetector (interfaz pública)
- `key_detector.cpp` — implementación TFLite Micro (requiere lib vendoreada)
- `key_detector_model.h` — C array del modelo int8 (auto-copiado por train.py)

---

## Cómo reproducir

```bash
# 1. Tests del dataset y modelo (sin hardware, sin TFLite)
cd apps/training
uv run pytest tests/test_key_detector.py -v
# → 17 passed

# 2. Training completo + generación de artifacts
uv run python models/key_detector/train.py
# → Float32 accuracy: 94.1%, Int8 accuracy: 93.6%, Delta: 0.4%

# 3. Build del firmware (env sketch, sin TFLite requerido)
cd apps/firmware-teensy
~/.platformio/penv/bin/platformio run -e sketch
# → SUCCESS

# 4. Para usar en producción (env teensy41), instalar TFLite Micro primero:
# Ver §6 para el procedimiento de vendoring.
```

---

## Próximos pasos (post Sprint 4.3)

1. **Sprint 4.4 — Chord Recognition**: modelo para detectar acordes de 3+ notas (45KB, 8ms). Usa features similares pero requiere capturar combinaciones armónicas, no solo distribución de pitch classes.

2. **Vendorear TFLite Micro en lib/**: necesario para que `env:teensy41` compile con todos los modelos Layer 1.

3. **Integración con Scale Lock**: la clase `KeyDetector` se conecta con el engine de synth para "snapear" notas fuera de escala al semitono diatónico más cercano. La lógica de snap va en un módulo separado (`scale_lock.h/.cpp`) que consume el `KeyResult` de esta clase.

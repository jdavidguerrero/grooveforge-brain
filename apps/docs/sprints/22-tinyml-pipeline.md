# Sprint 22 — TinyML Training Pipeline Setup

**Sprint:** 4.2  
**Feature:** Infraestructura de training para Layer 1 AI (Scale Lock, Chord Recognition, Beat Follower)  
**Hito demostrable:** `uv run pytest tests/ -v` pasa en verde. Notebook carga archivos MIDI reales y muestra pitch class histogramas. Pipeline de conversión genera C arrays compilables.  
**Tiempo estimado:** 10-14h  
**Cross-ref:** `apps/docs/04-ai-architecture.md` §1.2, §1.5, §8

---

## Theory

### §1. Qué es TinyML y por qué el Teensy 4.1 puede correrlo

TinyML es ML inference (no training) corriendo en microcontroladores con menos de 1MB de RAM, sin sistema operativo, sin FPU potente y frecuentemente sin conexión a internet. El énfasis en "inference" es la distinción clave: los MCU corren el modelo ya entrenado, no aprenden.

El Teensy 4.1 corre un ARM Cortex-M7 a 600MHz con 1MB de SRAM y 8MB de flash. Lo que lo pone en la frontera de lo posible para TinyML es la combinación de tres propiedades:

1. **FPU de doble precisión:** el Cortex-M7 tiene unidad de punto flotante real (no emulada). Para INT8 quantizado ni siquiera la usa — usa las instrucciones DSP del SIMD.
2. **1MB de RAM estática:** suficiente para los tensor arenas de los seis modelos Layer 1 corriendo secuencialmente.
3. **CMSIS-NN:** la librería de ARM para inferencia neural cuantizada en Cortex-M. Reemplaza operaciones de convolución y dense layers con instrucciones SIMD optimizadas para Cortex-M4/M7. El mismo runtime, 3-4x más rápido.

La analogía musical: **training es aprender a reconocer un acorde.** Escuchás C-E-G mil veces con un profesor que te dice "eso es C mayor", hasta que tu cerebro forma el patrón. Eso tarda horas y requiere mucha capacidad cognitiva. **Inference es reconocer el acorde en tiempo real.** Una vez que lo aprendiste, el reconocimiento es instantáneo, automático, usa poca energía. El Teensy solo hace lo segundo.

El training lo hace tu computadora (o Google Colab) con TensorFlow durante horas. El resultado es un archivo `.tflite` de 12KB a 100KB que se copia al Teensy como un array de bytes. El Teensy lo carga en flash y lo ejecuta en 4-18ms por inferencia.

---

### §2. Por qué TensorFlow/TFLite Micro y no PyTorch

La respuesta honesta es que ninguno es superior en matemáticas. La diferencia es el ecosistema embebido en 2025-2026:

| Aspecto | TensorFlow / TFLite Micro | PyTorch / ExecuTorch |
|---|---|---|
| Soporte en Cortex-M7 bare-metal | Maduro. TFLite Micro existe desde 2019, miles de deployments en producción en MCU. | En desarrollo activo. ExecuTorch (2023) es prometedor pero pocos deployments reales en bare-metal a esta escala. |
| Cuantización INT8 | `tf.lite.TFLiteConverter` con INT8 completo en dos líneas. Flujo documentado, probado, con herramientas de debugging. | Quantization API existe pero el flujo para MCU es más manual y menos documentado. |
| C++ runtime para MCU | TFLite Micro: ~30KB de flash, sin heap dinámico, sin excepciones C++. Diseñado explícitamente para bare-metal. | ExecuTorch: más pesado, pensado primero para edge computing (móviles, RPi) más que MCU. |
| Dinamismo en training | Grafos estáticos (default) son más rigurosos. Eager mode disponible para debugging. | Grafos dinámicos (`autograd`) son más fáciles de debuggear durante training. |
| El modelo final | Estático igual. El dinamismo de PyTorch no sobrevive la exportación a TFLite o a ExecuTorch. Una vez exportado, el grafo es fijo. | Mismo resultado final — el grafo dinámico es solo útil durante el desarrollo. |
| Ecosistema musical | Sin ventaja de ninguno. Librosa, music21 y pretty_midi son agnósticas al framework. | Idem. |

**Conclusión:** para Cortex-M7 bare-metal en 2026, TFLite Micro es el camino más seguro. No porque PyTorch sea peor — en muchos contextos es superior. Sino porque el ecosistema embebido de TFLite Micro tiene 3-4 años más de madurez en el tipo exacto de deployment que estamos haciendo.

Si en 18 meses ExecuTorch tiene documentación equivalente y deployments probados en Cortex-M7 bare-metal, la evaluación cambia. Esta es una decisión de ecosistema, no una decisión matemática.

---

### §3. Cuantización INT8 — qué es, por qué duele y por qué vale la pena

Esta es la parte más técnicamente densa del sprint. Tomarse el tiempo de entenderla bien paga dividendos en todos los modelos futuros.

#### El problema de memoria

Una red neuronal en Float32 usa 4 bytes por parámetro (peso o sesgo). Un modelo modesto de 50,000 parámetros ocupa 200KB. El budget total de flash para **todos los modelos Layer 1** es 500KB (`04-ai-architecture.md §1.2`). Con 6 modelos, el promedio disponible es ~83KB por modelo. En Float32, un modelo de 83KB tendría solo 20,750 parámetros — muy pequeño para tareas de reconocimiento musical.

En INT8 (1 byte por parámetro), 83KB = 83,000 parámetros. Cuatro veces más capacidad en el mismo espacio. La cuantización no es optimización prematura — es condición de viabilidad del proyecto.

#### El mapa lineal

El problema central de la cuantización es el siguiente: Float32 tiene rango [-3.4×10³⁸, 3.4×10³⁸] con 23 bits de mantisa (alta precisión). INT8 tiene rango [-128, 127] con 8 bits (baja precisión). ¿Cómo mapear uno al otro sin perder información importante?

La respuesta es un mapa lineal por capa:

```
float_value = scale × (int8_value - zero_point)
```

Donde:
- `scale`: cuánto "vale" cada paso entero en el dominio float. Se calcula observando el rango real de los pesos o activaciones de esa capa.
- `zero_point`: qué valor INT8 representa el 0.0 float. Necesario porque el rango de valores de una capa raramente es simétrico alrededor de cero.

**Ejemplo concreto:** una capa tiene pesos con valores entre -0.5 y +0.5.

```
min_float = -0.5,  max_float = +0.5
rango_float = 1.0

scale = rango_float / (127 - (-128)) = 1.0 / 255 ≈ 0.003922
zero_point = 0  (porque el rango es simétrico alrededor de cero)

Peso +0.3 en float → int8:
  int8_value = round(0.3 / 0.003922 + 0) = round(76.5) = 77
  Verificar: 0.003922 × (77 - 0) = 0.3019 ≈ 0.3  ✓

Peso -0.1 en float → int8:
  int8_value = round(-0.1 / 0.003922 + 0) = round(-25.5) = -26
  Verificar: 0.003922 × (-26 - 0) = -0.1020 ≈ -0.1  ✓
```

El error es ±0.002 en este ejemplo — pequeño, pero no cero. Eso es el costo de la cuantización.

#### Per-tensor vs per-channel — por qué importa

TFLite Micro usa **per-channel** para los pesos de capas Dense y Conv: un par `(scale, zero_point)` distinto por cada canal de salida (fila de la matriz de pesos). Esto es más preciso porque los pesos de cada canal tienen distribuciones distintas.

Para las activaciones (los resultados intermedios entre capas), usa **per-tensor**: un solo par para toda la capa. Esto es más eficiente en hardware.

Si se usa per-tensor para todo, la precisión cae. Si se usa per-channel para todo, el hardware no puede aprovecharlo en Cortex-M7 con CMSIS-NN. TFLite Micro elige el balance correcto automáticamente.

#### El representative dataset — por qué es la parte crítica

TFLite puede calcular `scale` y `zero_point` para los **pesos** por sí solo — los tiene en el modelo. Pero para las **activaciones** (salidas de cada capa durante la inferencia) necesita observar valores reales, porque dependen de los datos de entrada.

El `representative_dataset` es una muestra de 100-200 ejemplos del set de entrenamiento que TFLite corre en Float32 durante la conversión. Observa los valores mínimo y máximo de cada activación en cada capa para calibrar su `(scale, zero_point)`.

Si la muestra es:
- **Muy pequeña** (5-10 ejemplos): los rangos observados pueden ser atípicos. La cuantización será imprecisa → accuracy drop mayor.
- **No representativa** (solo un subconjunto sesgado): el modelo aprende a cuantizar bien solo ese subconjunto, mal para el resto.
- **Bien elegida** (100-200 ejemplos random del validation set): el modelo ve el rango real de las activaciones → cuantización precisa → accuracy drop mínimo.

```python
def representative_dataset_gen():
    for sample in val_data[:200]:  # 200 muestras random del validation set
        yield [sample[np.newaxis, :].astype(np.float32)]
```

#### El accuracy delta esperado

Para los modelos de este proyecto:

- **Scale Lock (key detection):** histograma de 12 dimensiones como input, salida de 24 clases (12 mayores + 12 menores). Modelo simple → delta típico <2%.
- **Chord Recognition:** input más denso, modelo más grande → delta típico 1-3%.
- **Beat Follower:** series temporales de onset intervals → delta típico 2-4%.

El spec acepta hasta 5% (`04-ai-architecture.md §1.5`). Si se supera en algún modelo, las causas más comunes son:
1. BatchNorm antes de ReLU en lugar de después — cambia la distribución de activaciones
2. Capas con rangos de activación muy distintos sin clipado previo
3. Representative dataset demasiado pequeño

No culpar a INT8 ciegamente. El problema es casi siempre arquitectural o de calibración.

#### El speedup en Cortex-M7

En Cortex-M7 con CMSIS-NN habilitado:
- Multiplicación float32: ~4 ciclos (FMUL instruction)
- Multiplicación int8×int8: ~1 ciclo (SMULBB con SIMD — procesa múltiples pares en paralelo)

En la práctica, un modelo INT8 corre **3-4x más rápido** que su equivalente Float32 en Cortex-M7. Para el target de <20ms p99, esto es la diferencia entre "posible" e "imposible" para los modelos más grandes (Genre Fingerprint: 100KB, 18ms target).

---

### §4. Pitch class histogram — la representación de entrada para key detection

La octava en que tocás una nota no determina la tonalidad de una pieza. C4 y C3 y C2 son todas "C" — la diferencia es timbral (grave vs agudo), no tonal (qué escala). El **pitch class** captura la identidad tonal descartando la octava:

```
pitch_class = nota_MIDI % 12

Mapping:
  0 = C    (Do)
  1 = C#   (Do sostenido / Re bemol)
  2 = D    (Re)
  3 = D#   (Re sostenido / Mi bemol)
  4 = E    (Mi)
  5 = F    (Fa)
  6 = F#   (Fa sostenido / Sol bemol)
  7 = G    (Sol)
  8 = G#   (Sol sostenido / La bemol)
  9 = A    (La)
  10 = A#  (La sostenido / Si bemol)
  11 = B   (Si)

Ejemplos:
  MIDI 60 = C4 → 60 % 12 = 0
  MIDI 72 = C5 → 72 % 12 = 0
  MIDI 67 = G4 → 67 % 12 = 7
```

El **pitch class histogram** pesa cada pitch class por cuánto tiempo sonó durante la pieza:

```
histogram[pc] += duración_de_la_nota  (en segundos)
```

Después se normaliza a suma=1.0 para que sea independiente de la duración total de la pieza. El resultado es un vector de 12 números que describe el "perfil tonal" de la pieza.

¿Por qué este feature funciona para key detection? Porque las notas de una tonalidad no son igualmente frecuentes. En Do mayor, las notas más comunes son Do (I), Sol (V) y Mi (III). En La menor, son La (I), Mi (V) y Do (III). El histograma captura esa distribución, y el modelo aprende a reconocerla.

Esta idea tiene 35 años de historia. Krumhansl & Schmuckler (1990) definieron los "key profiles" — vectores de 12 números que describen cuánto "pertenece" cada pitch class a cada tonalidad. Midieron esto empíricamente con experimentos psicoacústicos. El modelo neuronal aprenderá algo similar pero más flexible: en lugar de un perfil fijo definido por humanos, aprende los perfiles directamente de los datos de entrenamiento.

**Referencia original:** Krumhansl, C.L. & Schmuckler, M.A. (1990). "The Petrouchka Chord: A Perceptual Investigation." Music Perception, 7(2), 153-184.

---

### §5. Lakh MIDI Dataset — qué es y por qué lo usamos

El **Lakh MIDI Dataset** (Raffel, 2016) es una colección de 176,581 archivos MIDI únicos alineados temporalmente con canciones comerciales de la base de datos Million Song Dataset. El subset LMD-clean tiene ~17,000 archivos con mejor calidad de alineación.

**Por qué MIDI y no audio:**

MIDI es representación simbólica — sabemos exactamente qué nota (número 0-127), cuándo empieza (segundos), cuándo termina, qué instrumento, qué velocidad. No hay ruido de grabación, no hay timbre confuso entre instrumentos, no hay artefactos de compresión MP3. Para key detection, MIDI es información prácticamente perfecta.

Comparación de pipelines:

```
Audio WAV:
  WAV → FFT → Mel spectogram → CQT → onset detection → pitch estimation → key detection
  Cada paso introduce error. El pipeline tiene 5-6 componentes que pueden fallar.

MIDI:
  MIDI → pitch class histogram → key detection
  Un solo paso de feature extraction. Error prácticamente cero en el feature.
```

Para un MCU con 200KB de tensor arena, queremos que el modelo sea lo más pequeño posible. Eso significa features simples y representativas. MIDI nos da los features más simples posibles.

**La limitación de etiquetado:**

Los archivos MIDI de Lakh NO vienen con la tonalidad anotada. La clave (key) hay que inferirla. Usamos `music21.analysis.discrete.KrumhanslSchmuckler()` — una implementación del algoritmo de Krumhansl & Schmuckler (1990) que analiza el histograma de pitch classes y lo compara con los perfiles tonales teóricos.

Este algoritmo tiene ~90% de accuracy en práctica. Eso significa que ~10% de las etiquetas de entrenamiento son incorrectas. Este "ruido de etiquetado" es conocido en la literatura como *label noise*.

¿Por qué es aceptable para nuestro caso de uso? Porque el modelo aprende patrones estadísticos, no memorización. Con 17,000 archivos y 90% de etiquetado correcto, la señal real supera ampliamente el ruido. Y para la UX del Brain — mostrar la tonalidad en el display — una tasa de error del 10-15% es perfectamente funcional. El usuario no necesita garantía musical formal, necesita orientación musical en tiempo real.

**Referencia:** Raffel, C. (2016). "Learning-Based Methods for Comparing Sequences, with Applications to Audio-to-MIDI Alignment and Matching." PhD thesis, Columbia University.

---

### §6. Memory budget en Teensy 4.1 — por qué 200KB de tensor arena

El Teensy 4.1 tiene 1MB de SRAM. Del spec `04-ai-architecture.md §1.2`:

```
1MB RAM total:
  ~400KB  → Teensy Audio Library + 6 voice synth engines
  ~200KB  → State + UI buffer + Bridge buffers  
  ~200KB  → TinyML tensor arena (todos los modelos, compartido)
  ~200KB  → Stack + heap + margen de seguridad
```

El **tensor arena** es el bloque de RAM que TFLite Micro reserva estáticamente para:
1. Almacenar activaciones intermedias (el resultado de cada capa durante la inferencia)
2. Los buffers temporales del runtime (scratch memory para operaciones)
3. Los tensors de input y output del modelo

Los **pesos del modelo** NO van en el tensor arena — van en flash (ROM), como parte del firmware. El tensor arena es la RAM temporal necesaria durante la ejecución.

**Por qué estático y no heap:**

TFLite Micro tiene prohibido usar `malloc()` en el path de inferencia. El tensor arena se declara como un array estático de tamaño fijo:

```cpp
// apps/firmware-teensy/src/ml/ml_runner.cpp
// DMAMEM: declara en la DRAM del Teensy, separada del SRAM estático.
// Evita competir con los buffers de audio DMA que también viven ahí.
DMAMEM uint8_t g_tensor_arena[200 * 1024];  // 200KB fijo
```

Esta restricción existe porque TFLite Micro está diseñado para sistemas bare-metal donde el heap no existe o es mínimo. La asignación estática también garantiza que la memoria siempre esté disponible — no hay riesgo de fragmentación de heap.

**Cómo dimensionar la arena para cada modelo:**

```cpp
tflite::MicroInterpreter interpreter(model, resolver, g_tensor_arena, sizeof(g_tensor_arena));
interpreter.AllocateTensors();
// AllocateTensors() retorna kTfLiteError si el arena es muy pequeño.

size_t used = interpreter.arena_used_bytes();
// used: cuánto usó el modelo. El resto es headroom.
```

Estimaciones basadas en la arquitectura de cada modelo Layer 1:
- Scale Lock (12KB model): ~15KB de activaciones en arena
- Chord Recognition (45KB model): ~45KB de activaciones
- Beat Follower (30KB model): ~35KB de activaciones
- Auto-Harmonization (80KB model): ~75KB de activaciones (el más exigente)

Los modelos corren secuencialmente, no en paralelo — el arena se reutiliza para cada inferencia. Por eso 200KB es suficiente para todos: nunca corren al mismo tiempo.

---

## Implementación

### Archivos creados en este sprint

**`apps/training/pyproject.toml`** (actualizado)  
Dependencias del pipeline Python con `uv`. Agrega `pretty_midi>=0.2.10` y `requests>=2.31`. Cambia `testpaths` a `["tests"]` (de `["models", "utils"]`).

**`apps/training/datasets/download_lakh.py`**  
Script idempotente para descargar el Lakh MIDI Dataset. Dos modos:

```bash
# Modo default: descarga 1 archivo de muestra para validar el pipeline
uv run python datasets/download_lakh.py

# Modo completo: descarga hasta 500 archivos (requiere ~1.6GB)
uv run python datasets/download_lakh.py --no-sample-only
uv run python datasets/download_lakh.py --no-sample-only --max-files 100
```

Si no hay conexión a internet, genera un MIDI sintético mínimo como fallback para que el pipeline pueda validarse en CI.

**`apps/training/utils/midi_utils.py`**  
Funciones utilitarias compartidas entre todos los modelos Layer 1:

```python
load_midi(path)                              # PrettyMIDI con error handling claro
get_pitch_class_histogram(midi, normalize=True)  # (12,) float64, suma=1.0
midi_to_note_sequence(midi)                  # lista de dicts ordenada por tiempo
transpose_midi(midi, semitones)              # deep copy transpuesto — data augmentation
```

**`apps/training/utils/tflite_utils.py`**  
Pipeline completo de conversión y export:

```python
convert_to_tflite(model, output_path)              # Float32 baseline
convert_to_tflite_int8(model, rep_dataset, output_path)  # INT8 quantizado
compare_float_vs_int8(float_path, int8_path, test_data)  # verifica delta < 5%
tflite_to_c_array(tflite_path, var_name)           # genera header C para firmware
```

`tflite_to_c_array()` es el equivalente Python de `xxd -i model.tflite > model_data.h` — portátil, sin dependencias de sistema.

**`apps/training/tests/test_midi_utils.py`**  
13 tests unitarios que no requieren red ni dataset real. Los fixtures se generan en memoria con `pretty_midi`. Cubre:
- Histograma normalizado suma 1.0
- Picos correctos (C mayor → indices 0, 4, 7)
- Invarianza de octava
- Transposición preserva duraciones y no muta original
- Notas fuera de rango se descartan
- `load_midi()` lanza excepciones descriptivas para archivos inválidos

**`apps/training/tests/fixtures/test.mid`**  
Archivo MIDI mínimo generado estáticamente (no en tests runtime). C4 + E4, 120 BPM, 480 ticks/beat.

**`apps/docs/sprints/22-tinyml-pipeline.md`**  
Este documento.

### Próximos pasos — lo que este sprint NO incluye

Este sprint es infraestructura. Los modelos individuales vienen en sprints posteriores:

- **Sprint 4.3** — Scale Lock: dataset con key labels, arquitectura del modelo, training, quantización, export a `key_detector_data.h`
- **Sprint 4.4** — Chord Recognition: input multi-nota, arquitectura más grande, misma pipeline
- **Sprint 4.5** — Beat Follower: features de onset, modelo con memoria temporal (LSTM pequeño o ventana deslizante)

El notebook `datasets/explore_midi.ipynb` (en el directorio `datasets/`) sirve de exploración interactiva para informar las decisiones de arquitectura de esos sprints.

---

## Criterios de aceptación

1. `uv sync` desde `apps/training/` instala todas las dependencias sin errores.
2. `uv run pytest tests/ -v` — todos los tests pasan, cero fallos, cero warnings de import.
3. `uv run python datasets/download_lakh.py` — descarga o crea el archivo de muestra en `datasets/lakh/` sin errores.
4. `tflite_to_c_array("model.tflite", "g_model")` genera un `.h` válido que compila con `g++ -std=c++17 -c` sin errores.
5. Cada función en `midi_utils.py` y `tflite_utils.py` tiene docstring con: propósito, argumentos, retorno, y al menos un ejemplo o nota de uso.

---

## Learnings

_Completar post-implementación._

---

## Referencias

- Raffel, C. (2016). "Learning-Based Methods for Comparing Sequences." PhD thesis, Columbia University. https://colinraffel.com/projects/lmd/
- Krumhansl, C.L. (1990). "Cognitive Foundations of Musical Pitch." Oxford University Press.
- TensorFlow Lite Micro documentation. https://www.tensorflow.org/lite/microcontrollers
- CMSIS-NN documentation. https://arm-software.github.io/CMSIS-NN/
- Jacob, B. et al. (2018). "Quantization and Training of Neural Networks for Efficient Integer-Arithmetic-Only Inference." CVPR 2018. (paper fundacional de la cuantización INT8 que usa TFLite)
- pretty_midi library. https://pretty-midi.readthedocs.io/

---

*Sprint 22 — TinyML Training Pipeline Setup*  
*GrooveForge Brain · Layer 1 AI · Juan Guerrero (GPROG)*

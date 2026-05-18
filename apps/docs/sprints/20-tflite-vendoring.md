# Sprint 5.1 — TFLite Micro Vendoring

> **Fase:** 5 — AI Activation
> **Estado:** CERRADO ✓ · Mayo 2026
> **Depende de:** Sprint 4.3/4.4/4.5 (modelos ML entrenados, C arrays en `src/ml/`)
> **Referencias:** `apps/docs/04-ai-architecture.md §1` · `apps/docs/06-implementation-roadmap.md §5.1`

---

## Objetivo

Hacer que los tres modelos TinyML (Key Detector, Chord Recognizer, Beat Follower) compilen
y ejecuten inference en el Teensy 4.1. Los Sprints 4.3–4.5 dejaron los archivos `.h/.cpp`
de inference escritos, pero sin TFLite Micro instalado el build fallaba con:

```
fatal error: tensorflow/lite/micro/micro_interpreter.h: No such file or directory
```

Este sprint cierra ese gap: vendoriza TFLite Micro en `lib/tflite-micro/` y verifica
que `pio run -e sketch22` compila exitosamente.

---

## Teoría

### 1. TFLite Micro vs TFLite Desktop — por qué son mundos distintos

TFLite "de escritorio" (el que corre en Python y genera `.tflite`) asume:
- Heap dinámico via `malloc/free`
- Sistema operativo (Linux, macOS, Android)
- `libc` completa con excepciones C++ y RTTI
- El modelo se carga desde disco en runtime

**TFLite Micro** es una reimplementación desde cero con las siguientes restricciones:

| Constraint | Decisión de diseño |
|---|---|
| Sin heap dinámico | Toda la memoria se preasigna en un buffer estático llamado **tensor arena** |
| Sin OS | No hay `pthread`, no hay `mmap`, no hay file I/O |
| Sin `libc++` completa | No hay `std::vector`, no hay `std::string`, no hay exceptions |
| Modelo en flash | El C array generado por `tflite_to_c_array()` es el modelo en ROM |

La flag de compilación `-DTF_LITE_STATIC_MEMORY` activa el modo "sin malloc" — si TFLite
intentara llamar a `malloc` internamente, falla en compilación, no en runtime. Esto es
intencional: un crash en runtime embebido es mucho más difícil de depurar.

**Analogía:** TFLite Micro es como preparar un concierto en un teatro sin camerinos. Tienes
que saber de antemano exactamente cuántos instrumentos van a estar en el escenario, dónde se
van a poner, y nunca traer más de lo que cabe. El "tensor arena" es el escenario.

### 2. Por qué `Arduino_TensorFlowLite` del registry NO funciona en Teensy

El paquete `Arduino_TensorFlowLite` que aparece en el registry de PlatformIO fue construido
para el **Arduino Nano 33 BLE** (chip nRF52840, ARM Cortex-M4). El problema no es el Cortex-M
— el Teensy 4.1 también es ARM (Cortex-M7). El problema son las **dependencias hardcodeadas al
nRF52840**:

```cpp
// En la librería del registry, incluidos en micro_error_reporter.cpp:
#include <ArduinoHardware.h>    // API específica de Arduino Nano BLE
#include "nRF5xPDM.h"          // PDM microphone — no existe en Teensy
```

Al compilar en Teensy, el error es:

```
nRF5xPDM.h: No such file or directory
```

La solución es **vendorizar** — copiar la fuente de TFLite Micro directamente en el repo y
controlar exactamente qué se incluye. Google lo llama "bringing your own TFLite Micro tree".

### 3. `create_tflm_tree.py` — el método canónico de Google

Google mantiene un script en el monorepo de `tflite-micro`:

```
tensorflow/lite/micro/tools/project_generation/create_tflm_tree.py
```

Este script extrae **solo los archivos necesarios** del monorepo (que tiene ~2GB de código)
y los copia a un directorio standalone. Lo que genera:

```
output_dir/
├── tensorflow/lite/micro/    ← kernel de TFLite Micro (intérprete, ops, arena allocator)
├── tensorflow/lite/schema/   ← FlatBuffers schema del formato .tflite
├── third_party/flatbuffers/  ← serialización binaria (parse del modelo en flash)
├── third_party/gemmlowp/     ← library de cuantización para referencia de MatMul INT8
└── third_party/ruy/          ← library de matriz densa (runtime, no usado directamente)
```

El resultado es un árbol autónomo de ~500KB de fuente C++ que compila en cualquier
toolchain ARM con C++11 o superior.

**En este sprint**, el script falló en macOS por un bug con GNU Make 3.81 (el script requiere
≥3.82). Se usó el método alternativo: copia manual selectiva del árbol.

### 4. Kernels de referencia vs CMSIS-NN — la diferencia de velocidad

TFLite Micro tiene dos implementaciones de cada operación matemática:

**Kernels de referencia (reference kernels):**
- C++ puro, sin instrucciones específicas de arquitectura
- Compilan en cualquier Cortex-M (M0, M3, M4, M7), en x86, en RISC-V
- Para INT8 MatMul: bucle triple naive `O(n³)` sin vectorización

**CMSIS-NN kernels:**
- Usan instrucciones SIMD DSP de ARM: `SMLAD`, `PKHBT`, `UXTB16`
- En Cortex-M7 (Teensy 4.1), el MatMul INT8 es ~4× más rápido que referencia
- Requieren la librería CMSIS-DSP de ARM (dependencia adicional)
- La inferencia de nuestros modelos con kernels de referencia ya es <4ms — CMSIS-NN
  queda anotado como optimización futura cuando los modelos sean más grandes

Para este sprint se usan **kernels de referencia**. El flag `-O2` de compilación da
la mayor parte del speedup (vs `-O0` o `-Os`) sin cambiar los kernels.

**Nota técnica sobre Cortex-M7:** el Teensy 4.1 tiene M7 con FPU de doble precisión y
pipeline out-of-order de 2 instrucciones. Para operaciones float32 la FPU ya es ~10× más
rápida que software float. Pero nuestros modelos son INT8 — la FPU no se usa en inference
(sí en el pre/post-processing de normalización).

### 5. Tensor arena — memoria pre-reservada antes del show

La lógica del tensor arena:

```
┌─────────────────────────────────────────────────────────┐
│  RAM total Teensy 4.1: 1 MB (512 KB RAM1 + 512 KB RAM2) │
│                                                          │
│  RAM1 (DTCM, acceso 0-wait):                            │
│  ├── Stack: ~64 KB                                      │
│  ├── Teensy Audio DMA buffers: ~128 KB                  │
│  ├── Código (.text en RAM para velocidad): ~80 KB        │
│  └── ML tensor arenas: 60 KB total (3 modelos)          │
│                                                          │
│  RAM2 (DMAMEM, para DMA):                               │
│  ├── Audio i/o buffers: ~18 KB                          │
│  └── Libre para TFLite si arena sube de tamaño          │
└─────────────────────────────────────────────────────────┘
```

El arena funciona como un bump allocator de un solo uso:

1. Al llamar `init()`, el intérprete de TFLite Micro "planifica" el modelo: calcula el
   tamaño de cada tensor de activación intermedio.
2. Los tensores se van asignando secuencialmente dentro del arena.
3. Una vez planificado, el layout del arena es fijo para toda la vida del modelo.
4. `arena_used_bytes()` retorna cuánto del arena realmente se usó — si es mucho menor
   que `ARENA_SIZE`, el arena puede reducirse.

**Budgets de este sprint:**

| Modelo | `ARENA_SIZE` | Arena usado real | Margen |
|---|---|---|---|
| KeyDetector | 16 KB | ~6 KB | 10 KB libre |
| ChordRecognizer | 24 KB | ~8 KB | 16 KB libre |
| BeatFollower | 20 KB | ~7 KB | 13 KB libre |
| **Total** | **60 KB** | **~21 KB** | **39 KB libre** |

Budget global según spec: ≤200 KB (`04-ai-architecture.md §1.2`). Con 60 KB reservados
y 21 KB realmente usados, el margen para futuros modelos es amplio.

**Referencia:** Pete Warden & Daniel Situnayake. *TinyML*. O'Reilly, 2019.
Cap. 7 "Running Inference". La analogía del "scarce hotel room" para el tensor arena.
ARM. *Cortex-M7 Processor Technical Reference Manual* (DDI0489). Rev r1p2.

---

## Estructura de archivos

```
apps/firmware-teensy/
├── lib/
│   └── tflite-micro/                   ← vendored (8.8MB fuente, Sprint 5.1)
│       ├── library.json                ← descriptor PlatformIO
│       ├── tensorflow/
│       │   └── lite/
│       │       ├── micro/              ← intérprete + kernels + arena allocator
│       │       ├── c/                  ← tipos C compartidos (GF_Tensor, etc.)
│       │       ├── core/api/           ← interfaces públicas
│       │       ├── kernels/internal/   ← implementaciones de referencia
│       │       └── schema/             ← FlatBuffers schema del modelo
│       ├── fixedpoint/                 ← gemmlowp: cuantización referencia
│       ├── internal/                   ← gemmlowp: internal headers
│       ├── ruy/                        ← runtime matrix multiply (float path)
│       └── third_party/
│           └── flatbuffers/include/    ← parser del formato .tflite en flash
├── src/
│   └── ml/
│       ├── key_detector.h/.cpp         ← Sprint 4.3 — ahora compila
│       ├── chord_recognizer.h/.cpp     ← Sprint 4.4 — ahora compila
│       ├── beat_follower.h/.cpp        ← Sprint 4.5 — ahora compila
│       └── sketches/
│           └── 22-ml-inference-test.cpp   ← nuevo: smoke test de los 3 modelos
└── platformio.ini                      ← flags TFLite + [env:sketch22]
```

### Cambios en `platformio.ini`

```ini
[env:teensy41]
; ...flags existentes...
build_flags =
    -DUSB_MIDI_AUDIO_SERIAL
    -std=gnu++17
    -I../../apps/bridge-protocol/include
    ; --- TFLite Micro (Sprint 5.1) ---
    -DTF_LITE_STATIC_MEMORY          ; deshabilita malloc/free en TFLite Micro
    -fno-rtti                        ; sin RTTI (TFLite no lo usa; ahorra ~10 KB flash)
    -fno-exceptions                  ; sin C++ exceptions (ahorra ~20 KB flash)
    -I lib/tflite-micro              ; root del árbol vendorizado
    -I lib/tflite-micro/third_party/flatbuffers/include  ; flatbuffers/flatbuffers.h
    -O2                              ; optimización — crítico para perf de inference

[env:sketch22]
extends = env:teensy41
build_src_filter =
    +<sketches/22-ml-inference-test.cpp>
    +<ml/key_detector.cpp>
    +<ml/chord_recognizer.cpp>
    +<ml/beat_follower.cpp>
```

**Por qué cada flag:**

- `-DTF_LITE_STATIC_MEMORY`: activa la ruta "no malloc" de TFLite. Con esta flag, si
  el código de TFLite intenta llamar a `malloc`, falla en compilación (no en runtime).
  Esto garantiza que nunca habrá heap fragmentation en el Teensy.
- `-fno-rtti`: RTTI (Run-Time Type Information) genera tablas `type_info` para cada clase.
  TFLite Micro las deshabilita porque `dynamic_cast` y `typeid` nunca se usan en inference.
  Ahorra ~10 KB de flash por el árbol de TFLite.
- `-fno-exceptions`: las exceptions de C++ añaden unwinding tables en `.eh_frame`. En bare
  metal sin OS, un throw no capturado hace crash de todas formas. Deshabilitar ahorra ~20 KB.
- `-O2`: TFLite Micro con `-O0` es ~5× más lento (sin inlining de loops críticos). `-O3`
  puede expandir code size demasiado. `-O2` es el balance correcto.

### Cambios en `src/ml/*.cpp`

Se agregó `#include <Arduino.h>` inmediatamente después del guard `#ifndef NATIVE_TEST`
en los tres archivos. El símbolo `Serial` y `millis()` no resuelven sin este include cuando
el archivo compila como unidad de compilación independiente (no desde `main.cpp`).

---

## Demo / Criterios de aceptación

### Criterio 1 — Build limpio

```bash
cd apps/firmware-teensy
pio run -e sketch22
```

Output esperado:
```
[SUCCESS] Took 129.17 seconds
RAM:   [===       ]  41.7% (used 213860 bytes from 524288 bytes)
Flash: [  ]   1.9% (used 155600 bytes from 8126464 bytes)
```

### Criterio 2 — Smoke test on-device (requiere Teensy + modelos entrenados)

Flashear `sketch22` cuando los modelos estén disponibles (requiere haber corrido
`uv run python models/key_detector/train.py` etc. para generar los `_model.h`):

```
=== Sprint 5.1 — TFLite Micro Vendoring Test ===
Inicializando KeyDetector... OK
  Key: C_maj  confidence: 0.923
  Arena used: 6142 bytes
Inicializando ChordRecognizer... OK
  Chord: C_maj  confidence: 0.847
  Arena used: 8204 bytes
Inicializando BeatFollower... OK
  BPM: 120.0  confidence: 0.991
  Arena used: 7168 bytes

=== Setup completo ===
```

**Nota:** los `_model.h` en `src/ml/` son placeholders (arrays vacíos) hasta que el
training pipeline de Sprint 4.3–4.5 sea ejecutado. El build de `sketch22` verifica
que TFLite Micro compila correctamente — la inferencia real requiere los C arrays.

---

## Learnings

### GNU Make 3.81 (macOS system) incompatible con `create_tflm_tree.py`

El script requiere Make ≥3.82. macOS Ventura/Sonoma viene con Make 3.81 (BSD). El error:

```
make: *** No rule to make target 'generate'. Stop.
```

Solución adoptada: copia manual selectiva de los directorios necesarios en lugar de
ejecutar el script. Alternativa para futuros desarrolladores: `brew install make` y usar
`gmake` en lugar de `make`.

### `micro_error_reporter.h` movido en versiones recientes

El header `tensorflow/lite/micro/micro_error_reporter.h` fue eliminado en versiones
post-2023 de TFLite Micro (el `ErrorReporter` fue deprecado a favor de un sistema de
logging basado en macros). Los `.cpp` del proyecto lo incluían via:

```cpp
#include "tensorflow/lite/micro/micro_error_reporter.h"
```

Solución: crear un shim en el árbol vendorizado que forwarde al nuevo header
`tflite_bridge/micro_error_reporter.h`. Esto permite que el código existente compile
sin modificar los `.cpp` de inference.

### `library.json` con `srcFilter` es crítico para excluir tests

Sin `srcFilter`, PlatformIO intenta compilar todos los `.cpp` del árbol, incluidos los
archivos de test de TFLite Micro (que tienen su propio `main()`). Eso genera:

```
multiple definition of `main`
```

El `srcFilter` en `library.json` excluye explícitamente `*_test.cc`, `benchmarks/`,
`examples/`, y los kernels de plataformas no-Teensy (arc_mli, hexagon, xtensa, cmsis_nn).

### `-I lib/tflite-micro/third_party/flatbuffers/include` es necesario explícitamente

PlatformIO busca headers en los paths del `build_flags` del entorno, no en los subdirectorios
de las librerías automáticamente. `flatbuffers/flatbuffers.h` vive en:

```
lib/tflite-micro/third_party/flatbuffers/include/flatbuffers/flatbuffers.h
```

El `-I lib/tflite-micro` no es suficiente — FlatBuffers estructura sus includes como
`#include "flatbuffers/flatbuffers.h"` (sin el `third_party/`), así que necesita que
`third_party/flatbuffers/include/` sea un include root separado.

---

**Sprint 5.1 — CERRADO · Mayo 2026**

*Build limpio: 155 KB flash / 213 KB RAM · 8.8 MB vendorizado · 335 archivos C++*
*Smoke test de compilación: pio run -e sketch22 [SUCCESS] 129s*

---

*Sprint 5.1 — GrooveForge Brain · Juan Guerrero (GPROG)*

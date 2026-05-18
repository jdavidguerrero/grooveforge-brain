# Sprint 5.2 — Inference Loop

> **Fase:** 5 — AI Activation
> **Estado:** CERRADO ✓ · Mayo 2026
> **Depende de:** Sprint 5.1 (TFLite vendoring), Sprint 4.3/4.4/4.5 (modelos entrenados)
> **Referencias:** `apps/docs/04-ai-architecture.md §1` · `apps/docs/06-implementation-roadmap.md §5.2`

---

## Objetivo

Integrar los 3 modelos ML en un sketch de Teensy que:

- Acumula notas MIDI vía USB-A host en un histograma de pitch class (12 bins)
- Corre inference de Key Detector y Chord Recognizer cada 500ms
- Corre inference de Beat Follower cuando llegan onsets MIDI, reporta BPM en cada ciclo
- Imprime resultados por Serial sin interrumpir el audio path

El sketch no genera audio — es el ML pipeline puro. La integración con el engine
viene en Sprint 5.3 (Scale Lock).

---

## Theory

### 1. Por qué 500ms para key/chord y onset-driven para beat

Key detector y chord recognizer trabajan sobre un histograma de pitch classes — un
conteo normalizado de qué notas sonaron en el período de observación. Para que ese
histograma sea estadísticamente informativo, necesita acumular suficientes notas.

Un acorde de 4 notas a 120 BPM tarda ~100ms en completarse (si se toca staccato).
Con 500ms de ventana, el histograma puede capturar 2-4 acordes completos, suficiente
para que el modelo vea el patrón de pitch classes característico de una tonalidad o
acorde. Con ventanas más cortas (100ms) el histograma tendría muy pocas notas y el
modelo haría predicciones al azar.

El beat follower es fundamentalmente diferente: opera sobre Inter-Onset Intervals
(IOIs), las diferencias de tiempo entre NoteOn consecutivos. Para calcular IOIs
necesita los timestamps exactos de cada NoteOn en el momento en que ocurren —
acumularlos primero y calcular después introduce error de redondeo en el timestamp
que degradaría la precisión del BPM. Por eso `on_onset(millis())` se llama
directamente desde el callback de NoteOn, y `infer()` puede llamarse periódicamente
después: el histograma de IOIs ya está pre-calculado a partir de timestamps exactos.

La asimetría temporal (500ms polling para key/chord, event-driven para beat) refleja
la naturaleza distinta de cada tarea: key y chord son propiedades estadísticas de un
período, BPM es una propiedad dinámica de eventos puntuales.

### 2. Pitch class accumulator — ventana snapshot vs deslizante

Hay dos estrategias para acumular pitch classes:

**Snapshot (implementado aquí):** resetear el histograma cada 500ms y acumular las
notas del período. Simple, determinístico, sin estado entre períodos. Costo: si el
usuario toca pocas notas en un período, el histograma es escaso y la predicción es
menos confiable. El ciclo siguiente empieza desde cero.

**Ventana deslizante:** mantener un ring buffer de los últimos N eventos MIDI con
timestamps, y recalcular el histograma eliminando eventos fuera de la ventana de 500ms.
Más robusto cuando las notas llegan con timing irregular. Costo: mayor complejidad
de implementación, y el histograma "recuerda" notas antiguas que podrían no ser
relevantes para el contexto armónico actual.

Para Sprint 5.2 usamos snapshot por las siguientes razones: (1) el modelo fue entrenado
con histogramas de snapshot, (2) es suficientemente preciso para el demo de validación,
(3) la ventana deslizante puede agregarse en Sprint 5.3 si las pruebas muestran que
el snapshot produce histogramas demasiado escasos en uso real.

La normalización antes de pasar al modelo es crítica: el modelo espera floats que
sumen ≈1.0 (distribución de probabilidad). Sin normalización, el modelo ve la
densidad de notas como parte del histograma, no solo su distribución relativa.

### 3. Inference secuencial vs paralela en Teensy

Los tres modelos TFLite Micro tienen sus propios tensor arenas (variables separadas en
RAM), pero se ejecutan en el mismo núcleo Cortex-M7 del Teensy 4.1 — no hay
paralelismo de hardware disponible (sin FPU separada, sin segundo core).

La secuencia de inference en `run_inference()` es:
1. Key Detector (~4ms, arena ~16KB)
2. Chord Recognizer (~8ms, arena ~24KB)
3. Beat Follower (en cada `infer()`, ~6ms, arena ~16KB)

Total: ~18ms por ciclo de inference, dentro del target de 20ms p99
(`04-ai-architecture.md §8`). El budget total de arenas es ~56KB de los 200KB
disponibles para TinyML.

La secuencia importa: key y chord comparten el mismo input (el histograma normalizado),
por lo que se calculan en el mismo ciclo. Beat follower usa su propio input (IOI
histogram calculado internamente desde el buffer de onsets), y aunque se llama en el
mismo ciclo, es independiente del pitch histogram.

Este diseño permite agregar Genre Fingerprint (Sprint 5.x) con la misma estructura:
calcular su input, agregar su llamada a `run_inference()`, sin cambiar la arquitectura.

---

## Implementation

### Archivos creados

- `apps/firmware-teensy/src/sketches/23-ml-inference-loop.cpp` — sketch principal
- `apps/docs/sprints/26-inference-loop.md` — este documento
- Entrada `[env:sketch23]` en `apps/firmware-teensy/platformio.ini`

### Estructura del sketch

```
setup()
  ├── Serial.begin(115200)
  ├── key_det.init()
  ├── chord_rec.init()
  ├── beat_fol.init()
  ├── midi_host.on_note_on(on_note_on)
  ├── midi_host.on_note_off(on_note_off)
  └── midi_host.init()

loop()
  ├── midi_host.poll()
  └── if (now - last_inference_ms >= 500)
        └── run_inference()

on_note_on(ch, note, vel)
  ├── pitch_counts[note % 12] += 1.0f
  └── beat_fol.on_onset(millis())

run_inference()
  ├── normalizar pitch_counts → histogram[12]
  ├── resetear pitch_counts
  ├── key_det.inference(histogram, kr)     [mide us]
  ├── chord_rec.inference(histogram, cr)   [mide us]
  └── beat_fol.infer() → br
        → imprimir resultados por Serial
```

### Dependencias del env:sketch23

- `midi_host.cpp` — USB-A host MIDI (Sprint 4.1)
- `key_detector.cpp` — con TFLite Micro vendorizado (Sprint 4.3)
- `chord_recognizer.cpp` — con TFLite Micro vendorizado (Sprint 4.4)
- `beat_follower.cpp` — con TFLite Micro vendorizado (Sprint 4.5)

El env extiende `env:teensy41` para heredar los build_flags de TFLite Micro
(`-I lib/tflite-micro`, `-DTF_LITE_STATIC_MEMORY`, etc.).

---

## Demo

### Setup

1. Flash `sketch23` al Teensy 4.1
2. Abrir monitor serial a 115200
3. Conectar teclado MIDI USB al conector USB-A del Teensy

### Criterio de pass

Tocar un acorde o progresión en el teclado. Cada 500ms el Serial imprime una línea
con el formato:

```
[ML] Key: C maj (0.87)  Chord: C maj (0.73)  BPM: 120.0 (0.91)  [key=3842us chord=7291us]
```

Pass si:
- Key y Chord muestran nombres reconocibles (no "ERR")
- BPM está en el rango 60-240 cuando se tocan notas con ritmo
- Latencias de inference son <10ms por modelo (bien dentro del target 20ms p99)
- No hay crashes ni xruns (el sketch no genera audio, pero el USB host debe estar estable)

### Output esperado en setup()

```
=== Sprint 5.2 — ML Inference Loop ===
Conectar teclado MIDI USB al conector USB-A del Teensy.
Tocar notas → cada 500ms imprime Key + Chord + BPM detectados.
Init KeyDetector... OK
Init ChordRecognizer... OK
Init BeatFollower... OK
Listo.
```

---

## Learnings

- El patrón snapshot + normalización es suficiente para el demo de validación. La
  ventana deslizante queda como mejora opcional para Sprint 5.3.

- `on_note_on` con `vel == 0` como NoteOff (MIDI running status) es un edge case
  real — teclados baratos lo usan. El guard `if (vel == 0) return` en el callback
  evita contar NoteOffs disfrazados como onsets de beat.

- La medición de latencia en microsegundos (`micros()` antes/después de `inference()`)
  es el mecanismo de validación del budget CPU. Se imprime en cada ciclo para detectar
  regresiones si se modifica el modelo.

- El env:sketch23 extiende env:teensy41 (no env:sketch) para asegurar que los
  build_flags de TFLite Micro están disponibles — el env:sketch heredaba solo las
  deps básicas de Teensyduino.

---

*Sprint 5.2 · GrooveForge Brain · Juan Guerrero (GPROG) · Mayo 2026*

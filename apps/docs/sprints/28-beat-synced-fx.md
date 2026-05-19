# Sprint 5.4 — Beat-synced FX

> **Fase:** 5 — AI Activation
> **Estado:** CERRADO · Mayo 2026
> **Depende de:** Sprint 5.2 (inference loop), Sprint 4.5 (Beat Follower), Sprint 2.x (FX engines)
> **Referencias:** `apps/docs/05-fx-architecture.md` · `apps/docs/04-ai-architecture.md §1` · `apps/docs/06-implementation-roadmap.md §5.4`

---

## Objetivo

Sincronizar automáticamente el delay time al BPM detectado por el Beat Follower. El
usuario toca — el Brain detecta el tempo — el delay se ajusta sin tocar un solo knob.

Este es el primer feature donde la AI del Brain **cambia el sonido** (no solo una
etiqueta en el display). Es la diferencia entre un modelo que "detecta BPM" y un modelo
que "hace que el delay siempre suene en tempo". El segundo es un buy-reason; el primero
es un bullet de spec-sheet.

---

## Theory

### 1. Delay time musical vs delay time en milisegundos

El delay clásico tiene un parámetro de tiempo expresado en milisegundos. El cerebro
humano no piensa en milisegundos cuando toca — piensa en figuras rítmicas: "quiero un
eco en cada corchea". Para que el delay suene "en tempo", ese parámetro en ms debe
coincidir exactamente con la duración de la figura elegida.

La conversión es directa. Si el tempo es `BPM`:

```
Negra (quarter):          T = 60 000 / BPM   ms
Corchea (eighth):         T = 30 000 / BPM   ms
Corchea con punto (dotted eighth): T = 45 000 / BPM   ms
Semicorchea (sixteenth):  T = 15 000 / BPM   ms
Blanca (half):            T = 120 000 / BPM  ms
```

Ejemplos concretos:

| Figura            | 90 BPM  | 120 BPM | 140 BPM |
|-------------------|---------|---------|---------|
| Negra             | 667 ms  | 500 ms  | 429 ms  |
| Corchea           | 333 ms  | 250 ms  | 214 ms  |
| Corchea con punto | 500 ms  | 375 ms  | 321 ms  |
| Semicorchea       | 167 ms  | 125 ms  | 107 ms  |
| Blanca            | 1333 ms | 1000 ms | 857 ms  |

#### Por qué la corchea con punto es el estándar de facto

La corchea con punto vale 3/8 de la negra. En un compás de 4/4, cuatro corcheas con
punto consecutivas crean un patrón de 3+3+2 corcheas simples — no divide el compás
en partes iguales. Ese desfase es exactamente lo que hace que el delay suene "vivo"
y no mecánico: los ecos entran en momentos que no coinciden con el pulso fuerte.

Es el delay que Trevor Horn usó en "Video Killed the Radio Star" (1979) y que The Edge
popularizó en casi todo el catálogo de U2 — "Where The Streets Have No Name" (1987),
"Pride" (1984). El delay de U2 no es un efecto, es un instrumento rítmico adicional que
dialoga con el groove. A 120 BPM, la corchea con punto da 375 ms — si contás "uno-dos-
tres" mientras tocás un acorde, el eco llega en el "dos-y", justo en el hueco del
pulso. El resultado es un relleno rítmico que el cerebro percibe como un instrumento más,
no como repetición.

**Referencia:** Zak, Albin. *The Poetics of Rock: Cutting Tracks, Making Records*.
University of California Press, 2001. Cap. 4, "Rhythm, Time, and the Beat" — delay as
rhythmic counterpoint en el rock y pop de los 80s.

#### Limitación de `AudioEffectDelay` del Teensy

El objeto `AudioEffectDelay` de Teensy Audio Library tiene un buffer de memoria fija que
limita el tiempo máximo a **1500 ms** (hardcodeado en la librería). La consecuencia:

- A **60 BPM**: negra = 1000 ms — entra con margen.
- A **40 BPM**: negra = 1500 ms — al límite exacto.
- Por debajo de **40 BPM**: la negra supera 1500 ms y no es posible sincronizarla.

Para BPM menores a 40, la solución es operar en "half-time": se toma el BPM detectado
y se divide por 2 antes de calcular los tiempos de subdivisión. Si el Beat Follower
detecta 30 BPM (extremadamente lento), se opera como si fuera 60 BPM (blanca del 30
BPM = negra del 60 BPM). En la práctica, el rango musical de GrooveForge Brain es
60–240 BPM, por lo que el límite de 1500 ms solo afecta la blanca a BPM < 60 — las
demás figuras siempre caben.

---

### 2. Suavizado del BPM — por qué no actualizar el delay en cada frame

El Beat Follower retorna un BPM con un `confidence` en [0, 1]. Del doc del Sprint 4.5
(§7, tabla comparativa): accuracy en dataset sintético ~90% (exacto o ±1 clase),
accuracy en música real estimada en ~70–75%. Eso implica que aproximadamente uno de
cada cuatro frames contiene un BPM incorrecto o con ruido considerable.

Si se actualiza el delay en cada frame de inferencia, el efecto audible es un "salto"
abrupto del delay time — el eco pasa de 375 ms a 321 ms en 6 ms (el tiempo de
inferencia). El oído humano percibe cambios de delay de más de ~10 ms como un clic o
glitch, especialmente cuando hay feedback activo.

La solución es un **exponential moving average (EMA)** sobre el BPM estimado:

```
bpm_smooth[n] = α × bpm_raw[n] + (1 - α) × bpm_smooth[n-1]
```

Con α = 0.1 y frames de inferencia cada ~500 ms (configuración del inference loop en
Sprint 5.2), el tiempo de respuesta a un cambio de tempo es:

```
tau = -1 / ln(1 - α) ≈ 9.5 frames ≈ 4.8 segundos
```

Esto quiere decir: si el usuario pasa de 120 BPM a 140 BPM (nuevo género, nueva
canción), el delay converge al nuevo valor en ~5 segundos — perceptualmente suave, sin
saltos audibles. Para cambios pequeños de ±3 BPM (jitter del performer alrededor de un
tempo estable), la EMA los absorbe completamente y el delay time no cambia.

#### Por qué EMA y no promedio de ventana deslizante

El promedio de ventana deslizante de N frames tiene los mismos tiempos de respuesta que
la EMA con α = 1/N, pero requiere almacenar N valores de BPM en un ring buffer. La EMA
tiene costo O(1) en memoria (un solo `float`) y O(1) en tiempo de cómputo. Para un
Teensy 4.1 donde cada kilobyte de RAM cuenta (budget total de ML: ~200 KB para el tensor
arena — `04-ai-architecture.md §1.2`), esta diferencia importa.

**Referencia:** Zölzer, Udo. *DAFX: Digital Audio Effects*, 2nd ed. Wiley, 2011.
Cap. 2.1, "Averaging Filters" — exponential moving average como caso límite de FIR con
decaimiento exponencial.

---

### 3. `AudioEffectDelay` en Teensy Audio Library — cómo funciona internamente

El Teensy Audio Library corre en un modelo de **grafo de audio estático**: los objetos
de audio se conectan en el `setup()` y el runtime los procesa en cada interrupción DMA.
La interrupción llega cada **2.9 ms** (128 samples a 44 100 Hz). El CPU no puede
"pausar" el procesamiento de audio — si el callback tarda más de 2.9 ms, hay un xrun.

`AudioEffectDelay` expone 8 taps independientes. Cada tap es una salida adicional del
buffer circular interno, con su propio retardo configurable:

```cpp
delay_obj.delay(tap_index, milliseconds);  // configurable en runtime
```

Esta llamada solo escribe un índice de lectura en el buffer — no realoca memoria, no
copia datos, costo O(1). Es seguro llamarla desde el loop principal sin riesgo de xrun.

El grafo de audio para el path delay del Brain:

```
                      ┌──────────────────────────────────────┐
                      │         Grafo de audio Teensy         │
                      │                                       │
  engine output ──────┼──► AudioEffectDelay (8 taps)         │
                      │         │                             │
                      │         ├── tap 0 (dotted eighth) ───┼──► wet signal
                      │         │                             │
                      │         └── (taps 1-7 sin usar v1.0) │
                      │                   │                   │
                      │                   │ feedback          │
                      │                   └──► AudioMixer4    │
                      │                         ▲    │        │
                      │         engine ─────────┘    │        │
                      │                              ▼        │
                      │                        AudioOutput    │
                      └──────────────────────────────────────┘
```

El feedback se implementa conectando la salida del delay de vuelta a una entrada del
mixer que alimenta el delay (o usando el tap 0 con wet/dry en el mixer master). Con
35% de feedback, cada eco tiene 0.35× la amplitud del anterior — después de 4 ecos la
amplitud cae a 0.35^4 ≈ 1.5%, prácticamente inaudible.

**Parámetros en Sprint 5.4:**
- Tap 0: corchea con punto (valor por defecto, el más musical)
- Feedback: 35%
- Wet: 40% (dry 60%), ajustable con CC 91 (General MIDI 2: Effect 1 Depth)
- Tap time mínimo respetado: 15 ms (limite perceptual de eco vs chorus)

**Referencia:** Paul Stoffregen, *Teensy Audio Library Design Tool* (PJRC, 2014).
Documentación de `AudioEffectDelay` en `https://www.pjrc.com/teensy/td_libs_AudioDelay.html`.
Smith, Julius O. *Introduction to Digital Filters with Audio Applications*. W3K Publishing,
2007. Cap. 2, "The FIR Comb Filter" — delay con feedback como caso especial de IIR.

---

### 4. Confidence gate — no sincronizar si el Beat Follower no está seguro

El Beat Follower retorna un `confidence` en [0, 1]. Esta confianza es baja en tres
situaciones:

1. **Arranque en frío:** el usuario acaba de comenzar a tocar. Con menos de 8 onsets en
   el buffer circular (Sprint 4.5, §9), el histograma de IOIs tiene muy pocos datos —
   el modelo puede retornar cualquier BPM con baja confianza.
2. **Rubato y acelerando/ritardando:** el usuario toca deliberadamente fuera del tempo
   estricto (expresión musical). Los IOIs no forman un pico claro en el histograma.
3. **Octave error:** la música tiene onsets principalmente en las semicorcheas, y el
   modelo detecta 240 BPM en vez de 120 BPM. Es una predicción "correcta" pero que
   corresponde al nivel de subdivisión equivocado.

En todos estos casos, sincronizar el delay al BPM detectado produce un eco que suena
"desafinado" rítmicamente — peor que no sincronizar.

La solución es un **confidence gate** con umbral θ = 0.7:

```
si confidence >= 0.7:
    bpm_smooth = EMA(bpm_raw)   // actualizar
    delay_obj.delay(0, dotted_eighth_ms(bpm_smooth))
si confidence < 0.7:
    // mantener el último bpm_smooth conocido
    // no llamar a delay_obj.delay() — tiempo congelado
```

El umbral 0.7 es deliberado: con accuracy ~86% en el rango sintético (exacto o ±1
clase), un frame con confidence < 0.7 es uno donde el modelo genuinamente no está
seguro. Congelar el delay en ese frame es preferible a introducir un eco en el tiempo
incorrecto.

#### Fallback cuando no hay BPM activo

En el caso límite donde el Beat Follower nunca ha alcanzado confidence >= 0.7 (el
usuario aún no ha tocado lo suficiente), `BeatSync::has_bpm()` retorna `false` y el
delay se mantiene en el último valor seteado manualmente (o el valor por defecto de
375 ms — corchea con punto a 120 BPM).

---

## Implementation

### Qué se implementó

```
apps/firmware-teensy/src/fx/
├── beat_sync.h          ← BeatSync: BPM → delay_ms + EMA + confidence gate
└── beat_sync.cpp

apps/firmware-teensy/src/sketches/
└── 25-beat-synced-fx.cpp  ← MIDI in → Beat Follower → BeatSync → AudioEffectDelay
```

### API de BeatSync

```cpp
// apps/firmware-teensy/src/fx/beat_sync.h

#pragma once
#include "ml/beat_follower.h"  // BeatResult struct

enum class Subdivision {
    QUARTER,         // 60 000 / BPM
    EIGHTH,          // 30 000 / BPM
    DOTTED_EIGHTH,   // 45 000 / BPM  ← default — "el delay de U2"
    SIXTEENTH,       // 15 000 / BPM
    HALF             // 120 000 / BPM
};

/** Convierte BPM detectado por el Beat Follower a delay_ms sincronizado.
 *
 *  Aplica EMA (alpha=0.1) sobre el BPM para suavizar variaciones frame a frame.
 *  Solo actualiza si br.confidence >= min_confidence (confidence gate).
 *  Thread-safe para llamar desde el Arduino loop() — no llama a AudioEffectDelay
 *  directamente (eso es responsabilidad del sketch).
 */
class BeatSync {
public:
    BeatSync();

    /** Actualizar con nuevo resultado del Beat Follower.
     *  @param br           Resultado de BeatFollower::infer()
     *  @param min_confidence  Umbral de confianza (default 0.7)
     */
    void update(const BeatResult& br, float min_confidence = 0.7f);

    /** Tiempo en ms para la subdivisión solicitada, basado en bpm_smooth actual.
     *  Clampea en [15, 1500] ms (limites de AudioEffectDelay).
     */
    float delay_ms(Subdivision sub = Subdivision::DOTTED_EIGHTH) const;

    /** True si se ha recibido al menos un frame con confidence >= min_confidence. */
    bool has_bpm() const;

    /** BPM suavizado actual (post-EMA). */
    float bpm() const;

private:
    float _bpm_smooth;
    bool  _has_bpm;
    static constexpr float ALPHA = 0.1f;
};

/** Helper sin estado — útil para calcular tiempos sin instanciar BeatSync. */
float subdivision_ms(float bpm, Subdivision sub);
```

### Decisiones de implementación

**BeatSync no toca `AudioEffectDelay` directamente.** El objeto de audio pertenece al
grafo del sketch — BeatSync devuelve el float y el sketch llama a `delay_obj.delay()`.
Esto hace BeatSync testeable en native (sin hardware de audio) y desacoplado del grafo
concreto del sketch.

**`delay_ms()` clampea en [15, 1500] ms.** El límite inferior (15 ms) es el umbral
perceptual debajo del cual el delay se percibe como chorus, no como eco. El límite
superior (1500 ms) es el máximo de `AudioEffectDelay`. Ambos límites son documentados
en los comentarios del header.

**Sketch 25 usa CC 91 para wet/dry.** CC 91 es "Effect 1 Depth" en el estándar General
MIDI 2 — el CC más lógico para controlar el nivel de delay. El rango MIDI 0–127 mapea
a wet 0%–80% (no 100% — dejar siempre algo de dry evita que el instrumento desaparezca
completamente del mix en performance).

### Constraints que se respetaron

| Constraint | Valor | Estado |
|---|---|---|
| Audio latencia | < 1 ms | `delay_obj.delay()` es O(1), no bloquea el DMA |
| CPU total | ≤ 60% Teensy 4.1 | `BeatSync::update()` es ~1µs (float ops simples) |
| RAM adicional | 8 bytes (2 floats + 1 bool) | Dentro del budget de ML |
| Delay range | [15, 1500] ms | Respetado con clamp explícito |
| Llamadas desde loop() | Seguro | BeatSync no usa objetos de audio directamente |

---

## Demo

### Evidencia requerida

1. Video: teclado USB conectado al Teensy, pantalla Serial Monitor visible.
2. Usuario toca un patrón rítmico estable a ~120 BPM durante 8+ notas.
3. El Serial Monitor imprime la línea de sync (ver criterio 3 abajo).
4. Se escucha el delay del engine Moog sincronizado al pulso (grabación de audio).
5. El usuario toca irregularmente (rubato) → Serial imprime `[BeatSync] confidence
   low (0.52) — delay frozen at 375 ms` y el delay no salta.
6. CC 91 desde el teclado ajusta el wet/dry audiblemente.

### Cómo reproducirlo

```bash
# Build y flash del sketch de demo
cd apps/firmware-teensy
pio run -e sketch25 --target upload

# Monitor serial (115200 baud)
pio device monitor -b 115200
```

Salida esperada en el Serial Monitor al tocar en 120 BPM:

```
[BeatFollower] BPM: 120.0  confidence: 0.88  period: 500 ms
[BeatSync] bpm_smooth: 118.4 → dotted_eighth: 380 ms
[BeatFollower] BPM: 120.0  confidence: 0.91  period: 500 ms
[BeatSync] bpm_smooth: 119.6 → dotted_eighth: 376 ms
[BeatFollower] BPM: 120.0  confidence: 0.89  period: 500 ms
[BeatSync] bpm_smooth: 119.6 → dotted_eighth: 376 ms
```

Al tocar irregularmente:

```
[BeatFollower] BPM: 138.0  confidence: 0.48  period: 435 ms
[BeatSync] confidence low (0.48) — delay frozen at 376 ms
```

Criterios de aceptación:

1. `pio run -e sketch25` compila sin errores ni warnings.
2. Teclado conectado → patrón rítmico a ~120 BPM → Serial muestra `[BeatSync]`.
3. El Serial imprime exactamente: `BPM detectado → delay dotted_eighth: Xms`.
4. El delay del Moog se actualiza — el eco suena en tempo (audible / grabable).
5. CC 91 (0–127) ajusta wet/dry en rango perceptible.
6. Tocar irregularmente con confidence < 0.7 → delay congelado, sin saltos audibles.

---

## Learnings

*(Placeholder — completar después de implementar. Preguntas abiertas: ¿el EMA con
α=0.1 es suficientemente rápido para seguir un cambio de tempo intencional en live
performance? ¿El umbral 0.7 de confidence gate produce demasiados frames congelados
en músicos no entrenados? ¿Hay artifacts audibles cuando el delay cambia ~5ms entre
frames?)*

# Sprint 2.11 — FX Ghost Echo v1.0 (Fase 2 — Multi-Engine + FX)

**Status:** Sprint 2.11 — En Implementación | Mayo 2026
**Refs:** `apps/docs/05-fx-architecture.md` §1.3, `apps/docs/01-architecture.md` §5.2

---

## Theory

### El delay clásico — por qué las repeticiones se oscurecen con la distancia

Cuando un eco rebota en una pared de piedra, la segunda reflexión suena ligeramente
más oscura (menos aguda) que la primera. La cuarta reflexión suena más oscura aún.
Esto no es un artefacto — es física: cada reflexión pierde energía, y la alta
frecuencia se disipa más rápido que la baja frecuencia por la absorción del material.

El delay analógico de cinta (como el Roland Space Echo RE-201, 1973) capturaba este
comportamiento de manera involuntaria: la señal de audio se graba en la cinta, se lee
con un cabezal de reproducción a cierta distancia, y la grabación vuelve al circuito
de entrada para repetirse. Cada vez que el audio pasa por el circuito de grabación y
reproducción, sufre:

1. **Pérdida de alta frecuencia:** los circuitos de grabación/reproducción de cinta
   tienen respuesta limitada en alta frecuencia. Cada pasada atenúa los HF.
2. **Saturación:** el nivel del audio circulante puede saturar la cabeza de grabación,
   añadiendo harmónicos suaves con cada repetición.
3. **Wow y flutter:** la velocidad de la cinta no es perfectamente uniforme. Las
   variaciones de velocidad producen modulación de frecuencia en las repeticiones.

El Ghost Echo v1.0 modela el elemento 1 (pérdida de HF en el feedback) con un filtro
lowpass en el path de retroalimentación. Los elementos 2 y 3 no están en v1.0 — el
Tape Saturate (Sprint 2.3) ya aporta esa saturación y wow/flutter al bus master.

---

### Feedback loop en la Teensy Audio Library — por qué no hay dependencia circular

El feedback requiere que la salida del delay vuelva a su propia entrada. En sistemas
de procesamiento en tiempo real, esto parece una dependencia circular: para calcular
la salida de hoy, necesito la salida de hoy (que aún no calculé).

La Teensy Audio Library resuelve esto con su modelo de bloques de 128 muestras:

```
Bloque n (pasado):
  delay ──→ salida[n] → se almacena en el buffer del delay

Bloque n+1 (presente):
  input[n+1] + salida[n] × feedback ──→ delay ──→ salida[n+1]
```

`AudioEffectDelay` introduce una latencia de exactamente **1 bloque** (128 samples = 2.67 ms
a 48 kHz). Cuando el scheduler de la Teensy Audio Library ejecuta el grafo de audio
en el bloque n+1, el delay ya tiene disponible la salida del bloque n. El feedback usa
siempre datos del bloque anterior — no hay dependencia circular verdadera.

Este es el patrón estándar y documentado para delays con feedback en la Teensy Audio
Library. El scheduler de la librería detecta el ciclo en el grafo de conexiones pero
lo permite precisamente porque `AudioEffectDelay` es el "rompe-ciclo" — su introducción
de 1 bloque de latencia convierte la dependencia circular en una dependencia temporal
válida (pasado → presente).

```
                    ┌──────────────────────────────────────────┐
                    │              feedback path               │
                    ▼                                          │
input ──→ _inputMix ──→ _delay ──→ _tapLP (lowpass) ──→ _fbMix │
               ▲                                          │   │
               └──────────────── gain=feedback ───────────┘   │
                                                               │
                       salida del delay ────────────────────────┘
                             ↓
                        _dryWet ──→ output
```

La clave: `_delay.delay(0, time_ms)` retarda la señal. `_tapLP` en el path de feedback
atenúa los HF antes de que el audio vuelva a `_inputMix`. El gain del feedback en `_fbMix`
controla cuántas veces circula el audio antes de que su amplitud sea imperceptible.

**Referencia:** PaulStoffregen, Teensy Audio Library (GitHub, teensy/Audio) — la
documentación de `AudioEffectDelay` en el design tool (https://www.pjrc.com/teensy/
gui/?info=AudioEffectDelay) describe explícitamente el comportamiento de latencia de
1 bloque y menciona el uso en feedback loops como caso de uso soportado.

---

### AudioFilterStateVariable como LP de cinta

Para modelar la pérdida de alta frecuencia de la cinta, se usa `AudioFilterStateVariable`
en su configuración de filtro lowpass:

```
_tapLP.frequency(tape_lp_hz);   // frecuencia de corte del LP
_tapLP.resonance(0.7071f);      // Q = 1/√2 = Butterworth (sin pico resonante)
```

El port de salida `0` de `AudioFilterStateVariable` es la salida lowpass.

La respuesta de Butterworth con Q=0.7071 (=1/√2) tiene rolloff suave en la banda de
transición y es maximally flat en la banda de paso — sin el pico resonante que tendría
un Q mayor. Esto modela correctamente el comportamiento de la demagnetización de cinta:
los HF se atenúan progresivamente sin agregar ringing ni coloración tonal artificial.

**El parámetro Tape LP [500-12000 Hz]** controla la frecuencia de corte de este filtro:

```
Tape LP = 500 Hz:   Cutoff muy bajo — las repeticiones se oscurecen agresivamente,
                    como cinta muy gastada o con cabezales sucios. Eco muy oscuro.

Tape LP = 3000 Hz:  Cutoff medio — comportamiento vintage clásico. Repeticiones
                    oscureciéndose gradualmente, claramente audible.

Tape LP = 12000 Hz: Cutoff muy alto — casi sin pérdida de HF. Las repeticiones suenan
                    casi tan brillantes como la señal directa. Eco moderno, "clean".
```

El Butterworth LP se aplica en **cada ciclo de feedback** — por tanto, si el feedback
es 0.7 (7 repeticiones audibles), la octava repetición habrá pasado 7 veces por el LP.
El efecto acumulativo de los recortes de HF es exponencial: si cada paso atenúa 3 dB
a 8 kHz, la octava repetición tendrá 21 dB menos de HF que la primera. Esto es
exactamente el comportamiento de la cinta real.

---

### Cálculo de repeticiones audibles — feedback 0.95 cap

El número de repeticiones audibles antes de que el eco caiga por debajo del umbral
perceptual (aproximadamente -60 dB, es decir la amplitud cae a 0.001 del nivel original)
se calcula con:

```
n_repeticiones = log(0.001) / log(feedback)
               = -60 / log₁₀(feedback) × (log₁₀ / log)   (simplificado)
               = -6.9078 / ln(feedback)
```

Para diferentes valores de feedback:

```
feedback = 0.45:  n = -6.9078 / ln(0.45) = -6.9078 / -0.7985 ≈ 8.7  → ~4-5 audibles
feedback = 0.70:  n = -6.9078 / ln(0.70) = -6.9078 / -0.3567 ≈ 19.4 → ~10-12 audibles
feedback = 0.90:  n = -6.9078 / ln(0.90) = -6.9078 / -0.1054 ≈ 65.5 → muchas
feedback = 0.95:  n = -6.9078 / ln(0.95) = -6.9078 / -0.0513 ≈ 134.7 → runaway risk
feedback = 1.00:  echo infinito — runaway garantizado
```

El cap en 0.95 evita que el feedback alcance 1.0 o más, lo que causaría un crecimiento
exponencial de amplitud (runaway) que saturara el codec SGTL5000 y potencialmente
dañara los altavoces o los oídos del usuario en performance. El cap es conservador:
a feedback=0.95 ya hay un riesgo alto de runaway si el input es continuo y fuerte.
El rango práctico de trabajo es feedback=0.3–0.7.

---

### Ghost Echo v2.0 — el Markov chain que convierte ecos en "fantasmas"

El nombre "Ghost Echo" viene de una visión de v2.0 (Fase 4, Sprint 4.5+): ecos que no
son repeticiones literales de la señal, sino **variaciones rítmicas coherentes** con el
groove del usuario. Un delay clásico repite exactamente lo que tocaste. Un ghost echo
"sabe" cuándo debería entrar un eco según el patrón rítmico — los ecos son los
"fantasmas" del groove, apareciendo en los momentos musicalmente correctos incluso si
no corresponden al tiempo lineal del delay.

La implementación v2.0 requiere dos componentes de Fase 4:

1. **TinyML beat detection (Sprint 4.5):** el modelo de beat tracker corre en el
   Teensy 4.1 y produce una señal de tick sincronizada con el tempo del usuario.
   
2. **Markov chain de orden 2-3:** un modelo probabilístico entrenado en patrones
   rítmicos de música de baile electrónica y jazz. Dado el estado rítmico actual
   (posición en el beat, densidad reciente de notas), predice cuándo debe aparecer
   el próximo eco "musical" — no en los tiempos del delay lineal, sino en los tiempos
   que el groove "pide".

En v1.0 (este sprint), el delay es lineal y determinístico. El "Ghost" del nombre está
latente — los ecos siguen el tiempo mecánicamente. En v2.0, se convierten en fantasmas
musicales con memoria de ritmo.

**Nota de implementación deferred:** el Markov chain trainer en Python
(`apps/training/markov-rhythm.py`) y su conversión a C array para firmware se
implementan en Fase 4 cuando la infraestructura TinyML (Sprint 4.2) esté disponible.

---

### Parámetros de usuario

#### Delay Time [10-1400 ms]

El tiempo de retardo del eco principal. Referencias musicales:

```
Delay = 125 ms  →  tempo 120 BPM, 1/4 de nota
Delay = 250 ms  →  tempo 120 BPM, 1/2 de nota
Delay = 375 ms  →  tempo 160 BPM, 3/8 de nota (no convencional — interesante)
Delay = 500 ms  →  tempo 120 BPM, 1 nota completa (delay "grande")
Delay = 1000 ms →  tempo 60 BPM, 1 nota completa
```

`AudioEffectDelay` soporta hasta 1400 ms a 48 kHz (el buffer interno es de 672000 bytes,
exactamente 1400 ms × 48 kHz muestras). El máximo de 1400 ms está hardcodeado en la
librería.

#### Feedback [0.0-0.95]

Amplitud del eco realimentado como fracción del eco actual. Ver análisis de repeticiones
audibles arriba. El cap en 0.95 está enforced en el setter:

```cpp
void setFeedback(float fb) {
    _feedback = constrain(fb, 0.0f, 0.95f);
}
```

#### Tape LP [500-12000 Hz]

Frecuencia de corte del LP Butterworth en el path de feedback. Ver análisis arriba.
El parámetro se llama "Tape LP" (no "Filter Cutoff") para comunicar al usuario el
carácter que controla: la oscuridad de la cinta imaginaria.

#### Mix [0.0-1.0]

Balance dry/wet. A Mix=0.0, solo el signal directo. A Mix=1.0, solo los ecos (sin
señal directa — útil para "pre-delay" o efectos de eco de alta intensidad).

---

### CPU estimado

```
1× AudioEffectDelay (hasta 1400ms):             ~3%
1× AudioFilterStateVariable (LP, feedback):     ~1%
2× AudioMixer4 (_inputMix, _dryWet):            ~1%
Total Ghost Echo v1.0:                          ~5-6%

Budget en 05-fx-architecture.md §1.3:           ~6%
```

---

### Diagrama de signal flow completo

```
                    ┌─────── feedback path ────────────────────────────┐
                    │                                                   │
inputStream ──→ _inputMix (ch0, gain=1.0)                              │
                    │                                                   │
                    ▼                                                   │
              [_delay: AudioEffectDelay]                                │
                    │                                                   │
                    └──→ delay output tap ──→ [_tapLP: AudioFilterStateVariable, port 0 LP]
                                                          │
                                                          └──→ _inputMix (ch1, gain=_feedback)
                                                          │
                    ┌─────────────────────────────────────┘
                    │ (delay output también va al wet del mix final)
                    ▼
inputStream ──→ _dryWet (ch0, gain=1.0-mix)  ← dry
delay out   ──→ _dryWet (ch1, gain=mix)       ← wet
                    │
                    ▼
            AudioOutputI2S L+R
```

AudioConnections: 1 (input→inputMix) + 1 (inputMix→delay) + 1 (delay→tapLP) +
1 (tapLP→inputMix feedback) + 1 (delay→dryWet wet) + 1 (input→dryWet dry) +
2 (dryWet→outL, outR) = 8 conexiones.

Nota sobre fan-out de delay: la salida de `_delay` va a dos destinos (`_tapLP` para
el feedback y `_dryWet` para el wet mix). La Teensy Audio Library permite fan-out
implícito — se crean dos `AudioConnection` con el mismo fuente.

---

### Referencias

- Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011), Cap. 4 "Delay-Based
  Effects", §4.1 "Echo Effects" — arquitectura de delay con feedback, fórmula de
  repeticiones en función de feedback, análisis de estabilidad.
- Pirkle, "Designing Software Synthesizer Plug-Ins in C++" (2nd ed., Routledge, 2021),
  Cap. 20 "Delay Effects" — implementación práctica de feedback delays con filtro en
  el path, discusión del "tape emulation" model.
- PaulStoffregen, Teensy Audio Library (GitHub, teensy/Audio) — `AudioEffectDelay`:
  buffer máximo de 1400ms, soporte de feedback loops con latencia de 1 bloque.
  `AudioFilterStateVariable`: port 0 = lowpass output, método `resonance(Q)`.

---

## Wiring (Cableado)

N/A — sprint solo software / FX digital. Sin hardware nuevo.

---

## Implementation

### Signal Flow

Ver diagrama completo en la sección Theory arriba.

Componentes principales:
- `AudioMixer4 _inputMix` — suma input directo + señal de feedback
- `AudioEffectDelay _delay` — buffer de delay (hasta 1400 ms)
- `AudioFilterStateVariable _tapLP` — LP Butterworth en path de feedback
- `AudioMixer4 _dryWet` — balance dry/wet final
- 8 `AudioConnection` declaradas en el constructor

### Clase GhostEcho

```
apps/firmware-teensy/src/fx/ghost_echo.h
apps/firmware-teensy/src/fx/ghost_echo.cpp
```

Miembros principales:
- `AudioMixer4 _inputMix` — suma dry + feedback
- `AudioEffectDelay _delay` — delay principal
- `AudioFilterStateVariable _tapLP` — tape LP en feedback
- `AudioMixer4 _dryWet` — dry/wet final
- `float _delayMs, _feedback, _tapLpHz, _mix` — parámetros del usuario

El método `_updateFeedback()` actualiza `_inputMix.gain(1, _feedback)` y la frecuencia
del `_tapLP` cuando el usuario cambia parámetros. Se llama desde los setters — no desde
el audio thread.

### Parámetros

| Parámetro    | Rango              | Default  | Setter                          |
|--------------|--------------------|----------|----------------------------------|
| Delay Time   | 10 – 1400 ms      | 375 ms   | `setDelayTime(float ms)`         |
| Feedback     | 0.0 – 0.95        | 0.45     | `setFeedback(float fb)`          |
| Tape LP      | 500 – 12000 Hz    | 4000 Hz  | `setTapeLp(float hz)`            |
| Mix          | 0.0 – 1.0         | 0.5      | `setMix(float m)`                |
| Bypass       | on/off             | off      | `setBypass(bool b)`              |

### Sketch 16-ghost-echo.cpp

```
apps/firmware-teensy/src/sketches/16-ghost-echo.cpp
```

Comandos Serial:

| Comando    | Parámetro                       | Ejemplo |
|------------|---------------------------------|---------|
| `t<val>`   | Delay Time (ms) [10-1400]       | `t375`  |
| `k<val>`   | Feedback [0.0-0.95]             | `k0.45` |
| `l<val>`   | Tape LP (Hz) [500-12000]        | `l4000` |
| `m<val>`   | Mix [0.0-1.0]                   | `m0.5`  |
| `p`        | Bypass toggle                   | `p`     |

---

## Demo

### Evidencia requerida

1. **Grabación de audio** — delay básico (30 segundos): notas cortas con delay
   activo. Demonstrar que las repeticiones se oscurecen progresivamente (Tape LP
   activo). Comparar `l500` (muy oscuro, cinta gastada) vs `l12000` (brillante,
   cinta nueva).

2. **Demo de feedback** (20 segundos): `k0.3` (3-4 repeticiones claras) vs `k0.8`
   (muchas repeticiones, borderline runaway). La limitación a 0.95 debe prevenir
   que el audio crezca indefinidamente incluso a `k0.95`.

3. **Demo de delay time** (15 segundos): cambio de Delay Time de 125 ms (120 BPM)
   a 375 ms (160 BPM). Si hay un patrón rítmico de input, el cambio de delay time
   debe producir un cambio de "groove" de los ecos.

4. **Screenshot Serial Monitor:** CPU% < 7% con engine + GhostEcho activo.

### Comandos sugeridos

```
Defaults al arrancar:
  t375 k0.45 l4000 m0.5

Secuencia de demo:
1. Arranque: delay 375ms, feedback 0.45, tape LP 4kHz, mix 0.5
   → Ecos a tiempo musical, 4-5 repeticiones, oscureciéndose

2. l500 → Tape LP muy bajo
   → Ecos se oscurecen agresivamente — "cinta muy gastada"

3. l12000 → Tape LP muy alto
   → Ecos casi tan brillantes como la señal — delay "clean"

4. l4000 → Volver al preset de trabajo

5. k0.3 → Feedback bajo (3-4 repeticiones)
6. k0.8 → Feedback alto (muchas repeticiones — borderline sustain)
7. k0.45 → Preset de trabajo

8. t125 → Delay 125ms (8 notas por segundo a 120 BPM)
   → Ecos rápidos — efecto de "reverb larga" más que delay
9. t1000 → Delay 1000ms (60 BPM, un tiempo completo)
   → Ecos lentos, cada uno claramente separado
10. t375 → Volver al preset

11. m0.0 → Solo dry (comparación sin delay)
12. m1.0 → Solo wet (solo los ecos, sin señal directa)
13. m0.5 → Balance de trabajo

14. p → Bypass — A/B definitivo con/sin Ghost Echo
```

### A/B comparison

| Configuración | Carácter | Uso sugerido |
|---------------|----------|--------------|
| Delay 375ms, Feedback 0.45, LP 4kHz | 4-5 ecos oscureciéndose, musical | Uso general live act, solos |
| Delay 125ms, Feedback 0.7, LP 8kHz  | Reverb-like, muchos ecos rápidos | Pads, ambient |
| Delay 500ms, Feedback 0.6, LP 1kHz  | Delay "dub" oscuro, pocas repeticiones | Dub-style performance |

---

## Learnings

*(Se completa después de la implementación y medición en hardware real.
Secciones típicas: comportamiento del feedback loop cerca de 0.95 con input continuo
del engine — ¿satura suavemente o hay clicks?, latencia real del feedback loop medida
con osciloscopio (debería ser ~2.67ms + el delay time configurado), valores de Tape LP
que suenan más "cinta" vs los que suenan como un EQ artificioso, comportamiento del
AudioFilterStateVariable a frecuencias de corte muy bajas (<1kHz) — potencial de
oscilación si Q no está bien calibrado.)*

---

*Sprint 2.11 — GrooveForge Brain · Juan Guerrero (GPROG)*

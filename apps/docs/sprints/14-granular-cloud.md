# Sprint 2.9 — FX Granular Cloud (Fase 2 — Multi-Engine + FX)

**Status:** Sprint 2.9 — En Implementación | Mayo 2026
**Refs:** `apps/docs/05-fx-architecture.md` §1.2, `apps/docs/01-architecture.md` §5.2

---

## Theory

### El sonido como polvo — síntesis granular

Iannis Xenakis —compositor, arquitecto y matemático griego— propuso en los años 60 una
idea radical: el sonido no debería pensarse como una onda continua, sino como un enjambre
de partículas microscópicas. Igual que un haz de luz es un flujo de fotones, una nota
musical es —en su visión— un flujo de granos sonoros: fragmentos brevísimos de audio
que el oído integra como un todo continuo.

La formalización práctica llegó con Curtis Roads en "Microsound" (MIT Press, 2001): el
**grano** es una ventana de audio de entre 1 ms y 500 ms, con un envelope de amplitud
(tipicamente una ventana de Hanning o Gaussian) que evita discontinuidades en los
extremos. Superponer cientos de granos por segundo — tomados del mismo punto del buffer
o de puntos aleatoriamente dispersos — produce texturas que van desde el unísono
enriquecido hasta nubes de sonido completamente abstractas.

**El umbral perceptual clave:** el oído humano no puede distinguir granos individuales
por encima de aproximadamente 30 granos por segundo. Por debajo de ese umbral, se
perciben eventos discretos (clicks, granos separados). Por encima, el cerebro los
integra como timbre continuo. La síntesis granular explota este umbral: puede producir
tono continuo, textura o cualquier punto intermedio solo cambiando la densidad y
posición de los granos.

**Referencia:** Roads, "Microsound" (MIT Press, 2001), Cap. 2 "Granular Synthesis" —
definición formal del grano, ventanas de amplitud estándar (Hanning, Gaussian,
trapezoidal), y el análisis perceptual del umbral de integración temporal del oído
humano. La tabla 2.1 clasifica los fenómenos sonoros por escala temporal: infrasonido
(<20 Hz), mesonivel (20 Hz–20 kHz), micro (1 ms–100 ms) y subatómico (<1 ms).

---

### AudioEffectGranular — cómo funciona en la Teensy Audio Library

La clase `AudioEffectGranular` de la Teensy Audio Library implementa granulización
en tiempo real con dos modos de operación: **passthrough** y **freeze**.

#### El buffer externo int16_t

Antes de usar el granulizador, se le pasa un buffer de memoria donde almacenará el
audio de entrada:

```
AudioEffectGranular granular;
static int16_t grain_buf[12800];  // 12800 × 2 bytes = 25.6 KB

granular.begin(grain_buf, 12800);
```

El buffer almacena muestras en formato int16_t (16 bits, enteros con signo), no en
float32. La razón es presupuesto de RAM: el Teensy 4.1 tiene 1 MB de RAM total.
Un buffer float32 de 12800 muestras ocuparía 51.2 KB — el doble. Con int16_t, ocupa
25.6 KB.

La conversión float32 → int16_t al escribir al buffer y int16_t → float32 al leer
introduce una pérdida de 8 bits de resolución (de 24-bit efectivo del audio path a
16-bit). A 48 kHz y con audio de síntesis (no grabación de alta fidelidad), esta
pérdida es inaudible en el contexto de uso como FX.

**¿Por qué 12800 muestras?** A 48 kHz, 12800 muestras = **267 ms**. Esto alcanza
para granos de hasta 250 ms (el máximo del parámetro Grain Size) con 17 ms de margen.
Un grano de 250 ms es muy largo — la mayoría del uso práctico está en 5-80 ms — pero
tener buffer suficiente evita sobreescritura de granos aún en reproducción.

#### Modo passthrough — granulización en tiempo real

```
granular.startPlayback(false);   // false = no freeze
```

En este modo, el granulizador toma el audio de entrada, lo almacena en el buffer, y
reproduce granos extraídos del buffer con un pequeño retardo (la latencia es
aproximadamente `grain_size / 2` porque los granos se leen desde el centro del buffer
hacia atrás). El audio sale granulizado en tiempo real.

El parámetro `setSpeed(ratio)` controla la velocidad de reproducción de los granos:

```
setSpeed(1.0)  →  unison (mismo pitch que el input)
setSpeed(0.5)  →  octava baja (los granos se reproducen a la mitad de velocidad)
setSpeed(2.0)  →  octava alta (los granos se reproducen al doble de velocidad)
```

En passthrough mode, el pitch shift es exacto: los granos se reproducen a la velocidad
especificada independientemente del contenido frecuencial. No es un phase-vocoder (que
preservaría la duración), sino un cambio real de velocidad — el audio transpuesto
también cambia su duración aparente, pero como los granos son cortos y superpuestos,
el oído no percibe el cambio de duración individual.

#### Modo freeze — snapshot granulizado

```
granular.beginFreeze(grain_size_samples);
```

En freeze mode, el granulizador **congela** el contenido actual del buffer y lo
granuliza en loop indefinidamente. El audio de entrada ya no afecta la salida — el
FX está "congelado" en el momento en que se activó freeze.

Este es el caso de uso estrella del Granular Cloud en performance: el usuario toca un
chord (por ejemplo, C mayor), presiona el botón Freeze, y el chord se convierte en un
pad ambient que sigue vibrando indefinientemente. El usuario puede liberar las notas,
cambiar de engine, o incluso apagar el engine — el Granular Cloud sigue procesando
el snapshot congelado.

`beginFreeze(length)` recibe la longitud del snapshot en muestras. Con Grain Size = 200 ms,
`length = 200 × 10⁻³ × 48000 = 9600 muestras`.

---

### Por qué int16_t — presupuesto de RAM del Teensy 4.1

El Teensy 4.1 tiene 1 MB de RAM total (512 KB DTCM + 512 KB OCRAM). El budget de
memoria según `04-ai-architecture.md` §1.2 distribuye ese MB así:

```
Audio Library + engines:  ~400 KB
TinyML tensor arena:      ~200 KB
Otros / stack / overhead: ~400 KB
```

Con este budget, cada KB adicional de buffer de audio es una decisión de diseño. El
buffer del Granular Cloud en int16_t (25.6 KB) es 4.5× menor que si usara float32
(51.2 KB). En un sistema con múltiples FX y engines corriendo simultáneamente, esta
diferencia puede ser la que decide si el sistema corre establemente o empieza a tener
stack overflows.

La pérdida de resolución de int16_t vs float32 en el contexto de granulización es
también irrelevante perceptualmente: los artefactos de la granulización misma (las
superposiciones de granos, los saltos de posición en freeze mode) enmascaran
completamente el ruido de cuantización de 16-bit.

---

### Parámetros de usuario — física de cada uno

#### Grain Size [5-250 ms]

La duración de cada grano en milisegundos. En samples: `size_samples = size_ms × 48`.

```
5 ms   →   240 samples — granos muy cortos, textura granular perceptible, pitch ambiguo
50 ms  →  2400 samples — granos medios, pitch relativamente estable, shimmer
250 ms → 12000 samples — granos largos, casi continuo, pitch claro, reverb-like
```

Granos muy cortos (<10 ms) producen un efecto de "granular buzz" — el pitch se vuelve
ambiguo porque cada grano es tan corto que el oído no puede integrar suficientes ciclos
de la forma de onda para extraer pitch. Por encima de 30 ms, el pitch del grano
se percibe claramente.

#### Speed [0.25-4.0] — ratio de pitch

Solo aplicable en passthrough mode. En freeze mode, el pitch shift se controla con
`setSpeed` igualmente pero el "audio" que se reproduce es el snapshot congelado, no
el input en tiempo real.

```
Speed = 0.25  →  dos octavas abajo
Speed = 0.5   →  una octava abajo
Speed = 1.0   →  unison
Speed = 2.0   →  una octava arriba
Speed = 4.0   →  dos octavas arriba
```

#### Freeze [toggle]

Alterna entre passthrough mode y freeze mode. Cuando se activa: `beginFreeze()`.
Cuando se desactiva: `startPlayback(false)`.

En performance: Freeze es el parámetro más poderoso del Granular Cloud. Permite
capturar un momento musical (el peak de un build, un chord específico) y convertirlo
en un pad ambient independiente del playing.

#### Mix [0.0-1.0]

Balance dry/wet clásico. A Mix=0.0, el audio del engine pasa sin granulizar. A Mix=1.0,
solo la salida granulizada es audible. En freeze mode con Mix=1.0, el engine puede
silenciarse completamente y el pad granulizado sigue sonando.

---

### CPU estimado

```
AudioEffectGranular (granulización + interpolación):  ~12%
AudioMixer4 (dry/wet):                               ~0.3%
Lógica de freeze toggle en update():                  <0.1%

Total Granular Cloud:                                 ~12-13%

Budget en 05-fx-architecture.md §1.2:                ~12%
```

El overhead del granulizador es mayor que el de filtros o delay simples porque incluye
interpolación entre granos para suavizar las transiciones. La Teensy Audio Library
usa interpolación lineal entre muestras adyacentes al reproducir cada grano, lo que
agrega un costo de cómputo por grano reproducido.

---

### Diagrama conceptual — passthrough vs freeze

```
PASSTHROUGH MODE:
  Engine ──→ [buffer int16_t, 267ms] ──→ [granulizador] ──→ out
                  ↑ escritura continua      ↓ lectura con overlap
              input en tiempo real       granos superpuestos

FREEZE MODE:
  Engine ──→ [buffer int16_t] ← CONGELADO en el momento del toggle
                                      ↓ lectura en loop
                                [granulizador] ──→ out (pad infinito)

  Engine silenciado o cambiado → sin efecto en la salida (buffer congelado)
```

---

### Referencias

- Roads, "Microsound" (MIT Press, 2001) — Cap. 2 "Granular Synthesis": definición
  de grano, ventanas de amplitud, umbral perceptual de integración temporal, taxonomía
  de técnicas granulares. Referencia canónica del campo.
- Xenakis, "Formalized Music" (Pendragon Press, 1992 [1963]) — Cap. 1 "Free Stochastic
  Music": primera formalización matemática de la composición estocástica y la teoría
  de partículas sonoras que precede a la síntesis granular moderna.
- PaulStoffregen, Teensy Audio Library (GitHub, teensy/Audio) — `AudioEffectGranular`:
  implementación de `begin(buffer, maxSamples)`, `startPlayback(freeze)`, `beginFreeze(length)`,
  `setSpeed(ratio)`. El código fuente documenta el formato int16_t del buffer y el
  algoritmo de interpolación entre granos.

---

## Wiring (Cableado)

N/A — sprint solo software / FX digital. Sin hardware nuevo.

---

## Implementation

### Signal Flow

```
inputStream ──────────────────────────────────────────→ _granular (AudioEffectGranular)
                                                                ↓
inputStream ──────────────────────────────────────────→ _dryWet (ch0)  ← dry
_granular ────────────────────────────────────────────→ _dryWet (ch1)  ← wet
                                                                ↓
                                                       AudioOutputI2S L+R
```

Total AudioConnection: 1 (input→granular) + 1 (input→dryWet dry) + 1 (granular→dryWet wet)
+ 2 (dryWet→outL, outR) = 5 conexiones.

### Clase GranularCloud

```
apps/firmware-teensy/src/fx/granular_cloud.h
apps/firmware-teensy/src/fx/granular_cloud.cpp
```

Miembros principales:
- `AudioEffectGranular _granular` — procesador granular de la Teensy Audio Library
- `AudioMixer4 _dryWet` — balance dry/wet
- `AudioConnection` — 5 conexiones declaradas en el constructor
- `static int16_t _buf[12800]` — buffer de audio (declarado static para evitar que
  quede en el stack; va al heap/DTCM en función de la sección de memoria configurada
  en PlatformIO)
- `bool _frozen` — estado del freeze
- `float _grainMs, _speed, _mix` — parámetros del usuario

El buffer se inicializa en el constructor con `_granular.begin(_buf, 12800)`.

### Parámetros

| Parámetro    | Rango          | Default | Setter                          |
|--------------|----------------|---------|----------------------------------|
| Grain Size   | 5 – 250 ms    | 80 ms   | `setGrainSize(float ms)`         |
| Speed        | 0.25 – 4.0    | 1.0     | `setSpeed(float ratio)`          |
| Freeze       | on/off         | off     | `setFreeze(bool f)`              |
| Mix          | 0.0 – 1.0     | 0.5     | `setMix(float m)`                |
| Bypass       | on/off         | off     | `setBypass(bool b)`              |

### Sketch 14-granular-cloud.cpp

```
apps/firmware-teensy/src/sketches/14-granular-cloud.cpp
```

Arquitectura del sketch: chord C mayor sostenido con GranularCloud como capa FX.
Comandos Serial:

| Comando    | Parámetro             | Ejemplo |
|------------|-----------------------|---------|
| `g<val>`   | Grain Size (ms)       | `g50`   |
| `s<val>`   | Speed [0.25-4.0]      | `s0.5`  |
| `f`        | Freeze toggle         | `f`     |
| `m<val>`   | Mix [0.0-1.0]         | `m0.7`  |
| `p`        | Bypass toggle         | `p`     |

---

## Demo

### Evidencia requerida

1. **Grabación de audio** — passthrough mode (mínimo 30 segundos): chord con GranularCloud
   activo. Demonstrar Grain Size 5 ms→250 ms en pasos: la transición de textura
   granular inestable (5 ms) a shimmer (50 ms) a casi-reverb (250 ms) debe ser audible.

2. **Grabación de audio — freeze demo** (mínimo 20 segundos): tocar un chord, activar
   Freeze (comando `f`), luego silenciar el engine (enviar nota off, o volumen=0).
   El pad granulizado debe persistir indefinidamente. Esta es la evidencia clave del modo freeze.

3. **Demo de Speed:** grabación de Speed=0.5 (octava abajo, freeze mode) vs Speed=2.0
   (octava arriba). El pitch shift del granulizador debe ser claro aunque no perfecto
   (no es un phase-vocoder).

4. **Screenshot Serial Monitor:** CPU% < 15% con engine + GranularCloud activo.
   Confirmar que el buffer de 25.6 KB no produce stack overflow (verificar free memory).

### Comandos sugeridos

```
Defaults al arrancar:
  g80 s1.0 m0.5

Secuencia de demo:
1. Arranque: chord con Granular Cloud, Grain Size=80ms, Speed=1.0
   → Shimmer granular suave sobre el chord

2. g5 → Grain Size=5ms
   → Textura granular inestable, pitch ambiguo, buzz

3. g50 → Grain Size=50ms
   → Shimmer más definido, pitch empezando a aclararse

4. g250 → Grain Size=250ms
   → Casi reverb — granos largos, pitch claro, efecto sutil

5. g80 → Volver a working size

6. f → Freeze toggle (capturar el chord)
   → El chord queda congelado como pad

7. m1.0 → 100% wet
   → Solo el pad granulizado (engine directo silenciado del mix)
   → El engine puede apagarse y el pad persiste

8. s0.5 → Speed=0.5 (octava abajo del chord congelado)
9. s2.0 → Speed=2.0 (octava arriba)
10. s1.0 → Unison

11. f → Defreeze (volver a passthrough)

12. m0.0 → Comparación: solo dry, sin granulización
13. m0.5 → Balance de trabajo

14. p → Bypass definitivo — A/B con/sin Granular Cloud
```

### A/B comparison

| Escenario | Sin FX | Con GranularCloud |
|-----------|--------|-------------------|
| Chord C mayor, G=80ms | Timbre directo del engine | Shimmer granular — textura de granos superpuestos |
| Freeze activo, Mix=1.0 | Silencio si el engine se apaga | Pad ambient infinito del chord congelado |

---

## Learnings

*(Se completa después de la implementación y medición en hardware real.
Secciones típicas: RAM efectivamente consumida por el buffer int16_t medida en runtime,
comportamiento del granulizador cuando el grain_size_samples supera la mitad del buffer,
artefactos audibles en el toggle de freeze (click en el momento de congelar/descongelar),
valores de Grain Size útiles en el contexto del engine Moog vs Juno vs Prophet,
latencia adicional del granulizador medida con osciloscopio.)*

---

*Sprint 2.9 — GrooveForge Brain · Juan Guerrero (GPROG)*

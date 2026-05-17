# Sprint 2.10 — FX Spring + Plate (Fase 2 — Multi-Engine + FX)

**Status:** Sprint 2.10 — En Implementación | Mayo 2026
**Refs:** `apps/docs/05-fx-architecture.md` §1.10, `apps/docs/01-architecture.md` §5.2

---

## Theory

### Dos mecánicas, dos reverbs — spring y plate

El reverb no es un fenómeno único: es cualquier proceso en el que un sonido genera
reflexiones que se acumulan y decaen. La historia de la reverberación artificial en
estudios de grabación pasó por dos tecnologías mecánicas radicalmente diferentes —
el spring reverb y el plate reverb — que sobreviven en forma de emulación digital
porque sus caracteres sonoros son irreemplazables.

#### Spring reverb — Hammond (1941) y el "drip" característico

El spring reverb fue patentado por Laurens Hammond (el mismo fabricante del órgano
Hammond) para agregar reverberación a órganos en locales pequeños. El principio
físico es simple: el audio se convierte en vibración mecánica mediante un transductor
en un extremo de un resorte de metal, recorre el resorte, y llega a otro transductor
en el otro extremo que reconvierte la vibración en audio.

El sonido característico del spring reverb proviene de la física del resorte:

**Velocidad de propagación no uniforme:** en un resorte helicoidal, las ondas de
compresión (longitudinales) viajan a velocidad diferente que las ondas de torsión
(transversales). El input excita ambos tipos de onda simultáneamente, y llegan al
transductor de salida en tiempos diferentes. Esto produce el primer "drip" — ese
sonido peculiar de splash/splash que precede al cuerpo de la reverb.

**Reflexiones en los extremos:** cuando la onda llega al extremo del resorte (donde
está fijado), rebota. Las ondas de ida y vuelta generan patrones de interferencia
que el oído percibe como coloración tonal irregular en la reverb — la respuesta en
frecuencia del spring reverb no es plana. Algunas frecuencias resuenan más que otras
según la longitud del resorte.

**El "twang" al golpear el amplificador:** si el rack de amplificador con spring reverb
recibe un golpe físico, el resorte recibe un impulso mecánico masivo. El resultado es
ese sonido de "boing" que cualquiera que haya tocado una guitarra con Fender reverb
reconoce. No es un artefacto — es el comportamiento físico real del resorte.

**Referencia:** Parker, "Efficient simulation of spring reverb" (DAFX 2011, París) —
análisis de la respuesta en frecuencia medida de spring reverbs reales (Accutronics
Type 4 y Type 8), modelado de las características espectrales de coloración, y
comparación de eficiencia entre modelos de convolución, DWM y filtros recursivos.
La figura 3 muestra la respuesta al impulso medida con el "drip" inicial claramente
visible como conjunto de llegadas discretas antes del cuerpo de reverb.

#### Plate reverb — EMT 140 (1957) y la densidad sedosa

La EMT 140 fue el primer plate reverb comercial. Es un artefacto enorme: una placa de
acero de 2.4 metros de largo × 1.4 metros de ancho × 0.5 mm de espesor, suspendida
en tensión mediante muelles desde un marco de acero en una caja sellada. Un transductor
piezoeléctrico en el centro convierte el audio en vibración de la placa. Dos transductores
en posiciones distintas recogen la vibración y producen la salida estéreo.

El sonido del plate reverb difiere fundamentalmente del spring:

**Alta densidad modal:** una placa de 2.4 m × 1.4 m tiene miles de modos resonantes
en el rango audible — muchos más que un resorte. Esta densidad tan alta de reflexiones
produce una reverb percibida como "suave" y "sedosa": no hay coloración tonal irregular,
no hay "drip" inicial dramático. La densidad de modos es tan alta que suenan como
ruido continuo de alta densidad, no como reflexiones discretas.

**Sin efecto de Doppler por temperatura:** a diferencia del spring, la placa no tiene
elementos que se muevan longitudinalmente (solo vibración transversal). La reverb del
plate es tímbrica mente más "plana" (en el buen sentido) — no introduce coloraciones
erráticas.

**Tiempo de reverb controlable:** la EMT 140 tiene un amortiguador físico ajustable
(una plancha de material absorbente que puede acercarse o alejarse de la placa). Al
acercarlo, se reduce el T60 aumentando el amortiguamiento de los modos de alta
frecuencia primero (los HF se extinguen antes que los LF). Esto corresponde directamente
al parámetro Damping de AudioEffectFreeverb.

**Referencia:** Schroeder, "Natural Sounding Artificial Reverberation" (JAES, 1962) —
artículo fundacional que introduce las redes de filtros allpass + comb para sintetizar
reverb artificial. Schroeder diseñó el sistema de reverb del Deutsches Rundfunk que
precedió a la EMT 140, y este paper establece los principios matemáticos de densidad
de ecos que subyacen a todos los reverbs algorítmicos modernos incluyendo Freeverb.

---

### AudioEffectFreeverb — el algoritmo Schroeder/Moorer

La Teensy Audio Library incluye `AudioEffectFreeverb`, una implementación del algoritmo
Freeverb de Jezar at Dreampoint (publicado como código abierto en 2000). Freeverb es
una implementación práctica de la arquitectura que Schroeder describió en 1962:

```
input ──→ [comb₁] ──→ [allpass₁]
       ──→ [comb₂] ──→ [allpass₂] ──→ suma ──→ output
       ──→ [comb₃] ──→ [allpass₃]
       ──→ [comb₄] ──→ [allpass₄]
```

**Los filtros comb (retroalimentados):** cada comb filter introduce un delay de `D`
muestras con feedback. La respuesta en frecuencia del comb es una serie de picos en
`0, fs/D, 2fs/D, 3fs/D...` Hz. El feedback loop hace que la energía circule y decaiga
exponencialmente. Con varios combs en paralelo (longitudes de delay `D` diferentes y
primas entre sí para evitar coincidencias), la superposición crea densidad de ecos.

**El parámetro roomsize(0-1):** en Freeverb, `roomsize` escala directamente las
longitudes de los delays de los combs. Un roomsize mayor = delays más largos = T60
más largo. La relación no es lineal — la percepción de tamaño del espacio depende
logarítmicamente del T60.

**El parámetro damping(0-1):** cada comb filter tiene un filtro LP en su feedback loop.
`damping` controla la frecuencia de corte de ese LP: damping alto = LP más agresivo =
las altas frecuencias decaen más rápido. Esto modela el efecto del amortiguador de
la EMT 140 y el amortiguamiento del aire en espacios reales (el aire absorbe HF
preferentemente sobre LF).

```
roomsize = 0.3, damping = 0.9  →  T60 corto, HF mueren rápido  → spring-like (dry, coloración)
roomsize = 0.7, damping = 0.2  →  T60 largo, HF sostenidos      → plate-like (suave, brillante)
```

**Referencia:** Schroeder, "Natural Sounding Artificial Reverberation" (JAES, 1962),
§3 "Reverberators Using Feedback" — derivación de la condición de densidad de ecos
necesaria para que el reverb suene natural, y la arquitectura de comb + allpass en
paralelo que resuelve el problema de los picos de respuesta en frecuencia del comb solo.

---

### Dos Freeverb en paralelo — spring vs plate con crossfade

El Spring + Plate usa dos instancias de `AudioEffectFreeverb` simultáneamente:

- `_spring`: roomsize bajo (~0.3-0.4), damping alto (~0.7-0.9) — emula el carácter
  seco y coloreado del spring
- `_plate`: roomsize medio-alto (~0.6-0.8), damping bajo (~0.1-0.3) — emula la
  densidad sedosa del plate

El parámetro **Algorithm** selecciona el modo de operación:

```
Algorithm = 0 (spring):  _blendMix ganancia 1.0 para _spring, 0.0 para _plate
Algorithm = 1 (plate):   _blendMix ganancia 0.0 para _spring, 1.0 para _plate
Algorithm = 2 (blend):   _blendMix usa el parámetro Blend para mezclar ambas instancias
```

El crossfade entre algoritmos se hace via `AudioMixer4 _blendMix` — los gains se
actualizan en `update()`, no en el audio thread. La transición audible de spring a plate
(cuando Algorithm=2 y el usuario mueve el encoder de Blend) es un crossfade continuo,
no un switch abrupto.

```
_spring ──→ _blendMix (ch0, gain = 1.0 - blend)
_plate  ──→ _blendMix (ch1, gain = blend)
                ↓
          _dryWet (ch1)  ← wet
input   ──→ _dryWet (ch0) ← dry
                ↓
          AudioOutputI2S L+R
```

---

### Diferencia audible — spring vs plate en la práctica

La diferencia entre spring y plate se escucha claramente en el ataque de la reverb:

**Spring:** los primeros 30-80 ms de la reverb tienen presencia del "drip" — reflexiones
discretas del resorte. Si el audio de entrada tiene transientes duros (ataques de percusión,
notas staccato), el spring los decora con ese splash inicial. El cuerpo de la reverb
tiene coloración tonal (respuesta en frecuencia irregular), lo que da "carácter".

**Plate:** el ataque de la reverb es suave y denso desde el primer instante — no hay
drip porque no hay reflexiones discretas, solo densidad modal inmediata. El timbre de
la reverb es más "neutral" y sedoso — se percibe como un halo alrededor del sonido
en lugar de una respuesta de sala.

En la emulación con Freeverb (sin modelo físico exacto del spring), la diferencia
entre los dos algoritmos es principalmente de T60, damping y densidad de ecos (roomsize).
No es una emulación perfecta del spring físico — en particular, el "drip" es solo
aproximado. Pero el carácter diferencial (seco/coloreado vs suave/brillante) es
claramente audible.

---

### CPU estimado

```
2× AudioEffectFreeverb (spring + plate):   ~10%  (~5% cada uno)
1× AudioMixer4 (_blendMix):               ~0.5%
1× AudioMixer4 (_dryWet):                 ~0.3%

Total Spring + Plate:                      ~10-11%

Budget en 05-fx-architecture.md §1.10:    ~10%
```

---

### Parámetros de usuario

#### Algorithm [0/1/2]

```
0 = spring:  roomsize=0.35, damping=0.8 — spring tank vintage
1 = plate:   roomsize=0.75, damping=0.2 — EMT 140 plate
2 = blend:   mix manual controlado por el parámetro Blend
```

En Algorithm 0 y 1, los parámetros Room Size y Damping se fijan a los presets del
algoritmo. En Algorithm 2, Room Size y Damping controlan `_spring` y `_plate` se
mantienen en sus presets; el Blend hace el crossfade.

#### Room Size [0.0-1.0] — solo en Algorithm 2

Escala `roomsize` de `_spring` cuando Algorithm=2. El `_plate` mantiene su preset.
Permite al usuario personalizar la longitud del spring cuando está en modo blend.

#### Damping [0.0-1.0] — solo en Algorithm 2

Escala `damping` de `_spring` cuando Algorithm=2. Damping alto = spring más oscuro
(pierde HF rápido). Damping bajo = spring más brillante.

#### Blend [0.0-1.0] — solo en Algorithm 2

```
Blend = 0.0  →  100% spring, 0% plate
Blend = 0.5  →  50% spring, 50% plate
Blend = 1.0  →  0% spring, 100% plate
```

Los gains de `_blendMix` se calculan como:
```
_blendMix.gain(0, 1.0f - blend);  // spring
_blendMix.gain(1, blend);          // plate
```

#### Mix [0.0-1.0]

Balance dry/wet. A Mix=0.0, solo el signal directo del engine. A Mix=1.0, solo la
reverb (sin señal directa).

---

### Diagrama de algoritmos

```
Algorithm=0 (spring):
  input ──→ _spring (roomsize=0.35, damping=0.8) ──→ _blendMix (ch0, gain=1.0)
                                                   _blendMix (ch1, gain=0.0) ← _plate inactivo
  _blendMix ──→ _dryWet (ch1, wet)
  input     ──→ _dryWet (ch0, dry)

Algorithm=1 (plate):
  input ──→ _plate (roomsize=0.75, damping=0.2) ──→ _blendMix (ch1, gain=1.0)
                                                  _blendMix (ch0, gain=0.0) ← _spring inactivo
  _blendMix ──→ _dryWet (ch1, wet)
  input     ──→ _dryWet (ch0, dry)

Algorithm=2 (blend):
  input ──→ _spring ──→ _blendMix (ch0, gain=1.0-blend)
  input ──→ _plate  ──→ _blendMix (ch1, gain=blend)
  _blendMix ──→ _dryWet (ch1, wet)
  input     ──→ _dryWet (ch0, dry)
```

Nota: aunque en Algorithm=0 y 1 una de las dos instancias Freeverb tiene gain=0 en
_blendMix, ambas instancias siguen procesando el audio (CPU constante). Una optimización
futura sería detener la instancia inactiva, pero a 5% CPU cada una el overhead es
aceptable y simplifica la implementación.

---

### Referencias

- Schroeder, "Natural Sounding Artificial Reverberation" (JAES Vol. 10, Nro. 3, 1962)
  — arquitectura comb + allpass; fundamento matemático de Freeverb.
- Parker, "Efficient simulation of spring reverb" (DAFX 2011, París) — análisis físico
  del spring reverb real, mediciones de respuesta al impulso, modelo de filtros eficiente.
- Jezar at Dreampoint, "Freeverb" (código abierto, 2000) — implementación original de
  la arquitectura Schroeder/Moorer que la Teensy Audio Library porta como `AudioEffectFreeverb`.
- PaulStoffregen, Teensy Audio Library (GitHub, teensy/Audio) — `AudioEffectFreeverb`:
  `roomsize(val)`, `damping(val)`, implementación de los ocho combs y cuatro allpass.

---

## Wiring (Cableado)

N/A — sprint solo software / FX digital. Sin hardware nuevo.

---

## Implementation

### Signal Flow

```
inputStream ──→ _spring (AudioEffectFreeverb, roomsize=0.35, damping=0.8)
inputStream ──→ _plate  (AudioEffectFreeverb, roomsize=0.75, damping=0.2)

_spring ──→ _blendMix (ch0)  gain = 1.0 - blend (en Algorithm=2)
_plate  ──→ _blendMix (ch1)  gain = blend

_blendMix ──→ _dryWet (ch1)  ← wet
inputStream ─→ _dryWet (ch0) ← dry
                    ↓
           AudioOutputI2S L+R
```

Total AudioConnection: 2 (input→spring, input→plate) + 2 (spring,plate→blendMix) +
2 (blendMix,input→dryWet) + 2 (dryWet→outL, outR) = 8 conexiones.

### Clase SpringPlate

```
apps/firmware-teensy/src/fx/spring_plate.h
apps/firmware-teensy/src/fx/spring_plate.cpp
```

Miembros principales:
- `AudioEffectFreeverb _spring` — instancia spring
- `AudioEffectFreeverb _plate` — instancia plate
- `AudioMixer4 _blendMix` — crossfade spring↔plate
- `AudioMixer4 _dryWet` — balance dry/wet final
- `AudioConnection` — 8 conexiones declaradas en el constructor
- `int _algorithm` — 0=spring, 1=plate, 2=blend
- `float _roomSize, _damping, _blend, _mix` — parámetros del usuario

El método `_applyAlgorithm()` recalcula los gains de `_blendMix` y las llamadas a
`roomsize()` / `damping()` de las instancias Freeverb según el modo activo.

### Parámetros

| Parámetro    | Rango         | Default | Setter                          |
|--------------|---------------|---------|----------------------------------|
| Algorithm    | 0, 1, 2       | 0       | `setAlgorithm(int a)`            |
| Room Size    | 0.0 – 1.0    | 0.35    | `setRoomSize(float r)`           |
| Damping      | 0.0 – 1.0    | 0.8     | `setDamping(float d)`            |
| Blend        | 0.0 – 1.0    | 0.5     | `setBlend(float b)`              |
| Mix          | 0.0 – 1.0    | 0.5     | `setMix(float m)`                |
| Bypass       | on/off        | off     | `setBypass(bool b)`              |

### Sketch 15-spring-plate.cpp

```
apps/firmware-teensy/src/sketches/15-spring-plate.cpp
```

Comandos Serial:

| Comando    | Parámetro                     | Ejemplo |
|------------|-------------------------------|---------|
| `a<n>`     | Algorithm [0=spring,1=plate,2=blend] | `a1`    |
| `r<val>`   | Room Size [0.0-1.0]           | `r0.6`  |
| `d<val>`   | Damping [0.0-1.0]             | `d0.5`  |
| `b<val>`   | Blend [0.0-1.0]               | `b0.7`  |
| `m<val>`   | Mix [0.0-1.0]                 | `m0.5`  |
| `p`        | Bypass toggle                 | `p`     |

---

## Demo

### Evidencia requerida

1. **Grabación de audio** — A/B spring vs plate (mínimo 30 segundos): mismo chord,
   `a0` (spring) vs `a1` (plate). La diferencia de carácter debe ser audible: spring
   más seco y colorado, plate más suave y brillante.

2. **Demo de blend** (mínimo 20 segundos): `a2 b0.0` (100% spring) → `b0.5` (50/50)
   → `b1.0` (100% plate). El crossfade debe ser continuo sin clicks.

3. **Demo de damping en spring** (`a0` o `a2 b0.0`): `d0.1` (spring brillante) vs
   `d0.9` (spring oscuro). La diferencia de HF rolloff debe ser audible.

4. **Screenshot Serial Monitor:** CPU% < 12% con engine + Spring+Plate activo.

### Comandos sugeridos

```
Defaults al arrancar:
  a0 m0.5 (spring, mix=50%)

Secuencia de demo:
1. Arranque: spring, mix=0.5
   → Reverb con carácter de spring vintage

2. a1 → plate
   → Reverb más suave y brillante — comparación directa

3. a0 → volver a spring

4. a2 b0.0 → blend mode, 100% spring
5. b0.5    → 50/50 spring+plate
6. b1.0    → 100% plate (igual que a1)
7. b0.7    → preset de trabajo: más plate que spring

8. a0 d0.1 → spring con HF sostenido (brillante)
9. a0 d0.9 → spring con HF atenuado (oscuro, más vintage)

10. m0.0 → solo dry (comparación sin reverb)
11. m1.0 → solo wet (reverb pura)
12. m0.5 → balance de trabajo

13. p → bypass — A/B definitivo con/sin Spring+Plate
```

### A/B comparison

| Algoritmo | Carácter | Uso sugerido |
|-----------|----------|--------------|
| Spring (a0) | Seco, coloreado, "wobbly" | Leads, solos, dub effects |
| Plate (a1)  | Suave, brillante, uniforme | Pads, voicings, master bus |
| Blend 50% (a2 b0.5) | Intermedio — cuerpo del spring + suavidad del plate | Uso general live act |

---

## Learnings

*(Se completa después de la implementación y medición en hardware real.
Secciones típicas: comparación auditiva de los presets de spring/plate con spring
reverbs reales (Accutronics) y plates reales (EMT 140 via referencia de grabación),
CPU real de dos instancias Freeverb simultáneas vs el estimado, comportamiento del
crossfade cuando ambas instancias corren en paralelo a gain máximo (posible suma de
amplitud que supera 0 dBFS), ajuste de gains en _blendMix para evitar clipping.)*

---

*Sprint 2.10 — GrooveForge Brain · Juan Guerrero (GPROG)*

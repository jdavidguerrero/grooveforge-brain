# Sprint 2.8 — FX Cymatic Resonator (Fase 2 — Multi-Engine + FX)

**Status:** Sprint 2.8 — En Implementación | Mayo 2026
**Refs:** `apps/docs/05-fx-architecture.md` §1.1, `apps/docs/01-architecture.md` §5.2

---

## Theory

### El fenómeno cimático — superficies que revelan su propia voz

En 1787, el físico alemán Ernst Chladni descubrió que si esparcías arena sobre una placa
metálica y hacías vibrar la placa con un arco de violín, la arena migraba hacia los
**nodos** — los puntos de la placa que no vibran — y formaba patrones geométricos
perfectos: estrellas, círculos concéntricos, cuadrículas de diamantes. Los puntos que
vibran con mayor amplitud (los antinodos) expulsan la arena; los nodos la acumulan.

Cada frecuencia de resonancia de la placa produce un patrón diferente, tan específico
que puede usarse para "fotografiar" la frecuencia. A 300 Hz aparece un patrón. A 440 Hz
aparece otro completamente distinto. Estos son los **patrones de Chladni**, y son una
visualización directa de los modos resonantes del objeto.

**La analogía para el audio:** cuando el audio del engine pasa por el Cymatic Resonator,
el FX selecciona cuatro frecuencias de resonancia específicas — los "modos" del objeto
virtual — y las amplifica fuertemente mientras atenúa todo lo demás. El resultado es
como si el audio vibrara a través de ese cristal o esa placa metálica: el timbre cambia
según los modos resonantes del material imaginario.

---

### Por qué 4 filtros bandpass en paralelo — el modelo de modos resonantes

Un objeto vibrante real no tiene un solo modo resonante: tiene muchos, cada uno en una
frecuencia diferente. Cuando el audio excita el objeto (por ejemplo, cuando el sonido
del engine viaja por el cristal), cada modo responde independientemente según cuánta
energía del audio cae en su frecuencia.

La forma más directa de modelar esto digitalmente es usar **filtros bandpass resonantes
en paralelo**: un filtro por modo. Cada filtro solo deja pasar las frecuencias cercanas
a su frecuencia central, amplificando fuertemente las componentes del audio que coinciden
con esa resonancia y atenuando el resto.

Con cuatro filtros en paralelo, cada uno con una frecuencia diferente, la salida es la
superposición de cuatro resonancias simultáneas — el audio "visto a través de" un objeto
con esos cuatro modos. El `AudioMixer4` suma las cuatro salidas en una sola señal.

```
inputStream ──┬──→ [AudioFilterBiquad BP, f₁, Q] ──→ AudioMixer4 (ch0)
              ├──→ [AudioFilterBiquad BP, f₂, Q] ──→ AudioMixer4 (ch1)
              ├──→ [AudioFilterBiquad BP, f₃, Q] ──→ AudioMixer4 (ch2)
              └──→ [AudioFilterBiquad BP, f₄, Q] ──→ AudioMixer4 (ch3)
                                                          ↓
                                                   [AudioMixer4 dry/wet]
                                                          ↓
                                                   AudioOutputI2S L+R
```

La Teensy Audio Library permite múltiples `AudioConnection` desde el mismo objeto fuente
(fan-out implícito): se crean 4 conexiones desde `inputStream` hacia los cuatro filtros
sin necesidad de un splitter explícito.

---

### Los ratios 1:2:3:5 — harmonic series con skip del cuarto

La elección de ratios entre modos no es arbitraria. La serie armónica natural es 1, 2,
3, 4, 5, 6... — los overtones de una cuerda o un tubo. Pero usar los cuatro primeros
armónicos (1:2:3:4) produce un timbre predecible y "plano", porque las relaciones de
octava y quinta perfecta son exactamente lo que el oído ya espera.

El Cymatic Resonator usa los ratios **1:2:3:5**, saltando el cuarto armónico:

```
Modo 1: f₁ = BASE_FREQ × 1   →  La4 (440 Hz) a Tune=1.0
Modo 2: f₂ = BASE_FREQ × 2   →  La5 (880 Hz)   — octava
Modo 3: f₃ = BASE_FREQ × 3   →  Mi6 (1320 Hz)  — quinta más octava
Modo 4: f₄ = BASE_FREQ × 5   →  La6+mayor (2200 Hz) — tercera mayor dos octavas arriba
```

El salto del armónico 4 (la segunda octava de la fundamental, 1760 Hz) crea un hueco
en el espectro que el oído percibe como tensión — el espectro no está "lleno" de manera
predecible. La tercera mayor (× 5) aporta brillo sin redundar con los modos
anteriores, creando un timbre que suena a la vez armónico y levemente inesperado.

Este es el mismo principio por el que las campanas y los gongs suenan interesantes: sus
modos resonantes no siguen la serie armónica entera sino una versión modificada con
saltos e inarmónicidades.

**Referencia:** Chladni, "Entdeckungen über die Theorie des Klanges" (Leipzig, 1787) —
primera documentación sistemática de los patrones nodales en superficies vibrantes. Los
patrones se forman exactamente en las frecuencias propias (eigenfrequencies) del objeto,
que dependen de su geometría y material.

---

### Q del filtro — cómo controla el carácter de la resonancia

El parámetro Q (factor de calidad) del filtro bandpass controla el ancho de banda de cada
resonancia. La relación es directa: `BW = f₀ / Q`.

```
Q = 10,  f₀ = 440 Hz  →  BW = 44 Hz   — resonancia amplia, suena "suave"
Q = 30,  f₀ = 440 Hz  →  BW = 14.7 Hz — resonancia moderada
Q = 80,  f₀ = 440 Hz  →  BW = 5.5 Hz  — resonancia estrecha, suena "afinada" y metálica
```

Un Q bajo (10) produce una resonancia que colorea un rango amplio de frecuencias — el
sonido se percibe como un formante vocal o un EQ de presencia, no como una resonancia
afinada. Un Q alto (80) produce una resonancia tan estrecha que el oído la percibe
como una nota musical: el filtro selecciona componentes de frecuencia muy específicas
del audio de entrada y las amplifica como si el cristal tuviera ese pitch.

El rango del parámetro Resonance Q [10-80] permite al usuario recorrer el espacio
entre "coloración tímbrica suave" (Q=10, el Cymatic Resonator como EQ resonante) y
"efecto de afinación cristalina" (Q=80, el FX como resonador de pitch selectivo).

**Referencia:** Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011), Cap. 3
"Filters", §3.7 "Resonant Filters" — derivación de coeficientes biquad para bandpass
con parametrización de Q y análisis de la respuesta en frecuencia para diferentes valores.

---

### LFO modulando tune — el cristal que respira

Un filtro bandpass estático con Q alto suena artificial — el cristal del mundo real nunca
vibra exactamente en la misma frecuencia de manera sostenida. Las variaciones de
temperatura, tensión mecánica y amortiguamiento hacen que la frecuencia de resonancia
oscile levemente.

El LFO (Low Frequency Oscillator) a 0.1–3 Hz modula el parámetro `tune` en un rango
de ±5% alrededor del valor nominal. Esto significa que las frecuencias de los cuatro
modos se desplazan levemente en sincronía (todos suben y bajan juntos, manteniendo
los ratios 1:2:3:5 constantes).

El efecto audible a 0.5 Hz (una oscilación cada dos segundos) es de un cristal que
"respira" — el timbre del resonador se expande y contrae suavemente. A 3 Hz, el
efecto es más parecido a un vibrato del filtro, con un carácter más dramático.

La modulación se aplica en `update()` (equivalente a `loop()` en el contexto de clase),
no en el audio thread — se actualiza la frecuencia central de los cuatro filtros cada
vez que el LFO completa un incremento de fase. La Teensy Audio Library aplica los
nuevos coeficientes del filtro en el próximo bloque de 128 muestras sin producir
discontinuidades audibles.

```
tune_modulated = tune_base × (1.0 + lfo_depth × sin(2π × lfo_rate × t))

f_n = BASE_FREQ × ratio_n × tune_modulated

Para ratio_n = {1, 2, 3, 5}:
f₁ = 440 × 1 × tune_modulated
f₂ = 440 × 2 × tune_modulated
f₃ = 440 × 3 × tune_modulated
f₄ = 440 × 5 × tune_modulated
```

---

### Parámetros de usuario — física de cada uno

#### Tune [0.5-2.0]

Transpone todos los modos simultáneamente escalando `BASE_FREQ`. Los ratios 1:2:3:5
se mantienen constantes — el "color" del resonador no cambia, solo su registro:

```
Tune = 0.5:  BASE_FREQ × 0.5 = 220 Hz  →  La3 (octava baja)
Tune = 1.0:  BASE_FREQ × 1.0 = 440 Hz  →  La4 (referencia)
Tune = 2.0:  BASE_FREQ × 2.0 = 880 Hz  →  La5 (octava alta)
```

En performance: Tune hacia arriba hace el resonador brillante y estridente (modos en
agudos), Tune hacia abajo lo hace oscuro y con cuerpo (modos en graves).

#### Density [1-4 modos activos]

Controla cuántos modos están activos simultáneamente. Con Density=1, solo el modo 1
(f₁ = BASE_FREQ × 1) está activo — resonancia pura de frecuencia única, casi como un
pitch filter o un wah estático. Con Density=4, los cuatro modos resuenan en paralelo
y el timbre es complejo.

La implementación pone gain=0.0 en los canales del `AudioMixer4` correspondientes
a los modos inactivos.

#### Resonance Q [10-80]

El Q de todos los filtros simultáneamente (mismo Q para los cuatro modos). Ver análisis
arriba. En performance: Q bajo para una coloración suave y de fondo; Q alto para
un carácter cristalino y afinado que se impone en la mezcla.

#### LFO Rate [0.1-3.0 Hz]

Velocidad del LFO que modula `tune`. A 0.1 Hz (un ciclo cada 10 segundos), el efecto
es casi imperceptible — una deriva lenta del carácter. A 0.5 Hz es el punto de trabajo
recomendado para pad y ambient. A 3 Hz ya es un vibrato rápido, más adecuado para
solos o builds.

El LFO es sinusoidal puro — a diferencia del Phase Chorus (que tiene drift caótico),
el Cymatic Resonator usa una ola sinusoidal precisa. La organicidad viene de la
interacción entre los cuatro modos modulados simultáneamente, no de imperfección en
el LFO mismo.

#### Mix [0.0-1.0]

Balance dry/wet clásico:

```
gain_dry = 1.0 - mix
gain_wet = mix
```

El valor de trabajo recomendado es Mix=0.4–0.7. A Mix=1.0 (100% wet), el audio se
convierte enteramente en las cuatro resonancias — todo lo que no cae dentro del ancho
de banda de los filtros desaparece. Útil para efectos de "freeze" de frecuencia en
un drop.

---

### CPU estimado

```
4× AudioFilterBiquad (bandpass, Q variable): ~8.0%  (~2.0% cada uno)
1× AudioMixer4 (suma de modos):              ~0.5%
1× AudioMixer4 (dry/wet):                   ~0.3%
LFO update en update():                      <0.1%

Total Cymatic Resonator:                     ~8-9%

Budget en 05-fx-architecture.md §1.1:        ~8%
```

---

### Diagrama de modos — ratios y frecuencias de referencia

```
BASE_FREQ = 440 Hz, Tune = 1.0, Density = 4

Modo | Ratio | Frecuencia | Q=10 BW | Q=80 BW
-----|-------|------------|---------|--------
  1  |   ×1  |  440 Hz   |  44 Hz  |  5.5 Hz
  2  |   ×2  |  880 Hz   |  88 Hz  |  11 Hz
  3  |   ×3  | 1320 Hz   | 132 Hz  |  16.5 Hz
  4  |   ×5  | 2200 Hz   | 220 Hz  |  27.5 Hz

(skip ×4 = 1760 Hz — el hueco que crea tensión armónica)
```

---

### Referencias

- Chladni, "Entdeckungen über die Theorie des Klanges" (Leipzig, 1787) — primer
  tratamiento sistemático de los patrones nodales en superficies vibrantes; base
  conceptual de la síntesis modal.
- Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011), Cap. 3 "Filters",
  §3.7 "Resonant Filters" — coeficientes biquad para bandpass, análisis de Q.
- Smith, "Introduction to Digital Filters with Audio Applications" (W3K Publishing,
  2007), Cap. 8 "Biquad Filters" — estabilidad de filtros biquad en función de Q
  y frecuencia normalizada en aritmética float32.
- PaulStoffregen, Teensy Audio Library (GitHub, teensy/Audio) — `AudioFilterBiquad`
  con `setBandpass(stage, freq, Q)` usando coeficientes calculados en doble precisión
  antes de almacenar en float32.

---

## Wiring (Cableado)

N/A — sprint solo software / FX digital. Sin hardware nuevo.

---

## Implementation

### Signal Flow

```
inputStream ──┬──→ [_res0: AudioFilterBiquad BP, f=BASE×1,  Q] ──→ _resMix (ch0)
              ├──→ [_res1: AudioFilterBiquad BP, f=BASE×2,  Q] ──→ _resMix (ch1)
              ├──→ [_res2: AudioFilterBiquad BP, f=BASE×3,  Q] ──→ _resMix (ch2)
              └──→ [_res3: AudioFilterBiquad BP, f=BASE×5,  Q] ──→ _resMix (ch3)
                                                                        ↓
inputStream ──────────────────────────────────────────────────→ _dryWet (ch0)  ← dry
_resMix ──────────────────────────────────────────────────────→ _dryWet (ch1)  ← wet
                                                                        ↓
                                                               AudioOutputI2S L+R
```

Fan-out desde inputStream: 4 conexiones al audio de los filtros + 1 conexión al canal
dry del mixer final. Total AudioConnection: 4 + 4 + 1 + 1 + 2 = 12 conexiones.

### Clase CymaticResonator

```
apps/firmware-teensy/src/fx/cymatic_resonator.h
apps/firmware-teensy/src/fx/cymatic_resonator.cpp
```

Miembros principales:
- `AudioFilterBiquad _res[4]` — cuatro filtros bandpass paralelos
- `AudioMixer4 _resMix` — suma de los cuatro modos resonantes
- `AudioMixer4 _dryWet` — balance dry/wet final
- `AudioConnection` — 12 conexiones declaradas en el constructor
- `float _tune, _q, _lfoRate, _mix` — parámetros del usuario
- `float _lfoPhase` — fase acumulada del LFO (radianes)
- `void update()` — avanza el LFO y actualiza frecuencias de los filtros

Los ratios se definen como constantes `static constexpr float RATIOS[4] = {1.0f, 2.0f, 3.0f, 5.0f}`.

### Parámetros

| Parámetro    | Rango         | Default | Setter                          |
|--------------|---------------|---------|----------------------------------|
| Tune         | 0.5 – 2.0    | 1.0     | `setTune(float t)`               |
| Density      | 1 – 4        | 4       | `setDensity(int d)`              |
| Resonance Q  | 10 – 80      | 30      | `setQ(float q)`                  |
| LFO Rate     | 0.1 – 3.0 Hz | 0.5     | `setLfoRate(float hz)`           |
| Mix          | 0.0 – 1.0    | 0.5     | `setMix(float m)`                |
| Bypass       | on/off        | off     | `setBypass(bool b)`              |

### Sketch 13-cymatic-resonator.cpp

```
apps/firmware-teensy/src/sketches/13-cymatic-resonator.cpp
```

Arquitectura del sketch: chord C mayor sostenido (mismo approach que sprints anteriores)
con CymaticResonator como capa FX. Comandos Serial:

| Comando    | Parámetro       | Ejemplo |
|------------|-----------------|---------|
| `u<val>`   | Tune [0.5-2.0]  | `u1.5`  |
| `d<n>`     | Density [1-4]   | `d3`    |
| `q<val>`   | Resonance Q     | `q60`   |
| `l<val>`   | LFO Rate (Hz)   | `l0.5`  |
| `m<val>`   | Mix [0.0-1.0]   | `m0.6`  |
| `p`        | Bypass toggle   | `p`     |

---

## Demo

### Evidencia requerida

1. **Grabación de audio** (mínimo 45 segundos): chord C mayor con CymaticResonator activo.
   Demonstrar recorrido de Density 1→4: desde resonancia única (D=1) hasta timbre complejo (D=4).

2. **Demo de Q:** grabación de Q=10 (coloración suave) vs Q=80 (resonancia cristalina afinada).
   La diferencia debe ser audible incluso en altavoces sin subwoofer — el Q alto produce
   pitches claramente identificables en el wet signal.

3. **Demo de LFO:** grabación de LFO Rate=0.5 Hz (cristal respirando) vs Rate=3.0 Hz
   (vibrato de filtro). El LFO no debe producir artefactos ni clicks — actualización suave
   de coeficientes entre bloques.

4. **Demo de Tune:** sweep de Tune=0.5 (modos en graves) a Tune=2.0 (modos en agudos).
   Los cuatro modos se transponen en bloque, manteniendo los ratios 1:2:3:5.

5. **Screenshot Serial Monitor:** CPU% < 10% con engine + CymaticResonator activo.

### Comandos sugeridos

```
Defaults al arrancar:
  u1.0 d4 q30 l0.5 m0.5

Secuencia de demo:
1. Arranque: chord con CymaticResonator en defaults
   → Cuatro modos resonantes, LFO suave, mix 50%

2. d1 → Density=1 (solo modo 1)
   → Resonancia única a 440 Hz — pitch filter puro

3. d2 → Density=2 (modos 1+2)
   → Octava superpuesta — más cuerpo

4. d4 → Density=4 (todos)
   → Timbre complejo 440+880+1320+2200 Hz

5. q10 → Q bajo — resonancia amplia, suave
6. q80 → Q alto — resonancia estrecha, cristalina

7. l0.1 → LFO casi estático
8. l3.0 → LFO rápido — vibrato de filtro

9. u0.5 → Tune bajo — modos en registro grave
10. u2.0 → Tune alto — modos en registro brillante

11. m0.0 → solo dry (comparación sin FX)
12. m1.0 → solo wet (solo resonancias)
13. m0.5 → balance de trabajo

14. p → bypass
    → Comparación definitiva con/sin CymaticResonator
```

### A/B comparison

| Escenario | Sin FX | Con CymaticResonator |
|-----------|--------|----------------------|
| Chord C mayor, Q=80, D=4 | Timbre del engine solo | Timbre coloreado por 4 resonancias en 440/880/1320/2200 Hz |
| Lead mono, Tune=1.5 | Línea de lead directa | Lead con "cristal vibrando" — identidad tímbrica propia |

---

## Learnings

*(Se completa después de la implementación y medición en hardware real.
Secciones típicas: comportamiento del Q alto en frecuencias cercanas a fs/4,
artefactos de la actualización de coeficientes en bloques durante el LFO,
valores de Q audiblemente útiles vs los que producen aliasing en modos de alta
frecuencia, ajuste de BASE_FREQ post-escucha en hardware.)*

---

*Sprint 2.8 — GrooveForge Brain · Juan Guerrero (GPROG)*

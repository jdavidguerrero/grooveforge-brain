# Sprint 2.4 — FX Phase Chorus

**Status:** In Progress
**Refs:** `apps/docs/05-fx-architecture.md` §1.8, `apps/docs/01-architecture.md` §3.4

---

## Theory

### El problema fundamental: ¿por qué un chorus analógico suena "vivo"?

Antes de entrar en detalles técnicos, hay que establecer qué estamos imitando y por
qué es difícil hacerlo bien en digital.

Un chorus digital ingenuo funciona así: tomas la señal, la retrasas entre 3 y 20ms
con un LFO sinusoidal perfectamente regular, y mezclas con la señal seca. El
resultado es técnicamente correcto pero perceptualmente plano. Suena como un
plug-in de freeware de 1998.

El CE-2 de BOSS (1979) suena diferente. La razón no es misteriosa ni inaccesible —
es consecuencia directa de las imperfecciones físicas de sus componentes. Este
sprint documenta cuáles son esas imperfecciones, por qué el cerebro las percibe como
"vitalidad", y cómo aproximarlas en el dominio digital.

---

### BOSS CE-2 — física del chorus analógico con BBD

El BOSS CE-2 fue lanzado en 1979 y se convirtió en la referencia de chorus para
décadas. Sus usuarios incluyen a Kurt Cobain (en limpio), John Frusciante y casi
cualquier guitarrista de los 80s que quería ese sonido "ancho". El CE-2 también
aparece en el Juno-106 como el efecto de ensemble integrado del sintetizador.

El componente central es el **MN3007**, un chip Bucket Brigade Device (BBD)
fabricado por Panasonic/Matsushita. El BBD es una línea de delay analógica
construida con una cadena de condensadores que transfieren su carga de uno al
siguiente en cada ciclo de un reloj externo.

#### Bucket Brigade Device — cómo funciona la línea de delay analógica

El nombre viene de los equipos de bomberos del siglo XIX: filas de personas
pasándose cubetas de agua de mano en mano desde la fuente hasta el incendio. Cada
persona (condensador) recibe la cubeta (carga eléctrica) y la pasa al siguiente.

```
Entrada      C₁    C₂    C₃    C₄    ...    C₅₁₂    Salida
de audio  →  ●───→ ●───→ ●───→ ●───→ ───→  ●      →  audio
            [Q]   [Q]   [Q]   [Q]          [Q]        retardada
                                                       N ciclos
```

Cada condensador almacena la muestra de voltaje que recibió. En el próximo flanco
del reloj externo, toda la cadena se desplaza un paso: C₁ recibe la muestra nueva,
C₂ recibe lo que tenía C₁, C₃ lo que tenía C₂, y así hasta que la muestra emerge
del último condensador como señal retardada.

El MN3007 tiene **512 etapas** (condensadores). El tiempo de delay es:

```
t_delay = N_etapas / (2 × f_reloj)
```

El factor 2 es porque el BBD necesita dos fases de reloj (fase A y fase B) para
completar un paso de transferencia. Con N=512:

- `f_reloj = 25.6 kHz` → `t_delay = 512 / (2 × 25600) = 10 ms`
- `f_reloj = 128 kHz` → `t_delay = 512 / (2 × 128000) = 2 ms`

El LFO del CE-2 varía la frecuencia del reloj entre aproximadamente **12.8 kHz** y
**51.2 kHz**, produciendo tiempos de delay entre ~5 ms y ~20 ms.

**Por qué esto crea chorus y no solo delay:** cuando el tiempo de delay sube, la
señal delayed "corre" más lenta que la señal seca — se produce un descenso de pitch
percibido (como un Doppler: una fuente alejándose). Cuando el tiempo de delay baja,
la señal delayed "corre" más rápida — se produce un ascenso de pitch. La mezcla de
la señal seca (pitch fijo) con la señal delayed (pitch modulado) crea la ilusión de
dos instrumentos ligeramente desafinados tocando juntos — eso es el chorus.

Matemáticamente, si el delay varía como `t(τ) = t₀ + A·sin(2πfτ)`, la frecuencia
instantánea de la señal delayed es:

```
f_delayed(τ) = f_input × (1 - dt(τ)/dτ)
             = f_input × (1 - 2πfA·cos(2πfτ))
```

Para `f_input = 1 kHz`, `A = 5 ms = 0.005 s`, `f = 1 Hz (LFO)`:

```
Desviación de pitch = ±2π × 1 Hz × 0.005 s × 1000 Hz ≈ ±31.4 Hz
```

En cents: `±31.4/1000 × 1200 ≈ ±37 cents`. Perceptualmente, esto es una tercera
parte de un semitono de variación — muy audible pero no desafinado como para ser
molesto. El cerebro lo interpreta como "vibrato" de la señal delayed, que junto con
la señal seca estable crea el "ensanchamiento" del chorus.

**Referencia:** Zölzer, "DAFX: Digital Audio Effects" (2011), Chapter 2 "Delay
Effects", §2.6 "Chorus", pp. 58-65. Ecuación del pitch instantáneo de una línea de
delay modulada (eq. 2.31 y derivación). El MN3007 se menciona explícitamente como
caso de estudio en la edición de 2011.

**Referencia histórica:** BOSS CE-2 Service Notes (Roland Corporation, 1979) —
esquemático completo con valores de componentes del circuito VCO de reloj y el
LFO triangular. Disponible en varios repositorios de manuales vintage.

---

### Las imperfecciones del CE-2 que lo hacen sonar "vivo"

El CE-2 tiene tres fuentes de imperfección que el digital limpio no reproduce
de forma espontánea:

#### 1. El LFO triangular del CE-2 no es perfectamente lineal

El CE-2 usa un integrador de op-amp (TL072CP) para generar la onda triangular del
LFO. Un integrador ideal produce rampas perfectamente lineales. El TL072 real tiene:

- Corriente de polarización de entrada no nula que introduce una pequeña curvatura
- Corriente de offset que modifica ligeramente la pendiente de cada mitad del triángulo
- Slew rate limitado que redondea las esquinas del triángulo

El resultado es un triángulo con leves diferencias entre la rampa ascendente y
descendente, y esquinas suavizadas. La señal no es periódica perfecta en el sentido
matemático — tiene variaciones ciclo a ciclo de orden <1%, suficientes para que el
cerebro no reconozca la periodicidad exacta.

#### 2. El BBD introduce clock noise (aliasing de reloj)

El reloj del BBD es una señal cuadrada. Las transiciones abruptas del reloj se
acoplan capacitivamente a la señal de audio a través de las capacitancias parásitas
del substrato del chip. Esto introduce componentes de ruido en múltiplos de la
frecuencia del reloj — típicamente a 25-50 kHz (fuera del rango audible) pero con
intermodulación con el contenido de audio que puede producir artefactos sutiles en
el rango 8-16 kHz.

El CE-2 tiene filtros anti-aliasing (low-pass a ~6kHz) antes y después del BBD para
mitigar esto, pero los filtros introducen su propia coloración (rolloff de alta
frecuencia). El resultado neto es que el CE-2 tiene **bandwidth limitada** — el
efecto filtra ligeramente las altas frecuencias de la señal wet.

#### 3. Temperatura y tensión de alimentación afectan el LFO

La frecuencia del LFO del CE-2 depende de un capacitor y resistor de temporización.
Los valores de estos componentes tienen tolerancias del ±10-20% y sus propiedades
cambian ligeramente con la temperatura. Dos unidades CE-2 de la misma producción
pueden tener LFOs que difieren en frecuencia un 5-15%. Después de 45 años, la
deriva es mayor.

Esto significa que cada CE-2 físico tiene un carácter propio, ligeramente diferente
al de cualquier otra unidad. Lo que los usuarios llaman "mi CE-2 tiene algo especial"
es frecuentemente consecuencia de esta variación de manufactura.

**Conclusión para nuestra implementación:** reproducir exactamente el CE-2 es un
ejercicio de arqueología de componentes que no aportaría ventaja real. Lo que
debemos capturar es la **consecuencia perceptual** de estas imperfecciones: la
ausencia de periodicidad exacta en el LFO. El enfoque es el mismo que en Sprint 2.3
para el wow/flutter de cinta — un LFO compuesto que rompe la regularidad de forma
determinística.

---

### AudioEffectChorus — implementación en la Teensy Audio Library

La Teensy Audio Library provee `AudioEffectChorus` (archivo `effect_chorus.h` en la
librería de PaulStoffregen). La implementación interna usa una línea de delay en
memoria RAM con uno o varios "taps" (puntos de lectura) cuyas posiciones se mueven
según el LFO interno del objeto.

#### API y configuración del buffer

```cpp
// Buffer externo — el usuario lo declara en el sketch
short chorusBuf[CHORUS_DELAY_LENGTH];

AudioEffectChorus chorus;

// Inicialización — llamar en setup()
chorus.begin(chorusBuf, CHORUS_DELAY_LENGTH, nVoices);

// Cambio de voces en runtime
chorus.voices(n);  // n: 1-4
```

El buffer es de tipo `short` (int16_t, 16 bits) con signo. El tamaño mínimo está
dado por:

```
CHORUS_DELAY_LENGTH_min = AUDIO_BLOCK_SAMPLES × nVoices_max
                        = 128 × 4
                        = 512 samples
```

Sin embargo, el LFO interno necesita mover los taps de lectura dentro del buffer. Si
el buffer es exactamente del tamaño mínimo, el LFO tiene headroom cero y los taps
pueden "wrappear" el buffer produciendo artefactos. La práctica recomendada es
usar al menos el doble del mínimo. En este sprint usamos **1200 samples**:

```
1200 samples × 2 bytes/sample = 2400 bytes ≈ 2.3 KB de RAM
```

Este costo es pequeño comparado con el tensor arena de TinyML (200 KB) y los
buffers de delay de otros FX. Es aceptable.

**Por qué 1200 y no 1024 (potencia de 2):** la implementación de `AudioEffectChorus`
no usa operaciones de módulo que se beneficien de potencias de 2. El tamaño 1200
fue elegido para cubrir aproximadamente 25 ms de audio a 48 kHz (48000 × 0.025 =
1200), dando headroom suficiente para el LFO modulando el delay entre 3 y 20 ms.

#### Qué hace cada voz

`AudioEffectChorus` implementa N voces donde cada voz es un tap diferente del mismo
buffer de delay, con un LFO que varía su posición. Las voces comparten el mismo
buffer circular de escritura (la señal de entrada se escribe una vez) pero cada voz
tiene su propio puntero de lectura con su propio LFO.

La señal de salida del objeto es la suma de todas las voces (normalizadas por el
número de voces para mantener el nivel). Esta suma es la señal **wet**. En el signal
chain, mezclamos wet con dry en un `AudioMixer4` para el control Depth.

| Voces | Carácter sonoro |
|---|---|
| 1 | Vibrato sutil — casi solo pitch modulation, poco ensanchamiento |
| 2 | Chorus clásico CE-2 — warm, stereo-spread percibido en mono |
| 3 | Ensemble — más denso, evoca strings ensemble en sintetizadores |
| 4 | Lush — espeso, casi detune de múltiples osciladores |

**Nota de implementación:** con 4 voces, la suma de 4 taps produce una señal cuya
energía puede aproximarse a la del dry path (por suma coherente parcial), pero en
la práctica los taps están suficientemente desfasados como para que la suma sea
parcialmente incoherente. El nivel de salida del chorus con 4 voces puede ser 3-6 dB
más bajo que con 1 voz. La ganancia del canal wet en el mixer debe compensar esto si
se quiere volumen consistente al cambiar Voices.

**Referencia:** PaulStoffregen, "Teensy Audio Library — effect_chorus.h" (GitHub,
PJRC Audio Library). El source code del objeto muestra el cálculo de posición de
taps y el LFO interno. Disponible en: https://github.com/PaulStoffregen/Audio

---

### LFO drift caótico — rompiendo la periodicidad perfecta

Este es el núcleo técnico del sprint. La Teensy Audio Library tiene su propio LFO
interno en `AudioEffectChorus`, pero no expone control sobre él. La modulación
externa que implementamos actúa sobre el nivel del canal wet — no sobre el delay
directamente. Es una aproximación, pero produce el efecto deseado de "breathing".

#### Por qué un LFO sinusoidal puro suena robótico

Considera una senoide de 0.5 Hz. Su período exacto es 2 segundos. El cerebro humano
es extraordinariamente bueno detectando regularidad temporal en el audio — es la
base de la percepción del ritmo y la melodía. Una modulación perfectamente periódica
a 0.5 Hz se vuelve audiblemente "mecánica" después de 4-6 ciclos (8-12 segundos),
cuando el patrón se establece en la memoria de trabajo.

El CE-2 real nunca tiene esta regularidad perfecta por las razones descritas en la
sección anterior. El resultado es que el cerebro no puede "calcular" el patrón y se
rinde — lo percibe como una textura orgánica, no como una periodicidad artificial.

#### La solución: senoide compuesta con componentes inarmónicas

El enfoque es sumar varias senoides cuyas frecuencias no son múltiplos enteros entre
sí (inarmónicas, o más precisamente, irracionales entre sí):

```
drift_lfo(t) = sin(2π × rate × t)
             + drift × 0.4 × sin(2π × rate × 0.743 × t)
             + drift × 0.2 × sin(2π × rate × 1.329 × t)
```

Los factores `0.743` y `1.329` son cruciales. Son irracionales respecto a `1.0` —
no hay número entero n tal que `n × 0.743 = entero` y `m × 1.329 = entero` para
pequeños valores de n, m. Esto significa que las tres senoides **nunca se alinean
exactamente en fase** para ningún tiempo finito razonablemente escuchable.

Para verificar: el período del sistema completo sería el mínimo común múltiplo de
los tres períodos. Como los ratios son irracionales, ese MCM es técnicamente
infinito — la señal nunca se repite exactamente. En la práctica, el patrón tarda
decenas de minutos en aproximarse a su estado inicial. Para una actuación en vivo,
es efectivamente aperiódico.

La elección de 0.743 y 1.329 no es arbitraria en el sentido de sus propiedades
matemáticas:
- `0.743 ≈ φ - 1` donde φ = 1.618... (razón áurea). Las propiedades irracionales
  de φ garantizan mala aproximabilidad racional — es el número "más irracional"
  en el sentido de la teoría de números.
- `1.329` fue ajustado empiricamente para que la señal compuesta tenga carácter
  audible de "respiración" a rates típicos de chorus (0.3–2 Hz).

A `drift = 0.0`: solo el primer término — senoide pura al rate seleccionado.
A `drift = 1.0`: los tres términos activos — señal que parece irregular sin serlo.

**Por qué no usar ruido blanco o ruido rosado:** el ruido sería más "caótico" pero
perdería la sensación de movimiento organizado que define al chorus. El oído necesita
sentir que hay una dirección en el movimiento (lento, orgánico) pero que esa
dirección no es completamente predecible. La senoide compuesta da eso. El ruido
blanco da el equivalent de un trémolo aleatorio — diferente carácter.

**Referencia:** Zölzer, "DAFX" (2011), §2.6 "Chorus", pp. 60-62 — discusión de LFO
shapes y su impacto perceptual en el chorus. La edición de 2011 incluye análisis de
por qué los LFOs no-sinusoidales suenan más "naturales" (cita a Moorer, "About This
Reverberation Business", CCRMA 1979).

---

### Saturación pre-chorus — calidez antes del BBD

En el CE-2, la señal de audio pasa por un buffer de op-amp (TL072) antes de entrar
al MN3007. El TL072 en configuración de seguidor de tensión introduce una saturación
de clase A muy leve: las señales de alta amplitud experimentan una compresión suave
de ~0.3-0.5 dB por parte del op-amp operando cerca de sus límites de corriente de
salida.

Esta saturación pre-chorus tiene dos efectos:
1. Redondea los transientes antes de que entren al BBD (que no maneja bien los
   picos afilados — puede producir artefactos por capacitancias insuficientes en las
   etapas más rápidas de transferencia).
2. Colorea el audio con armónicos de tercer orden muy sutiles (~0.1-0.3% THD) que
   el oído interpreta como "calidez".

En nuestra implementación, esto se modela con un `AudioEffectWaveshaper` usando una
tabla tanh con `drive = 1.5`. Este valor es prácticamente lineal (la pendiente en el
origen es 1.0 y solo se desvía ligeramente para amplitudes cercanas a 1.0), pero
introduce la coloración de tercer armónico deseada:

```
Para x = 0.7 (amplitud típica de audio con headroom):
  tanh(1.5 × 0.7) / tanh(1.5) = tanh(1.05) / 0.9051
                                 = 0.7818 / 0.9051
                                 = 0.864
```

La desviación de linealidad es `(0.7 - 0.864 × 0.7/0.7) ≈ ...`. Más directamente:
sin pre-sat, un tono de 1 kHz a amplitud 0.7 sale a amplitud 0.7. Con drive=1.5,
sale a `0.864` normalizado, que es `0.864 × 0.7 = 0.605` — compresión de ~1.3 dB.
El tercer armónico generado tiene amplitud proporcional a `x³` — a x=0.7, el tercer
armónico es `0.7³ ≈ 0.34` veces el nivel de la fundamental (antes de la
normalización). Con drive=1.5 suave, el armónico efectivo es mucho más pequeño,
del orden del 1-3% — suficiente para dar color, insuficiente para sonar distorsionado.

La tabla del waveshaper pre-chorus es estática (drive fijo en 1.5, no expuesto al
usuario). Se genera una vez en `setup()` y no se modifica. Esto simplifica la
implementación respecto al Tape Saturate donde el drive es un parámetro.

**Referencia:** BOSS CE-2 Service Notes (Roland, 1979), schematic page 2 — op-amp
buffer stage con TL072CP en configuración unity-gain antes del MN3007.
Smith, "Introduction to Digital Filters with Audio Applications" (W3K Publishing,
2007), Chapter 8 "Nonlinear Filters" — análisis de generación de armónicos por
soft-clipping con funciones tipo tanh.

---

### Cómo el LFO drift se aplica al chorus: modulación de nivel wet

`AudioEffectChorus` no expone acceso directo a su LFO interno. Esto es una
limitación de la API de la Teensy Audio Library — el LFO del chorus está
encapsulado dentro del objeto.

La estrategia de este sprint es modular el **nivel del canal wet** en el mixer
`dryWetMix` usando el `drift_lfo`:

```
wet_gain(t) = depth × (1.0 + drift_amount × drift_lfo(t))
```

donde:
- `depth` ∈ [0.0, 1.0]: profundidad base del efecto (controlada por ENC R en FX mode)
- `drift_amount` ∈ [0.0, 1.0]: cuánto varia la ganancia wet con el LFO
- `drift_lfo(t)` ∈ [-1.0, 1.0] aproximadamente: la señal del LFO caótico

Esto produce una modulación de amplitud en la señal wet — el efecto chorus sube y
baja de nivel rítmicamente pero de forma no-periódica. El cerebro interpreta esto
como el "breathing" del chorus analógico: momentos donde el efecto es más presente,
seguidos de momentos donde retrocede levemente.

**¿Es esto igual que modular el delay directamente?** No exactamente. En el CE-2
real, lo que varía es el tiempo de delay (y por lo tanto el pitch instantáneo de la
señal wet). En nuestra implementación, lo que varía es el nivel de la señal wet
(su amplitud, no su pitch). El resultado perceptual es diferente pero complementario:
la variación de nivel produce un efecto de trémolo sobre el chorus que da la
sensación de "movimiento" sin ser exactamente el mismo mecanismo.

Para v1.0 esto es una aproximación aceptable. Una implementación más fiel requeriría
un `AudioEffectChorus` con LFO externo controlable, lo que implicaría modificar el
código fuente de la Teensy Audio Library — posible pero fuera del scope de este
sprint.

**Referencia:** Zölzer, "DAFX" (2011), §2.6.2 "Implementation", pp. 63-64 — describe
la modulación de tiempo de delay como el mecanismo correcto. La modulación de amplitud
se menciona como aproximación válida cuando no hay control de LFO externo.

---

### Arquitectura del signal flow

```
3× AudioSynthWaveform (C4, E4, G4 — sawtooth)
            ↓
      AudioMixer4 srcMix
            ↓
   [AudioEffectWaveshaper]     pre-sat tanh, drive=1.5 fijo
            ↓
   [AudioEffectChorus]         1-4 voces, buffer 1200 samples
   short chorusBuf[1200]       2.3 KB RAM estático
            ↓
      AudioMixer4 dryWetMix
       ch0: dry (desde srcMix, antes del pre-sat)
       ch1: wet (desde chorus)
            ↓
   AudioOutputI2S + AudioControlSGTL5000
```

El dry path se toma **antes** del pre-sat waveshaper. Esto preserva la señal seca
sin coloración, de modo que `depth=0` (solo dry) reproduce el sonido original del
engine sin ninguna modificación. El waveshaper y el chorus solo afectan a la señal
wet.

El `dryWetMix` tiene su ganancia en ch1 (wet) actualizada cada ciclo de `loop()`
por el cálculo del `drift_lfo`. La ganancia en ch0 (dry) se mantiene en
`1.0 - depth` para que la suma dry+wet preserve el nivel total percibido
aproximadamente constante (aunque no exactamente, porque el chorus introduce
desfase).

#### Grafo de objetos AudioConnection

```cpp
// Conexiones declaradas en el orden del signal flow:
AudioConnection p1(osc_C, 0, srcMix, 0);
AudioConnection p2(osc_E, 0, srcMix, 1);
AudioConnection p3(osc_G, 0, srcMix, 2);

// pre-sat: srcMix → waveshaper
AudioConnection p4(srcMix, 0, preSat, 0);

// chorus: waveshaper → chorus
AudioConnection p5(preSat, 0, chorus, 0);

// dry/wet mixer:
//   ch0: dry (srcMix directo — señal limpia)
//   ch1: wet (chorus output)
AudioConnection p6(srcMix, 0, dryWetMix, 0);
AudioConnection p7(chorus, 0, dryWetMix, 1);

// salida
AudioConnection p8(dryWetMix, 0, i2sOut, 0);
AudioConnection p9(dryWetMix, 0, i2sOut, 1);
```

---

### Voces (1–4) — análisis de stacking

Cada voz adicional en `AudioEffectChorus` agrega un tap de delay con un desfase de
LFO diferente. La relación entre el número de voces y el caracter del sonido se
puede entender desde la perspectiva de beatings entre osciladores.

Con N voces, hay `N(N-1)/2` pares de señales interactuando. Cada par produce un
beating a la frecuencia diferencia de sus LFOs:

```
N=1: 0 pares  — vibrato solo (no hay señal de referencia wet)
N=2: 1 par    — beating a Δf₁₂ (el beating clásico del CE-2)
N=3: 3 pares  — beatings múltiples, densificación del movimiento
N=4: 6 pares  — beating muy denso, quasi-ensemble
```

En la práctica, los N LFOs dentro de `AudioEffectChorus` no están uniformemente
distribuidos en fase — la implementación específica de PJRC distribuye los taps en
posiciones fijas del buffer. El resultado perceptual es:

| Voces | Analogía histórica |
|---|---|
| 1 | Vibrato de Juno-60 (solo un oscilador con pitch modulation) |
| 2 | BOSS CE-2 estándar (el sonido "original" del efecto) |
| 3 | Roland Dimension D en Small mode (ensemble de strings emulado) |
| 4 | Roland Dimension D en Large mode (espacialidad máxima) |

**Referencia:** Pirkle, "Designing Software Synthesizer Plug-Ins in C++" (Focal Press,
2014), Chapter 15 "Modulation Effects", §15.3 "Chorus Effects" — análisis del stacking
de voces y la relación con el carácter del sonido. Pirkle usa N=2 como punto de
partida para el "sound" y N=4 como el límite práctico antes de que el efecto suene
"sintético" por acumulación de fase.

---

### CPU y memoria estimados

#### Bloques AudioMemory

```
Objetos nuevos respecto al engine standalone (3 OSCs + mixer):
  preSat (waveshaper):   1 bloque de salida
  chorus:                1 bloque de salida
  dryWetMix:             1 bloque de salida
  Total adicional: 3 bloques sobre los ~5 del engine de 3 OSCs

Total estimado: ~8 bloques activos
AudioMemory(20) → 20 / 8 ≈ 2.5× headroom — suficiente para este sketch
```

El buffer `chorusBuf[1200]` es RAM estática declarada globalmente — no usa el pool
de AudioMemory. Son 2400 bytes de RAM estática, bien dentro del budget de 1MB.

#### CPU estimado

| Objeto | CPU estimado | Justificación |
|---|---|---|
| `AudioEffectWaveshaper` (pre-sat) | ~1.5% | Lookup table 257 puntos, interpolación lineal |
| `AudioEffectChorus` (4 voces, peor caso) | ~3.0% | Estimado del spec §1.8 total ~5% |
| `AudioMixer4` (dry/wet) | ~0.2% | Suma ponderada trivial |
| LFO drift en loop() | negligible | Actualización a rate <10 Hz, sin ISR |
| **Total Phase Chorus** | **~4.7%** | Vs. spec §1.8: ~5% — consistente |

```
CPU total estimado (engine 3 OSCs + Phase Chorus):
  3× AudioSynthWaveform + mixer: ~1.5%  (medido en sprints anteriores)
  AudioEffectChorus (4 voces):   ~3.0%
  AudioEffectWaveshaper (pre-sat): ~1.5%
  Total:                          ~6.0% CPU

Budget specs (05-fx-architecture.md §2): engines + 5 FX worst case ≤60%
6.0% << 60% — headroom muy amplio
```

**Referencia:** Teensy Audio Library documentation, CPU benchmarks —
https://www.pjrc.com/teensy/td_libs_AudioProcessorUsage.html
PaulStoffregen publica mediciones en Teensy 4.1 a 48kHz/128 samples/block. El
`AudioEffectChorus` no figura con número explícito en la tabla pública; el estimado
de ~3% para 4 voces se basa en el análisis del código fuente (4 interpolaciones de
delay + 4 sumas por sample) y la regla general de ~0.5-0.8% por voz para efectos
de delay simple.

---

## Wiring

Sin cambios de hardware respecto a Sprint 2.3. El efecto es completamente digital:

- Mismo Teensy 4.1 + Audio Shield Rev D2 (SGTL5000)
- Salida por jack 3.5mm del Audio Shield (auriculares o monitores con nivel de línea)
- No se requiere ningún componente adicional
- No hay cambios en pin mapping respecto a `01-architecture.md §3.3`
- Sin filter discreto 2N3904 involucrado en este sprint (Sprint 1.4 deferred)

---

## Implementation Notes

### Inicialización del chorus — orden obligatorio

`AudioEffectChorus` requiere que `begin()` se llame **después** de `AudioMemory()`:

```cpp
void setup() {
    AudioMemory(20);                              // primero siempre
    sgtl5000.enable();
    sgtl5000.volume(0.5);

    // Tabla pre-sat (estática, generada una vez)
    for (int i = 0; i < 257; i++) {
        float x = (float)i / 128.0f - 1.0f;
        preSatTable[i] = tanhf(1.5f * x) / tanhf(1.5f);
    }
    preSat.shape(preSatTable, 257);

    // Chorus — begin() DESPUÉS de AudioMemory
    chorus.begin(chorusBuf, CHORUS_DELAY_LENGTH, CHORUS_VOICES_DEFAULT);

    // Osciladores: C4=261.63Hz, E4=329.63Hz, G4=392.00Hz
    osc_C.begin(WAVEFORM_SAWTOOTH);
    osc_C.frequency(261.63f);
    osc_C.amplitude(0.4f);   // 0.4 para headroom antes del mixer
    osc_E.begin(WAVEFORM_SAWTOOTH);
    osc_E.frequency(329.63f);
    osc_E.amplitude(0.4f);
    osc_G.begin(WAVEFORM_SAWTOOTH);
    osc_G.frequency(392.00f);
    osc_G.amplitude(0.4f);
}
```

Si `begin()` se llama antes de `AudioMemory()`, el objeto inicializa sus punteros
internos con el pool de memoria no inicializado, produciendo comportamiento
indefinido — típicamente un bloqueo o ruido aleatorio en la salida.

### LFO drift en loop() — implementación completa

```cpp
// Variables de estado del LFO (globales o static en loop())
static float lfo_phase_1  = 0.0f;
static float lfo_phase_2  = 0.0f;
static float lfo_phase_3  = 0.0f;
static uint32_t last_us   = 0;

void update_lfo() {
    uint32_t now = micros();
    float dt = (now - last_us) * 1e-6f;  // segundos desde último update
    last_us = now;

    // Avance de fase — fmodf para evitar acumulación de error float
    lfo_phase_1 = fmodf(lfo_phase_1 + rate_hz * dt, 1.0f);
    lfo_phase_2 = fmodf(lfo_phase_2 + rate_hz * 0.743f * dt, 1.0f);
    lfo_phase_3 = fmodf(lfo_phase_3 + rate_hz * 1.329f * dt, 1.0f);

    // Señal compuesta (drift=0 → solo lfo_1; drift=1 → tres componentes)
    float lfo_signal = sinf(2.0f * M_PI * lfo_phase_1)
                     + drift * 0.4f * sinf(2.0f * M_PI * lfo_phase_2)
                     + drift * 0.2f * sinf(2.0f * M_PI * lfo_phase_3);

    // Normalización aproximada: a drift=1, amplitud pico ≈ 1.0 + 0.4 + 0.2 = 1.6
    // Dividimos por 1.6 para mantener rango ≈ [-1.0, 1.0]
    lfo_signal /= (1.0f + drift * 0.6f);

    // Ganancia wet: depth × (1 + drift_amount × lfo_signal)
    // drift_amount controla cuánto modula el LFO el nivel wet
    float wet_gain = depth * (1.0f + drift * 0.3f * lfo_signal);
    wet_gain = constrain(wet_gain, 0.0f, 1.0f);  // safety clamp

    dryWetMix.gain(0, 1.0f - depth);   // dry: constante
    dryWetMix.gain(1, wet_gain);        // wet: modulado por LFO
}
```

El factor `0.3f` en `drift * 0.3f * lfo_signal` limita la profundidad de modulación
del nivel wet a ±30% del depth base. Con `depth=0.7` y `drift=1.0`, el wet gain
varía entre `0.7 × (1 - 0.3) = 0.49` y `0.7 × (1 + 0.3) = 0.91`. Este rango produce
el "breathing" buscado sin que el efecto desaparezca o sature.

### Cambio de número de voces en runtime

`AudioEffectChorus` permite cambiar voces en runtime via `chorus.voices(n)`. Sin
embargo, el cambio puede producir un click audible si hay audio activo, porque el
objeto reorganiza sus punteros de tap internamente. Para evitar el click:

```cpp
void set_voices(int n) {
    // Fade out wet antes de cambiar voces
    dryWetMix.gain(1, 0.0f);
    delay(10);                    // 10ms para que el fade sea audible
    chorus.voices(n);
    // El fade-in lo retoma el próximo ciclo de update_lfo()
    current_voices = n;
}
```

Esta solución es simple y efectiva para un sketch de demo con Serial commands. En
producción (con UI de encoders), el cambio ocurriría entre frases musicales o con
un mecanismo de rampa más fino.

### Patrón de clase PhaseChorus (para integración futura)

Cuando este FX se integre al sistema completo (Sprint 2.x→3.x), seguirá el mismo
patrón de clase que TapeSaturate:

```
PhaseChorus
  ├── begin(AudioStream& input, float volume)  — crea AudioConnections dinámicas
  ├── update(float dt_ms)                      — avanza LFO, actualiza gains
  ├── setRate(float hz)                        — 0.1–10.0 Hz
  ├── setDepth(float d)                        — 0.0–1.0 (mapea a ENC R en FX mode)
  ├── setDrift(float d)                        — 0.0–1.0
  ├── setVoices(int n)                         — 1–4, con fade anti-click
  ├── setMix(float m)                          — wet level adicional
  └── setBypass(bool b)                        — bypass clean sin clicks
```

El mapeado de ENC R (FX mode) a `Depth` está definido en Panel v5 Final del spec
de UI (aplicación ESP32-S3 LVGL, Sprint 3.x).

---

## Demo

**Sketch:** `apps/firmware-teensy/src/sketches/09-phase-chorus.cpp`

### Setup al arrancar

El sketch inicia con el acorde C mayor (C4+E4+G4, sawtooth, igual que Sprint 2.3)
y Phase Chorus activo con valores que demuestran el carácter "Juno-106" de entrada:

```
Rate    = 0.5 Hz   (lento, movimiento de pad)
Depth   = 0.7      (chorus claramente audible)
Drift   = 0.3      (algo de respiración, no perfecto)
Voices  = 2        (CE-2 estándar, 1 par de taps)
Mix     = 0.7      (wet nivel base)
Bypass  = false    (efecto activo)
```

### Comandos Serial (115200 baud)

| Comando | Parámetro | Rango | Ejemplo |
|---|---|---|---|
| `r<valor>` | Rate Hz | 0.1–10.0 | `r0.5` |
| `d<valor>` | Depth | 0.0–1.0 | `d0.7` |
| `t<valor>` | Drift (turbulence) | 0.0–1.0 | `t0.3` |
| `v<n>` | Voices | 1–4 | `v2` |
| `m<valor>` | Mix wet | 0.0–1.0 | `m0.7` |
| `b` | Bypass toggle | — | `b` |

El comando para Drift usa `t` (turbulence) para no colisionar con `d` (Depth).

### Reporte Serial cada 2 segundos

```
CPU: 6.1% | Mem: 8 bloques | Rate: 0.50Hz | Depth: 0.70 | Drift: 0.30 | Voices: 2
```

### Secuencia de demo sugerida

La secuencia está diseñada para demostrar progresivamente el espacio de parámetros
y comparar con referencias conocidas:

```
1. Escuchar defaults (r0.5, d0.7, t0.3, v2) — "Juno-106 chord"
   Referencia: esto debe sonar como el efecto de chorus del Roland Juno-106

2. bypass 'b' → comparar con señal seca
   La diferencia de "ensanchamiento" percibido debe ser clara

3. 'b' → volver al chorus activo

4. v4 → cuatro voces — "Roland Dimension D"
   Más denso, casi ensemble. Comparar con v2.

5. v1 → una voz — vibrato sutil
   El ensanchamiento desaparece, queda solo pitch modulation

6. v2 → volver a dos voces

7. r5.0 → LFO rápido
   A 5 Hz, el chorus se convierte en quasi-vibrato rápido (flutter-chorus)

8. r0.1 → LFO muy lento
   Movimiento casi imperceptible — útil para pads de larga duración

9. r0.5 → volver a velocidad nominal

10. t1.0 → Drift máximo
    Comparar con t0.0 — la diferencia de "organicidad" debe ser perceptible
    especialmente en exposición larga (>10 segundos)

11. t0.0 → LFO perfecto — efecto robótico
    En looping de ~20 segundos, la regularidad se vuelve obvia

12. t0.3 → volver a valor nominal
    Demostración de valor de trabajo real

13. d1.0 t1.0 r0.8 v4 → máximo de todo
    Exagerado pero útil para demostrar los extremos del efecto
```

### Evidencia requerida para validar el hito

1. **Grabación de audio** (30 segundos mínimo): acorde C mayor con bypass off/on
   comparado. La forma de onda en Audacity debe mostrar modulación de amplitud leve
   en la señal wet. El espectro debe mostrar ensanchamiento de los picos espectrales
   (las notas del acorde aparecen ligeramente más anchas en frecuencia cuando el
   chorus está activo).

2. **Comparación de Drift**: grabación de t0.0 (LFO puro) vs t1.0 (drift máximo)
   durante 20 segundos cada uno. La diferencia perceptual de "mecánico vs orgánico"
   debe ser audible en el playback.

3. **Screenshot Serial Monitor**: CPU% < 10% con 4 voces activas. Verificar que
   `AudioMemory` no está en máximo (no debe superar 15 de 20 bloques).

4. **Comparación de Voices**: grabación de v1, v2, v3, v4 con el mismo acorde
   durante 5 segundos cada uno. Cada transición debe ser distinguible en carácter.

### Cómo reproducirlo

```bash
cd apps/firmware-teensy
pio run -e sketch --target upload

# En Serial Monitor (115200 baud):
# El acorde C mayor arranca automáticamente con Phase Chorus activo.
# Probar la secuencia de demo completa o comandos individuales.

# Para comparar con Tape Saturate (Sprint 2.3):
# Flashear 08-tape-saturate.cpp y comparar el carácter sonoro
```

---

## Learnings

(Pendiente — completar después del demo en hardware Teensy 4.1 + Audio Shield Rev D2)

Aspectos a medir y reportar:

- CPU real vs. estimado (~6% estimado, ¿qué reporta el hardware con 4 voces?)
- Audibilidad real del drift caótico: ¿el factor 0.3 en la modulación de nivel wet
  es suficiente para dar "respiración" o hay que aumentarlo?
- Click audible al cambiar Voices en runtime: ¿el fade de 10ms es suficiente para
  eliminarlo o se necesita una rampa más larga?
- Consistencia del nivel al cambiar Voices (1→4): ¿hay diferencia de nivel
  perceptible que requiera compensación de ganancia por número de voces?
- Comportamiento de `AudioEffectChorus.begin()` con buffer de 1200 samples: ¿hay
  artifacts audibles con LFO interno en los extremos del buffer?
- Perceptibilidad real de la diferencia t0.0 vs t1.0 en el acorde C mayor
  con sawtooth — puede que con señales más complejas la diferencia sea más o menos
  notable
- Rate máximo usable antes de que el chorus suene como vibrato/trémolo en lugar de
  chorus (¿es el límite 3 Hz? ¿5 Hz? — depende del contenido de audio)
- Interacción con Tape Saturate (Sprint 2.3): si se encadenan Phase Chorus → Tape
  Saturate, ¿hay problemas de nivel o intermodulación?

---

*Sprint 2.4 — GrooveForge Brain · Juan Guerrero (GPROG)*

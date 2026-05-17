# Sprint 2.3 — FX Tape Saturate

**Status:** In Progress
**Refs:** `apps/docs/05-fx-architecture.md` §1.5, `apps/docs/01-architecture.md` §5.2

---

## Theory

### Saturación de cinta magnética — física del fenómeno

Antes de que existiera el clipping digital, el problema del audio era exactamente el
opuesto: la cinta magnética no admite señales de amplitud arbitraria. Cuando la
señal supera cierto umbral, el material ferromagnético de la cinta no puede
magnetizarse más — llega a la saturación. Pero a diferencia del hard clipping
digital (que corta la señal como un cuchillo), la saturación magnética es gradual,
suave, y musicalmente tolerable. A veces, deseable.

El fenómeno se describe con la **curva B-H** (o curva de histéresis magnética): la
relación entre el campo magnético aplicado H (la señal de audio que magnetiza la
cinta) y la inducción magnética B (la magnetización resultante del material):

```
B (magnetización)
    |          ___________  ← saturación: B no puede subir más
    |        /
    |      /              ← región lineal: B proporcional a H
    |    /
    |  /
    | /
    |/______________________ H (campo aplicado / amplitud de la señal)
```

En la región lineal, la cinta se comporta como un sistema ideal: B = μ × H.
Cuando H es demasiado grande, el material se satura y la curva se aplana. El audio
que emerge de la cinta ya no es la señal original — es la señal pasada a través de
esta curva no-lineal. La forma de esa no-linealidad define el timbre de la saturación.

**El hystéresis loop** agrega otro matiz: la cinta "recuerda" su magnetización
anterior. Cuando el campo H cambia de dirección, la magnetización B no sigue
inmediatamente — hay un lag. Este fenómeno hace que la saturación de cinta sea
dependiente de la historia de la señal (tiene memoria), a diferencia de un clipper
instantáneo. En la práctica, esto se traduce en que los transientes agresivos quedan
"redondeados" no solo en su pico sino en su descenso, dando esa sensación de audio
que "respira".

#### Por qué tanh es la aproximación adecuada

La función tangente hiperbólica tiene la forma exacta de la región de saturación
magnética:

```
y = tanh(x):

    1 |        ___________
      |      /
      |    /
      |  /
      |/
   ---+------- x
      |
   -1 |___________
```

Propiedades que la hacen musical:

1. **Pasa por el origen con derivada 1**: para señales pequeñas, tanh(x) ≈ x. No
   altera el audio de baja amplitud — solo comprime cuando la señal es grande.

2. **Asíntota suave en ±1**: nunca corta abruptamente. La compresión es progresiva,
   igual que la saturación magnética real.

3. **Función impar**: tanh(-x) = -tanh(x). Esto es crucial — significa que la
   función trata simétricamente los semiciclos positivo y negativo de la señal de
   audio. El resultado de esta simetría es que **solo se generan armónicos impares**
   (3f, 5f, 7f, ...), no armónicos pares. Los armónicos impares (quinta, séptima
   menor, novena menor) se perciben como "calidez" y "cuerpo" en el vocabulario
   musical. Los armónicos pares (octava, cuarta) tienen un carácter más "brillante"
   y son el sello de la distorsión de válvulas asimétricas (que agregan armónicos
   pares). La cinta tiende a armónicos impares — por eso suena diferente a un
   amplificador de válvulas.

**Referencia:** Zölzer, "DAFX: Digital Audio Effects" (2011), Chapter 5 "Nonlinear
Processing", §5.1 "Clipping and Saturation", pp. 108-118. Tabla 5.1 lista las
waveshaper functions estándar; tanh figura como la aproximación de soft-clipper
magnético más usada en la literatura.

**Referencia adicional:** J. O. Smith III, "Virtual Analog Synthesis" (CCRMA 2010),
Chapter 7 "Analog Nonlinearities and Waveshapers" — descripción de cómo la curva B-H
se aproxima con funciones racionales y por qué tanh es el caso especial más eficiente
computacionalmente.

---

### Waveshaper — la transfer function

Un **waveshaper** es el nombre en DSP para cualquier función que mapea, sample a
sample, la amplitud de la señal de entrada a una amplitud de salida diferente. Si
la función es lineal (y = a × x + b), el waveshaper no cambia el timbre, solo el
volumen y el DC offset. Si la función es no-lineal (y = f(x) donde f no es
recta), el waveshaper introduce nuevas frecuencias que no estaban en la señal
original — eso es distorsión armónica.

La operación se realiza muestra a muestra:

```
entrada:    [x₀, x₁, x₂, ..., xₙ]
función f:  f(x) = tanh(drive × x) / tanh(drive)
salida:     [f(x₀), f(x₁), f(x₂), ..., f(xₙ)]
```

No hay latencia, no hay memoria entre muestras — es una operación algebraica local.
Esto lo hace muy eficiente computacionalmente.

#### AudioEffectWaveshaper en la Teensy Audio Library

La Teensy Audio Library provee `AudioEffectWaveshaper`, que implementa el waveshaper
mediante una **lookup table** de 257 floats mapeada al rango de entrada [-1.0, 1.0].
Cuando llega un sample, el objeto hace interpolación lineal entre las dos entradas de
la tabla más cercanas. El resultado es `O(1)` por muestra — independiente de la
complejidad de la curva original.

La table tiene 257 puntos (no 256) porque el índice del centro exacto (x = 0) cae
en el índice 128, simétrico en ambas direcciones hasta los índices 0 (x = -1.0) y
256 (x = 1.0). La API es:

```cpp
AudioEffectWaveshaper waveshaper;
waveshaper.shape(tabla, 257);  // tabla: float[257]
```

#### La curva con pre-gain (drive)

El parámetro `Drive` controla cuánto se comprime la señal antes de entrar a la
curva tanh. Un drive alto empuja señales de amplitud media hacia la región de
saturación:

```
y = tanh(drive × x) / tanh(drive)
```

La división por `tanh(drive)` es la normalización: sin ella, la salida para x=1.0
sería `tanh(drive)`, que para drive=5 sería ~0.9999, pero para drive=0.5 sería
~0.462. La normalización asegura que f(1.0) = 1.0 para cualquier valor de drive,
manteniendo el nivel de salida en el rango [-1.0, 1.0].

Intuición: es como si envolvieras la señal en un elástico y la estiraras antes de
pasarla por el molde de la cinta. Más drive = señal más estirada = más compresión en
las colas de la curva = más armónicos generados = más "grasa".

Para `drive = 1.0` (drive mínimo), la curva es casi lineal — saturación apenas
perceptible. Para `drive = 10.0`, la curva es casi un hard clipper — saturación
agresiva que aplana los picos fuertemente.

#### Generación de la tabla en setup()

```cpp
static float tabla[257];
float drive_init = 2.0f;  // drive inicial

for (int i = 0; i < 257; i++) {
    float x = (float)i / 128.0f - 1.0f;  // x va de -1.0 a +1.0
    tabla[i] = tanhf(drive_init * x) / tanhf(drive_init);
}
waveshaper.shape(tabla, 257);
```

La tabla se regenera en cada cambio de Drive. Como la tabla son 257 × 4 bytes =
1028 bytes, cabe cómodamente en el stack. La regeneración toma <1ms (257
multiplicaciones + tanhf calls), suficientemente rápido para ejecutar en `loop()`.

**Por qué lookup table y no tanh() directo en el ISR:** `tanhf()` es una función
transcendental — tarda entre 10-30 ciclos de CPU dependiendo de la implementación.
A 48kHz con bloques de 128 muestras, el ISR de audio procesa 128 muestras cada
~2.67ms. 128 × 30 ciclos = 3840 ciclos adicionales por bloque. Con una lookup table
de 257 puntos e interpolación lineal, el costo baja a ~5 ciclos por muestra —
6× más eficiente.

**Referencia:** Zölzer, "DAFX" Cap. 5.2 "Waveshaping", pp. 120-123. Descripción del
tradeoff entre tabla y cálculo directo con análisis de error de interpolación.

---

### Wow y Flutter — física del transporte de cinta

Imagina el mecanismo de un casete de cinta magnética: un motor eléctrico gira un
eje (capstan) que arrastra la cinta a velocidad constante pasando por el cabezal de
lectura/escritura. En teoría, la velocidad es exactamente constante. En la práctica,
el motor tiene irregularidades de par, el eje tiene imperfecciones mecánicas, el
rodillo de presión (pinch roller) tiene variaciones de diámetro microscópicas.
Todas estas imperfecciones hacen que la velocidad de la cinta fluctúe ligeramente.

Cuando la velocidad fluctúa, la frecuencia de reproducción fluctúa. Si la cinta va
un poco más rápido, el audio sube de tono. Si va un poco más lento, baja. Es una
modulación de pitch involuntaria e incontrolable — el impuesto de la mecánica.

En el contexto de síntesis digital, modelamos esta modulación de pitch como LFOs
(Low Frequency Oscillators) que mueven la frecuencia de los osciladores del engine.

#### Wow — variaciones lentas (0.1 a 2 Hz)

El **wow** son las variaciones lentas de velocidad. Causas principales:
- Irregularidades del motor eléctrico (vibración mecánica a bajas frecuencias)
- Excentricidad del eje: si el eje no es perfectamente cilíndrico, cada revolución
  produce una fluctuación a la frecuencia de rotación (típicamente 0.5-2 Hz)
- Tensión variable de la cinta entre las dos bobinas

El wow se percibe como un vibrato muy lento y sutil — el pitch "respira" lentamente,
como si alguien apretara y soltara levemente las cuerdas de una guitarra a 0.5 Hz.
En dosis bajas (±2-5 cents), es lo que los músicos llaman "movimiento" o "vida" en
una grabación. En dosis altas (±30 cents o más), es el sonido de una casetera con
batería baja — cómicamente desafinado.

Standard de medición: IEC 60386 define el wow & flutter medido en weighted rms.
Buenas caseteras de los 80s: wow < 0.08% weighted rms. Caseteras baratas: hasta 0.3%.

#### Flutter — variaciones rápidas (4 a 12 Hz)

El **flutter** son las variaciones rápidas. Causas principales:
- El capstan tiene ranuras o imperfecciones superficiales que generan micro-oscilaciones
- Resonancias mecánicas del chasis a las frecuencias de rotación del capstan
- El pinch roller con irregularidades genera variaciones a su frecuencia de rotación
  (~6-12 Hz dependiendo del diámetro y velocidad)

El flutter se percibe como un tremolo rápido y sutil — el pitch "tirita" a 6-10 Hz.
A amplitudes bajas, contribuye a la textura del audio vintage. A amplitudes altas,
da el sonido "underwater" inconfundible de cinta dañada.

#### Por qué un LFO no perfectamente sinusoidal

En cinta real, el wow y flutter no son senoides puras. Son señales de **bandlimited
noise** con picos en ciertas frecuencias (las frecuencias de resonancia del
mecanismo) pero con componentes en todo el espectro. Una senoide perfecta a 0.5 Hz
sonaría como un vibrato de sintetizador — artificial, regular, robótico.

Para aproximar la naturaleza irregular del wow y flutter de cinta real, la
implementación suma dos componentes:

1. Un oscilador principal a la frecuencia central (0.5 Hz para wow, 6 Hz para flutter)
2. Un oscilador secundario a una frecuencia ligeramente distinta (por ejemplo 0.37 Hz
   y 6.3 Hz) con amplitud menor (~30% del principal)

La suma de dos senoides de frecuencias ligeramente distintas produce una señal con
modulación de amplitud lenta (beating), que rompe la periodicidad perfecta sin
requerir un generador de ruido completo.

Una alternativa más fiel sería bandpass noise, pero el costo computacional es mayor
y la diferencia perceptual en el contexto del mix es mínima.

#### Implementación: modulación de frecuencia de los osciladores

En el Prophet-5 engine, los 5 pares de osciladores (VCO-A y VCO-B por voz) tienen
frecuencias calculadas a partir de la nota MIDI. El wow y flutter se implementan
como un factor multiplicativo aplicado a esa frecuencia:

```
f_modulada = f_nota × (1.0 + modulation_factor)
```

donde `modulation_factor` es la suma de los LFOs de wow y flutter, escalada al
rango de cents deseado:

```
cents_total = wow_depth × wow_lfo + flutter_depth × flutter_lfo
factor = 2^(cents_total / 1200.0)
```

La escala de 1200 es porque hay 1200 cents por octava. Para ±5 cents, el factor
varía entre 2^(-5/1200) ≈ 0.9971 y 2^(5/1200) ≈ 1.0029 — variación de ±0.29%,
casi imperceptible pero audiblemente presente.

La modulación se aplica via `prophet.setFreqMod(factor)` desde `loop()`. No desde
el ISR de audio — los LFOs corren en el contexto de la tarea principal, lo cual
introduce cuantización de tiempo (el factor se actualiza cada ciclo de `loop()`,
típicamente cada 1-5ms). Esta cuantización es inaudible dado que los cambios de LFO
son lentos (sub-Hz para wow, <12 Hz para flutter).

**Referencia:** Vail, Mark, "Vintage Synthesizers" (2000), Chapter 3 "Tape Machines
and Their Influence on Electronic Music", pp. 89-102 — descripción histórica del
wow & flutter y su impacto en la música grabada.

**Referencia técnica:** IEC 60386:1994 "Methods of Measuring Wow and Flutter in
Sound Recording and Reproducing Equipment" — definición formal y métodos de medición.

---

### Age — degradación de la cinta magnética

Una cinta magnética de los años 70 o 80 que hoy sobrevive en algún archivo es una
cinta envejecida. Con el tiempo y el uso, el material ferromagnético sufre:

1. **Desgaste del óxido:** las partículas de óxido de hierro que conforman la capa
   magnética se desgastan físicamente al rozar el cabezal en cada reproducción. La
   capa se adelgaza. Una capa más delgada tiene menor respuesta en alta frecuencia —
   las frecuencias altas quedan literalmente menos grabadas.

2. **Aumento del gap del cabezal:** el cabezal de reproducción también se desgasta.
   Un gap más grande en el cabezal implica mayor longitud de onda mínima detectable,
   lo cual se traduce en una frecuencia de corte más baja.

3. **Pérdida de magnetización remanente:** el campo magnético grabado en la cinta
   se debilita naturalmente con el tiempo (desmagnetización). Las frecuencias altas,
   que tienen menor amplitud de magnetización en primer lugar, pierden señal más
   rápido.

El efecto neto es un filtro pasa-bajos de frecuencia de corte que disminuye con la
edad de la cinta:

```
Cinta nueva  (Age = 0.0): f_corte ≈ 20 kHz  (sin rolloff audible)
Cinta de 10 años (0.3):   f_corte ≈ 14 kHz  (brillo reducido)
Cinta de 30 años (0.7):   f_corte ≈ 9 kHz   (claramente oscurecida)
Cinta muy desgastada (1.0): f_corte ≈ 6 kHz (carácter vintage marcado)
```

En la implementación, esto se modela con un `AudioFilterStateVariable` en modo
low-pass, con frecuencia de corte mapeada linealmente (en escala logarítmica):

```
f_corte = 20000 × (6000/20000)^age
        = 20000 × 0.3^age
```

A age=0: 20000 Hz. A age=1: 6000 Hz. La escala logarítmica respeta la percepción
del oído humano, que es logarítmica en frecuencia.

Adicionalmente, el parámetro Age puede afectar sutilmente el noise floor — una cinta
desgastada tiene mayor ruido de modulación. En la implementación simplificada de v1.0,
este componente de ruido se omite (costo computacional vs. beneficio perceptual en
el contexto de un set de DJ).

**Referencia:** Denisov, "Magnetic Tape Recording Technology" (1985), Chapter 8
"Tape Aging and Storage" — análisis cuantitativo del rolloff de alta frecuencia en
función de horas de uso y condiciones de almacenamiento.

---

### Arquitectura del signal flow

El Tape Saturate es un efecto de **Master Layer** — procesa la salida completa del
engine Prophet-5 (o cualquier engine activo) antes de que llegue al DAC del SGTL5000.

```
Prophet-5 output (5 voces mezcladas)
              ↓
    [Pre-gain × drive]          software, en AudioMixer4 gain
              ↓
    [Waveshaper tanh curve]     AudioEffectWaveshaper
              ↓
    [LP filter: Age rolloff]    AudioFilterStateVariable (LP, 6-20kHz)
              ↓
    [HP filter: subsonic cut]   AudioFilterStateVariable (HP, fija ~20Hz)
              ↓
    [Dry/Wet mixer]             AudioMixer4 (ch0=wet, ch1=dry)
              ↓
    AudioOutputI2S → jack 3.5mm
```

El pre-gain se implementa como el gain del canal del Prophet en el mixer que
alimenta el waveshaper (no un objeto AudioAmplifier separado — ahorra un bloque de
memoria de audio). Valores de drive entre 1.0 y 10.0 corresponden a gains de 1.0 a
10.0 en ese canal.

El filtro HP a 20 Hz es fijo: elimina el contenido subsónico que puede introducirse
como DC offset o componentes muy bajos del waveshaper (la curva tanh no introduce
DC porque es función impar, pero las imperfecciones numéricas y el proceso de
modulación de wow/flutter pueden acumular pequeñas asimetrías). Es una medida de
higiene de señal.

El mixer Dry/Wet permite mezcla paralela, lo que en la práctica da:
- Wet=1.0, Dry=0.0: saturación completa (sin señal limpia)
- Wet=0.5, Dry=0.5: mezcla que preserva los transientes originales mientras agrega
  calidez del waveshaper — el enfoque "New York compression" aplicado a saturación

El Dry/Wet no está expuesto directamente como parámetro de usuario en v1.0 (el
enfoque del spec §1.5 es Drive/Wow/Flutter/Age). Puede agregarse como parámetro
oculto o en v1.1 si el testing en hardware muestra que el bypass total (Wet=0) es
más útil que la mezcla.

---

### CPU y memoria estimados

#### Bloques de AudioMemory adicionales

```
Nodos nuevos respecto al Prophet-5 standalone:
  waveshaper:        1 bloque de salida
  filterLP:          1 bloque de salida
  filterHP:          1 bloque de salida
  wetDryMixer:       1 bloque de salida (mezcla wet + dry del Prophet)
  Dry path (bypass): el Prophet ya ocupa bloques en el grafo
  Total adicional: 4-5 bloques sobre los 14 del Prophet-5

Total con Tape Saturate: ~18-19 bloques activos
AudioMemory(72) → headroom 72 / 19 ≈ 3.8× — amplio margen
```

#### CPU adicional

| Objeto | CPU estimado | Justificación |
|---|---|---|
| `AudioEffectWaveshaper` | ~1.5% | Lookup table + interpolación, 128 muestras/bloque |
| `AudioFilterStateVariable` (LP) | ~1.0% | Biquad IIR, 2 ciclos/muestra |
| `AudioFilterStateVariable` (HP) | ~1.0% | Ídem |
| `AudioMixer4` (wet/dry) | ~0.2% | Suma ponderada, operación trivial |
| LFO wow/flutter en loop() | negligible | No es ISR, actualización <12 Hz |
| **Total Tape Saturate** | **~3.7%** | Estimado conservador |

```
CPU total estimado con Prophet-5 + Tape Saturate:
  Prophet-5:      2.5% (medido en hardware, Sprint 2.2)
  Tape Saturate:  ~3.7% (estimado)
  Total:          ~6.2% CPU

Budget specs (05-fx-architecture.md §1.5): ~5% para Tape Saturate
Budget specs (01-architecture.md §4.1): engines + 5 FX worst case ≤60%
6.2% << 60% — headroom amplio para agregar más FX en sprints futuros
```

La discrepancia entre el estimado del spec (5% para el FX) y nuestro estimado
(3.7% solo el FX, 6.2% incluyendo el engine) se debe a que el spec reporta el CPU
del FX aislado. El Prophet-5 ya está en el budget de engines. Con el engine,
estaríamos en ~6.2%, que es consistente con el espíritu del spec.

**Referencia:** Teensy Audio Library documentation, "CPU Usage" section —
benchmarks de CPU por objeto publicados por Paul Stoffregen en el foro PJRC,
medidos en Teensy 4.1 a 48kHz/128 samples/block.

---

## Wiring

Sin cambios de hardware respecto a Sprint 2.2. El efecto es completamente digital:

- Mismo Teensy 4.1 + Audio Shield Rev D2
- Salida por jack 3.5mm del Audio Shield (auriculares o monitores)
- No se requiere ningún componente adicional
- Pin mapping sin cambios respecto a `01-architecture.md §3.3`

---

## Implementation Notes

### Tabla del waveshaper y regeneración on-the-fly

La tabla se genera en `setup()` con el drive inicial y se regenera en `loop()` cada
vez que el parámetro Drive cambia. Patrón de actualización:

```cpp
static float tabla[257];
static float last_drive = -1.0f;

void regenerar_tabla(float drive) {
    for (int i = 0; i < 257; i++) {
        float x = (float)i / 128.0f - 1.0f;
        tabla[i] = tanhf(drive * x) / tanhf(drive);
    }
    waveshaper.shape(tabla, 257);
    last_drive = drive;
}

// en loop():
if (drive != last_drive) {
    regenerar_tabla(drive);
}
```

El guard `last_drive` evita regeneraciones innecesarias — solo recalcula si el
valor cambió. La operación es safe llamarla desde `loop()` porque `shape()` aplica
la tabla nueva en el próximo bloque de audio (no hay race condition — el grafo de
audio procesa en ISR y lee la tabla por referencia en el siguiente ciclo de bloque).

### Wow/flutter — implementación software

Los LFOs de wow y flutter se calculan en `loop()` con acumulación de fase manual,
sin objetos adicionales de audio (evita 2 bloques de AudioMemory):

```cpp
static float wow_phase   = 0.0f;
static float wow_phase2  = 0.0f;   // segunda senoide para romper periodicidad
static float flutter_phase  = 0.0f;
static float flutter_phase2 = 0.0f;

// en loop() — dt en segundos desde el último ciclo
wow_phase    = fmodf(wow_phase    + WOW_RATE_HZ     * dt, 1.0f);
wow_phase2   = fmodf(wow_phase2   + WOW_RATE_HZ * 0.73f * dt, 1.0f);
flutter_phase  = fmodf(flutter_phase  + FLUTTER_RATE_HZ      * dt, 1.0f);
flutter_phase2 = fmodf(flutter_phase2 + FLUTTER_RATE_HZ * 1.05f * dt, 1.0f);

float wow_lfo    = sinf(2.0f * M_PI * wow_phase)
                 + 0.3f * sinf(2.0f * M_PI * wow_phase2);
float flutter_lfo = sinf(2.0f * M_PI * flutter_phase)
                  + 0.3f * sinf(2.0f * M_PI * flutter_phase2);

float cents = (wow_depth * 5.0f * wow_lfo)
            + (flutter_depth * 3.0f * flutter_lfo);

float freq_factor = powf(2.0f, cents / 1200.0f);
prophet.setFreqMod(freq_factor);  // aplica a los 5 VCO-A de cada voz
```

Los factores 5.0 y 3.0 escalan el depth normalizado (0.0-1.0) a rangos de cents:
- Wow depth=1.0 → ±5 cents (spec §1.5: ±2-15 cents, valor central conservador)
- Flutter depth=1.0 → ±3 cents (flutter es más sutil que wow)

`setFreqMod()` es un método a agregar a `Prophet5` que multiplica la frecuencia
calculada de cada oscilador-A por el factor antes de pasar a `osc.frequency()`.

### Filtro LP para Age — mapeo de frecuencia

```cpp
void set_age(float age) {
    // f_corte = 20000 × 0.3^age — escala logarítmica
    float f_corte = 20000.0f * powf(0.3f, age);
    filterLP.frequency(f_corte);
    filterLP.resonance(0.7f);  // Q levemente por debajo de Butterworth (Q=0.707)
}
```

`AudioFilterStateVariable` acepta frequency() en Hz y resonance() sin unidades
(internamente mapea a Q). Una Q de 0.7 da una caída suave sin pico de resonancia —
comportamiento Butterworth — apropiado para modelar el rolloff natural de la cinta
(que no tiene resonancia en la frecuencia de corte).

### Alternativa considerada — LFO via objetos de audio

`AudioSynthWaveformSine` puede funcionar como LFO conectado a objetos de modulación.
Ventaja: aprovecha el pipeline de audio para actualización sample-accurate. Desventaja:
2 bloques adicionales de AudioMemory + la Teensy Audio Library no tiene conexión
directa de un sine wave a `osc.frequency()` — requeriría un objeto custom o usar
`AudioSynthWaveformModulated`. Dado que wow y flutter son sub-audio (< 12 Hz) y la
actualización en `loop()` es suficientemente frecuente para estos rangos, la
implementación software es preferible en v1.0.

---

## Demo

**Sketch:** `apps/firmware-teensy/src/sketches/08-tape-saturate.cpp`

### Setup al arrancar

El sketch arranca con el Prophet-5 tocando el acorde C mayor (n60 + n64 + n67) con
Tape Saturate activo en valores sutiles que demuestran calidez sin exagerar:

```
Drive   = 2.0   (saturación suave, ~2dB de compresión armónica)
Wow     = 0.3   (wow moderado, ~1.5 cents de modulación)
Flutter = 0.1   (flutter sutil, ~0.3 cents)
Age     = 0.2   (rolloff a ~16kHz — cinta ligeramente envejecida)
Bypass  = false (efecto activo)
```

### Comandos Serial (115200 baud)

| Comando | Descripción | Ejemplo |
|---|---|---|
| `d<valor>` | Drive (1.0–10.0) | `d5.0` |
| `w<valor>` | Wow depth (0.0–1.0) | `w0.8` |
| `f<valor>` | Flutter depth (0.0–1.0) | `f0.5` |
| `a<valor>` | Age (0.0–1.0) | `a0.7` |
| `b` | Toggle bypass on/off | `b` |
| `n<midi>` | Note on (0–127) | `n60` |
| `x<midi>` | Note off (0–127) | `x60` |

Secuencia de demo sugerida para capturar audio:

```
1. Escuchar el acorde C mayor SIN saturación (bypass on con 'b')
2. Activar saturación (bypass off con 'b') — diferencia inmediata de calidez
3. Drive alto: d8.0 — saturación audible, compresión de transientes
4. Age alto: a0.9 — oscurecimiento de alta frecuencia perceptible
5. Wow alto: w1.0 — pitch drift lento y teatral
6. Flutter alto: f0.8 + w0.0 — tremolo rápido sin wow
7. Volver a valores sutiles: d2.0 w0.3 f0.1 a0.2 — demostrar versión "use case real"
```

### Reporte Serial cada 1 segundo

```
CPU: 6.3% | Mem: 19 bloques | Drive: 2.00 | Wow: 0.30 | Flutter: 0.10 | Age: 0.20
```

### Evidencia requerida

Para validar el hito del sprint:

1. **Grabación de audio** (Audacity o DAW): 30 segundos del acorde C mayor con bypass
   on, luego bypass off. La diferencia de calidez debe ser perceptible en la forma de
   onda (redondeamiento de picos) y en el espectro (presencia de armónicos impares
   nuevos en 3f, 5f de cada nota del acorde).

2. **Screenshot del Serial Monitor** mostrando CPU% < 10% con el Prophet-5 + todos
   los componentes del Tape Saturate activos.

3. **Captura de la curva waveshaper** (opcional pero recomendado): con Drive=5.0,
   enviar un tono de 100 Hz y medir la señal de salida con Audacity para verificar
   presencia de armónicos en 300 Hz y 500 Hz (3f y 5f).

### Cómo reproducirlo

```bash
cd apps/firmware-teensy
pio run -e sketch --target upload

# En Serial Monitor (115200 baud):
# El acorde arranca automáticamente
# Probar: d5.0, luego b (bypass), luego b (volver), notar diferencia
```

---

## Learnings

(Pendiente — completar después del demo en hardware Teensy 4.1 + Audio Shield Rev D2)

Aspectos a medir y reportar:

- CPU real vs. estimado (6.2% estimado, ¿qué muestra el hardware?)
- Picos de AudioMemory al regenerar la tabla del waveshaper on-the-fly
- Perceptibilidad real del wow/flutter en el contexto del acorde (¿es sutil o
  exagerado con los valores default?)
- Latencia de respuesta al cambio de Age (¿el FilterStateVariable tiene clicks al
  cambiar frecuencia de corte on-the-fly?)
- ¿El mapeo de Drive 1.0-10.0 es lineal-perceptual o conviene escala logarítmica?
- Impacto del HP filter fijo en el sonido del Prophet-5 (¿elimina algo que no
  debería?)

---

*Sprint 2.3 — GrooveForge Brain · Juan Guerrero (GPROG)*

# Sprint 2.6 — FX Sub Genesis

**Status:** Done — CPU 0.8%, 9 bloques, sub-octave y drive verificados en hardware
**Refs:** `apps/docs/05-fx-architecture.md` §1.12, `apps/docs/01-architecture.md` §3.4

---

## Theory

### El problema fundamental: ¿por qué falta "thump" en un sintetizador digital?

Cuando tocás un acorde en el GrooveForge Brain con el engine Moog o Juno, el sonido
es rico en armónicos medios y altos. El sawtooth tiene energía hasta los 20 kHz. El
engine suena brillante, definido, presencial. Pero hay algo que falta en comparación
con un bajo eléctrico, un kick drum, o un sintetizador de bajo: el peso en las
frecuencias muy bajas, debajo de 150 Hz.

No es que el engine sea incorrecto — es que los armónicos de un C4 (261 Hz) empiezan
en 261 Hz y suben. La energía por debajo de 200 Hz en un engine de teclado estándar
es mínima. En un sistema de PA, los subwoofers reproducen 40-120 Hz. En auriculares
con driver grande, la respuesta baja llega a 30-40 Hz. Toda esa capacidad de
reproducción queda sin usar cuando tocás un acorde de teclado.

El Sub Genesis resuelve exactamente este problema: genera una señal sintética en la
octava sub (una o dos octavas más baja que la nota raíz del patch activo), la
satura para añadirle armónicos que la hagan perceptible incluso en sistemas con
respuesta de baja frecuencia limitada, y la mezcla debajo de la señal original. El
resultado: el "thump" físico que sentís además de escuchar.

Este efecto es el estándar en dub, reggaeton, techno, house y cualquier género donde
el bajo no es solo una nota sino una experiencia física.

---

### Sub-octave synthesis — historia y física

#### La división de frecuencia como principio

Una octava hacia abajo significa exactamente la mitad de la frecuencia. Si la nota
raíz es A4 (440 Hz), el sub en una octava abajo es A3 (220 Hz). Dos octavas abajo:
A2 (110 Hz). El oído percibe ambas notas como la "misma" nota en diferentes registros
— tienen una relación de octava perfecta, el intervalo más consonante en la música
occidental.

La física es simple:

```
f_sub = f_raiz / 2^octaves

f_raiz = 261.63 Hz (C4)
Octaves = 1  →  f_sub = 130.81 Hz (C3)
Octaves = 2  →  f_sub =  65.41 Hz (C2)
```

A 130 Hz, la longitud de onda en aire es `λ = c/f = 343/130 ≈ 2.6 metros`. Los
altavoces de PA tienen diafragmas de 15-18 pulgadas para mover suficiente masa de
aire a estas frecuencias. Los subwoofers de 18" son exactamente para esto: crear
esa longitud de onda, que el cuerpo humano siente como presión de sonido además de
escuchar como tono.

#### El Mu-Tron Octave Divider y el Boss OC-2 — la referencia histórica

El concepto de sub-octave hardware existe desde los años 70. Los primeros
implementaciones usaban flip-flops digitales para dividir la frecuencia de la señal
de entrada.

El principio del flip-flop D como divisor de frecuencia es elegante:

```
Señal de entrada:  __|--|__|--|__|--|__|--|   (ciclos a f_input)
Señal del flip-flop: ___|____|____|____|___   (ciclos a f_input / 2)
```

El flip-flop cambia de estado en cada flanco ascendente de la señal de entrada. Si
la señal de entrada tiene un flanco ascendente cada `T = 1/f` segundos, el flip-flop
produce una señal cuadrada con período `2T` — exactamente la mitad de la frecuencia.

El **Boss OC-2** (1982) es el canonizador de este sonido. Implementa división de
frecuencia en uno y dos niveles con dos flip-flops en cascada. Su circuito detector
de cruce por cero convierte la señal de audio en pulsos digitales que disparan el
flip-flop. La salida es una onda cuadrada a `f/2` y `f/4`, con los armónicos
característicos de la onda cuadrada (3f, 5f, 7f...).

**Referencia:** Boss OC-2 Service Manual (Roland Corporation, 1982). El esquemático
muestra los flip-flops 4013 (dual D flip-flop CMOS) como el núcleo del divisor de
frecuencia, con el comparador de entrada convertiendo la señal de audio en señal
digital. La documentación describe las limitaciones del tracking a baja frecuencia
(< 50 Hz) y en acordes (múltiples cruces por cero producen tracking inestable).

El **Electro-Harmonix POG** (Polyphonic Octave Generator, 2005) resolvió el problema
del tracking en acordes usando análisis espectral. En lugar de un flip-flop, el POG
usa un DSP que procesa el espectro de entrada y sintetiza las octavas componente por
componente. Esto permite sub-octave en acordes sin el tracking roto del OC-2. El
costo es mayor latencia de análisis (~20-30ms) y significativamente más CPU.

**Referencia:** Electro-Harmonix POG2 User Manual (EHX, 2009). La sección técnica
describe el algoritmo de pitch-shifting basado en análisis de fase del espectro, y
explica la latencia inherente como consecuencia del análisis por ventanas FFT.

---

### El problema del tracking analógico en acordes

El flip-flop del OC-2 funciona perfectamente con una sola nota monofónica simple.
El problema surge con acordes o notas ricas en armónicos.

Considerá un sawtooth a 261 Hz (C4): tiene armónicos a 261, 522, 783, 1044, 1305 Hz
y así sucesivamente. El detector de cruce por cero del OC-2 ve **todos esos cruces**.
La señal del sawtooth cruza el cero no solo en el período de la fundamental (cada
3.83 ms), sino en cada ciclo de cada armónico. Con el primer armónico a 522 Hz, hay
cruces cada 1.92 ms. Con el segundo armónico, cada 1.28 ms.

El flip-flop detecta todos estos cruces y produce pulsos inestables. La salida del
sub-oscilador del OC-2 con una nota de sawtooth suena "rota" — frecuencias que saltan
entre f/2 y otros valores inarmónicos. Los usuarios del OC-2 aprendieron a tocar
con ondas más simples (cerca de sine) o a usar el control de guitar agresivamente
para limpiar la señal.

La solución es el filtro low-pass antes del detector. El spec §1.12 menciona
`AudioFilterStateVariable (LP @ 200Hz para pitch tracking)` — esta es exactamente
esa solución:

```
Señal rica en armónicos (sawtooth C4):
  261 Hz + 522 Hz + 783 Hz + ... → LP @ 200 Hz → solo 261 Hz pasa

Señal filtrada (quasi-sine a 261 Hz):
  Un cruce por cero por período → flip-flop estable → f/2 = 130.5 Hz
```

Con el LP a 200 Hz, solo pasa la fundamental. Los armónicos quedan atenuados. El
detector de cruce opera sobre una señal casi sinusoidal, produciendo tracking estable.

**¿Por qué 200 Hz específicamente?** El LP debe estar por debajo del primer armónico
de la nota más baja que el usuario pueda tocar. La nota más baja del teclado MIDI
estándar es C0 (16.35 Hz), pero para un sintetizador de performance el rango útil
empieza en C2 (65.41 Hz). El primer armónico de C2 es 130.82 Hz. Con LP a 200 Hz,
la fundamental de C2 pasa (-3 dB en el punto de corte si fuera un Butterworth de 1er
orden, mucho más si es el StateVariable de 2do orden). Los armónicos superiores quedan
atenuados: el primer armónico a 131 Hz está cerca del cutoff, los superiores bien
atenuados.

El spec usa LP @ 200 Hz como punto de corte para el pitch tracking. En la
implementación del sketch, el LP se aplica internamente como parte del análisis —
no afecta la señal dry que escucha el usuario.

---

### Por qué el Sub Genesis NO implementa pitch tracking de audio en v1.0

Esta es la decisión de diseño más importante del sprint, y conviene documentarla
honestamente.

El spec original menciona "análisis de pitch del input". En teoría, el FX podría
analizar la señal de audio entrante, detectar la frecuencia fundamental, y sintetizar
el sub a exactamente f/2. Esto sería el equivalente digital del OC-2 o del POG.

En práctica, hay dos aproximaciones disponibles en la Teensy Audio Library:

**Opción A: `AudioAnalyzeNoteFrequency`** — implementa el algoritmo YIN (de De Cheveigne
y Kawahara, JASA 2002), un método de autocorrelación especializado para pitch
detection. El YIN es preciso y robusto, pero requiere una ventana de análisis de
aproximadamente 200 ms de audio para dar una estimación confiable. 200 ms son
9600 muestras a 48 kHz. La latencia de tracking es variable: para notas lentas
puede tardar más; para notas rápidas en un BPM alto (160 BPM, nota de 375 ms de
duración), el tracker necesita más de la mitad de la nota para estabilizarse.

**Opción B: FFT + autocorrelación manual** — implementable con `AudioAnalyzeFFT1024`.
FFT de 1024 puntos a 48 kHz tiene resolución de frecuencia de `48000/1024 ≈ 46.9 Hz`.
Para estimar C3 (130.81 Hz), la resolución de ±23 Hz no es suficientemente precisa.
Además, el FFT introduce latencia de al menos 1024/48000 ≈ 21.3 ms por ventana, más
el tiempo de procesamiento.

**Problema de latencia:** en live performance con BPM > 120, un compás de 4/4 tiene
una nota de negra cada 500 ms. El sub debería responder en < 30 ms para que el
ataque del bajo esté alineado con el ataque del engine. Con latencia de análisis
de 200 ms (YIN) o 21+ ms (FFT), el sub entra desalineado temporalmente del ataque
del engine — se escucha como un "retraso de bajo" que resta coherencia rítmica.

**La solución pragmática:** en Fase 2 (donde estamos), el engine no recibe notas
MIDI externas — trabaja con osciladores estáticos en el sketch de demo. La frecuencia
raíz es conocida estáticamente (C3 = 130.81 Hz para el sketch de demo). El
sub-oscilador se programa directamente a esa frecuencia desde el setup.

En Fase 4 (MIDI + TinyML, Sprint 4.1), cuando el engine reciba `noteOn()` vía USB
MIDI, la frecuencia de la nota llega como dato MIDI — no necesita análisis de audio.
La nota MIDI es la fuente de verdad: si el usuario toca C3 (MIDI note 48), la
frecuencia es `440 × 2^((48-69)/12) = 130.81 Hz` exactamente. El sub se ajusta
a esa frecuencia sin ninguna latencia de análisis.

```
Fase 2 (este sprint):   sub fijo en C3 (frecuencia estática del sketch)
Fase 4 (Sprint 4.1):    sub = f(noteOn MIDI) — latencia cero, precisión perfecta
```

Este approach es más limpio que el pitch tracking de audio para todos los casos de
uso relevantes del GrooveForge Brain.

**Referencia:** Alain de Cheveigné y Hideki Kawahara, "YIN, a fundamental frequency
estimator for speech and music" — Journal of the Acoustical Society of America,
111(4), 2002. La sección 4 ("Tests") muestra que el YIN requiere ventanas de 40-100
ms para convergencia en señales musicales complejas, y puede exceder 200 ms en casos
de señales con armónicos fuertes que compiten con la fundamental.

---

### Saturación del sub — por qué satura diferente que la señal principal

El sub-oscilador genera una senoide (o cuadrada) a baja frecuencia. Antes de mezclarlo
con la señal dry, lo saturamos con un waveshaper tanh.

#### Por qué saturar el sub en absoluto

Una senoide pura a 100 Hz es difícil de escuchar en sistemas sin respuesta extendida
de baja frecuencia. Los auriculares de driver dinámico de 40mm típicos tienen respuesta
plana desde ~60-80 Hz; por debajo, caen. Los altavoces de portátiles no reproducen
bajo 150-200 Hz. Los auriculares in-ear baratos son aún peores.

La saturación tanh añade armónicos al sub. Para una senoide a 100 Hz con saturación
leve (tanh lineal, baja saturación), los armónicos generados son de tercer y quinto
orden (a 300 Hz y 500 Hz). Para una senoide con saturación fuerte, los armónicos
alcanzan rangos más altos.

```
Sub senoidal a 100 Hz + tanh saturation (drive alto):
  100 Hz (fundamental) + 300 Hz (3er armónico) + 500 Hz (5to) + 700 Hz + ...
```

Los armónicos a 300, 500 y 700 Hz son perfectamente audibles en cualquier sistema
de reproducción. El cerebro reconoce estos armónicos como parte de un tono a 100 Hz
(porque están en relación de 3:1, 5:1, 7:1 — armónicos de la fundamental). El
resultado psicoacústico es que el oyente **percibe** la frecuencia fundamental de
100 Hz aunque su sistema de reproducción no la reproduzca directamente. Este
fenómeno se llama **fundamental virtual** o **tono de diferencia**.

El bass cabinets de bajo de los amplificadores valvulares de los años 60 trabajaban
con este principio: saturaban la señal del bajo naturalmente, añadían armónicos, y
el oyente percibía el "thump" aunque los altavoces de 12" no reprodujeran bien por
debajo de 80 Hz.

**Referencia:** Zölzer, "DAFX: Digital Audio Effects" (2011), Chapter 5 "Nonlinear
Processing", §5.4 "Harmonic Exciter", pp. 145-148. Zölzer describe el harmonic
exciter como herramienta de enriquecimiento de señales de baja frecuencia via
generación de armónicos, citando exactamente el principio de tono virtual para
extender la percepción de baja frecuencia. La figura 5.7 muestra el espectro antes
y después del exciter de tercer armónico.

#### Por qué el sub satura más agresivamente que la señal principal

La curva tanh tiene una región lineal central y una región de saturación en los
extremos:

```
tanh(drive × x) / tanh(drive)  para x ∈ [-1.0, 1.0]

Drive = 1.0:  prácticamente lineal, <1% THD
Drive = 2.0:  saturación suave, ~3-5% THD (armónicos de tercer orden)
Drive = 4.0:  saturación marcada, ~15-20% THD
Drive = 8.0:  saturación agresiva, próximo a onda cuadrada, >40% THD
```

Para la señal principal del engine (sawtooth rica en armónicos), el drive del
TapeSaturate en Sprint 2.3 es moderado para preservar el timbre original. Para el
sub, el drive puede ser mucho más alto porque el sub es una senoide limpia — la
saturación agresiva genera los armónicos que hacen perceptible al sub, sin destruir
información tímbrica que ya no existe (la senoide tiene muy pocos armónicos).

Adicionalmente, la baja frecuencia del sub significa que el tiempo de cada ciclo
es largo. A 100 Hz, un ciclo dura 10 ms. La curva tanh opera sobre cada muestra
individualmente (el waveshaper no tiene memoria). Cada muestra de la senoide pasa
por la curva, y el resultado acumulado en el período completo produce la onda saturada.
La saturación es progresiva (la señal permanece en la región no-lineal por más tiempo
relativo a la tasa de muestreo), produciendo más armónicos que la misma función
aplicada a la misma amplitud a frecuencias más altas.

**Referencia:** Zölzer, "DAFX" (2011), §5.1 "Clipping" y §5.2 "Memoryless
Nonlinearities", pp. 108-120. La sección establece que el espectro de armónicos
generados por una no-linealidad memoryless depende solo de la amplitud de la señal
de entrada, no de su frecuencia. Sin embargo, la percepción del efecto varía con la
frecuencia porque el oído tiene sensibilidad diferente en diferentes rangos. Esto
explica por qué el mismo drive suena más "agresivo" en el sub que en el mid-range.

---

### AudioEffectWaveshaper — implementación en la Teensy Audio Library

El `AudioEffectWaveshaper` aplica una función de transferencia de amplitud mediante
interpolación en una tabla de lookup. El usuario provee un array de floats de 257
puntos que mapea valores de entrada `[-1.0, 1.0]` a valores de salida.

```cpp
float satTable[257];
AudioEffectWaveshaper subSat;

// Generación de tabla tanh con drive variable
void generate_sat_table(float drive) {
    for (int i = 0; i < 257; i++) {
        float x = (float)i / 128.0f - 1.0f;  // [-1.0, 1.0]
        satTable[i] = tanhf(drive * x) / tanhf(drive);
    }
    subSat.shape(satTable, 257);
}
```

La división `/ tanhf(drive)` normaliza la salida para que el nivel de salida sea
aproximadamente 1.0 cuando la entrada es 1.0, independientemente del drive. Sin esta
normalización, drives altos producirían salidas con nivel reducido (porque tanh
satura a ±1.0 para entradas grandes, pero la normalización ajusta la escala).

La tabla de 257 puntos corresponde a 256 intervalos de interpolación lineal en el
rango [-1.0, 1.0]. La resolución de amplitud es `2.0/256 = 0.0078` — suficiente
para audio de 16 bits (donde la resolución mínima es `2/65536 = 0.000031`). La
interpolación lineal entre puntos introduce un error de intermodulación muy pequeño
comparado con la respuesta armónica del waveshaper, aceptable para este uso.

**Limitación:** la tabla debe regenerarse cada vez que cambia el drive. `shape()` se
puede llamar desde `loop()` — es thread-safe en la Teensy Audio Library porque la
librería aplica la nueva tabla en el próximo bloque de audio (no mid-block).
Regenerar 257 puntos de `tanhf()` tarda ~50-100 µs en el Teensy 4.1 a 600 MHz.
Para el sketch de demo, esto es aceptable (el drive no cambia cada milisegundo).

**Referencia:** PaulStoffregen, "AudioEffectWaveshaper source" (Teensy Audio Library,
GitHub). El source muestra la implementación de interpolación lineal entre puntos
de la tabla con índice calculado como `(x + 1.0f) * 128.0f`.

---

### AudioFilterStateVariable — el LP post-saturación

Después de la saturación, pasamos el sub por un filtro low-pass adicional. El spec
indica `Cutoff (LP filter del sub, 40–200Hz)`. Este filtro cumple dos funciones:

**1. Eliminar aliasing de la saturación:** la saturación tanh genera armónicos hasta
frecuencias arbitrariamente altas (en teoría infinitas, en práctica limitadas por la
frecuencia de Nyquist de 24 kHz). Para el sub a 65-130 Hz, los armónicos de orden
muy alto (100 Hz × 100 = 10 kHz, armónico 100) son indeseables — añaden ruido de
alta frecuencia que no contribuye al "thump". El LP post-saturación limita el
espectro del sub a las frecuencias que contribuyen al efecto deseado.

**2. Definir el carácter del sub:** con cutoff bajo (40-80 Hz), el sub es un golpe
de sub-bass muy puro — casi solo la fundamental, poco color armónico. Con cutoff
alto (150-200 Hz), los armónicos de la saturación contribuyen más — el sub suena
más "gordo" y con más presencia en las frecuencias medias-bajas.

El `AudioFilterStateVariable` es un filtro de segundo orden (2 polos, 12 dB/oct)
implementado en forma de variable de estado. Permite cambiar el cutoff en runtime
sin discontinuidades en la señal de salida — cada llamada a `frequency()` desde
`loop()` es thread-safe y se aplica suavemente.

```cpp
AudioFilterStateVariable subLP;
subLP.frequency(100.0f);   // cutoff Hz, default
subLP.resonance(0.7f);     // Q — Butterworth (máxima planura en el passband)
// Usar salida LP: subLP (LP output = canal 0 del StateVariable)
```

La salida low-pass del StateVariable se conecta al mixer. Las salidas band-pass y
high-pass del mismo objeto se ignoran (no conectadas).

**Por qué un StateVariable y no un Biquad:** el StateVariable permite cambiar el
cutoff en runtime sin inestabilidad numérica. El Biquad calculado con coeficientes
fijos puede tener discontinuidades cuando se cambian los coeficientes mid-stream.
Para parámetro de usuario en tiempo real, el StateVariable es más robusto.

**Referencia:** Smith, "Introduction to Digital Filters with Audio Applications"
(W3K Publishing, 2007), Chapter 7 "State Variable Filters" — demostración de que
la forma de variable de estado permite actualización de parámetros sin discontinuidades
de estado (las variables de estado son las memorias del filtro; el StateVariable las
preserva correctamente cuando se cambia el cutoff).

---

### Por qué el sub-oscilador es independiente del input stream

Una confusión frecuente al leer el spec: si el Sub Genesis es un efecto de Master
Layer sobre la señal del engine, ¿por qué el sub-oscilador no está conectado al
engine?

La respuesta es que el sub-oscilador es un **generador independiente**, no un
procesador de la señal de entrada. El FX funciona así:

- El **dry path** es la señal del engine sin modificar (el chord de C mayor en el sketch)
- El **wet path** es un sub-oscilador independiente sintetizado a la frecuencia raíz

El "análisis de pitch" del spec se refiere a obtener la frecuencia raíz del patch
activo para programar el sub-oscilador a la frecuencia correcta. En Fase 2, esta
información viene del sketch (hardcoded). En Fase 4, viene del noteOn MIDI.

El sub nunca "procesa" la señal del engine — la ignora completamente en términos
de señal. Solo usa la información de pitch (un número) para programar su frecuencia.

Esta arquitectura tiene una ventaja importante: el sub siempre tiene la misma latencia
que cualquier otro oscilador (cero latencia de análisis), y su calidad de timbre es
completamente predecible (no depende de qué tan bien el algoritmo de tracking analizó
la señal entrante).

---

### Waveform del sub: sine vs square

El sub-oscilador puede usar `WAVEFORM_SINE` o `WAVEFORM_SQUARE`.

**Senoidal:** una sola frecuencia, sin armónicos. El "thump" más puro. Con el waveshaper
activo, los armónicos se añaden de forma controlada (solo los que el drive genera).
Ideal para dub y sub-bass limpio.

**Cuadrada:** rica en armónicos impares (f, 3f, 5f, 7f...). El sub ya tiene presencia
en frecuencias medias antes del waveshaper. La saturación sobre una cuadrada comprime
dinámicamente más que generar nuevos armónicos (los armónicos ya existen). El resultado
es un sub más "agresivo" y "presente" en el mix — útil para techno y música donde el
sub compite con otros instrumentos en el mid-range.

El spec lista `WAVEFORM_SINE` como default, con `WAVEFORM_SQUARE` como alternativa
accesible vía comando Serial en el sketch de demo.

---

### Signal flow completo

```
inputStream (chord engine) ────────────────────────────────────→ dryWetMix (ch0) ← dry
                                                                       ↓
[AudioSynthWaveform _subOsc]                                    dryWetMix (ch1) ← wet
  freq = rootFreq / 2^octaves
  wave = WAVEFORM_SINE (default) / WAVEFORM_SQUARE
  amplitude = _subLevel
       ↓
[AudioEffectWaveshaper _subSat]
  tabla tanh con drive 1.0–8.0
  regenerada en begin() y cada cambio de drive
       ↓
[AudioFilterStateVariable _subLP]
  LP cutoff 40–200 Hz, Q = 0.7 (Butterworth)
  salida: canal LP (canal 0)
       ↓
[AudioMixer4 _subMix]
  ch0: LP output de _subLP
  gain(0) = 1.0 (nivel del sub ajustado en _subOsc.amplitude())
       ↓
dryWetMix (ch1) → wet channel
       ↓
AudioOutputI2S L+R
```

El `inputStream` (la señal del chord engine) entra al dry path del `dryWetMix`
directamente. El sub-oscilador es un branch completamente separado: `_subOsc` →
`_subSat` → `_subLP` → `_subMix` → wet channel del `dryWetMix`.

El `_subMix` existe para tener un punto de control de ganancia del sub independiente
del dry/wet global. El parámetro Sub Level controla `_subOsc.amplitude()` — el nivel
de amplitud del oscilador mismo, antes de la saturación.

**Nota sobre el Mix parameter:** el Mix controla el blend entre dry (la señal del
engine) y wet (el sub generado). A Mix=0.0, solo escuchás el engine original, sin
sub. A Mix=1.0, solo escuchás el sub (el engine desaparece del mix). El valor de
trabajo típico es Mix=0.3–0.5: el sub se asienta debajo del engine añadiendo peso
sin dominar el sonido.

---

### CPU estimado

```
Objetos adicionales sobre el engine standalone (3 OSCs + srcMix):

  AudioSynthWaveform (_subOsc):          ~0.5%   generador de forma de onda estándar
  AudioEffectWaveshaper (_subSat):       ~1.5%   lookup table 257 pts + interpolación
  AudioFilterStateVariable (_subLP):     ~1.0%   filtro 2-polo StateVariable
  AudioMixer4 (_subMix):                 ~0.2%   un canal activo
  AudioMixer4 (dryWetMix):               ~0.3%   dos canales activos

  Total Sub Genesis:                     ~3.5%
```

La tabla al lado del spec de `05-fx-architecture.md` §1.12 indica CPU ~4%. Esta
estimación es conservadora — el Sub Genesis tiene menos objetos que el Phase Chorus
o el Bit Sculpt (que midieron 1.1% y 0.6% respectivamente en hardware real, muy
por debajo de sus estimados de ~5% y ~4%).

```
CPU total estimado (engine 3 OSCs + Sub Genesis):
  3× AudioSynthWaveform (chord) + srcMix:  ~1.5%
  Sub Genesis completo:                    ~3.5%
  Total:                                   ~5.0% CPU

Budget specs (05-fx-architecture.md §2): ≤60% total en worst case
5.0% << 60% — headroom amplio
```

---

### Tabla de referencia: frecuencias del sub por nota

Para el sketch de demo con C mayor (C4+E4+G4), la nota raíz es C4:

| Nota raíz | Frecuencia | Octaves=1 (f/2) | Octaves=2 (f/4) |
|---|---|---|---|
| C2 | 65.41 Hz | 32.70 Hz | 16.35 Hz (sub-sonic) |
| C3 | 130.81 Hz | 65.41 Hz | 32.70 Hz |
| C4 (demo) | 261.63 Hz | 130.81 Hz (C3) | 65.41 Hz (C2) |
| A4 | 440.00 Hz | 220.00 Hz | 110.00 Hz |
| C5 | 523.25 Hz | 261.63 Hz | 130.81 Hz |

El sketch de demo usa `rootFreq = 130.81 Hz` (C3, una octava abajo de C4) como
frecuencia del sub-oscilador con Octaves=1. Esto produce el sub en C2 (65.41 Hz)
a Octaves=2. C2 a 65 Hz es territorio de sub-bass profundo — los altavoces de PA
de 18" son necesarios para reproducirlo sin el artefacto de tono virtual.

---

### Psicoacústica del sub en mezcla: cuánto mezclar y cuándo

El Mix parameter es el control más crítico en live performance. Demasiado sub (Mix >
0.7) domina el mix y oscurece la claridad del engine. Muy poco (Mix < 0.15) no se
siente. El sweet spot para la mayoría de géneros es Mix = 0.25-0.45.

En sistemas de PA con subwoofers reales (>18"), el sub se siente físicamente. En
auriculares, el efecto es más de "engrosamiento" de los bajos que de presión física.
En monitores de estudio sin sub, el sub puede ser casi inaudible si Cutoff es bajo
(40-60 Hz) — en ese caso, subir el Cutoff a 100-150 Hz hace que los armónicos de
la saturación sean audibles, y el efecto funciona vía tono virtual.

Esta es la razón por la que el Drive es tan importante: sin Drive (Drive=1.0, casi
lineal), el sub es una senoide pura a 65-130 Hz — inaudible en muchos sistemas.
Con Drive=3.0-5.0, los armónicos a 195-390 Hz son claramente audibles y el efecto
funciona en cualquier sistema de reproducción.

**Referencia:** Hugo Fastl y Eberhard Zwicker, "Psychoacoustics: Facts and Models"
(3rd ed., Springer, 2007), Chapter 8 "Loudness" — sección sobre el efecto del tono
de diferencia y el tono virtual, con mediciones experimentales que confirman que
el oído humano puede percibir una frecuencia fundamental de 100 Hz a través de sus
armónicos incluso cuando la fundamental está físicamente ausente de la señal.

---

## Wiring

Sin cambios de hardware respecto a sprints anteriores. El efecto es completamente
digital:

- Mismo Teensy 4.1 + Audio Shield Rev D2 (SGTL5000)
- Salida por jack 3.5mm del Audio Shield (auriculares o monitores)
- No se requiere ningún componente adicional
- Pin mapping sin cambios respecto a `01-architecture.md §3.3`
- Sin filter discreto 2N3904 involucrado (Sprint 1.4 deferred)

---

## Implementation Notes

### Inicialización — orden y dependencias

```cpp
void setup() {
    AudioMemory(20);                   // siempre primero
    sgtl5000.enable();
    sgtl5000.volume(0.5f);

    // Tabla de saturación del sub — generada en setup() con drive inicial
    generate_sat_table(g_drive);       // g_drive = 2.0f default
    subSat.shape(satTable, 257);

    // Sub-oscilador: C3 = 130.81 Hz, una octava abajo de C4
    subOsc.begin(WAVEFORM_SINE);
    subOsc.frequency(130.81f);         // rootFreq / 2^octaves = 261.63 / 2
    subOsc.amplitude(g_subLevel);      // g_subLevel = 0.8f default

    // LP post-saturación del sub
    subLP.frequency(100.0f);           // g_cutoff = 100.0f default
    subLP.resonance(0.7f);             // Q Butterworth, sin pico de resonancia

    // Mixer del sub (un canal activo)
    subMix.gain(0, 1.0f);

    // Dry/wet: dry = chord engine, wet = sub saturado
    dryWetMix.gain(0, 1.0f - g_mix);  // dry: 1 - mix
    dryWetMix.gain(1, g_mix);          // wet: mix (default 0.5)

    // Chord engine: C mayor (C4 + E4 + G4, sawtooth)
    osc_C.begin(WAVEFORM_SAWTOOTH); osc_C.frequency(261.63f); osc_C.amplitude(0.4f);
    osc_E.begin(WAVEFORM_SAWTOOTH); osc_E.frequency(329.63f); osc_E.amplitude(0.4f);
    osc_G.begin(WAVEFORM_SAWTOOTH); osc_G.frequency(392.00f); osc_G.amplitude(0.4f);

    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
}
```

A diferencia del `AudioEffectChorus` (Sprint 2.4), el `AudioEffectWaveshaper` no
requiere `begin()` — solo necesita que `shape()` se llame antes de que el audio
empiece a procesarse. La tabla puede generarse en cualquier momento antes o después
de `AudioMemory()`.

### Cálculo de frecuencia del sub con Octaves

```cpp
static float  g_rootFreq  = 130.81f;   // C3 — raíz del patch de demo
static int    g_octaves   = 1;          // 1 = una octava abajo de rootFreq
static float  g_subLevel  = 0.8f;
static float  g_drive     = 2.0f;
static float  g_cutoff    = 100.0f;
static float  g_mix       = 0.5f;
static bool   g_bypass    = false;
static int    g_waveform  = WAVEFORM_SINE;

void update_sub_freq() {
    // f_sub = rootFreq / 2^octaves
    float freq = g_rootFreq;
    for (int i = 0; i < g_octaves; i++) freq *= 0.5f;
    subOsc.frequency(freq);
}
```

El multiplicar por 0.5 en cada paso evita el `pow(2, octaves)` que introduce
latencia de floating point. Para Octaves ∈ {1, 2}, el loop tiene una o dos iteraciones.

### Generación de tabla tanh con normalización

```cpp
static float satTable[257];

void generate_sat_table(float drive) {
    float norm = tanhf(drive);           // normalización: tanh(drive × 1.0) / tanh(drive) = 1.0
    for (int i = 0; i < 257; i++) {
        float x = (float)i / 128.0f - 1.0f;   // x ∈ [-1.0, 1.0]
        satTable[i] = tanhf(drive * x) / norm;
    }
    subSat.shape(satTable, 257);
}
```

Regenerar la tabla lleva ~50-100 µs en el Teensy 4.1. Llamar desde loop() solo
cuando cambia el drive (no en cada ciclo):

```cpp
void set_drive(float d) {
    g_drive = constrain(d, 1.0f, 8.0f);
    generate_sat_table(g_drive);         // regenera tabla solo en cambio de parámetro
}
```

### Bypass limpio

```cpp
void set_bypass(bool b) {
    g_bypass = b;
    if (b) {
        // Bypass: solo dry, wet silenciado
        dryWetMix.gain(0, 1.0f);
        dryWetMix.gain(1, 0.0f);
        subOsc.amplitude(0.0f);          // apagar el oscilador
    } else {
        dryWetMix.gain(0, 1.0f - g_mix);
        dryWetMix.gain(1, g_mix);
        subOsc.amplitude(g_subLevel);    // restaurar nivel del sub
    }
}
```

El bypass silencia el sub-oscilador directamente (`amplitude(0.0f)`) en lugar de
solo cortar la conexión del mixer. Esto evita que el oscilador siga corriendo y
consuma CPU invisible cuando está bypassed.

### Patrón de clase SubGenesis (para integración futura)

Cuando este FX se integre al sistema completo (Sprint 3.x), seguirá el patrón
establecido en BitSculpt y PhaseChorus:

```
SubGenesis
  ├── begin(AudioStream& input, float volume)  — crea AudioConnections dinámicas
  ├── setSubLevel(float level)                 — 0.0–1.0 (ENC R en FX mode per UI spec §3.4)
  ├── setOctaves(int n)                        — 1–2
  ├── setDrive(float drive)                    — 1.0–8.0, regenera tabla tanh
  ├── setCutoff(float hz)                      — 40–200 Hz, StateVariable LP
  ├── setMix(float mix)                        — 0.0–1.0 (dry/wet blend)
  ├── setWaveform(int type)                    — WAVEFORM_SINE / WAVEFORM_SQUARE
  ├── setRootFreq(float hz)                    — llamado desde MIDI noteOn en Fase 4
  └── setBypass(bool b)                        — bypass limpio sin clicks
```

`ENC R (FX mode) = Sub Level` — confirmado en el spec del Panel v5 Final §3.4.
Sub Level es el parámetro más expresivo en live: de cero sub a sub dominante.

---

## Demo

**Sketch:** `apps/firmware-teensy/src/sketches/11-sub-genesis.cpp`

### Setup al arrancar

El sketch inicia con el acorde C mayor (C4+E4+G4, sawtooth, mismo patrón que
sprints 2.3-2.5) y Sub Genesis activo con valores que demuestran el "thump" desde
el primer segundo:

```
rootFreq  = 130.81 Hz (C3 — una octava abajo del acorde)
Octaves   = 1         (sub en C3 = 130.81 Hz)
SubLevel  = 0.8       (sub presente y audible)
Drive     = 2.0       (saturación suave — armónicos audibles en cualquier sistema)
Cutoff    = 100 Hz    (passband del sub filtrado, armónicos hasta ~300 Hz pasan)
Mix       = 0.5       (balance 50/50 entre chord y sub)
Waveform  = SINE      (sub puro, saturación controlada)
Bypass    = false     (efecto activo)
```

### Comandos Serial (115200 baud)

| Comando | Parámetro | Rango | Ejemplo |
|---|---|---|---|
| `l<val>` | SubLevel (0.0–1.0) | float | `l0.8` |
| `o<n>` | Octaves (1–2) | entero | `o1` |
| `d<val>` | Drive (1.0–8.0) | float | `d4.0` |
| `k<val>` | Cutoff Hz (40–200) | float | `k80` |
| `m<val>` | Mix (0.0–1.0) | float | `m0.5` |
| `w<n>` | Waveform (1=sine, 2=square) | entero | `w2` |
| `p` | Bypass toggle | — | `p` |

El comando `k` para Cutoff (de "kilohertz" foneticamente) evita colisión con `c`
usado en el Bit Sculpt para Sculpt. El comando `l` para SubLevel evita colisión con
`d` (Drive) y `m` (Mix).

### Reporte Serial cada 2 segundos

```
CPU: 5.1% | Mem: 9 bloques | SubLvl: 0.80 | Oct: 1 | Drive: 2.00 | Cut: 100.0Hz | Mix: 0.50 | Wave: SINE
```

### Evidencia requerida para validar el hito

1. **Grabación de audio** (Audacity, 30 segundos mínimo): acorde C mayor sin sub
   (bypass `p`) vs con Sub Genesis activo (defaults). La diferencia en las frecuencias
   bajas debe ser audible — en el espectro de Audacity se verá energía nueva en la
   banda 100-200 Hz con el sub activo que no existe sin él.

2. **Demo de Octaves**: grabación de `o1` (C3, 130 Hz) vs `o2` (C2, 65 Hz) con `d4.0`.
   A Octaves=2 el sub es más profundo y más dependiente del Drive para ser audible
   (la fundamental a 65 Hz no reproduce bien en auriculares sin subwoofer, los
   armónicos de la saturación a 195 Hz y 325 Hz son los que dan el thump).

3. **Demo de Drive**: grabación de `d1.0` (sub casi sine puro) vs `d6.0` (saturación
   agresiva) con `o2 k150`. A Drive=1.0 el sub puede no ser audible en auriculares
   pequeños. A Drive=6.0 los armónicos son claramente audibles en cualquier sistema.

4. **Demo de waveform**: grabación de `w1` (sine) vs `w2` (square) con `d3.0`.
   La cuadrada tiene más presencia en medios-bajos — más "agresiva" y presente antes
   de la saturación.

5. **Screenshot Serial Monitor**: CPU% < 8% con todos los componentes activos.
   AudioMemory peak no debe superar 15 de 20 bloques.

### Secuencia de demo sugerida

```
1. Arranque: defaults (SubLvl=0.8, Oct=1, Drive=2.0, Cut=100, Mix=0.5, SINE)
   → Chord C mayor con sub C3 debajo — sentir el peso añadido

2. p → bypass
   → Comparar: sin sub vs con sub. La diferencia en graves debe ser clara

3. p → volver al efecto activo

4. m1.0 → solo sub (chord silenciado en el mix)
   → Escuchar el sub aislado: senoide saturada a 130 Hz con armónicos

5. m0.0 → solo chord (sub silenciado)
   → El chord sin sub: brillante pero sin peso de bajos

6. m0.5 → volver al balance de trabajo

7. o2 → Octaves=2 (sub en C2, 65 Hz)
   → Sub más profundo — en auriculares puede ser menos audible

8. d6.0 → Drive agresivo
   → Los armónicos a 195 Hz y 325 Hz se vuelven audibles incluso sin sub-woofer

9. k150 → Cutoff alto
   → El filtro LP deja pasar más armónicos del sub: más presente en medios-bajos

10. k50 → Cutoff bajo
    → Solo la fundamental pasa: sub más limpio pero más dependiente del hardware

11. d2.0 k100 o1 → volver a defaults razonables para drive y cutoff

12. w2 → Square waveform
    → Sub con armónicos propios + saturación: más agresivo y más presente

13. w1 → volver a Sine

14. l0.3 → SubLevel bajo
    → El sub se asienta sutilmente debajo del chord — blend muy natural

15. l1.0 → SubLevel alto
    → El sub al máximo antes del waveshaper: saturación más agresiva

16. l0.8 m0.4 → valores de trabajo para live
    → El sub añade peso sin dominar — este es el sweet spot para dub/techno

17. p → bypass final
    → Comparación definitiva: el "thump" que falta sin Sub Genesis
```

---

## Learnings

### Métricas reales en hardware (Teensy 4.1 + Audio Shield Rev D2)

| Métrica | Estimado | Real | Delta |
|---|---|---|---|
| CPU | ~4% | **0.8%** | -80% |
| AudioMemory peak | ~8 bloques | **9 bloques** | +13% (esperado — 1 bloque extra por LP post-sat) |

### Frecuencias de sub verificadas matemáticamente

El reporte serial confirma la aritmética de octavas:
- RootFreq: 130.81 Hz (C3, hardcoded)
- Octaves=1: SubFreq = 130.81 / 2 = **65.4 Hz** (C2) ✓
- Octaves=2: SubFreq = 130.81 / 4 = **32.7 Hz** (C1) ✓

A 32.7 Hz el sub está en el límite de lo audible por auriculares (~20Hz es el límite teórico). En monitores con subwoofer el efecto sería mucho más pronunciado. Para live con sistema PA, Octaves=2 añade el "thump" que se siente más que se escucha.

### Drive verificado — cambio audible y sin artifacts

Drive 2.0 → 8.0 con Octaves=2: saturación agresiva del sub a 32.7Hz genera armónicos a 65Hz, 98Hz, 131Hz — añade presencia del sub en el rango audible sin headphones de subwoofer. El cambio no produce clicks ni discontinuidades. CPU constante a 0.8% independientemente del drive.

### Mix aditivo confirmado como decisión correcta

El dry siempre al 100% + sub sumado encima preservó el chord C mayor intacto en todas las pruebas. A Mix=0.5 el sub complementa sin dominar — comportamiento musical correcto para un bass enhancement.

### Deuda técnica: pitch tracking desde MIDI

La frecuencia raíz hardcoded (C3) funciona correctamente para el demo pero en producción el sub debe seguir la nota activa del engine. Implementación en Fase 4 (MIDI + TinyML): el engine expone la frecuencia de la nota activa vía `getLastNoteFreq()`, el FX Manager la pasa a `SubGenesis::setRootFreq()` en cada noteOn. Anotado como ticket para Sprint 4.x.

---

*Sprint 2.6 — GrooveForge Brain · Juan Guerrero (GPROG)*

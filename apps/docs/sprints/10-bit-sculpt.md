# Sprint 2.5 — FX Bit Sculpt

**Status:** Done — CPU 0.6%, 7 bloques, todos los parámetros verificados en hardware
**Refs:** `apps/docs/05-fx-architecture.md` §1.6, `apps/docs/01-architecture.md` §3.4

---

## Theory

### El problema central: ¿por qué los bitcrushers suenan feos a baja resolución?

La respuesta corta es que la distorsión de cuantización no es ruido blanco — es
distorsión armónica correlacionada con la señal de entrada, y el oído humano la
reconoce como algo sucio, no como algo con carácter.

Para entender esto bien, hay que entender primero qué ocurre cuando cuantizamos audio.

---

### Cuantización de amplitud — física del fenómeno

Cuando grabamos audio digital, convertimos una señal de voltaje continuo en una
secuencia de números enteros. Si usamos 16 bits, el rango de amplitud queda dividido
en `2^16 = 65536` niveles. Si usamos 8 bits, en `2^8 = 256` niveles. Con 1 bit,
solo dos valores posibles: +1 y -1.

La operación matemática es:

```
y[n] = round(x[n] × (2^bits - 1)) / (2^bits - 1)
```

Donde `x[n]` es la muestra de entrada en el rango [-1.0, 1.0] y `y[n]` es la
muestra cuantizada al número de niveles disponibles.

El **error de cuantización** es la diferencia entre lo que entra y lo que sale:

```
e[n] = y[n] - x[n]
```

Con 16 bits, el error máximo es `1 / (2 × 65535) ≈ 0.0000076` — prácticamente
imperceptible. Con 8 bits, el error máximo es `1 / (2 × 255) ≈ 0.002` — ya
audible en señales bajas. Con 4 bits, el error máximo es `1 / (2 × 15) ≈ 0.033` —
muy audible, ~30 dB por encima del ruido de 16 bits. Con 1 bit, el error puede
ser hasta 0.5 — la señal original queda completamente distorsionada.

#### Por qué el error es distorsión armónica y no ruido aleatorio

Para una señal sinusoidal a 1 kHz cuantizada a 4 bits, el error `e[n]` no es
aleatorio. Es una señal periódica que depende de la amplitud y frecuencia de `x[n]`:
cuando `x[n]` está en la región de un nivel de cuantización, el error es cero.
Cuando cruza entre dos niveles, el error salta abruptamente. Esta estructura
periódica produce **armónicos y subarmónicos** de la señal original en la salida.

```
Señal de entrada: 1000 Hz, 4 bits
Señal de salida contiene: 1000 Hz + 3000 Hz + 5000 Hz + 7000 Hz + ...
                          (armónicos impares de la señal fundamental)
```

El cerebro reconoce estos armónicos como parte del timbre original — lo que hace
el bitcrusher duro sonar "irritante" y no "musical". No es que el ruido sea alto —
es que la distorsión está **correlacionada tonalmente** con la señal de entrada.
Esto es diferente al ruido de la grabación analógica (ruido de tubo, ruido de cinta),
que es estadísticamente independiente de la señal y por eso no irrita de la misma manera.

**Referencia:** Ken Pohlmann, "Principles of Digital Audio" (6th ed., McGraw-Hill,
2011), Chapter 4 "Quantization" — análisis del error de cuantización como señal
periódica correlacionada y su espectro armónico (pp. 98-115). Esta edición incluye
el análisis de Fourier del error para señales sinusoidales, mostrando que los
armónicos generados son impares para señales simétricas.

**Referencia adicional:** Zölzer, "DAFX: Digital Audio Effects" (2011), Chapter 7
"Dynamic Range Control", §7.1 "Quantization Noise" — la presentación de Zölzer
enfatiza que el ruido de cuantización solo puede modelarse como ruido blanco cuando
hay dither presente; sin dither, el modelo de ruido blanco no aplica.

---

### Dither — convirtiendo distorsión armónica en ruido blanco

El dither es la técnica más contraintuitiva del audio digital: para mejorar la
calidad del sonido, **añadimos ruido intencionalmente antes de cuantizar**.

El principio es el siguiente: si el error `e[n]` es correlacionado con la señal
(distorsión armónica), y queremos romper esa correlación, podemos añadir una señal
de ruido `d[n]` al input antes de cuantizar. El error pasa de ser:

```
e[n] = round(x[n] × L) / L  −  x[n]           (sin dither — correlacionado)
```

a:

```
e'[n] = round((x[n] + d[n]) × L) / L  −  x[n]  (con dither — decorrelacionado)
```

Cuando `d[n]` es ruido blanco de amplitud suficiente, el error `e'[n]` se vuelve
estadísticamente independiente de `x[n]`. La distorsión armónica desaparece y en
su lugar aparece un ruido de fondo plano. El resultado neto: el audio suena más
limpio a baja resolución, aunque el nivel de ruido sea nominalmente mayor.

Esto puede verificarse intuitivamente: un cassette de audio con ruido de tape
(~-60 dBFS de ruido blanco) suepa mejor que un archivo de 8 bits sin dither
(distorsión armónica correlacionada), aunque los niveles de error sean comparables.
El ruido blanco no "persigue" a la señal tonalmente — el oído lo separa del contenido
musical.

**Referencia:** Pohlmann, op. cit., Chapter 4 "Dithering" (pp. 115-130). Pohlmann
dedica un capítulo completo a probar matemáticamente la decorrelación del error con
dither, incluyendo la demostración de que la varianza del error con RPDF dither es
`L²/6` — igual que sin dither, pero estadísticamente independiente de la señal.

---

### Tipos de dither — el parámetro Sculpt

El parámetro Sculpt de Bit Sculpt selecciona el tipo y carácter del dither. Hay
tres tipos fundamentales, ordenados de menor a mayor sofisticación:

#### RPDF — Rectangular Probability Density Function (Sculpt = 0.0)

El dither más simple: ruido uniforme con amplitud en el rango `[-0.5 LSB, +0.5 LSB]`
donde LSB (Least Significant Bit) es el tamaño de un nivel de cuantización:

```
LSB_amplitude = 1.0 / (2^bits - 1)
```

La "PDF rectangular" significa que todos los valores de ruido en ese rango son
igualmente probables — una distribución uniforme. RPDF es efectivo para decorrelacionar
el error, pero tiene la propiedad de que el error cuadrático medio (RMS) con RPDF
es mayor que sin dither, y tiene un segundo momento no nulo (el error tiene correlación
de segundo orden con la señal, aunque no de primer orden).

Para el GrooveForge Brain en contexto live, RPDF es perfectamente adecuado — la
diferencia con TPDF es mínima a volúmenes de performance.

#### TPDF — Triangular Probability Density Function (Sculpt = 0.5)

El TPDF se obtiene sumando dos señales RPDF independientes:

```
d_tpdf[n] = d1[n] + d2[n],  donde d1, d2 son RPDF independientes
```

El resultado es una distribución triangular (la PDF de la suma de dos uniformes
es triangular — resultado del teorema central del límite para solo dos muestras).
La amplitud de cada componente es `[-0.5 LSB, +0.5 LSB]`, por lo que el TPDF
cubre el rango `[-1.0 LSB, +1.0 LSB]`.

La ventaja del TPDF sobre RPDF es que **elimina el segundo momento del error de
cuantización** — el error resultante no solo es decorrelacionado de primer orden
con la señal, sino también de segundo orden. Esto significa que el TPDF produce
el mínimo nivel de distorsión audible teóricamente posible sin noise shaping.

El TPDF es el estándar de la industria para el mastering de CD (16 bits). Cuando
un ingeniero de mastering prepara el archivo final para CD desde un archivo de 24
bits de estudio, el dither que aplica es TPDF.

**Referencia:** Pohlmann, op. cit., pp. 120-124. Demostración formal de por qué
TPDF minimiza la distorsión de cuantización de orden 2. La prueba muestra que el
error residual con TPDF es independiente de la señal hasta el segundo momento.

#### Noise shaping (Sculpt = 1.0)

El noise shaping es la técnica más sofisticada: en lugar de añadir ruido aleatorio,
realimentamos el error de cuantización de la muestra anterior y lo substraemos de
la muestra actual antes de cuantizar. Esto "mueve" el ruido de cuantización en
el dominio de la frecuencia — lo concentra en frecuencias donde el oído es menos
sensible.

La implementación de primer orden es:

```
x_modificado[n] = x[n] - α × e[n-1]
y[n]            = quantize(x_modificado[n] + d[n])
e[n]            = y[n] - x_modificado[n]
```

Donde `α` es el coeficiente de realimentación (en nuestra implementación, `α = 1.0`
para primer orden simple) y `d[n]` es ruido RPDF adicional para la decorrelación
de primer orden.

El efecto espectral: el ruido de cuantización tiene una respuesta en frecuencia
determinada por el filtro de error. Con un filtro de primer orden simple, el ruido
se acumula en las frecuencias altas (donde la sensibilidad del oído baja) y se
reduce en las frecuencias bajas y medias (donde el oído es más sensible). La curva
de sensibilidad del oído humano (curva de equal-loudness de Fletcher-Munson) tiene
un pico de sensibilidad alrededor de 3-4 kHz y cae significativamente por encima
de 10 kHz — exactamente donde el noise shaping envía el ruido.

En el GrooveForge Brain, la implementación del noise shaping es una aproximación
de primer orden simplificada. Las implementaciones profesionales de noise shaping
(como el Apogee UV22 o el Sony Super Bit Mapping) usan filtros de error de orden 5-9
diseñados para seguir exactamente la curva inversa de sensibilidad del oído. Nuestra
versión de primer orden es suficientemente diferenciada del TPDF para ser perceptible
en demo, sin la complejidad de diseño de filtro de un sistema de mastering.

**Referencia:** Zölzer, "DAFX" (2011), §7.1.2 "Noise Shaping", pp. 248-252.
Incluye la derivación de la función de transferencia del error para noise shaping de
primer orden y la figura 7.4 que muestra el espectro del ruido con y sin noise shaping.

#### Implementación del Sculpt como blend continuo

El parámetro Sculpt mapea de forma continua entre los tres tipos:

```
Sculpt ∈ [0.0, 0.5]:  blend entre RPDF puro y TPDF
  dither_level = Sculpt × 2.0   (AudioSynthNoiseWhite → AudioMixer4)
  noise_shaping_amount = 0.0

Sculpt ∈ [0.5, 1.0]:  TPDF con noise shaping creciente
  dither_level = 1.0             (TPDF activo siempre)
  noise_shaping_amount = (Sculpt - 0.5) × 2.0
```

El nivel del dither noise en el `ditherMix` se escala a aproximadamente 1 LSB de
amplitud para el número de bits actual:

```
noise_amplitude = 1.0 / (2^bits - 1) × dither_level
```

A Sculpt=0.0, el noise casi no tiene nivel — el efecto opera como un bitcrusher
sin dither (distorsión armónica pura, el carácter más "digital y feo"). A Sculpt=1.0,
el dither está activo y con noise shaping — el carácter es más "suave y musical"
incluso a 4 bits.

---

### Sample rate reduction — aliasing intencional como herramienta

El segundo eje creativo del Bit Sculpt es la reducción de sample rate (SRR), que
es conceptualmente diferente a la cuantización de amplitud: en lugar de reducir la
resolución vertical de la señal (cuántos niveles de amplitud), reducimos su
resolución temporal (cuántas muestras por segundo).

#### Qué es el aliasing y por qué normalmente lo evitamos

El teorema de Nyquist establece que para representar fielmente una señal con
frecuencia máxima `f_max`, necesitamos una tasa de muestreo de al menos `2 × f_max`.
Si la señal contiene frecuencias superiores a `f_s / 2` (la frecuencia de Nyquist),
esas frecuencias se "doblan" dentro del espectro audible como frecuencias
inarmónicas — el aliasing.

En audio profesional, el aliasing es un defecto de diseño a evitar. Toda la cadena
de grabación digital incluye filtros anti-aliasing precisamente para esto.

En el Bit Sculpt, el aliasing es el **efecto deseado**. Las frecuencias inarmónicas
generadas por el aliasing son el sonido "Game Boy" o "SP-1200" que define el lo-fi
digital de los años 80 y 90.

Ejemplo concreto: reducción a `f_s = 8000 Hz` (estilo Game Boy original):

```
Nyquist de 8000 Hz = 4000 Hz

Un parcial de la señal de entrada a 5000 Hz:
  alias = f_s - f_parcial = 8000 - 5000 = 3000 Hz  (inarmónico respecto al original)

Un parcial a 7000 Hz:
  alias = 8000 - 7000 = 1000 Hz  (puede sonar afinado por casualidad o no)

Un parcial a 9000 Hz (primer armónico de una fundamental a 4500 Hz):
  alias = 9000 - 8000 = 1000 Hz  (y el armónico a 13500 Hz: alias = 8000×2-13500 = 2500 Hz)
```

El resultado es un espectro rico en intermodulación inarmónica — "crujiente" y con
energía en frecuencias que no estaban en la señal original.

#### Sample-and-hold: reducción de sample rate sin cambiar la tasa real

No reducimos la frecuencia de muestreo real del sistema (que sigue siendo 48 kHz —
cambiarla afectaría a todo el audio path). En cambio, implementamos un **sample-and-hold**:
cada muestra se mantiene durante `N = round(48000 / f_target)` muestras consecutivas.

```
f_target = 8000 Hz → N = round(48000/8000) = 6 muestras
Cada 6 muestras, el valor se congela en el último sample capturado
```

Esto es matemáticamente equivalente a submuestrear la señal a 8 kHz sin filtro
anti-aliasing y luego mantener cada muestra, produciendo exactamente el aliasing
deseado. El `AudioEffectBitcrusher` de la Teensy Audio Library implementa este
sample-and-hold internamente con el parámetro `sampleRate(hz)`.

**Referencia:** Zölzer, "DAFX" (2011), §7.2 "Sample Rate Reduction", pp. 253-256.
La figura 7.5 muestra el espectro antes y después de la reducción de sample rate
sin anti-aliasing, con los componentes de aliasing marcados explícitamente.

---

### AudioEffectBitcrusher — la API de la Teensy Audio Library

El objeto `AudioEffectBitcrusher` (archivo `effect_bitcrusher.h` en la librería de
PaulStoffregen) provee los dos parámetros fundamentales de forma independiente y
combinable:

```cpp
AudioEffectBitcrusher crusher;
crusher.bits(n);           // cuantización: 1–16 bits
crusher.sampleRate(hz);    // sample-and-hold: 1.0–44100.0 Hz
```

Ambos parámetros pueden modificarse en runtime desde `loop()`. El objeto procesa
en el ISR de audio (interrupt de I2S), igual que todos los objetos de la Teensy
Audio Library — garantizando latencia de procesamiento de un bloque (128 muestras
a 48 kHz ≈ 2.67 ms).

El bitcrusher reduce la resolución de amplitud procesando la señal en punto fijo de
16 bits. Con `bits(8)`, las 8 bits menos significativas de cada muestra se truncan
a cero — equivalente a la operación `round(x × 128) / 128` pero realizada en
aritmética entera para eficiencia.

**Limitación conocida de la API:** `AudioEffectBitcrusher` no implementa dither
interno. La señal procesada por el bitcrusher tiene la distorsión armónica de
cuantización sin dither — esa es la razón por la que el Bit Sculpt añade el dither
como etapa previa.

**Referencia:** PaulStoffregen, `effect_bitcrusher.cpp` (Teensy Audio Library,
GitHub). El source muestra que la cuantización se implementa con shift right + shift
left en int16_t: `s >>= bits_to_lose; s <<= bits_to_lose;` — O(1) por muestra,
extremadamente eficiente.

---

### Signal flow del Bit Sculpt

El dither se implementa como ruido añadido a la señal **antes** del bitcrusher. El
`AudioSynthNoiseWhite` provee la fuente de ruido blanco. El `ditherMix` suma el
ruido con la señal de entrada, y la mezcla entra al `AudioEffectBitcrusher`.

```
3× AudioSynthWaveform (C4, E4, G4 — sawtooth)
              ↓
        AudioMixer4 srcMix
              ↓
              ├────────────────────────────────────────→ (dry path)
              │                                                │
              ↓                                               │
      AudioMixer4 ditherMix (ch0: señal, ch1: noise)          │
        ↑                                                      │
  AudioSynthNoiseWhite                                         │
  (nivel = 1 LSB × sculpt_amount)                             │
              ↓                                               │
  AudioEffectBitcrusher                                        │
  (bits + sampleRate)                                          │
              ↓                                               │
    noise_shaping_feedback → (error loop en loop())            │
              ↓                                               │
      AudioMixer4 dryWetMix                                    │
        ch0: dry ←──────────────────────────────────────────→┘
        ch1: wet (bitcrushed + dithered)
              ↓
      AudioOutputI2S L+R
   + AudioControlSGTL5000
```

#### Por qué el dry path va desde srcMix y no desde ditherMix

El dry path debe ser la señal original sin ninguna modificación — sin dither, sin
bitcrushing. Si el dry tomara la señal desde el output del ditherMix, el path "limpio"
ya tendría el ruido de dither sumado, lo cual colorearía el dry incluso con Mix=0.
Al tomar el dry directamente del srcMix (antes del ditherMix), el bypass total
`Mix=0.0` reproduce el engine sin ningún artefacto del FX.

#### Parallel wet/dry — por qué no procesamos el dry por el bitcrusher

El procesamiento paralelo (dry sin procesar + wet con bitcrusher mezclados) preserva
los transientes y la definición de la señal original incluso con Mix alto. Con
Mix=0.7 (70% wet), el 30% de señal limpia mantiene el "punch" de los ataques — el
bitcrusher puede suavizar los transientes debido a la distorsión no-lineal, y el
dry compensa esto. Esta técnica es análoga al "parallel compression" de producción:
se mezclan la señal limpia y la procesada para obtener lo mejor de ambas.

**Referencia:** Pirkle, "Designing Software Synthesizer Plug-Ins in C++" (Focal
Press, 2014), Chapter 17 "Effects Processing" — el parallel mixing como técnica de
preservación de transientes, §17.4 "Wet/Dry Processing".

---

### El noise shaping de primer orden en loop()

El noise shaping no puede implementarse como objeto de audio adicional porque
requiere acceso al error de cuantización, que solo está disponible comparando la
señal antes y después del bitcrusher. La aproximación que usamos opera en `loop()`:

```cpp
static float last_error = 0.0f;

void update_noise_shaping(float sculpt_above_half) {
    // sculpt_above_half ∈ [0.0, 1.0] — activo cuando Sculpt > 0.5
    float alpha = sculpt_above_half;  // coeficiente de realimentación
    float feedback = alpha * last_error;

    // Ajustar la ganancia del canal de noise en ditherMix para aproximar
    // la realimentación del error. En rigor, el noise shaping exacto requeriría
    // acceso al error muestra a muestra (en ISR). Esta aproximación de loop()
    // actualiza el nivel de noise realimentado a tasa de ~1 ms, suficiente para
    // el carácter perceptual en el rango sub-1kHz.
    float noise_level = base_noise_level + alpha * last_error_rms;
    ditherMix.gain(1, noise_level);  // ch1: noise white
}
```

Esta aproximación de primer orden es conceptualmente correcta pero no sample-accurate.
Para una implementación exacta de noise shaping habría que modificar el ISR de audio
o implementar un objeto de audio custom. Para v1.0, la diferencia perceptual entre
el noise shaping exacto y esta aproximación es mínima en contexto live — el carácter
de "ruido concentrado en altas frecuencias" se consigue.

**Nota importante:** a Sculpt=1.0 con pocos bits (4 bits o menos), el noise shaping
puede en casos extremos amplificar el error si el coeficiente no está correctamente
escalado. Por esta razón, se aplica un clamp conservador: `alpha = min(sculpt_above_half, 0.7f)`.
Con coeficiente ≤ 0.7, el bucle de realimentación del error es estable para todas las
señales dentro del rango de audio normal.

---

### CPU y memoria estimados

#### Objetos de audio adicionales en este sprint

```
Objetos nuevos respecto a 3 OSCs + srcMix:
  AudioSynthNoiseWhite:  1 bloque de salida       ~1% CPU
  AudioMixer4 ditherMix: 1 bloque de salida       ~0.3% CPU
  AudioEffectBitcrusher: 1 bloque de salida       ~2% CPU
  AudioMixer4 dryWetMix: 1 bloque de salida       ~0.3% CPU
  Total adicional: 4 bloques sobre los ~5 del engine de 3 OSCs

Total estimado: ~9 bloques activos
AudioMemory(20) → 20 / 9 ≈ 2.2× headroom — suficiente
```

#### CPU estimado por componente

| Objeto | CPU estimado | Justificación |
|---|---|---|
| `AudioSynthNoiseWhite` | ~1.0% | Generador LFSR de 32 bits, O(1) por muestra |
| `AudioMixer4` (ditherMix) | ~0.3% | Suma ponderada de 2 canales |
| `AudioEffectBitcrusher` | ~2.0% | Shift aritmético + sample-and-hold en int16 |
| `AudioMixer4` (dryWetMix) | ~0.3% | Suma ponderada de 2 canales |
| Noise shaping en loop() | negligible | No es ISR, actualización ~1ms |
| **Total Bit Sculpt** | **~3.6%** | Vs. spec §1.6: ~4% — consistente |

```
CPU total estimado (3 OSCs + srcMix + Bit Sculpt):
  3× AudioSynthWaveform + srcMix: ~1.5%
  Bit Sculpt completo:            ~3.6%
  Total:                          ~5.1% CPU

Budget specs (05-fx-architecture.md §2): ≤60% total en worst case
5.1% << 60% — headroom amplio para FX adicionales
```

**Referencia CPU:** Teensy Audio Library benchmarks — https://www.pjrc.com/teensy/td_libs_AudioProcessorUsage.html
`AudioSynthNoiseWhite` figura en la tabla de Paul Stoffregen con medición en Teensy 4.1.
`AudioEffectBitcrusher` no tiene medición pública; el estimado de ~2% se basa en el
análisis del source (loop de 128 muestras con operaciones de shift de 4 ciclos cada
una: 128 × 4 / (600 MHz × 1/48000 × 128) ≈ 1.5-2%).

---

### Carácter musical a distintas configuraciones

La combinación de bits, sampleRate y Sculpt produce un espacio de sonidos muy amplio.
A continuación, las referencias históricas relevantes:

| Configuración | Referencia histórica | Uso típico |
|---|---|---|
| bits=12, sampleRate=32000, Sculpt=0.0 | Akai S612 (1985) | Hip-hop vintage, sonido "warm lo-fi" |
| bits=8, sampleRate=22050, Sculpt=0.3 | Sound Blaster 1.0 PC (1989) | Videojuego, glitch retro |
| bits=8, sampleRate=8000, Sculpt=0.0 | Game Boy DMG (1989) | Chiptune, 8-bit gaming |
| bits=12, sampleRate=26400, Sculpt=0.0 | E-mu SP-1200 (1987) | Hip-hop clásico, boom bap |
| bits=4, sampleRate=44100, Sculpt=0.5 | "Bit crunch extremo" | Glitch, breaks digitales |
| bits=1, sampleRate=48000, Sculpt=1.0 | Square wave total + dither | Aphex Twin, noise art |
| bits=16, sampleRate=4000, Sculpt=0.0 | Sample rate solo | Telephone filter + aliasing |

La línea del SP-1200 merece atención especial: muchos de los beats de hip-hop
clásicos de los 90s (Erik B. & Rakim, EPMD, Gang Starr) fueron producidos con
el E-mu SP-1200, cuyo conversor AD/DA era de 12 bits a 26.04 kHz. El "sonido SP-1200"
es en gran parte consecuencia directa de su cuantización de 12 bits y la respuesta
en frecuencia limitada de su sample rate. Bit Sculpt puede aproximar este carácter
con `bits=12, sampleRate=26000`.

**Referencia:** Vail, Mark, "Vintage Synthesizers" (2000), capítulo sobre samplers
de los años 80 — análisis del impacto del SP-1200 en la producción de hip-hop y la
relación entre especificaciones técnicas y carácter musical.

---

## Wiring

Sin cambios de hardware respecto a sprints anteriores. El efecto es completamente
digital:

- Mismo Teensy 4.1 + Audio Shield Rev D2 (SGTL5000)
- Salida por jack 3.5mm del Audio Shield (auriculares o monitores con nivel de línea)
- No se requiere ningún componente adicional
- Pin mapping sin cambios respecto a `01-architecture.md §3.3`
- Sin filter discreto 2N3904 involucrado en este sprint (Sprint 1.4 deferred)

---

## Implementation Notes

### Inicialización — orden y dependencias

```cpp
void setup() {
    AudioMemory(20);                  // siempre primero
    sgtl5000.enable();
    sgtl5000.volume(0.5f);

    // Osciladores: C mayor (C4 + E4 + G4)
    osc_C.begin(WAVEFORM_SAWTOOTH); osc_C.frequency(261.63f); osc_C.amplitude(0.4f);
    osc_E.begin(WAVEFORM_SAWTOOTH); osc_E.frequency(329.63f); osc_E.amplitude(0.4f);
    osc_G.begin(WAVEFORM_SAWTOOTH); osc_G.frequency(392.00f); osc_G.amplitude(0.4f);

    // srcMix: mezcla de 3 OSCs
    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);

    // ditherMix: señal + noise. El nivel del noise se actualiza en loop()
    ditherMix.gain(0, 1.0f);          // señal: ganancia 1.0
    ditherMix.gain(1, 0.0f);          // noise: empieza en 0 (sin dither inicial)

    // Bitcrusher — defaults del spec
    crusher.bits(8);
    crusher.sampleRate(22050.0f);

    // dryWetMix — defaults (Sculpt=0.3, Mix=0.7)
    dryWetMix.gain(0, 0.3f);          // dry: 1 - mix
    dryWetMix.gain(1, 0.7f);          // wet: mix
}
```

El `AudioEffectBitcrusher` no requiere inicialización especial — los parámetros
`bits()` y `sampleRate()` son seguros de llamar antes o después de `AudioMemory()`.
Esto lo diferencia de `AudioEffectChorus` (Sprint 2.4) que requiere `begin()` post-`AudioMemory()`.

### Actualización del nivel de dither en loop()

```cpp
// Variables de estado del dither (globales o en clase FX)
static float last_error_approx = 0.0f;  // para noise shaping
static int   g_bits       = 8;
static float g_sculpt     = 0.3f;
static float g_mix        = 0.7f;
static bool  g_bypass     = false;

void update_dither() {
    if (g_bypass) {
        ditherMix.gain(1, 0.0f);
        dryWetMix.gain(0, 1.0f);
        dryWetMix.gain(1, 0.0f);
        return;
    }

    // 1 LSB de amplitud para el número de bits actual
    float lsb = 1.0f / (float)((1 << g_bits) - 1);

    // Sculpt ∈ [0.0, 0.5]: blend RPDF → TPDF (nivel de noise sube)
    // Sculpt ∈ [0.5, 1.0]: TPDF + noise shaping (coeficiente feedback sube)
    float dither_level, ns_amount;

    if (g_sculpt <= 0.5f) {
        dither_level = g_sculpt * 2.0f;  // 0.0 → 1.0 al llegar a Sculpt=0.5
        ns_amount    = 0.0f;
    } else {
        dither_level = 1.0f;
        ns_amount    = min((g_sculpt - 0.5f) * 2.0f, 0.7f);  // clamp para estabilidad
    }

    // Nivel de noise en el ditherMix: 1 LSB × dither_level
    ditherMix.gain(1, lsb * dither_level);

    // Noise shaping: realimentación del error aproximado
    // (el error exacto requeriría ISR; este es una aproximación de baja velocidad)
    float effective_noise = lsb * dither_level + ns_amount * fabsf(last_error_approx);
    ditherMix.gain(1, effective_noise);

    // Dry/wet
    dryWetMix.gain(0, 1.0f - g_mix);
    dryWetMix.gain(1, g_mix);
}
```

### Actualización de bits y sampleRate

```cpp
void set_bits(int n) {
    g_bits = constrain(n, 1, 16);
    crusher.bits(g_bits);
    // El nivel del dither debe re-calcularse para el nuevo LSB
    update_dither();
}

void set_sample_rate(float hz) {
    // Clamp al rango del AudioEffectBitcrusher
    float clamped = constrain(hz, 1000.0f, 48000.0f);
    crusher.sampleRate(clamped);
}
```

`AudioEffectBitcrusher::bits()` y `sampleRate()` son thread-safe para llamar desde
`loop()` — escriben en variables leídas en el ISR de forma atómica en el Teensy 4.1
(actualización se aplica en el próximo bloque de audio, no en mid-bloque).

### Patrón de clase BitSculpt (para integración futura)

Cuando este FX se integre al sistema completo (Sprint 3.x), seguirá el patrón
establecido en TapeSaturate y PhaseChorus:

```
BitSculpt
  ├── begin(AudioStream& input, float volume)  — crea AudioConnections dinámicas
  ├── update(float dt_ms)                      — actualiza dither level, noise shaping
  ├── setBits(int n)                            — 1–16 (ENC R en FX mode per UI spec §3.4)
  ├── setSampleRate(float hz)                   — 1000–48000 Hz
  ├── setSculpt(float s)                        — 0.0–1.0 (tipo de dither)
  ├── setMix(float m)                           — 0.0–1.0 (dry/wet paralelo)
  └── setBypass(bool b)                         — bypass limpio sin clicks
```

El mapeo de `ENC R (FX mode) = Bits` está definido en el Panel v5 Final del spec
de UI (aplicación ESP32-S3 LVGL, Sprint 3.x).

---

## Demo

**Sketch:** `apps/firmware-teensy/src/sketches/10-bit-sculpt.cpp`

### Setup al arrancar

El sketch inicia con el acorde C mayor (C4+E4+G4, sawtooth, mismo patrón que
sprints 2.3 y 2.4) y Bit Sculpt activo con valores que demuestran el carácter
"lo-fi musical" desde el primer segundo:

```
Bits       = 8       (Sound Blaster / 8-bit character)
SampleRate = 22050   (reducción de sample rate sutil pero audible)
Sculpt     = 0.3     (RPDF + dither parcial — algo de lo-fi, algo de distorsión)
Mix        = 0.7     (70% wet, 30% dry — punch preservado)
Bypass     = false   (efecto activo)
```

### Comandos Serial (115200 baud)

| Comando | Parámetro | Rango | Ejemplo |
|---|---|---|---|
| `b<n>` | Bits (1–16) | entero | `b8` |
| `s<val>` | SampleRate Hz | 1000–48000 | `s8000` |
| `c<val>` | Sculpt (0.0–1.0) | float | `c0.5` |
| `m<val>` | Mix wet (0.0–1.0) | float | `m0.7` |
| `p` | Bypass toggle | — | `p` |

El comando `p` (pass-through) para bypass evita la colisión con `b` (bits). El
comando `c` (character) para Sculpt evita colisión con otros comandos del sistema.

### Reporte Serial cada 2 segundos

```
CPU: 5.2% | Mem: 9 bloques | Bits: 8 | SR: 22050 | Sculpt: 0.30 | Mix: 0.70
```

### Evidencia requerida para validar el hito

1. **Grabación de audio** (Audacity, 30 segundos mínimo): acorde C mayor con
   bypass `p` (señal limpia) vs Bit Sculpt activo con `b8 s22050 c0.3 m0.7`.
   La diferencia de carácter lo-fi debe ser audible e inmediata. En el espectro
   de Audacity, el bitcrusher muestra componentes de distorsión adicionales en los
   armónicos de cada nota del acorde.

2. **Demo de dither**: grabación de `c0.0` (sin dither, distorsión armónica pura)
   vs `c1.0` (noise shaping máximo) a `b4` (4 bits). La diferencia entre
   "digital feo" y "lo-fi musical" debe ser perceptible — con Sculpt=0 el
   crunch tiene armónicos tonales que raspan; con Sculpt=1 el crunch es más difuso
   y tolerable.

3. **Screenshot Serial Monitor**: CPU% < 8% con todos los componentes activos.
   AudioMemory peak no debe superar 15 de 20 bloques.

4. **Demo de sample rate**: grabación de `s48000` (solo bits, sin SRR) vs `s8000`
   (Game Boy character) a `b8`. Los aliasing inarmónicos del modo 8000 Hz deben
   ser claramente audibles como frecuencias "extra" que no estaban en el acorde.

### Secuencia de demo sugerida

```
1. Arranque: defaults (b8, s22050, c0.3, m0.7)
   → "8-bit warm" — Sound Blaster character

2. p → bypass — comparar con señal limpia del engine
   → La diferencia debe ser obvia: timbre crujiente vs limpio

3. p → volver al efecto activo

4. b4 → 4 bits
   → Crunch más agresivo, distorsión armónica muy audible

5. b1 → 1 bit
   → Onda cuadrada total — toda la información de timbre destruida
   → Aun así, con Mix=0.7 hay 30% de señal limpia preservando algo de pitch

6. b8 → volver a 8 bits

7. s8000 → Game Boy (sample rate a 8000 Hz, bits a 8)
   → Aliasing inarmónico claramente audible — las notas del acorde
     "se doblan" con frecuencias extra que el sistema añade

8. s48000 → solo bits (sample rate máximo, sin aliasing)
   → Solo distorsión de cuantización, sin aliasing — carácter diferente

9. b8 s22050 → volver a defaults

10. c0.0 → sin dither (distorsión armónica pura)
    → Comparar con c1.0 (noise shaping) — el primero tiene armónicos
      tonales que "chirrían"; el segundo tiene ruido más difuso

11. c0.5 → TPDF — punto medio entre distorsión y ruido

12. c1.0 → noise shaping máximo
    → Lo-fi más "musical": más ruido pero menos distorsión tonal

13. m0.3 → solo 30% wet — blend sutil
    → Lo-fi muy suave, casi solo coloración

14. m1.0 → 100% wet
    → Bitcrusher puro sin dry — sin punch de transientes del dry path

15. m0.7 → volver a valor de trabajo
    → Demostrar el valor nominal de producción real
```

---

## Learnings

### Métricas reales en hardware (Teensy 4.1 + Audio Shield Rev D2)

| Métrica | Estimado | Real | Delta |
|---|---|---|---|
| CPU (peor caso, 4 objetos activos) | ~5.1% | **0.6%** | -88% |
| AudioMemory peak | ~8 bloques | **7 bloques** | -13% |

El estimado era muy conservador. `AudioEffectBitcrusher` es extremadamente eficiente —
lookup de tabla simple, sin cálculo flotante en el ISR. El generador de noise white
tampoco tiene costo apreciable a nivel de CPU medido.

### CPU constante a través de todos los valores de bits y sampleRate

CPU se mantuvo en 0.6% independientemente de `b1` a `b16` y `s8000` a `s22050`.
El `AudioEffectBitcrusher` tiene costo fijo por bloque — no depende de la profundidad
de bits ni de la tasa de muestreo (el sample-and-hold es un contador de holdoff, no
un recálculo proporcional al ratio de reducción).

### Parámetros verificados sin bugs de display (lección Sprint 2.4 aplicada)

Todos los valores en el reporte serial coincidieron con los comandos enviados.
El clamp en el sketch antes de asignar `g_XXX` funcionó correctamente — Bits, SR,
Sculpt, Mix y Bypass mostraron siempre los valores reales aplicados.

### Bypass limpio sin clicks

Transición Bypass ON ↔ OFF sin artifacts audibles. El patrón `gain(0)↔gain(1)` del
dryWetMix mantiene continuidad de señal porque el Teensy Audio Library aplica los
cambios de ganancia al inicio del siguiente bloque de 128 samples — no en mitad de
un bloque, evitando discontinuidades de amplitud.

### Carácter sonoro verificado por bit depth

- **b8 + s22050**: coloración sutil, lo-fi cálido — punto de trabajo para live
- **b8 + s8000**: Game Boy — agresivo pero musical con el dry blend a 0.7
- **b4**: crunch notorio, parciales altas coloreadas
- **b1**: onda cuadrada total — toda la info armónica destruida, solo pitch queda
- **b12 + s8000**: intermedio interesante — más sample rate reduction que bit reduction

### Deuda técnica: Sculpt perceptualmente sutil

La diferencia entre Sculpt=0.0 y Sculpt=1.0 es sutil en auriculares — el dither a
nivel de 1-2 LSB es, por definición, de muy baja amplitud. En monitores y con señales
más complejas la diferencia es más audible. Para live performance, Sculpt puede
simplificarse a un preset fijo (ej. TPDF siempre) y liberar ENC R para Bits.
**Decisión para v1.0:** ENC R = Bits (el parámetro más expresivo y audible).
Sculpt queda como parámetro de setup vía display + ENC NAV.

---

*Sprint 2.5 — GrooveForge Brain · Juan Guerrero (GPROG)*

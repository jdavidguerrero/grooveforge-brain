# Gemelo · Fase 1: núcleo DSP compartido y shim nativo de la Audio Library

> **Fecha:** 2026-10-29 · **Estado:** BORRADOR para revisión de Juan · **Rama:** `feat/gemelo-dsp`
> **Depende de:** ADR-006 (gemelo en software, fases 1–2 aceptadas), ADR-003 (filtro digital `MoogLadder4P`),
> theory/prophet-5-completo.md, theory/fidelidad-engines.md.
> **Decisión de Juan (2026-10-29):** usar un *shim* nativo de la Teensy Audio Library "si es posible". La prueba de
> §2.3 muestra que sí.

## 0. Qué resuelve este documento

El gemelo necesita que el **mismo código** que suena en la Teensy produzca audio en el Mac. Hay dos caminos, y este
documento usa los dos, cada uno para lo suyo:

| Camino | Qué es | Sirve para | No sirve para |
|---|---|---|---|
| **Shim** (§2) | Una Audio Library mínima de mentira para el Mac: el código del firmware compila **sin cambios** | Referencia golden "antes" del refactor; `gf-render` temprano | Plugin: la frecuencia queda fija al compilar y el bloque es de 128 muestras |
| **Núcleo** `libs/gf-dsp/` (§3) | El refactor del ADR-006: cada engine con `prepare(sample_rate, max_block)` / `process` | Plugin AU/VST3 (fase 3), frecuencia y bloque variables | — (es el destino) |

El shim **no reemplaza** el refactor del ADR. Lo vuelve más seguro, porque da la referencia contra la que se prueba, y
adelanta los renders mientras el refactor avanza engine por engine.

## 1. Inventario (2026-10-29, rama `feat/studio-engines-10x`)

### 1.1 Lo que ya está listo

- **`src/dsp/`: 20 núcleos en C++ puro, 1 865 líneas**, sin Arduino ni Audio Library, con tests nativos (154 tests en
  total): `polyblep_core`, `zdf_ladder`, `analog_env`, `sem_svf_core`, `juno_chorus_core`, `drive_core`, `tape_core`,
  `phaser_core`, `pingpong_delay_core`, `pump`, `output_stage_core`, `color_noise_core`, `voice_spread`, `lfo`, etc.
- **Los núcleos ya reciben la frecuencia como argumento** (`set_cutoff(hz, fs)`, `render(..., fs)`, `set_rate(fs)`).
  En código, la constante `AUDIO_SAMPLE_RATE_EXACT` solo aparece en 10 adaptadores de `src/audio/`, en
  `prophet_5.cpp`, en `dx7.cpp` y en `fx/granular_cloud.cpp`.
- `prophet_osc_core` y `dx7_core` ya son núcleos nativos.
- Presets GFP v5 y capas GFL v2: `preset_decode` / `layer_decode` ya compilan en nativo; `gf_presets.py` los lee en
  Python.
- La aleatoriedad es **determinista**: son LCG con semilla fija (PolyBLEP, color noise, `voice_spread`, el `_rng` del
  Prophet). La semilla depende del orden de construcción, que es fijo.

### 1.2 Lo que falta

| Grupo | Líneas | Veredicto |
|---|---|---|
| Control de los 6 engines (`engines/<x>.cpp/.h`: voces, glide, envolventes del filtro, LFO, `update()`) | ~3 100 | **Adaptar**: separar control + render por voz en un núcleo |
| `AudioMixer4` (en los 6 engines, el bus y la cadena SFX) | — | **Reemplazar** en el núcleo: una suma en float |
| `AudioSynthNoiseWhite` (Juno, OB-6), `AudioEffectMultiply` (ARP) | — | **Reemplazar**: LCG de PJRC portado, una multiplicación |
| `fx/phase_chorus` (`AudioEffectChorus` + `AudioEffectWaveshaper`), `fx/modal_reverb` (6 × `AudioFilterBiquad`), `plate_reverb` (`AudioEffectFreeverbStereo`) | ~950 | **Reemplazar** (solo para el render "con FX") |
| Pegamento GFP → engine del sketch 49 (`engine_patch`, `engine_osc`, `engine_filter`, `engine_lfo`, macros, unison, `layer_apply`, `mod_tick`) | ~600 | **Extraer** a lógica pura con test nativo |

### 1.3 Correcciones al ADR-006 (para que Juan decida si se corrige el ADR)

1. **La frecuencia de muestreo del Brain es 44 100 Hz, no ≈ 44 117,6 Hz.** `platformio.ini` no define
   `AUDIO_SAMPLE_RATE_EXACT`, así que vale 44100.0f (`cores/teensy4/AudioStream.h`). En la Teensy 4.x,
   `AudioOutputI2S::config_i2s` calcula el PLL4 **fraccional** a partir de esa constante, así que el reloj real queda
   en 44,1 kHz con error de partes por millón. Los 44 117,6 Hz son de la Teensy 3.x, cuyo reloj sale de dividir el de
   la CPU. Los comentarios de `audio_graph.h:84` y `audio_graph.cpp:36` arrastran el mismo error.
2. **"6 de 9 headers de engines heredan de AudioStream"**: en `engines/` heredan solo `Prophet5Osc` y `Dx7Voice`. Las
   clases de engine son **dueñas** de un grafo de objetos `AudioStream`, pero no heredan de él.
3. **"14 archivos con la constante"**: son 16 si se cuentan los comentarios; con uso en código, 13 (10 en `audio/`, 2
   en `engines/`, 1 en `fx/`).
4. **El ADR no menciona el pegamento del sketch 49 (§1.2).** Sin él, un render no suena como el preset en el Brain.

## 2. El shim nativo

### 2.1 Qué es

Unos headers `Arduino.h` y `Audio.h` propios, con un `.cpp`, que implementan **solo** lo que usan los engines. En el
build nativo reemplazan a Teensyduino. El código del firmware compila tal cual.

| Pieza | Comportamiento | Fuente de la semántica |
|---|---|---|
| `audio_block_t` | 128 muestras int16 con contador de referencias | `cores/teensy4/AudioStream.h` |
| `AudioStream` | `allocate`, `release`, `transmit`, `receiveReadOnly`, `receiveWritable` (copia si el bloque es compartido); `update_all()` recorre los objetos **en orden de construcción**, igual que la librería | ídem (`first_update` / `next_update`) |
| `AudioConnection` | Lista de destinos por salida; un bloque por entrada y por ciclo | ídem |
| `AudioMixer4` | Ganancia en Q16 (`gain × 65536`), producto con `>> 16` y saturación a int16, suma saturada; canal con ganancia 1 sin multiplicar | `libraries/Audio/mixer.cpp` (ruta `__ARM_ARCH_7EM__`) |
| `WAVEFORM_*` | Mismas constantes | `libraries/Audio/synth_waveform.h` |
| `millis()`, `micros()` | **Reloj virtual** que avanza el render (§4) | — |
| `__disable_irq`, `AudioNoInterrupts`, `DMAMEM` | No-ops: en el Mac no hay ISR | — |

La Audio Library es de licencia MIT (PJRC). Cuando se porte la aritmética de un objeto oficial, se cita el archivo de
origen en el código.

### 2.2 Qué no implementa (a propósito)

I2S, USB, SGTL5000 y todo lo que toca hardware. Los objetos oficiales de FX (`AudioEffectChorus`, `AudioFilterBiquad`,
Freeverb, `AudioEffectWaveshaper`) y `AudioSynthNoiseWhite` se agregan **cuando un engine o la cadena SFX los pida**,
portando la aritmética entera de PJRC para que el resultado coincida en el bit con la Teensy.

### 2.3 Prueba de viabilidad (2026-10-29, scratchpad, sin commit)

- `prophet_5.cpp`, `prophet_osc_core.cpp` y `moog_ladder.cpp` **sin modificar** + shim (~150 líneas) + un sumidero que
  captura `prophet.getOutput()`.
- Acorde C3–G3–C4, 1,2 s sostenido y 0,8 s de release, a 44,1 kHz, con el control a 1 kHz.
- Resultado: RMS 3 698 en el ataque, 2 772 en el sustain, 1 109 en el release y 6 al final (int16). Es la forma
  esperada de la envolvente.
- **Dos corridas dan el mismo WAV byte a byte** (sha256 idéntico): el render es reproducible.

### 2.4 Límite: la frecuencia se fija al compilar

Con el shim, `AUDIO_SAMPLE_RATE_EXACT` es un macro, así que la frecuencia de muestreo queda fija en cada binario.

- **A 44 100 Hz** el render es el del Brain: sirve como referencia golden.
- **A 48 000 Hz** (lo que pide `produccion/protocolo-captura.md`) se compila con
  `-DAUDIO_SAMPLE_RATE_EXACT=48000.0f`. No hay remuestreo, así que no aparecen artefactos de remuestreo. Riesgo: algo
  derivado en muestras fijas que no pase por el macro, por ejemplo el `_fs = 44100.0f` inicial de
  `pingpong_delay_core.h:85`, que `init()` sobrescribe. Se cubre con las pruebas de §5.3.

## 3. El núcleo `libs/gf-dsp/` (refactor del ADR-006)

### 3.1 Forma de cada engine

```cpp
class Prophet5Core {
public:
    void prepare(float sample_rate, int max_block);   // recalcula coeficientes; reserva buffers
    void note_on(uint8_t note, float velocity);
    void note_off(uint8_t note);
    void set_*(...);                                  // los mismos setters de hoy
    void control(float dt_ms);                        // lo que hoy hace update() con millis()
    void process(float* out, int n);                  // n ≤ max_block, en float, sin cuantizar
};
```

- La voz es la composición de núcleos que ya existen: `Prophet5OscCore` → `ZdfLadder` (con el drive y el recorte de
  `MoogLadder4P::processSample`) → `AnalogEnv` del VCA. El `AnalogEnv` del filtro va a tasa de control.
- En el firmware, `Prophet5` queda como **adaptador**. Su `update()` calcula `dt` con `millis()` y llama a
  `control(dt)`. Un único objeto `AudioStream` llama a `process()` y convierte a int16 una sola vez, al final.
- `prepare` es el único lugar donde entra la frecuencia. Ningún archivo de `libs/gf-dsp/` incluye `Audio.h` ni
  `Arduino.h`; un test de build lo garantiza.

### 3.2 Lo que cambia en el sonido del Brain (y por qué es aceptable)

Hoy la voz se **cuantiza a int16 tres veces**: oscilador ×20 000 → int16, ladder → int16, VCA → int16. Después vienen
los mixers en Q16 con saturación. El núcleo en float elimina esas cuantizaciones intermedias. Cada una suma ruido del
orden de 1 LSB, ≈ −98 dBFS: el ruido de cuantización de N bits está a 6,02·N + 1,76 dB del pleno (Widrow & Kollár
2008). La diferencia queda por debajo de lo audible, pero **no es bit a bit**, así que la prueba golden es con
tolerancia (§5.2).

La **saturación** de los mixers sí puede oírse si un acorde de 5 voces se pasa de escala. El núcleo la conserva: un
recorte a ±1 en el mismo punto (la suma final), no en cada etapa.

## 4. El reloj de control

En la Teensy, `loop()` llama a `engine.update()` cuando puede. `update()` lee `millis()` y avanza el glide, la
envolvente del filtro, el LFO y el wheel-mod con `dt` en milisegundos enteros. Eso es **asíncrono** respecto de los
bloques de audio (2,9 ms) y su ritmo depende de la carga de la UI.

En el render (shim y núcleo):

- un reloj virtual en microsegundos avanza con las muestras;
- antes de cada bloque se llama al control una vez por cada milisegundo transcurrido (`dt = 1`), como un `loop()` a
  1 kHz sin carga;
- los eventos MIDI se aplican en el borde de bloque más cercano. Es la misma granularidad que en la Teensy, donde una
  nota que entra a mitad de bloque suena en el próximo `update_all()`.

El *jitter* del `loop()` real **no** se reproduce. Es la diferencia esperada entre el "Brain digital ideal" y el
hardware (ADR-006, fidelidad percibida).

## 5. Plan de pruebas

### 5.1 Shim (`pio test -e native`)

1. `AudioMixer4`: vectores contra la aritmética de `mixer.cpp` (ganancia 1, 0,25, negativa, saturación positiva y
   negativa).
2. Orden de `update_all` = orden de construcción; un bloque compartido por dos destinos se copia en
   `receiveWritable`.
3. Determinismo: dos renders del mismo caso dan el mismo sha256.

### 5.2 Golden del Prophet-5 (antes / después del refactor)

**Referencia "antes":** el código actual de la rama, sin modificar, renderizado con el shim a 44 100 Hz. Se guarda
como `.raw` int16 mono, 1,5 s por caso (~130 KB), en `apps/firmware-teensy/test/golden/prophet5/`, con un sha256 del
commit de origen.

| Caso | Qué ejercita |
|---|---|
| G1 acorde saw A+B | osciladores, ladder, VCA, mezcla de 3 voces |
| G2 pulse + PWM por LFO | `pw_a`/`pw_b`, `LfoMod` a tasa de control |
| G3 POLY-MOD → filtro y FREQ A | modulación a velocidad de audio, OSC B en LO FREQ |
| G4 glide + legato | `_currentHz`, `glide_k` con `dt` |
| G5 resonancia alta (k ≈ 4) | auto-oscilación del ZDF |
| G6 5 voces + robo de voz | asignación rotativa, `_stealOldestVoice`, saturación de la suma |

**Tolerancia "después"** (núcleo en float, cuantizado al final):
- residuo `después − antes` con RMS ≤ **−80 dB** respecto de la señal, y pico ≤ **−60 dBFS**;
- en G5, la auto-oscilación amplifica diferencias mínimas de fase. Ahí se compara la **envolvente** (RMS por ventanas
  de 10 ms, ±0,5 dB) y la **frecuencia del pico** del espectro (±1 cent), no la forma de onda.

Si un caso no pasa, el refactor cambió el sonido. Se corrige el núcleo; la tolerancia no se afloja.

### 5.3 Frecuencia de muestreo (núcleo a 48 kHz y shim compilado a 48 kHz)

- Una saw de A4 a 48 kHz: el pico espectral está en 440 Hz ± 1 cent.
- Un ataque de 10 ms y un release de 500 ms: el tiempo medido (10 % → 90 %) cae dentro del ±2 % del de 44,1 kHz.
- El cutoff del ladder en 1 kHz (resonancia 0): −3 dB a la misma frecuencia en Hz en las dos frecuencias de muestreo
  (±2 %).

### 5.4 Firmware (puerta del piloto)

- Los 154 tests nativos + los nuevos pasan.
- `pio run -e sketch` compila.
- **CPU on-device**, con el mismo acorde de 5 voces sostenido en el sketch 49, antes y después:
  `AudioProcessorUsageMax()` global y `processorUsageMax()` de los objetos del Prophet (PJRC), con un reset y una
  ventana de 10 s. **Puerta:** el después no supera al antes en más de 2 puntos de CPU, y el total sigue ≤ 60 %
  (`01` §5.2).
- **Juan escucha** el Brain con los presets de fábrica del Prophet antes y después (A/B a ciegas si se puede) y no
  distingue un cambio.

## 6. Orden propuesto (una cosa a la vez, cada paso con su puerta)

1. **Shim en el repo** (`libs/teensy-audio-shim/`) + tests de §5.1 + golden "antes" del Prophet (§5.2).
   *Puerta:* tests verdes y golden reproducible.
2. **Piloto Prophet-5 en `libs/gf-dsp/`** + adaptador en el firmware. *Puerta:* §5.2, §5.3 y §5.4.
3. **`gf-render` v0** (`[env:render]` de PlatformIO, programa nativo): GFP + MIDI → WAV 48 kHz / 24 bit + ficha.
   Requiere extraer el pegamento del sketch 49 para el Prophet. Sin cadena SFX, y la ficha lo dice
   (`fx: none`).
4. Resto de engines; cadena SFX (chorus, modal y plate, primero portados al shim y después como núcleos); plugin.

**Alternativa que adelanta audio:** con el shim, `gf-render` v0 puede ir **antes** del paso 2, renderizando el código
actual compilado a 48 kHz. Da audio para el dataset días antes. Costo: cuando el piloto pase, `gf-render` cambia de
camino (shim → núcleo), y los renders hechos antes se marcan en la ficha con `dsp: shim@<commit>`. El plan del ADR pone
la fase 2 después de la 1; **elegir el orden es decisión de Juan**.

## 7. Qué no cubre esta fase

- El codec SGTL5000 y la etapa de salida analógica (fase 4: validación contra el hardware).
- El add-on analógico 2N3904 (ADR-003).
- El groove (POCKET, arpegiador, humanizado): el MIDI se renderiza tal cual hasta que se extraiga `groove_tick`.
- Los FX de `fx/` fuera de la cadena del Studio.

## Referencias

- PJRC, Teensy Audio Library (MIT): `cores/teensy4/AudioStream.h` (bloques, orden de actualización,
  `AUDIO_SAMPLE_RATE_EXACT`), `libraries/Audio/mixer.cpp` (aritmética Q16 con saturación),
  `libraries/Audio/output_i2s.cpp` (`config_i2s`: PLL4 fraccional desde la constante), `AudioProcessorUsageMax`.
- Välimäki, V. y Huovilainen, A. "Antialiasing Oscillators in Subtractive Synthesis". *IEEE Signal Processing
  Magazine* 24(2), 2007: PolyBLEP; el residuo depende del incremento de fase, o sea de la frecuencia de muestreo.
- D'Angelo, S. y Välimäki, V. "An Improved Virtual Analog Model of the Moog Ladder Filter". ICASSP 2013; Zavalishin,
  V. *The Art of VA Filter Design*, rev. 2.1, 2018: el ZDF, cuyo coeficiente `g = tan(π·fc/fs)` es la dependencia
  explícita de `fs`.
- Widrow, B. y Kollár, I. *Quantization Noise*. Cambridge University Press, 2008: ruido de cuantización uniforme,
  6,02·N + 1,76 dB.
- Steinberg VST3 SDK (`setupProcessing` / `process`) y Apple AUv3 (`allocateRenderResources` / `internalRenderBlock`):
  frecuencia y bloque máximo conocidos antes de procesar, con bloque variable.

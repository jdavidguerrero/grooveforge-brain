# Skill: Teensy Audio Library

Referencia operacional para usar la Teensy Audio Library (PaulStoffregen/Audio) en el
proyecto GrooveForge Brain. Cross-ref: `apps/docs/05-fx-architecture.md`.

---

## Objetos principales

### Generadores (sources)

| Objeto | Uso en Brain | Notas |
|---|---|---|
| `AudioSynthWaveform` | VCO principal en todos los engines | Soporta SINE, SQUARE, SAWTOOTH, TRIANGLE, PULSE |
| `AudioSynthWaveformPWM` | LFO con drift en Phase Chorus y Tape Saturate | PWM width modulable en runtime |
| `AudioSynthWaveformDc` | CV offset / bias en engines | Output DC constante o ramped |
| `AudioSynthNoisePink` | Noise en Moog Model D | Pink noise (-3dB/oct) |
| `AudioSynthNoiseWhite` | Noise alternativo | Flat spectrum |
| `AudioSynthKarplusStrong` | Futuro (physical modeling) | String synthesis |

### Procesadores (insert)

| Objeto | FX del Brain | CPU est. |
|---|---|---|
| `AudioFilterStateVariable` | SVF LP/BP/HP simultáneo | ~3% |
| `AudioFilterBiquad` | Cymatic Resonator (4×BP resonante), Modal Reverb | ~2% cada uno |
| `AudioEffectEnvelope` | ADSR en todos los engines | ~1% |
| `AudioEffectChorus` | Phase Chorus base | ~4% |
| `AudioEffectDelay` | Ghost Echo (1.4s max), Glitch Stutter | ~6% |
| `AudioEffectGranular` | Granular Cloud | ~12% |
| `AudioEffectBitcrusher` | Bit Sculpt | ~3% |
| `AudioEffectFreeverb` | Spring + Plate (2 instancias) | ~5% cada una |
| `AudioEffectWaveshaper` | Tape Saturate (custom curve), Sub Genesis | ~3% |
| `AudioEffectPitchShift` | Pitch Mosaic v1.0 (2 instancias) | ~9% cada una |
| `AudioEffectRingMod` | ARP 2600 ring mod | ~2% |

### Analizadores

| Objeto | Uso |
|---|---|
| `AudioAnalyzeFFT1024` | Spectral Smear (resíntesis), Sub Genesis (pitch tracking) |
| `AudioAnalyzeNoteFrequency` | Pitch detection para TinyML Beat Follower |
| `AudioAnalyzePeak` | Level metering para UI display |
| `AudioAnalyzeRMS` | RMS level para Mix-Aware feature (Layer 3) |

### Mixers y routing

| Objeto | Uso |
|---|---|
| `AudioMixer4` | Mezcla de osciladores, parallel FX sends |
| `AudioAmplifier` | Gain control / VCA (controlar con `.gain()`) |

### I/O

| Objeto | Uso | Notas |
|---|---|---|
| `AudioOutputI2S` | Output principal → SGTL5000 DAC | Pin 22 (TX), 20 (LRCLK), 21 (BCLK) |
| `AudioInputI2S` | Input → SGTL5000 ADC (filter return) | Pin 13 (RX) |
| `AudioOutputUSB` | USB Audio out (composite device) | Requiere `USB_MIDI_AUDIO_SERIAL` |
| `AudioInputUSB` | USB Audio in desde DAW | Para DAW Bridge Layer 3 |
| `AudioControlSGTL5000` | Control del codec (volume, enable) | Via I2C1 pins 18/19 |

---

## Patrón de routing (declarativo)

```cpp
#include <Audio.h>
#include <Wire.h>

// 1. Declarar objetos (orden no importa)
AudioSynthWaveform       osc1, osc2, osc3;
AudioMixer4              osc_mixer;
AudioEffectEnvelope      env;
AudioAmplifier           vca;
AudioOutputI2S           audio_out;
AudioControlSGTL5000     codec;

// 2. Declarar conexiones (AudioConnection es RAII)
// AudioConnection(source, source_ch, dest, dest_ch)
AudioConnection c1(osc1, 0, osc_mixer, 0);
AudioConnection c2(osc2, 0, osc_mixer, 1);
AudioConnection c3(osc3, 0, osc_mixer, 2);
AudioConnection c4(osc_mixer, 0, env, 0);
AudioConnection c5(env, 0, vca, 0);
AudioConnection c6(vca, 0, audio_out, 0);  // L
AudioConnection c7(vca, 0, audio_out, 1);  // R

void setup() {
    AudioMemory(20);          // ← SIEMPRE primero en setup()
    codec.enable();
    codec.volume(0.7);
    osc1.begin(WAVEFORM_SAWTOOTH);
    osc1.frequency(440.0f);
    osc1.amplitude(0.6f);
}
```

---

## AudioMemory: cómo calcular el budget

Cada bloque de audio = 128 samples × 2 bytes = 256 bytes.

Regla: contar las conexiones activas simultáneas + buffer de seguridad.

```cpp
// Fórmula: (conexiones simultáneas × 2) + 8 buffer
// Para un engine Moog con 3 osc + mixer + env + vca + delay send:
AudioMemory(30);  // ~7.5KB — dentro del budget 400KB total

// Budget total project: 400KB / 256 bytes = ~1600 bloques máximo
// En la práctica: AudioMemory(400) = límite superior seguro
```

Verificar uso real en runtime:
```cpp
Serial.println(AudioMemoryUsageMax());  // pico desde el boot
```

---

## Gotchas críticos

### MCLK nativo del Teensy 4.1
El Teensy 4.1 genera MCLK internamente en el pin 23 — **no necesitás oscilador externo**.
La Teensy Audio Library lo configura automáticamente al incluir `<Audio.h>`.

### USB type: debe ser composite
En `platformio.ini`:
```ini
build_flags = -DUSB_MIDI_AUDIO_SERIAL
```
Sin esto: no hay USB Audio ni MIDI simultáneo. Es el tipo que habilita los 3 (Audio + MIDI + CDC).

### No usar `delay()` en el audio path
La Teensy Audio Library corre en una ISR de alta prioridad. `delay()` en el loop principal no
bloquea el audio (es safe), pero dentro de callbacks de audio o en código que se llama desde
la ISR de audio: **nunca `delay()`, nunca operaciones bloqueantes**.

### `AudioConnection` debe ser global (no local)
Si declarás una `AudioConnection` dentro de una función, se destruye al salir del scope y
la conexión se rompe. Siempre declaralas globales o en un struct de larga vida.

### Actualizar parámetros desde `loop()`
La Library es thread-safe para lectura de parámetros. Para cambios atómicos de múltiples
parámetros:
```cpp
AudioNoInterrupts();   // pausa audio ISR
osc1.frequency(note_freq);
osc1.amplitude(velocity_gain);
AudioInterrupts();     // reanuda
```

### `AudioEffectDelay` buffer máximo
El delay máximo soportado es ~1494ms @ 44100Hz. Para Ghost Echo esto es suficiente.
El buffer vive en la RAM del Teensy — reservar `AudioMemory` acorde.

---

## Herramienta de diseño visual

Usar el **Teensy Audio System Design Tool** (online) para diseñar el grafo de audio visualmente
y exportar el código de conexiones: https://www.pjrc.com/teensy/gui/

Útil para verificar routing complejo antes de escribir código.

---

## Referencias

- Teensy Audio Library docs: https://www.pjrc.com/teensy/td_libs_Audio.html
- Objeto index completo: https://www.pjrc.com/teensy/gui/index.html
- `apps/docs/05-fx-architecture.md` — CPU budget y stack técnico de cada FX
- `apps/docs/01-architecture.md` §4.1 — Synth engines y polyphony targets

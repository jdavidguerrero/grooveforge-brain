# Sprint 1.2 — Multi-Oscillator + ADSR

> **Fase:** 1 — Audio Core
> **Estimado:** 1-2 sesiones (~3-5h)
> **Status:** 🟢 Done
> **Refs:** `apps/docs/06-implementation-roadmap.md` §2 Sprint 1.2
> **Demo target:** ataque/decay/sustain/release audibles al disparar nota vía Serial

---

## Theory

### ADSR — el sobre de amplitud

Cuando tocás una nota en un piano físico, el sonido no aparece y desaparece abruptamente.
Tiene una forma en el tiempo: sube rápido cuando golpea el martillo, baja un poco mientras
sostenés la tecla, y se apaga gradualmente cuando la soltás. Esa forma se llama **envelope**
(sobre de amplitud), y el modelo más usado en síntesis es el **ADSR**:

```
Amplitud
  1.0 │         ╭─────╮
      │        ╱       ╲         sustain level
  0.7 │       ╱         ╲───────────────╮
      │      ╱                           ╲
  0.0 │─────╯                             ╰──────
      │← A →│← D →│←──── S ────╯← R →│
            Attack  Decay   Sustain   Release
            tiempo que   nivel       tiempo que
            tarda en     que         tarda en
            llegar a     mantiene    volver a 0
            máximo                  después del
                                    noteOff
```

| Etapa | Qué hace | Rango AudioEffectEnvelope |
|---|---|---|
| **Attack** | Tiempo desde noteOn hasta amplitud máxima (1.0) | 0 – 11.880 ms |
| **Decay** | Tiempo desde máximo hasta nivel de sustain | 0 – 11.880 ms |
| **Sustain** | Nivel de amplitud mientras se mantiene la nota | 0.0 – 1.0 |
| **Release** | Tiempo desde noteOff hasta silencio | 0 – 11.880 ms |

**Intuición musical:**
- Piano: attack corto (~5ms), decay largo, sustain bajo, release corto → percusivo
- Pad de cuerdas: attack largo (~500ms), decay corto, sustain alto, release largo → suave
- Bajo: attack corto, decay medio, sustain medio, release corto → punteado

### Por qué los Moog vintage suenan "fat" — el detune

Un solo oscilador perfecto suena "digital" — demasiado limpio, estático. Los sintetizadores
analógicos vintage nunca tienen dos osciladores afinados exactamente igual: la temperatura,
los componentes, el voltaje fluctúan. Esa **imperfección es el sonido**.

Cuando dos osciladores están ligeramente desafinados entre sí, sus ondas se suman e interfieren.
La interferencia produce un **beating** (batimiento): una variación periódica de amplitud a la
frecuencia diferencia entre los dos osciladores:

```
osc1 = 440 Hz
osc2 = 442 Hz
─────────────────────────────────────────────
Beating = |442 - 440| = 2 Hz → la amplitud
sube y baja 2 veces por segundo

Resultado percibido: el sonido "respira" —
un chorus/vibrato natural sin ningún efecto
```

Con 3 osciladores y detune escalonado:

```
osc1: 440 Hz  (referencia)
osc2: 442 Hz  (+2 Hz detune) → beating de 2 Hz con osc1
osc3: 220 Hz  (sub-octave)   → fundamenta el bajo, añade cuerpo
```

El osc3 a 220Hz (mitad de frecuencia = una octava abajo) engrosa el sonido hacia los graves
sin agregar complejidad armónica — es la técnica del **sub-bass** que usan todos los basses
analógicos.

### Armónicos — por qué cada forma de onda suena diferente

Una onda sinusoidal pura solo tiene la frecuencia fundamental. Las demás formas de onda
contienen **armónicos** — múltiplos enteros de la fundamental que le dan el timbre:

```
Fundamental: 440 Hz (1er armónico)
2° armónico: 880 Hz
3° armónico: 1320 Hz
...
```

**Sawtooth (diente de sierra):** contiene TODOS los armónicos (1°, 2°, 3°, 4°...).
Es la onda más rica en contenido armónico → brillante, agresiva. Es la base del sonido
Moog y de los leads de síntesis substractiva. Se llama "substractiva" porque el filtro
*resta* armónicos de esta onda rica.

```
Amplitud de armónicos (sawtooth):
1°  │████████████████████  (1/1)
2°  │██████████            (1/2)
3°  │███████               (1/3)
4°  │█████                 (1/4)
5°  │████                  (1/5)
... (cae a razón de 1/n)
```

**Square (cuadrada):** solo armónicos impares (1°, 3°, 5°, 7°...). Suena "hueca" —
como un clarinete o un oboe. Los sintetizadores Juno-106 y Prophet-5 la usan mucho.

**Triangle (triangular):** armónicos impares como la square, pero caen mucho más rápido
(1/n²). Casi sinusoidal — muy suave, sin brillo. Útil para bajos suaves o flautas.

**Para este sprint:** usamos sawtooth en los tres osciladores — es la onda más
característica del sonido Moog y la más interesante para escuchar el efecto del detune.

### El grafo de audio del sprint

```
osc1 (440Hz, saw) ──→ mixer (ch0, gain 0.5) ─┐
osc2 (442Hz, saw) ──→ mixer (ch1, gain 0.5)  ├→ envelope ──→ audioOut (L+R)
osc3 (220Hz, saw) ──→ mixer (ch2, gain 0.3) ─┘
```

`AudioMixer4` suma las señales de los 3 osciladores con gains independientes.
`AudioEffectEnvelope` aplica el ADSR a la mezcla — controla el volumen en el tiempo.

### AudioMemory para este sprint

```
Conexiones: osc1→mixer, osc2→mixer, osc3→mixer, mixer→env, env→outL, env→outR = 6
Bloques estimados: 6 × 2 + 4 buffer = 16 → AudioMemory(16)
```

### Referencias

- Gordon Reid, "Synth Secrets" (Sound On Sound) — partes 1-4: osciladores y formas de onda
- Bob Moog, "A Voltage-Controlled Low-Pass High-Pass Filter" (AES, 1965) — origen del detune intencional
- PJRC, `AudioEffectEnvelope` docs — https://www.pjrc.com/teensy/td_libs_Audio.html
- `apps/docs/01-architecture.md` §4.1 — polyphony targets (este sprint: 1 voz, monofónico)

---

## Wiring (Cableado)

N/A — sprint solo software. El hardware es idéntico al Sprint 1.1 (Teensy 4.1 +
Audio Shield Rev D2 en protoboard). No se agregan componentes. Ver wiring completo
en `01-hello-tone.md §Wiring`.

---

## Implementation

### Archivos

| Archivo | Descripción |
|---|---|
| `apps/firmware-teensy/src/sketches/02-multi-osc-adsr.cpp` | Sketch principal del sprint |
| `apps/firmware-teensy/platformio.ini` | Actualizar `build_src_filter` en `[env:sketch]` |

### Código

```cpp
// apps/firmware-teensy/src/sketches/02-multi-osc-adsr.cpp
// Sprint 1.2 — Multi-OSC + ADSR: 3 osciladores con detune + envelope ADSR.
// Theory, wiring y demo: apps/docs/sprints/02-multi-osc-adsr.md

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

AudioSynthWaveform    osc1;
AudioSynthWaveform    osc2;
AudioSynthWaveform    osc3;
AudioMixer4           mixer;
AudioEffectEnvelope   envelope;
AudioOutputI2S        audioOut;
AudioControlSGTL5000  codec;

AudioConnection c1(osc1,     0, mixer,    0);
AudioConnection c2(osc2,     0, mixer,    1);
AudioConnection c3(osc3,     0, mixer,    2);
AudioConnection c4(mixer,    0, envelope, 0);
AudioConnection c5(envelope, 0, audioOut, 0);  // L
AudioConnection c6(envelope, 0, audioOut, 1);  // R

// Parámetros — ajustables por Serial en runtime
float baseFreq   = 440.0f;   // A4 — referencia (ver Sprint 1.1: A4 como parámetro)
float detuneHz   = 2.0f;     // beating de 2Hz entre osc1 y osc2
float subLevel   = 0.3f;     // nivel del sub-octave en el mixer

void setup() {
    Serial.begin(115200);
    AudioMemory(16);

    codec.enable();
    codec.volume(0.5);

    // Formas de onda
    osc1.begin(WAVEFORM_SAWTOOTH);
    osc2.begin(WAVEFORM_SAWTOOTH);
    osc3.begin(WAVEFORM_SAWTOOTH);

    // Frecuencias: osc1 referencia, osc2 con detune, osc3 sub-octave
    osc1.frequency(baseFreq);
    osc2.frequency(baseFreq + detuneHz);
    osc3.frequency(baseFreq / 2.0f);

    // Amplitudes: osc1+osc2 al máximo — el envelope controla el volumen final
    osc1.amplitude(1.0f);
    osc2.amplitude(1.0f);
    osc3.amplitude(1.0f);

    // Mixer: osc1 y osc2 al 50% cada uno, sub al 30%
    // Suma máxima: 0.5 + 0.5 + 0.3 = 1.3 — leve saturación intencional (warm)
    mixer.gain(0, 0.5f);
    mixer.gain(1, 0.5f);
    mixer.gain(2, subLevel);
    mixer.gain(3, 0.0f);   // canal 4 sin usar

    // ADSR — sonido tipo pad suave para demostrar todas las etapas claramente
    envelope.attack(50.0f);    // 50ms — ataque suave, audible
    envelope.decay(200.0f);    // 200ms — decay notable
    envelope.sustain(0.7f);    // 70% — sustain alto
    envelope.release(300.0f);  // 300ms — release audible al soltar

    Serial.println("Sprint 1.2 — Multi-OSC + ADSR");
    Serial.println("Comandos: n=noteOn  f=noteOff");
    Serial.println("          a<ms>=attack  d<ms>=decay  s<0-100>=sustain  r<ms>=release");
    Serial.println("          t<hz>=detune  b<hz>=baseFreq");
}

void loop() {
    // Comandos Serial para control en runtime
    if (Serial.available()) {
        char cmd = Serial.read();
        float val = Serial.parseFloat();

        switch (cmd) {
            case 'n':
                envelope.noteOn();
                Serial.println("noteOn");
                break;
            case 'f':
                envelope.noteOff();
                Serial.println("noteOff");
                break;
            case 'a':
                envelope.attack(val);
                Serial.printf("attack: %.0f ms\n", val);
                break;
            case 'd':
                envelope.decay(val);
                Serial.printf("decay: %.0f ms\n", val);
                break;
            case 's':
                envelope.sustain(val / 100.0f);
                Serial.printf("sustain: %.0f%%\n", val);
                break;
            case 'r':
                envelope.release(val);
                Serial.printf("release: %.0f ms\n", val);
                break;
            case 't':
                detuneHz = val;
                osc2.frequency(baseFreq + detuneHz);
                Serial.printf("detune: %.1f Hz (beating: %.1f Hz)\n", detuneHz, detuneHz);
                break;
            case 'b':
                baseFreq = val;
                osc1.frequency(baseFreq);
                osc2.frequency(baseFreq + detuneHz);
                osc3.frequency(baseFreq / 2.0f);
                Serial.printf("baseFreq: %.1f Hz\n", baseFreq);
                break;
        }
    }

    // Métricas cada segundo
    static uint32_t last = 0;
    if (millis() - last > 1000) {
        Serial.printf("CPU: %.1f%% | Mem: %d blocks\n",
            AudioProcessorUsageMax(),
            AudioMemoryUsageMax());
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        last = millis();
    }
}
```

### Decisiones de implementación

- **Sawtooth para los 3 OSC:** onda más rica en armónicos — el efecto del detune y
  del envelope es más audible que con sine.
- **Detune 2Hz:** beating lento y claramente perceptible. Ajustable con `t<hz>` en runtime.
- **Sub-octave a 30%:** engrosa el bajo sin dominar la mezcla. Ajustable vía `subLevel`.
- **ADSR pad:** parámetros elegidos para que cada etapa sea claramente audible en el demo.
  Fácil de modificar por Serial para explorar sonidos percusivos o de lead.
- **Comandos Serial simples:** `n`/`f` para noteOn/Off + parámetros con prefijo de letra.
  Sin parser complejo — testing manual en este sprint. MIDI real llega en Sprint 1.5.

---

## Demo

### Qué valida este demo

Que el Teensy puede mezclar múltiples osciladores y controlar su amplitud en el tiempo
con un envelope ADSR. Es la base de cualquier voz de sintetizador — sin esto no hay
instrumento, solo tonos continuos.

### Cómo reproducirlo

```bash
cd apps/firmware-teensy
# Actualizar platformio.ini: build_src_filter = +<sketches/02-multi-osc-adsr.cpp>
~/.platformio/penv/bin/pio run -e sketch -t upload

# Serial monitor
~/.platformio/penv/bin/pio device monitor -b 115200
```

En el monitor serial:
```
n          → noteOn  (escuchás el ataque)
f          → noteOff (escuchás el release)
a10        → attack 10ms (más percusivo)
a500       → attack 500ms (más suave, tipo pad)
t5         → detune 5Hz (beating más rápido)
t0.5       → detune 0.5Hz (beating muy lento, casi imperceptible)
b220       → bajar una octava (A3)
```

### Evidencia a capturar

- [ ] Grabación de audio 10-20s mostrando: noteOn, sustain, noteOff con release audible
  ```
  # macOS: QuickTime → Nueva grabación de audio → "Teensy Audio"
  # Guardar: apps/docs/sprints/demos/02-multi-osc-adsr-demo.wav
  ```
- [ ] Screenshot del serial monitor con métricas CPU/memoria
- [ ] Demo de detune: `t0` (sin detune) vs `t3` (con detune) — diferencia tímbrica audible

### Criterios de pass

- [ ] 3 osciladores mezclados audibles, sin distorsión severa
- [ ] ADSR claramente perceptible: ataque, decay, sustain, release distinguibles
- [ ] Detune crea beating audible entre osc1 y osc2
- [ ] CPU < 10% (3 OSC + mixer + envelope — todavía muy por debajo del budget)
- [ ] `AudioMemoryUsageMax()` ≤ 12 blocks (dentro de AudioMemory(16))
- [ ] Comandos Serial funcionan en runtime sin reiniciar

---

## Tests

```bash
# Build limpio
cd apps/firmware-teensy
~/.platformio/penv/bin/pio run -e sketch 2>&1 | grep -E "warning:|error:" || echo "OK"
```

Tests unitarios formales para parámetros de envelope y mixer: Sprint 1.5
(cuando se formalice la clase `MoogModelD`).

---

## Learnings

### Qué salió diferente al plan

- **AudioMemoryUsageMax() = 3 blocks** (estimado 16, target ≤12): el pool de 16 bloques
  es muy generoso para este grafo. La Audio Library reutiliza bloques agresivamente cuando
  los consumidores los liberan rápido. Ajuste para sprints futuros: estimar por pico real,
  no por fórmula conservadora.
- **CPU 0.3%** (target <10%): 3 osciladores + mixer + envelope consumen menos de lo
  estimado. El Teensy 4.1 @ 600MHz tiene margen amplio incluso para polyphony de 6 voces.
- **Saturación intencional del mixer (gain suma 1.3):** audible como warm clipping suave —
  comportamiento esperado y deseable para el carácter analógico.

### Qué tomaría diferente

- El comando Serial es suficiente para testing manual, pero `Serial.parseFloat()` bloquea
  brevemente el loop esperando dígitos. No es problema ahora (1 voz), pero en Sprint 1.5
  con polyphony habría que usar un parser non-blocking.

### Dependencias para el siguiente sprint

- Sprint 1.3 es hardware (matching jig transistores) — no depende de este sketch.
- Sprint 1.4 (filter analógico) usa el output de audio de este sprint como señal de entrada.
- A4 como parámetro seleccionable en runtime: anotado en Sprint 1.1 learnings, pendiente
  para sprint de UI.

### Tiempo real vs estimado

- Estimado: 1-2 sesiones (~3-5h)
- Real: 1 sesión (~2h)
- Delta: -1h — grafo más directo de implementar de lo esperado

---

*Sprint 1.2 completado: 2026-05-16*
*Siguiente sprint: [03-matching-jig.md](03-matching-jig.md) — Matching Jig + Transistor Pairs*

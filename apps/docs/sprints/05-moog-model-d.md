# Sprint 1.5 — Moog Model D Skeleton

> **Fase:** 1 — Audio Core
> **Estimado:** 1-2 sesiones (~3-5h)
> **Status:** 🟢 Done
> **Refs:** `apps/docs/06-implementation-roadmap.md` §2 Sprint 1.5
> **Demo target:** Engine `MoogModelD` responde a comandos Serial noteOn/noteOff con
> VCO + VCF (digital placeholder) + VCA doble-envelope audibles

---

## Theory

### Arquitectura del Moog Minimoog Model D

El Minimoog Model D (1970) es el sintetizador más influyente de la historia. Su
arquitectura define lo que hoy llamamos síntesis "substractiva": generar una onda rica en
armónicos y luego moldearla con un filtro que resta frecuencias. A diferencia de un
instrumento acústico, donde el timbre es fijo por la física del material, en el Moog el
músico controla activamente qué armónicos sobreviven.

```
                 MOOG MINIMOOG MODEL D — SIGNAL FLOW
                 ─────────────────────────────────────

  ┌─────────┐  ┌─────────┐  ┌─────────┐
  │  VCO 1  │  │  VCO 2  │  │  VCO 3  │  (Voltage-Controlled Oscillators)
  │ sawtooth│  │ sawtooth│  │ sawtooth│
  │ 440 Hz  │  │ 440+det │  │ 220 Hz  │  VCO3: sub-octave, dobla el cuerpo grave
  └────┬────┘  └────┬────┘  └────┬────┘
       │            │            │
       └────────────┴────────────┘
                    │
               ┌────▼────┐
               │  MIXER  │  Mezcla VCOs + noise; gains independientes
               │ + NOISE │
               └────┬────┘
                    │
               ┌────▼────┐  ┌─────────────────┐
               │   VCF   │◄─│  Filter Envelope │  ADSR independiente
               │ 24dB/oct│  │  (ATTACK/DECAY/  │  modula el cutoff en el tiempo
               │ low-pass│  │  SUSTAIN/RELEASE)│
               └────┬────┘  └─────────────────┘
                    │
               ┌────▼────┐  ┌─────────────────┐
               │   VCA   │◄─│   VCA Envelope  │  ADSR separado
               │(volume) │  │  (ATTACK/DECAY/  │  controla la amplitud en el tiempo
               └────┬────┘  └─────────────────┘
                    │
               ┌────▼────┐
               │  AUDIO  │
               │   OUT   │
               └─────────┘

  VCO = Voltage-Controlled Oscillator
  VCF = Voltage-Controlled Filter (ladder 4-pole)
  VCA = Voltage-Controlled Amplifier
```

### Por qué 2 envelopes separados dan expresividad que un solo envelope no puede

La mayoría de los sintetizadores económicos de la era (y muchos actuales) usan un solo
envelope que controla tanto el filtro como el volumen. El Minimoog tiene dos envelopes
independientes — y esa es una de las razones principales de su expresividad.

Con un solo envelope, el timbre y el volumen siempre cambian juntos. Con dos:

```
  Escenario 1: VCA corto + Filter largo
  ──────────────────────────────────────
  VCA   │  ╭─╮            (nota corta, percusiva)
        │──╯  ╰──         attack 5ms, release 50ms
        └─────────────▶

  Filter│     ╭──────╮    (filtro se abre despuésdel pico)
        │────╱        ╲── attack 200ms, decay 800ms
        └─────────────▶

  Resultado: el sonido aparece y desaparece rápido
  pero el filtro sigue filtrando/abriendo → se escucha
  cómo el timbre evolucionó incluso en nota corta.
  Imposible con un solo envelope.

  Escenario 2: VCA largo + Filter corto
  ──────────────────────────────────────
  VCA   │  ╭──────────╮   (nota larga, sostenida)
        │──╯            ╰─ attack 100ms, release 800ms
        └─────────────▶

  Filter│ ╭╮              (filter "pluck": se abre rápido y cierra)
        │─╯ ╲─────────── attack 2ms, decay 150ms, sustain 0%
        └─────────────▶

  Resultado: el timbre es brillante al inicio (pluck)
  y se vuelve oscuro enseguida, pero la nota sigue
  sonando. Crea el "snap" característico de bajos Moog.
```

Este diseño permite voicings que van desde cuerdas suaves (VCA y filter lentos) hasta
bajos con "snap" (filter muy rápido, VCA sostenido) — todo dentro del mismo instrumento.

### Glide (portamento): deslizamiento entre notas

En un instrumento acústico de cuerda o viento, es posible deslizarse suavemente de una
nota a otra en lugar de saltar abruptamente. Ese efecto se llama **portamento** (italiano:
"llevar") o **glide** en síntesis.

Musicalmente es fundamental para: bajos de funk, leads de synth, solos expresivos, y
todo lo que suene a Kraftwerk, Nile Rodgers, o Giorgio Moroder.

Matemáticamente es interpolación de frecuencia en el tiempo:

```
  noteOn(A4 = 440Hz) seguido de noteOn(E5 = 659Hz)
  con glide_ms = 200ms

  Cada vez que se llama update() (cada ~1ms):

    freq_step = (target_freq - current_freq) * dt_ms / glide_ms
    current_freq += freq_step

  Timeline:
  t=0ms:   current = 440.0 Hz  (A4 recién presionada)
  t=50ms:  current = 494.8 Hz  (interpolando...)
  t=100ms: current = 549.5 Hz  (mitad del camino)
  t=200ms: current = 659.0 Hz  (llegó a E5)
```

La interpolación es lineal en Hz en esta implementación — suficiente para un skeleton.
Una implementación más avanzada usaría interpolación exponencial (linear en V/oct) para
que el slide suene igual de largo en cualquier rango del teclado, pero la diferencia
perceptual es sutil en glides cortos (<500ms).

### Cross-modulation: feature futura (Sprint 1.x)

El Minimoog original permite rutear VCO3 como fuente de modulación de la frecuencia del
filtro (cutoff CV) y/o de la frecuencia de VCO1 (FM). Esto genera timbres complejos e
inarmónicos que van desde vibrato sutil hasta metalicos estridentes.

No se implementa en este sprint por dos razones:
1. El filtro analógico (Sprint 1.4) no está disponible — modular un filtro digital no
   tiene el mismo interés sonoro.
2. Cross-modulation requiere un VCO3 dedicado a la modulación, lo que rompe el rol de
   sub-octave que cumple ahora.

Se documenta aquí para el Sprint 2.x cuando el ladder analógico esté operativo.

### Digital filter como placeholder hasta Sprint 1.4

`AudioFilterStateVariable` de la Teensy Audio Library es un filtro de variable de estado
(SVF) de 2 polos (12dB/oct). No es un ladder Moog (4 polos, 24dB/oct, no-lineal).

Diferencias clave que se perderán hasta tener el ladder:
- Pendiente: 12dB/oct vs 24dB/oct — el Moog filtra mucho más agresivamente
- No-linealidad: el ladder satura suavemente a alta resonancia; el SVF es completamente
  lineal
- Auto-oscilación: el SVF puede auto-oscilar matemáticamente pero suena "digital"; el
  ladder oscila con la calidez de los transistores 2N3904

El SVF en modo LP es una aproximación útil para validar la arquitectura del engine
(routing, envelopes, glide, interfaz) mientras el hardware no está disponible.

### Por qué clase C++ en lugar de sketch

Los sprints 1.1 y 1.2 usaron sketches — archivos `.cpp` con `setup()` y `loop()` directos.
Un sketch mezcla lógica de audio con la interfaz serial y el hardware initialization, lo
que no escala. Para el sprint 1.5 formalizamos el engine como clase C++:

| Approach | Ventaja | Desventaja | Decisión |
|---|---|---|---|
| Sketch directo | Simple, rápido de prototipar | Estado global, no reutilizable, difícil polyphony | Sprint 1.1-1.3 |
| Clase C++ | Estado encapsulado, interfaz limpia, preparada para polyphony | Más código inicial | ✅ Sprint 1.5+ |
| Pure Data object | Portable entre plataformas | No compila en Teensy, latencia variable | ❌ Descartada |

La interfaz `noteOn(midi_note, velocity)` / `noteOff()` es el contrato que todos los
engines del Brain van a respetar — Juno-106, Prophet-5, etc. Definirlo ahora evita
refactoring en sprints futuros.

### Estimación de CPU

```
Objetos de audio activos en MoogModelD:
  AudioSynthWaveform × 3         → ~1.5% × 3 = 4.5%
  AudioSynthNoiseWhite × 1       → ~0.3%
  AudioMixer4 × 1                → ~0.5%
  AudioFilterStateVariable × 1   → ~1.5%
  AudioEffectEnvelope × 1        → ~0.5%
  AudioOutputI2S × 1             → ~1.0% (siempre activo)
  ──────────────────────────────
  Total estimado: ~8.3%

Budget disponible (uso normal ≤60%):  60%
Budget engines @ 6 voces:            30%
Este engine (1 voz monofónica):      ~8-10% ✅ muy por debajo
```

Fuente constraints: `apps/docs/01-architecture.md` §5.2, `apps/docs/05-fx-architecture.md` §2

### AudioMemory para este grafo

```
Conexiones: osc1→mixer, osc2→mixer, osc3→mixer, noise→mixer,
            mixer→filter, filter→vcaEnv, vcaEnv→outL, vcaEnv→outR = 8

Estimación de bloques: 8 conexiones × ~1.5 bloques promedio = 12
Más buffer de seguridad: 8 bloques
Total: AudioMemory(20)  →  20 × 256 bytes = 5.12KB de 1MB disponible
```

### Referencias

- Gordon Reid, "Synth Secrets" (Sound On Sound) — Partes 1-8: osciladores, filtros, envelopes
- Bob Moog, "A Voltage-Controlled Low-Pass High-Pass Filter" (AES Journal, 1965) — arquitectura ladder original
- David Biro, "The Moog Ladder Filter" (2012) — análisis de topología y comportamiento no-lineal
- PJRC, Teensy Audio Library docs — `AudioFilterStateVariable`, `AudioEffectEnvelope`: https://www.pjrc.com/teensy/td_libs_Audio.html
- `apps/docs/01-architecture.md` §4.1 — polyphony targets: Moog Model D 6 voces (este sprint: 1 voz)
- `apps/docs/06-implementation-roadmap.md` §2 Sprint 1.5 — spec del sprint

---

## Wiring (Cableado)

N/A — sprint solo software. El hardware es idéntico al Sprint 1.1 (Teensy 4.1 +
Audio Shield Rev D2 en protoboard). No se agregan componentes físicos.

El filtro analógico 2N3904 ladder (Sprint 1.4) está diferido — en este sprint el
routing analógico no existe y el VCF es el `AudioFilterStateVariable` digital.
Ver wiring completo en `01-hello-tone.md §Wiring`.

---

## Implementation

### Archivos creados / modificados

| Archivo | Descripción |
|---|---|
| `apps/firmware-teensy/src/engines/moog_model_d.h` | Header público de la clase — interfaz y audio objects |
| `apps/firmware-teensy/src/engines/moog_model_d.cpp` | Implementación completa |
| `apps/firmware-teensy/src/sketches/05-moog-model-d.cpp` | Sketch de test con comandos Serial |
| `apps/firmware-teensy/platformio.ini` | `[env:sketch]` apunta al nuevo sketch + engine |

### Grafo de audio

```
  _osc1 (SAWTOOTH, baseFreq) ──────────────→ _mixer (ch0, gain 0.45)
  _osc2 (SAWTOOTH, baseFreq × detune) ─────→ _mixer (ch1, gain 0.45)
  _osc3 (SAWTOOTH, baseFreq / 2) ──────────→ _mixer (ch2, gain 0.30)   sub-octave
  _noise (white) ───────────────────────────→ _mixer (ch3, gain 0.00)   off por defecto

  _mixer → _filter (AudioFilterStateVariable, LP)
                    ↑
                    │ frequency controlada por software envelope en update()
                    │ TODO Sprint 1.4: reemplazar con routing SGTL5000 ADC
                    │ cuando el ladder analógico esté disponible

  _filter → _vcaEnv (AudioEffectEnvelope)

  _vcaEnv → _out (ch0, L)
  _vcaEnv → _out (ch1, R)
```

### Constraints respetados

| Constraint | Valor target | Estimado | Fuente |
|---|---|---|---|
| CPU Teensy (este engine) | ≤60% total / ~30% engines | ~8-10% | `01-architecture.md` §5.2 |
| AudioMemory | ≤400KB | 20 bloques × 256B = 5.12KB | `04-ai-architecture.md` §1.2 |
| Latencia audio | <1ms | Determinístico (bare-metal) | `01-architecture.md` §5.2 |

### Decisiones de implementación

- **Filter envelope en software (no sample-accurate):** `AudioEffectEnvelope` se usa
  para el VCA. Para el filtro, un envelope en software en `update()` (llamado cada ~1ms
  desde `loop()`) es suficiente — la resolución temporal es imperceptible para oídos
  humanos por encima de ~5ms de granularidad. La alternativa (segundo `AudioEffectEnvelope`
  más una fuente DC y un pitch shifter para modular el filtro) es posible pero agrega
  complejidad de grafo innecesaria para un skeleton.

- **Interpolación de glide en Hz (lineal):** Matemáticamente más simple que V/oct.
  A diferencia de un sistema CV/gate analógico donde V/oct tiene sentido directo, el
  dominio digital trabaja en Hz. La diferencia perceptual es imperceptible para glides
  < 500ms, que cubre el 95% de uso musical.

- **`_osc3` a baseFreq/2 (sub-octave fija):** En el Minimoog original, VCO3 es un
  oscilador completamente independiente que el músico puede afinar libremente. Aquí lo
  fijamos a sub-octave por simplicidad. Se puede desbloquear con `setWaveform(2, ...)` y
  frecuencia manual cuando el engine evolucione.

- **`AudioMemory(20)` en `begin()`:** Se llama en el método de la clase, no en `setup()`
  global. Esto es válido porque `AudioMemory` simplemente llama `AudioStream::initialize_memory()`
  que puede ser invocada en cualquier momento antes de que empiece el audio routing.
  El sketch llama `engine.begin()` antes de cualquier otro init.

---

## Demo

### Qué valida este demo

Que la clase `MoogModelD` encapsula correctamente un engine de síntesis con:
- 3 VCOs con detune y sub-octave
- Doble envelope independiente (VCF + VCA)
- Glide funcional entre notas
- Interfaz limpia `noteOn`/`noteOff` lista para conectar a MIDI en sprints futuros

### Cómo reproducirlo

```bash
cd apps/firmware-teensy

# Build + flash
~/.platformio/penv/bin/pio run -e sketch -t upload

# Monitor serial
~/.platformio/penv/bin/pio device monitor -b 115200
```

En el monitor serial:
```
n69     → noteOn A4 (440Hz)
n60     → noteOn C4 (261Hz)
n81     → noteOn A5 (880Hz)
f       → noteOff

c800    → filter cutoff 800Hz
c4000   → filter cutoff 4000Hz (más brillante)
c200    → filter cutoff 200Hz  (oscuro, casi mudo)

g200    → glide 200ms (deslizamiento audible entre notas)
g0      → glide instantáneo (desactivado)

d10     → detune 10 cents (beating sutil)
d50     → detune 50 cents (beating agresivo)

v50     → VCA attack 50ms
v500    → VCA attack 500ms (pad suave)
```

### Evidencia a capturar

- [ ] Grabación de audio: noteOn A4 → noteOn E5 con glide 300ms → noteOff
  ```
  # macOS: QuickTime → Nueva grabación de audio → seleccionar "Teensy Audio"
  # Guardar: apps/docs/sprints/demos/05-moog-model-d-demo.wav
  ```
- [ ] Screenshot del serial monitor mostrando CPU% y memory blocks
- [ ] Demo de doble envelope: `c200` (cutoff bajo) + noteOn → filter env se abre audiblemente
- [ ] Demo de glide: `g300` + notas rápidas → deslizamiento entre notas

### Criterios de pass

- [ ] `noteOn`/`noteOff` produce audio con envelope de amplitud perceptible
- [ ] Filter envelope modula el timbre independientemente del VCA envelope
- [ ] Glide audible entre notas distintas con `g200` o mayor
- [ ] Detune crea beating perceptible con `d5` o mayor
- [ ] CPU < 15% reportado por `AudioProcessorUsageMax()`
- [ ] `AudioMemoryUsageMax()` ≤ 18 blocks (dentro de AudioMemory(20))
- [ ] Comandos Serial funcionan en runtime sin reiniciar

---

## Tests

```bash
# Build limpio — criterio mínimo de pass
cd apps/firmware-teensy
~/.platformio/penv/bin/pio run -e sketch 2>&1 | tail -5
```

Tests unitarios formales de la clase `MoogModelD` (parámetros dentro de rango, output
no NaN/Inf, polyphony sin crash) se agregan en Sprint 2.x cuando la clase esté
estabilizada. Ver `CLAUDE.md` §Testing para el plan completo.

---

## Learnings

### Qué salió diferente al plan

- **AudioMemory pico = 8 blocks** (estimado 20, target <20) ✅ — el pico de 8 ocurre
  después del noteOff mientras el VCA envelope drena. Durante noteOn activo: 4 blocks.
  La diferencia entre on/off refleja el pipeline del filter+envelope vaciándose.
  AudioMemory(20) tiene margen para ~2.5× más complejidad sin riesgo.
- **CPU 0.6%** con 3 VCO + filter + 2 envelopes — menos del 1% para una voz completa.
  Budget para 6 voces simultáneas: ~3.6% — muy por debajo del target de 30%.
- **Lista de inicialización del constructor** fue crítica — las AudioConnections deben
  construirse con referencias a objetos ya existentes. Sin la lista de inicialización,
  el grafo de audio no se registra correctamente en la Library.

### Qué tomaría diferente

- El filter envelope software tiene una simplificación en RELEASE: toma el nivel de
  sustain como punto de partida, no el nivel real al momento del noteOff. Si el
  noteOff ocurre durante ATTACK o DECAY, el release decae desde un nivel incorrecto.
  Solución: guardar snapshot de `_filterEnvLevel` al entrar a RELEASE. Pendiente para
  cuando el engine salga de "skeleton" a producción.

### Dependencias para el siguiente sprint

- Sprint 1.4 (filter analógico): cuando estén disponibles TL072 y CD4066, reemplazar
  `AudioFilterStateVariable` con routing DAC → ladder → ADC. El `_filterCutoff` pasará
  a controlar el exponential converter del circuito analógico.
- Sprint 2.1 (Juno-106): `MoogModelD` establece la API de referencia para todos los
  engines (`noteOn`/`noteOff`/`begin()`/`update()`). `JunoEngine` debe respetarla.

### Tiempo real vs estimado

- Estimado: 1-2 sesiones (~3-5h)
- Real: 1 sesión (~2h)
- Delta: -1h — la arquitectura de clase fue directa; el mayor tiempo fue el filter
  envelope software y verificar el orden de construcción de AudioConnections

---

*Sprint 1.5 completado: 2026-05-17*
*Siguiente sprint: Sprint 2.1 — Engine Juno-106*
*Siguiente sprint: [01-architecture-filter.md] Sprint 1.4 (retomar cuando lleguen TL072 + CD4066)*

# 📖 GrooveForge Brain — Implementation Roadmap v0.1 (Phase & Sprint Spec)

> **OpenSpec de implementación** — fases, sprints y hitos para construir el GrooveForge Brain desde cero hasta shipping v1.0.
> **Parent:** GFD v3.0 — Master Strategy & Spec (SSoT)
> **Sibling docs:** OpenSpec v0.3, AI Architecture v0.1, FX Architecture v0.1, Filter Design Spec v0.1, Bridge Protocol v0.1
> **Status:** v0.1 — Mayo 15, 2026
> **Owner:** Juan Guerrero (GPROG)
> **Methodology:** OpenSpec spec-first — cada fase es "vendible" o "demostrable" antes de pasar a la siguiente.

---

## 0. Filosofía de implementación

### 0.1 Principios rectores

1. **Educational-first**: cada implementación incluye explicación teórica + código + razón de las decisiones. El código no es solo "que funcione" — es "que entiendas por qué funciona".
2. **Spec-first**: ninguna feature se implementa sin spec técnico aprobado. El spec se escribe antes que el código.
3. **Phase gates**: cada fase tiene un hito demostrable y observable. Sin demo audible/visible no se pasa a la siguiente.
4. **Time budget honest**: Brain es prioridad #2 (GroovePilot es #1). 10-18h/sem realistas, no 40h.
5. **One thing at a time**: nunca implementar 2 features simultáneas. Una termina, se valida, se pasa a la siguiente.
6. **Vertical slices**: cada sprint entrega algo end-to-end (audio in → audio out), no "todo el backend antes que cualquier UI".

### 0.2 Mapeo a North Star (5 niveles)

| Fase | Nivel del Brain | Outcome |
|---|---|---|
| **Fase 0** | Foundation | Tooling, monorepo, skills, agents, CLAUDE.md |
| **Fase 1** | Nivel 0 — Audio core | Tono 440Hz → first engine → filter analog |
| **Fase 2** | Nivel 0 — Multi-engine + FX | 3 engines + 11 FX signature |
| **Fase 3** | Nivel 0 — UI + display | Encoders + buttons + display GC9A01 funcional |
| **Fase 4** | Nivel 1 — MIDI + TinyML | USB-A host + 3 TinyML models |
| **Fase 5** | Nivel 3 — WiFi + Cloud | ESP32-S3 con WiFi + cloud sync |
| **Fase 6** | Nivel 4 — DAW bridge | GroovePilot VST3 integration |
| **Fase 7** | PCB + manufactura | PCB v0.1 → prototype → batch 1 |
| **Fase 8** | Shipping | Pre-orders → ensamblaje → envios |

Nivel 2 (slaves via pogo) queda para post-launch v1.x.

---

## 1. Fase 0 — Foundation (Semana 1-2, Mayo 2026)

**Objetivo:** monorepo + tooling + agents + CLAUDE.md completos antes de escribir una sola línea de código de firmware.

### Sprint 0.1 — Setup monorepo

**Entregables:**
- [ ] Repo `grooveforge-brain/` en GitHub (private)
- [ ] Estructura Nx monorepo con apps:
  - `apps/firmware-teensy/` (PlatformIO C++)
  - `apps/firmware-esp32/` (PlatformIO C++)
  - `apps/bridge-protocol/` (shared C headers)
  - `apps/hardware/` (KiCad PCB schematics)
  - `apps/design/` (industrial design Figma files / 3D models)
  - `apps/docs/` (technical documentation Markdown)
  - `apps/training/` (Python TinyML model training)
- [ ] `CLAUDE.md` raiz con principios educational-first
- [ ] `.claude/agents/` con sub-agents especializados
- [ ] `.claude/skills/` con skills personalizados
- [ ] CI/CD GitHub Actions básico (firmware build + tests)

### Sprint 0.2 — Documentación y Specs

**Entregables:**
- [ ] `apps/docs/01-architecture.md` — traducción del OpenSpec v0.3
- [ ] `apps/docs/02-bridge-protocol.md` — protocolo Teensy-ESP32-VST3
- [ ] `apps/docs/03-filter-design.md` — filter discreto 2N3904
- [ ] `apps/docs/04-ai-architecture.md` — 3-layer AI spec
- [ ] `apps/docs/05-fx-architecture.md` — 12 signature effects
- [ ] `apps/docs/06-implementation-roadmap.md` — este doc

**Hito Fase 0:** Estructura del monorepo completa, agents funcionando, primer commit con "hello world" en firmware-teensy.

---

## 2. Fase 1 — Audio Core (Semana 3-6, Junio 2026)

**Objetivo:** del silencio al primer engine Moog Model D pasando por el filter analog discreto.

### Sprint 1.1 — Hello World Audio (1 sesión, ~3h)

**Tema:** I2S audio output con Teensy 4.1 + SGTL5000.

**Theory section a documentar:**
- Qué es I2S y por qué los chips de audio lo usan
- Cómo el Teensy genera MCLK nativo (vs Pi necesita oscilador externo)
- Audio block size, sample rate, AudioMemory
- Teensy Audio Library design (declarative connections)

**Implementation:**
- [ ] `apps/firmware-teensy/src/sketches/01-hello-tone.cpp`
- [ ] Test 440Hz sine wave output
- [ ] Documentation `docs/sprints/01-hello-tone.md`

**Demo audible:** tono 440Hz por jack del Audio Shield.

### Sprint 1.2 — Multi-Oscillator + Envelope (1-2 sesiones)

**Theory:**
- ADSR envelope: attack, decay, sustain, release
- Por qué los Moog vintage suenan "fat": detune entre osciladores
- Sub-octave generation
- Sawtooth vs square vs triangle waveforms

**Implementation:**
- [ ] `apps/firmware-teensy/src/sketches/02-multi-osc-adsr.cpp`
- [ ] 3 osciladores + ADSR + mixer
- [ ] Note on/off via Serial commands para testing

**Demo audible:** ataque/decay/sustain audible al disparar nota.

### Sprint 1.3 — Matching Jig + Transistor Pairs (1 sesión hardware)

**Theory:**
- Teoría diferencial pair (Q1/Q2 emisores cortos)
- Por qué ΔVbe matching afecta el sonido del filter
- Constant current source (fuente espejo)
- ADC measurement precision

**Implementation hardware:**
- [ ] Arduino jig con socket DIP/ZIF + LCD
- [ ] Código Arduino para medir Vbe at 100µA
- [ ] Procesar batch de 100 transistores 2N3904
- [ ] Resultado: 8 pairs ΔVbe < 2mV

**Demo visible:** batch de pairs etiquetados con valores Vbe.

### Sprint 1.4 — Filter Discreto Ladder Protoboard (2-3 sesiones hardware) ⏸ DEFERRED

> **Technical debt:** componentes TL072CP y CD4066 pendientes de conseguir.
> Re-medición de transistores con protocolo térmico correcto pendiente (ver Sprint 1.3 learnings).
> Se retoma después de Sprint 1.5+. No bloquea el avance — el engine Moog Model D
> funciona sin el filter físico; se agrega el routing analógico cuando esté disponible.

**Theory:**
- Topología transistor ladder Moog 1970
- 4-pole low-pass (24dB/oct)
- Resonance feedback path
- V/oct exponential converter
- Cómo cada stage agrega un polo de 6dB/oct

**Implementation hardware:**
- [ ] Armado en protoboard del ladder con pairs matched
- [ ] Calibration 5 pasos (cutoff range, V/oct, resonance, bypass match, pass/fail)
- [ ] Test sweep 20Hz-20kHz audible
- [ ] Self-oscillation a 80% resonance pot

**Demo audible:** sweep del filter sobre el output del Teensy (Sprint 1.2).

### Sprint 1.5 — Engine Moog Model D Skeleton (1-2 sesiones)

**Theory:**
- Arquitectura Moog Minimoog: 3 VCO + Mixer + VCF (ladder) + VCA + 2 envelopes
- Por qué detune entre osc1/osc2 da "warmth"
- Cross-modulation y noise
- Glide (portamento)

**Implementation:**
- [ ] `apps/firmware-teensy/src/engines/moog_model_d.cpp` (clase completa)
- [ ] Parámetros user-controllable via Serial
- [ ] Audio routing through analog filter (audio in via ADC)

**Demo audible:** Moog Model D engine + filter discreto sonando juntos.

**HITO FASE 1**: Engine Moog Model D + filter analog discreto. Esto solo es ya un instrumento usable.

---

## 3. Fase 2 — Multi-Engine + FX (Semana 7-14, Jul-Ago 2026)

**Objetivo:** 3 engines completos + 11 efectos signature.

### Sprint 2.1 — Engine Juno-106 (2 sesiones) ✅

**Theory:**
- Roland Juno-106 arquitectura: 1 DCO + sub + chorus + VCF + VCA
- Digital-Controlled Oscillator (DCO) vs VCO
- Bucket Brigade Delay (BBD) chorus original
- Por qué los Juno suenan "hi-fi" vs los Moog "warm"

**Implementation:**
- [x] `apps/firmware-teensy/src/engines/juno_106.cpp`
- [x] Chorus modelado del BBD vintage
- [x] Engine switch via Serial command

**Demo audible:** A/B Moog vs Juno con misma nota.

### Sprint 2.2 — Engine Prophet-5 (2 sesiones) ✅

**Theory:**
- Sequential Circuits Prophet-5 arquitectura
- 2 oscillators with cross-modulation
- Curtis CEM3340 chip (original) y cómo emularlo digital
- Por qué el Prophet es el "sonido de los 80s"

**Implementation:**
- [x] `apps/firmware-teensy/src/engines/prophet_5.cpp`
- [x] Cross-modulation algorithm
- [x] Polyphony management (5 voices)

**Demo audible:** 3 engines disponibles, switch entre ellos.

### Sprint 2.3 — FX Tape Saturate (1 sesión) ✅

**Theory:**
- Saturación vs distorsión vs clipping
- Waveshaper transfer functions
- Wow/flutter en cinta magnetofonica
- Por qué el LFO caótico vs sinusoidal

**Implementation:**
- [x] `apps/firmware-teensy/src/fx/tape_saturate.cpp`
- [x] Custom waveshaper curve
- [x] LFO drift caotic generator

**Demo audible:** Engine sin/con tape saturate.
**Doc:** `apps/docs/sprints/08-tape-saturate.md`

### Sprint 2.4 — FX Phase Chorus (1 sesión) ✅

**Theory:**
- Chorus = delay corto modulado por LFO
- Bucket-Brigade Devices (BBD) y por qué "respiran"
- Multi-voice chorus stacking

**Implementation:**
- [x] `apps/firmware-teensy/src/fx/phase_chorus.cpp`
- [x] LFO con drift caótico (no perfecto sinusoidal)
- [x] Voice stacking 1-4 chorus voices

**Demo audible:** Engine + tape + chorus = signature warmth.
**Doc:** `apps/docs/sprints/09-phase-chorus.md`

### Sprint 2.5 — FX Bit Sculpt (1 sesión) ✅

> **Nota:** El roadmap original asignaba este slot a "Modal Reverb". El Modal Reverb se
> implementó como Sprint 2.7 (ver abajo). Bit Sculpt se adelantó como sprint
> independiente por su menor complejidad de señal y aporte inmediato al CPU budget
> del FX stack.

**Theory:**
- Bitcrusher: reducción de word-length y sample rate
- Dithering estratégico para preservar pitch en bit-depths bajos
- Por qué quantization noise tiene carácter musical vs ruido aleatorio

**Implementation:**
- [x] `apps/firmware-teensy/src/fx/bit_sculpt.cpp`
- [x] Custom dither algorithm sobre AudioEffectBitcrusher
- [x] Sample rate reduction independiente de bit-depth

**CPU medido:** 0.6%
**Demo audible:** Engine + Bit Sculpt en descenso 16→1 bit.
**Doc:** `apps/docs/sprints/10-bit-sculpt.md`

### Sprint 2.6 — FX Sub Genesis (1 sesión) ✅

> **Nota:** El roadmap original asignaba este slot a "Ghost Echo". Ghost Echo se
> implementa como Sprint 2.11 (versión simplificada sin Markov chain). Sub Genesis
> se adelantó porque el análisis de pitch es más directo con la Teensy Audio Library
> y completa el FX stack de "enriquecimiento" (sub + distorsión + coro).

**Theory:**
- Sub-octave synthesis: dividir la frecuencia fundamental por 2
- Pitch tracking con AudioFilterStateVariable LP para aislar fundamental
- Saturación analógica modelada para sub-bass con carácter

**Implementation:**
- [x] `apps/firmware-teensy/src/fx/sub_genesis.cpp`
- [x] Pitch tracking via LP filter + zero-crossing detection
- [x] AudioSynthWaveform (sub-octave generator) + waveshaper saturation

**CPU medido:** 0.8%
**Demo audible:** Engine + Sub Genesis — bass weight sin EQ surgery.
**Doc:** `apps/docs/sprints/11-sub-genesis.md`

### Sprint 2.7 — FX Modal Reverb (2 sesiones) ✅

> **SPRINT AGREGADO** respecto al roadmap original. El Modal Reverb es el
> diferenciador de producto más fuerte de la Fase 2 — "reverb de guadua colombiana"
> es un elemento de marketing y de identidad cultural que ningún competidor a $599
> puede replicar. Su complejidad (6 filtros bandpass paralelos + envelopes por modo)
> justifica un sprint dedicado. Este sprint hace que la Fase 2 entregue 8 FX en lugar
> de los 6-7 originalmente planeados — todos dentro del CPU budget documentado.

**Theory:**
- Síntesis modal: modos resonantes de objetos físicos (barras, placas, campanas)
- Ecuación de Euler-Bernoulli para barras libres — ratios β_nL inarmónicos
- Q del filtro vs T60 de decay: arquitectura dual para evitar instabilidad numérica
- 4 materiales: campana (Cu-Sn), guadua (Guadua angustifolia), cristal, madera

**Implementation:**
- [x] `apps/firmware-teensy/src/fx/modal_reverb.cpp`
- [x] 6× AudioFilterBiquad bandpass paralelos (Q=50)
- [x] Decay envelopes por modo en loop() (no audio thread)
- [x] 4 materiales con frecuencias y T60 físicamente fundamentados

**CPU medido:** 1.2%, Memoria: 5/11 bloques AudioMemory
**Demo audible:** Chord C mayor con reverb de guadua — carácter inconfundible.
**Doc:** `apps/docs/sprints/12-modal-reverb.md`

### Sprint 2.8 — FX Cymatic Resonator (1 sesión)

**Theory:**
- Patrones cimáticos (Chladni, 1787): superficies vibrando crean nodos y antinodos
- Por qué 4 BP filters en paralelo modelan modos resonantes del objeto virtual
- Harmonic series con skip del 4to parcial (ratios 1:2:3:5) para crear tensión armónica
- LFO lento (0.1–3Hz) modulando tune: sensación de cristal que respira

**Implementation:**
- [ ] `apps/firmware-teensy/src/fx/cymatic_resonator.cpp`
- [ ] 4× AudioFilterBiquad BP en paralelo, Q=10–80
- [ ] AudioMixer4 (paralelo) + dry/wet final
- [ ] LFO modulando ratios de frecuencia en update()

**Parámetros:** Tune [0.5-2.0], Density [1-4 modos], Resonance Q [10-80], LFO Rate [0.1-3.0 Hz], Mix, Bypass
**CPU estimado:** ~8%
**Doc:** `apps/docs/sprints/13-cymatic-resonator.md`

### Sprint 2.9 — FX Granular Cloud (1-2 sesiones)

**Theory:**
- Síntesis granular (Xenakis, 1960s; Roads, "Microsound", 2001): el sonido se construye de granos
- Umbral perceptual: ≥30 granos/seg = tono continuo; <30 = textura
- AudioEffectGranular: buffer int16_t de 12800 muestras = 267ms a 48kHz
- Pitch via setSpeed(ratio): 1.0=unison, 0.5=octava baja, 2.0=octava alta
- Modo freeze: snapshot del audio granulizado en loop (pad ambient independiente del input)

**Implementation:**
- [ ] `apps/firmware-teensy/src/fx/granular_cloud.cpp`
- [ ] AudioEffectGranular con buffer externo int16_t
- [ ] Grain Size [5-250 ms], Speed [0.25-4.0], Freeze toggle

**Parámetros:** Grain Size, Speed, Freeze, Mix, Bypass
**CPU estimado:** ~12%
**Doc:** `apps/docs/sprints/14-granular-cloud.md`

### Sprint 2.10 — FX Spring + Plate (1 sesión)

**Theory:**
- Spring reverb (Hammond, 1941): reflexiones físicas en resorte de metal — carácter "wobbly"
- Plate reverb (EMT 140, 1957): placa 2.4m × 1.4m con transductor piezo — densa y suave
- AudioEffectFreeverb: red Schroeder/Moorer de allpass + comb filters
- roomsize(0-1) → longitudes de delay de los combs; damping(0-1) → LP de feedback (HF decay rate)
- Diferencia audible: spring tiene "drip" en transientes, plate es uniforme

**Implementation:**
- [ ] `apps/firmware-teensy/src/fx/spring_plate.cpp`
- [ ] 2× AudioEffectFreeverb (spring + plate) en paralelo
- [ ] Crossfade matrix spring↔plate via _blendMix

**Parámetros:** Algorithm [spring/plate/blend], Room Size [0-1], Damping [0-1], Blend [0-1], Mix, Bypass
**CPU estimado:** ~10% (2× Freeverb ≈ 5% cada uno)
**Doc:** `apps/docs/sprints/15-spring-plate.md`

### Sprint 2.11 — FX Ghost Echo — v1.0 sin Markov (1 sesión)

**Theory:**
- Delay con feedback + tape color: cada repetición pasa por LP filter que atenúa HF
- Feedback loop válido en Teensy Audio Library: AudioEffectDelay introduce latencia de
  exactamente 1 bloque (128 samples = 2.67ms) — el feedback usa siempre samples de n-1 bloques,
  no hay dependencia circular verdadera
- AudioFilterStateVariable (port 0 = lowpass) con Q=0.7 (Butterworth) como LP de cinta
- Tape character: setTapeLP(hz) controla pérdida de HF por pasada — 500Hz = eco oscuro (cinta
  gastada), 12000Hz = eco brillante (cinta nueva)

**Markov chain deferred (v2.0):** la versión v1.0 es un delay clásico con tape color.
En v2.0 (Fase 4, Sprint 4.5+), la TinyML beat detection alimentará un Markov chain que
predice beats y hace que los ecos "anticipen" el groove — las repeticiones literales se
convierten en variaciones rítmicas coherentes. Este es el "Ghost" del nombre: los ecos
saben cuándo aparecer.

**Implementation:**
- [ ] `apps/firmware-teensy/src/fx/ghost_echo.cpp`
- [ ] AudioEffectDelay (hasta 1400ms) + AudioFilterStateVariable LP feedback
- [ ] 2× AudioMixer4 (feedback path + dry/wet)

**Parámetros:** Delay Time [10-1400 ms], Feedback [0-0.95], Tape LP [500-12000 Hz], Mix, Bypass
**CPU estimado:** ~6%
**Doc:** `apps/docs/sprints/16-ghost-echo.md`

**HITO FASE 2:** 3 engines + 11 FX signature ejecutándose. Esto es ya un producto demostrable.

---

## 4. Fase 3 — UI + Display (Semana 15-20, Sep-Oct 2026)

**Objetivo:** display GC9A01 + encoders + buttons + LEDs full UI funcional.

### Sprint 3.1 — ESP32-S3 setup + display LVGL (2 sesiones)

**Theory:**
- LVGL framework: por qué usándolo en lugar de drivers manuales
- Circular displays: mapeo de coordenadas circulares vs rectangulares
- Frame buffer management con poca RAM
- DMA SPI transfers

**Implementation:**
- [ ] `apps/firmware-esp32/src/display/main.cpp`
- [ ] LVGL initialized, first "hello" screen
- [ ] Boot animation

### Sprint 3.2 — UART Bridge Protocol Teensy ↔ ESP32 (1-2 sesiones)

**Theory:**
- Frame structure [CMD][LEN][SEQ][PAYLOAD][CRC8]
- Baud rate 921600: por qué este número
- CRC8 algorithm and why we need it
- Async vs sync communication patterns

**Implementation:**
- [ ] `apps/bridge-protocol/protocol.h` (shared header)
- [ ] `apps/firmware-teensy/src/bridge/uart_master.cpp`
- [ ] `apps/firmware-esp32/src/bridge/uart_slave.cpp`
- [ ] Test: Teensy sends "engine changed to Moog" → ESP32 updates display

### Sprint 3.3 — Encoders + Buttons UI (1-2 sesiones)

**Theory:**
- Encoder pulse decoding (quadrature)
- Button debouncing
- WS2812B protocol (1-wire timing critical)
- Por qué Teensy maneja UI mejor que ESP32 (real-time GPIO)

**Implementation:**
- [ ] `apps/firmware-teensy/src/ui/encoders.cpp`
- [ ] `apps/firmware-teensy/src/ui/buttons.cpp`
- [ ] `apps/firmware-teensy/src/ui/leds.cpp`
- [ ] Mapeo encoders → parameters de engine activo

**Demo audible+visible:** girar encoder → cutoff del filter cambia + display muestra valor.

### Sprint 3.4 — Full UI menu navigation (2 sesiones)

**Implementation:**
- [ ] Menu system en ESP32 (LVGL)
- [ ] Engine selection, FX selection, preset browser
- [ ] Visual feedback de parámetros

**HITO FASE 3:** Brain completamente operable con encoders + buttons + display. Sin computadora.

---

## 5. Fase 4 — MIDI + TinyML (Semana 21-28, Nov-Dec 2026)

**Objetivo:** USB-A host MIDI input + Layer 1 TinyML features.

### Sprint 4.1 — USB-A Host MIDI Input (1-2 sesiones)

**Theory:**
- USB host vs device modes
- USB Audio class vs USB MIDI class
- Teensy USBHost_t36 library
- MIDI message parsing (NoteOn, NoteOff, CC)

**Implementation:**
- [ ] `apps/firmware-teensy/src/usb/midi_host.cpp`
- [ ] Plug a USB MIDI keyboard, toca notas, escuchá engine

### Sprint 4.2 — TinyML Training Pipeline Setup (1 sesión)

**Theory:**
- TensorFlow vs PyTorch para este caso
- Quantization int8: por qué y cómo afecta accuracy
- TFLite Micro vs full TFLite
- Datasets para musica: Lakh MIDI, Maestro

**Implementation:**
- [ ] `apps/training/setup.py` (uv + dependencies)
- [ ] `apps/training/datasets/` (download scripts)
- [ ] First notebook: data exploration

### Sprint 4.3 — Key Detection Model (2 sesiones)

**Theory:**
- Music theory: key vs scale vs mode
- Pitch class distribution
- Chord profiles (Krumhansl-Schmuckler)
- Why ML beats rule-based for ambiguous cases

**Implementation:**
- [ ] `apps/training/models/key_detector.py`
- [ ] Train on MIDI dataset
- [ ] Quantize to TFLite Micro int8
- [ ] Bundle en firmware como C array
- [ ] `apps/firmware-teensy/src/ml/key_detector.cpp`

**Demo:** tocas 5-6 notas → display muestra "Em detected".

### Sprint 4.4 — Chord Recognition Model (2 sesiones)

**Theory:**
- Chord types: triads, 7ths, extended
- Why 3+ simultaneous notes
- Common chord progressions per genre

**Implementation:**
- [ ] `apps/training/models/chord_recognizer.py`
- [ ] Train + quantize + bundle
- [ ] `apps/firmware-teensy/src/ml/chord_recognizer.cpp`

**Demo:** tocás acorde con mano izquierda → display muestra "Em7".

### Sprint 4.5 — Beat Follower Model (1-2 sesiones)

**Theory:**
- Onset detection
- Tempo estimation (autocorrelation)
- Beat tracking vs tempo tracking

**Implementation:**
- [ ] `apps/training/models/beat_tracker.py`
- [ ] Quantize + bundle
- [ ] `apps/firmware-teensy/src/ml/beat_tracker.cpp`

**Demo:** tocás ritmo → display muestra BPM detectado en tiempo real.

**HITO FASE 4:** Brain Nivel 1 completo — teclado MIDI + 3 TinyML features instantáneos.

---

## 6. Fase 5 — WiFi + Cloud (Semana 29-32, Ene 2027)

**Objetivo:** ESP32-S3 conectado a GroovePilot cloud para Layer 2 AI.

### Sprint 5.1 — ESP32-S3 WiFi setup (1 sesión)

**Theory:**
- WiFi WPA2 connection flow
- mDNS discovery
- HTTPS client + certificate validation

**Implementation:**
- [ ] `apps/firmware-esp32/src/wifi/manager.cpp`
- [ ] WiFi config via web setup portal
- [ ] Cloud connectivity check

### Sprint 5.2 — GroovePilot Cloud API integration (2 sesiones)

**Theory:**
- REST vs WebSocket
- Authentication patterns
- Rate limiting

**Implementation:**
- [ ] `apps/firmware-esp32/src/cloud/client.cpp`
- [ ] Patch search endpoint
- [ ] Display search results en GC9A01

**Demo:** encoder + text input (via app or buttons) → patches encontrados.

### Sprint 5.3 — OTA updates (1-2 sesiones)

**Theory:**
- OTA partitions en ESP32
- Rollback strategies
- Firmware signing

**Implementation:**
- [ ] OTA update mechanism
- [ ] Update Teensy firmware via UART + bootloader

**HITO FASE 5:** Brain con WiFi funcional + cloud features.

---

## 7. Fase 6 — DAW Bridge (Semana 33-40, Feb-Mar 2027)

**Objetivo:** integración GroovePilot VST3 via USB-CDC.

### Sprint 6.1 — USB Audio + MIDI composite (1-2 sesiones)

**Implementation:**
- [ ] Teensy USB type configured for composite
- [ ] DAW reconoce Brain como audio + MIDI device

### Sprint 6.2 — GroovePilot VST3 communication (2-3 sesiones)

**Implementation:**
- [ ] CDC serial channel
- [ ] Bridge Protocol over CDC
- [ ] VST3 plugin updates con info del Brain

### Sprint 6.3 — Mix-Aware features (3-4 sesiones)

**Implementation:**
- [ ] VST3 envia mix analysis al Brain
- [ ] Brain ajusta automáticamente (cutoff, level)
- [ ] Frequency conflict detection on display

**HITO FASE 6:** Brain Nivel 4 funcional — integración profunda con Ableton + GroovePilot.

---

## 8. Fase 7 — PCB + Manufactura (Semana 41-48, Abr-May 2027)

### Sprint 7.1 — PCB v0.1 schematic completo en KiCad (2-3 sesiones)

### Sprint 7.2 — PCB v0.1 layout + routing (3-4 sesiones)

### Sprint 7.3 — Prototype fabrication + hand soldering (2 sesiones)

### Sprint 7.4 — Enclosure CNC + 3D printing (3-4 sesiones)

**HITO FASE 7:** 5 prototipos completos producidos.

---

## 9. Fase 8 — Shipping (Semana 49+, Junio 2027+)

### Sprint 8.1 — Beta testing program (10 units a beta testers PTDJA)

### Sprint 8.2 — Manufactura batch 1 (JLCPCB SMT + assembly)

### Sprint 8.3 — Pre-order fulfillment

---

## 10. Tracking & Metrics

### 10.1 Por sprint

- [ ] Theory document escrito antes del code
- [ ] Code review (con Claude Code) antes de merge
- [ ] Demo audible/visible/grabable
- [ ] Tests passing
- [ ] Documentation actualizado

### 10.2 Por fase

- Hito tangible (audio output, display showing data, working PCB, etc)
- Engineering log update con learnings
- Time spent vs estimated (track for future fase estimation)
- Decisions log con alternativas consideradas

## 11. Anti-patterns que vamos a evitar

- ❌ Implementar varias features en paralelo sin terminar ninguna
- ❌ Saltarse el spec y empezar a codear directo
- ❌ Olvidar el theory document (code without understanding)
- ❌ Optimización prematura ("esto podría ser más eficiente")
- ❌ Scope creep ("y si agregamos esto otro feature")
- ❌ No grabar demos audibles (no hay evidencia después de meses)

## 12. Living document

Este documento se actualiza después de cada sprint con:
- Status real (terminado, en progreso, blocked)
- Time spent vs estimate
- Learnings y decisions tomadas
- Re-prioritización si necesaria

---

*End of Implementation Roadmap v0.1*
*GrooveForge Brain · Spec-first development · Juan Guerrero (GPROG)*

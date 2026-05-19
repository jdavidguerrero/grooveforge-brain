# 📖 GrooveForge Brain — Implementation Roadmap v0.2 (Phase & Sprint Spec)

> **OpenSpec de implementación** — fases, sprints y hitos para construir el GrooveForge Brain desde cero hasta shipping v1.0.
> **Parent:** GFD v3.0 — Master Strategy & Spec (SSoT)
> **Sibling docs:** OpenSpec v0.3, AI Architecture v0.1, FX Architecture v0.1, Filter Design Spec v0.1, Bridge Protocol v0.1
> **Status:** v0.2 — Mayo 18, 2026
> **Owner:** Juan Guerrero (GPROG)
> **Methodology:** OpenSpec spec-first — cada fase es "vendible" o "demostrable" antes de pasar a la siguiente.

---

## Changelog v0.1 → v0.2 — Reprioritización AI-first

Este documento es *living* (`CLAUDE.md` §Jerarquía de autoridad: el roadmap es
ajustable sin violar `00–05`). La v0.2 reordena las fases para **priorizar AI/ML**:

- **Fases 0–4 cerradas** — audio core, 3 engines, 11 FX, UI/display, MIDI host y los
  3 modelos TinyML entrenados (key, chord, beat). Detalle archivado en `sprints/01–25`.
- **Nueva Fase 5 — AI Activation:** los 3 modelos hoy compilan pero **no corren** en
  el device (falta vendorear TFLite Micro). Esta fase los activa end-to-end. Sin ella,
  el "Nivel 1" no está realmente completo.
- **Nueva Fase 6 — Layer 1 AI completo:** los features de AI on-device restantes.
- **Cloud, DAW, PCB y Shipping se corren** a Fases 7–10 (eran 5–8 en v0.1).
- **Genre Fingerprint removido** de Layer 1 (era gimmick — el género es cultural, no
  acústico, y desde un synth solo hay poca señal). Su remanente útil — sugerir un pack
  de presets — se absorbe en Layer 2 cloud (Sprint 7.4).
- **Note Continuation movido a la nube** (Sprint 7.3): un modelo generativo *bueno* no
  cabe con calidad en el Teensy; la nube no tiene límite de tamaño de modelo.

**Tradeoff aceptado conscientemente:** shipping pasa de Fase 8 → Fase 10. La fecha de
venta se mueve meses. Mitigación: toda la AI se desarrolla en dev boards (Teensy 4.1 +
ESP32-S3) — el PCB es para *shipping*, no para *desarrollo*.

**Nota de SSoT:** este reorden NO contradice `00-master-strategy.md`. El North Star de
5 niveles sigue intacto — la v0.2 solo adelanta los niveles de AI (1 y 3) frente al
hardware. Una eventual revisión del North Star, si se decide, es cambio aparte.

---

## 0. Filosofía de implementación

### 0.1 Principios rectores

1. **Educational-first**: cada implementación incluye explicación teórica + código + razón de las decisiones. El código no es solo "que funcione" — es "que entiendas por qué funciona".
2. **Spec-first**: ninguna feature se implementa sin spec técnico aprobado. El spec se escribe antes que el código.
3. **Phase gates**: cada fase tiene un hito demostrable y observable. Sin demo audible/visible no se pasa a la siguiente.
4. **Time budget honest**: Brain es prioridad #2 (GroovePilot es #1). 10-18h/sem realistas, no 40h.
5. **One thing at a time**: nunca implementar 2 features simultáneas. Una termina, se valida, se pasa a la siguiente.
6. **Vertical slices**: cada sprint entrega algo end-to-end, no "todo el backend antes que cualquier UI".
7. **AI que cambia el sonido, no la pantalla** *(v0.2)*: un modelo que solo pone un label en el display es un bullet de spec-sheet. Un modelo que cambia lo que sale del parlante es un buy-reason. Priorizar el segundo.

### 0.2 Mapeo a North Star (5 niveles)

| Fase | Nivel del Brain | Outcome |
|---|---|---|
| **Fase 0** | Foundation | Tooling, monorepo, skills, agents, CLAUDE.md |
| **Fase 1** | Nivel 0 — Audio core | Tono 440Hz → first engine → filter analog |
| **Fase 2** | Nivel 0 — Multi-engine + FX | 3 engines + 11 FX signature |
| **Fase 3** | Nivel 0 — UI + display | Encoders + buttons + display GC9A01 funcional |
| **Fase 4** | Nivel 1 — MIDI + modelos | USB-A host + 3 modelos TinyML entrenados |
| **Fase 5** | Nivel 1 — AI Activation | Los modelos corren en el device y reaccionan |
| **Fase 5B** | Nivel 1 — Modo FX Processor | El Brain procesa audio externo (estilo RMX-1000) |
| **Fase 6** | Nivel 1 — Layer 1 completo | 6+ features AI on-device, offline |
| **Fase 7** | Nivel 3 — Layer 2 Cloud AI | ESP32-S3 con WiFi + cloud generativo |
| **Fase 8** | Nivel 4 — DAW bridge | GroovePilot VST3 integration |
| **Fase 9** | PCB + manufactura | PCB v0.1 → prototype → batch 1 |
| **Fase 10** | Shipping | Pre-orders → ensamblaje → envíos |

Nivel 2 (slaves via pogo) queda para post-launch v1.x.

---

## 1–4. Fases 0–4 — CERRADAS ✓

Detalle completo de cada sprint archivado en `apps/docs/sprints/`. Resumen:

### Fase 0 — Foundation ✓
Monorepo Nx, PlatformIO, agents, skills, CLAUDE.md, specs `00–06`, CI/CD.

### Fase 1 — Audio Core ✓
| Sprint | Entregable | Doc |
|---|---|---|
| 1.1 | Hello tone 440Hz I2S + SGTL5000 | `01-hello-tone.md` |
| 1.2 | Multi-oscilador + ADSR | `02-multi-osc-adsr.md` |
| 1.3 | Matching jig transistores 2N3904 | `03-matching-jig.md` |
| 1.4 | Filter ladder discreto | ⏸ DEFERRED — componentes pendientes (ver Fase 9) |
| 1.5 | Engine Moog Model D | `05-moog-model-d.md` |

### Fase 2 — Multi-Engine + FX ✓
3 engines (Moog Model D, Juno-106, Prophet-5) + 11 FX signature. Docs `06–16`.
Tape Saturate, Phase Chorus, Bit Sculpt, Sub Genesis, Modal Reverb, Cymatic Resonator,
Granular Cloud, Spring+Plate, Ghost Echo.

### Fase 3 — UI + Display ✓
| Sprint | Entregable | Doc |
|---|---|---|
| 3.1 | ESP32-S3 + display GC9A01 + LVGL | `17-esp32-display.md` |
| 3.2 | UART Bridge Protocol Teensy↔ESP32 | `19-bridge-protocol.md` |
| 3.3 | Encoders + buttons + LEDs (SW — HW pendiente cableado) | `20-encoders-buttons.md` |
| 3.4 | Carrusel 23 vistas LVGL | `18-display-ui-carousel.md` |

### Fase 4 — MIDI + modelos TinyML ✓
| Sprint | Entregable | Doc |
|---|---|---|
| 4.1 | USB-A host MIDI input | `21-usb-midi-host.md` |
| 4.2 | TinyML training pipeline (uv + TFLite) | `22-tinyml-pipeline.md` |
| 4.3 | Key Detection Model — 94% acc, 19KB int8 | `23-key-detector.md` |
| 4.4 | Chord Recognition Model — 61 clases, 23KB int8 | `24-chord-recognizer.md` |
| 4.5 | Beat Follower Model — BPM, 11KB int8 | `25-beat-follower.md` |

**Estado real:** los 3 modelos están entrenados y sus clases C++ existen, pero **no
corren en el Teensy** — falta el runtime TFLite Micro vendoreado. Eso es la Fase 5.

---

## 5. Fase 5 — AI Activation

**Objetivo:** los 3 modelos pasan de "compilan" a "corren en el device y reaccionan".
Es la fase que hace real el Nivel 1. Sin ella no hay AI demostrable.

**Por qué es una fase y no un sprint:** activar TFLite Micro, calcular features en
firmware, y conectar los resultados al audio y al display son 5 piezas verticales
distintas. Cada una tiene su demo.

### Sprint 5.1 — TFLite Micro vendoring + runtime en Teensy

**Theory:**
- Por qué `Arduino_TensorFlowLite` del registry NO sirve para Teensy (periféricos de
  Arduino Nano hardcodeados → no compila para Cortex-M7 Teensy)
- Generación del árbol mínimo de TFLite Micro (`create_tflm_tree.py` de Google)
- CMSIS-NN: kernels DSP optimizados para Cortex-M — por qué acelera int8 ~3-4×
- Tensor arena en `DMAMEM` vs `RAM1` — por qué no competir con el DMA de audio

**Implementation:**
- [ ] `apps/firmware-teensy/lib/tflite-micro/` — runtime vendoreado
- [ ] `platformio.ini` — include paths, `-DTF_LITE_STATIC_MEMORY`, CMSIS-NN
- [ ] Sketch que carga `key_detector` y corre 1 inferencia con histograma hardcodeado

**Demo:** el Teensy imprime por Serial el resultado de inferir un histograma de C mayor → `C maj`.

### Sprint 5.2 — Pitch class histogram en C++ + inference loop

**Theory:**
- El histograma en firmware — equivalente C++ de `midi_utils.get_pitch_class_histogram()`
- Ventana deslizante: acumular notas activas en una ventana de ~2s
- Pesado temporal: notas recientes pesan más vs ventana plana — tradeoffs
- Correr 3 modelos secuencialmente compartiendo un solo tensor arena (~56KB)

**Implementation:**
- [ ] `apps/firmware-teensy/src/ml/pitch_histogram.cpp` — acumulador
- [ ] `apps/firmware-teensy/src/ml/ml_engine.cpp` — orquestador de los 3 modelos
- [ ] Sketch: MIDI in → histograma → 3 inferencias → Serial

**Demo:** tocás teclado USB → Serial muestra key + chord + BPM en tiempo real.

### Sprint 5.3 — Scale Lock (Tier S)

**Theory:**
- Del key detectado a la escala: qué pitch classes pertenecen a Em, C maj, etc.
- Snap: nota fuera de escala → semitono más cercano dentro de escala
- Trigger por usuario (botón "Lock Scale") — el usuario confirma, esquiva la ambigüedad
- Latencia: el snap debe ocurrir antes del `noteOn()` al engine (<1ms)

**Implementation:**
- [ ] `apps/firmware-teensy/src/ml/scale_lock.cpp`
- [ ] Integración en el path MIDI → engine
- [ ] UI: botón de lock + LED feedback

**Demo audible:** con Scale Lock en Em, tocás cromático → todo suena en Em. Nadie toca mal.

### Sprint 5.4 — Beat-synced FX (Tier A)

**Theory:**
- Beat Follower → BPM → período en ms (`60000 / BPM`)
- Sincronizar Ghost Echo: delay time = negra o subdivisiones del beat
- Sincronizar Phase Chorus LFO al tempo
- Confidence gate: si confidence < umbral, NO sincronizar (un sync errado suena peor
  que ningún sync). Fallback a tap tempo manual.

**Implementation:**
- [ ] Conectar `BeatFollower` a `ghost_echo.cpp` y `phase_chorus.cpp`
- [ ] Confidence gating + tap tempo fallback

**Demo audible:** tocás en 120 BPM → el delay se engancha al tempo automáticamente.

### Sprint 5.5 — Display AI views vía Bridge Protocol

**Theory:**
- Resultados ML → frames Bridge Protocol → ESP32 → carrusel
- Comando: extender el protocolo con un `AI_RESULT` o reusar `PARAM_CHANGED`
- Throttling: enviar solo cambios significativos, no cada inferencia

**Implementation:**
- [ ] Teensy `bridge_master` envía resultados ML
- [ ] ESP32 `bridge_handlers` actualiza views 02 (synth main), 10 (ai processing), 16 (scale lock)

**Demo visible:** tocás → el display del ESP32 muestra key/chord/BPM en vivo.

**HITO FASE 5:** tocás el teclado → el Brain detecta tonalidad/acorde/tempo y **reacciona**
(sonido + display). Primer demo de AI real end-to-end. Nivel 1 verdaderamente activo.

---

## 5B. Fase 5B — Modo FX Processor

**Objetivo:** habilitar el segundo modo de uso del Brain — procesador de efectos
estilo Roland RMX-1000. Audio externo entra por el jack `FX IN`, el Teensy aplica
los 12 FX, y sale procesado. El filtro analógico 2N3904 queda fuera del path.

**Depende de hardware nuevo:** el 74HC4053 (switch de ruteo) y el jack `FX IN` 1/4"
TRS — ver `01-architecture.md §2.1` y `apps/docs/theory/audio-routing-dual-mode.md`.
Estos componentes deben sourcearse antes de cablear este modo. La Fase 5B arranca
después del sprint intermedio de integración (`sprints/29-hardware-integration.md`).

**Por qué después de Fase 5:** el modo FX necesita el audio path completo del
SGTL5000 (ADC + DAC) operando junto con los FX. Más fácil con el hardware ya
cableado y validado.

### Sprint 5B.1 — Audio Input Routing & Mode Switch

**Theory:**
- `AudioInputI2S` / `AudioOutputI2S` de Teensy Audio Library — singletons, un grafo
- Ruteo del SGTL5000: selección de fuente del ADC, control del 74HC4053 por GPIO 27
- Switch de modo Synth↔FX: doble-push ENC NAV → GPIO 27 + reconfiguración del grafo
- Bypass: `LINE_IN → DAC` directo (dry, wet=0)

**Implementation:**
- [ ] `apps/firmware-teensy/src/audio/mode_switch.cpp` — control GPIO 27 + estado de modo
- [ ] Sketch de prueba: audio in → out passthrough, verificar el jack FX IN

**Criterio de pass:** señal en el jack FX IN sale limpia por el output; el switch de
modo no genera pop audible.

### Sprint 5B.2 — FX Chain on Audio Input

**Theory:**
- Los 12 FX son source-agnostic (`05-fx-architecture.md §0.2`) — el mismo código DSP
  procesa audio del engine o del ADC
- Wet/dry mix, layers INSERT/SEND/MASTER aplicados al audio externo
- Mapeo de controles en Modo FX (`01-architecture.md §3.4`)

**Implementation:**
- [ ] Rutear `AudioInputI2S` → FX chain → `AudioOutputI2S`
- [ ] ENC L = dry/wet, ENC R = parámetro del FX activo

**Demo audible:** entra un loop de batería por FX IN → le aplicás reverb/delay/bitcrush
en vivo con los encoders.

### Sprint 5B.3 — Audio Onset Detection → Beat Follower

**Theory:**
- Onset detection desde audio: spectral flux del FFT / derivada del RMS
- Los onsets de audio alimentan el Beat Follower existente (mismo modelo, IOI histogram)
- Diferencia con el modo MIDI: en MIDI los onsets son NoteOn; en audio hay que detectarlos

**Implementation:**
- [ ] `apps/firmware-teensy/src/ml/audio_onset.cpp`
- [ ] FFT1024 → spectral flux → onset → `beat_follower.on_onset()`

**Demo audible:** entra un loop por FX IN → el beat-synced FX se sincroniza al BPM
del audio, sin MIDI.

### Sprint 5B.4 — ML FX Recommendation (Tier A)

**Theory:**
- Sistema de recomendación: lee el audio entrante, recomienda FX + parámetros
- Features de FFT: spectral centroid, RMS por banda, flatness, dominant frequency
- v1 reglas heurísticas (determinístico, sin training); v2 modelo TFLite entrenado
- Por qué reglas primero: validar el concepto sin training, baseline para el modelo

**Implementation:**
- [ ] `apps/firmware-teensy/src/ml/fx_recommender.cpp` — extracción de features + reglas
- [ ] (v2) `apps/training/models/fx_recommender/` — modelo TFLite

**Demo:** entra un pad → el Brain sugiere reverb oscuro. Entra percusión → sugiere
delay corto + beat-sync. La sugerencia aparece en el display.

**HITO FASE 5B:** el Brain funciona como procesador de FX — insertable en el
send/return de un mixer, con FX reactivos al audio que entra.

---

## 6. Fase 6 — Layer 1 AI completo

**Objetivo:** los features de AI on-device restantes — offline, instantáneos.

### Sprint 6.1 — Multi-task backbone refactor

**Theory:**
- Un modelo, múltiples cabezas: backbone compartido + heads para key / chord / mood
- Por qué: 1 inferencia en vez de N → menos CPU, menos arena, escala el *conteo de
  features* sin escalar el *costo*
- Entrenamiento multi-task: loss combinada, weighting de heads
- Tradeoff: acoplamiento — re-entrenar un head implica re-validar todos

**Implementation:**
- [ ] `apps/training/models/multitask/`
- [ ] Reemplaza `key_detector` + `chord_recognizer` por un modelo unificado

**Criterio de pass:** debe mostrar una **ganancia de CPU medible** vs los 2 modelos
separados; si no, no se mergea (no refactorizar por refactorizar).

### Sprint 6.2 — Auto-Harmonization (Tier S)

**Theory:**
- Armonización diatónica: dada key + nota → tercera/sexta diatónica
- Por qué es ~90% teoría musical (la tabla de intervalos es determinística) y dónde
  entra el ML — elegir el intervalo que mejor suena según contexto y densidad
- Voice leading básico (evitar saltos disonantes)

**Implementation:**
- [ ] `apps/firmware-teensy/src/ml/auto_harmonize.cpp`
- [ ] Genera la voz de armonía → segunda voz en el engine polifónico

**Demo audible:** tocás una nota → escuchás dos voces en armonía, siempre en escala.

### Sprint 6.3 — Smart Arpeggiator (Tier S)

**Theory:**
- Arpegiador clásico vs "smart": el patrón se adapta al acorde detectado
- Markov chain sobre transiciones de notas, condicionada a acorde + key
- Por qué Markov y no red neuronal: CPU-trivial, sin tensor arena, igualmente generativo
- Sync al Beat Follower — el arpegio corre al BPM detectado

**Implementation:**
- [ ] `apps/firmware-teensy/src/ml/smart_arp.cpp`
- [ ] Integración con chord recognizer + beat follower

**Demo audible:** mantenés un acorde → arpegio inteligente, en escala, en tempo.

### Sprint 6.4 — Groove Humanizer (Tier A)

**Theory:**
- Quantización rígida vs groove humano — micro-timing
- Perfil de desviaciones del usuario, extraído del trabajo de IOIs del Beat Follower
- Versión no-personalizada (jitter con buen gusto) vs personalizada (aprende del usuario)

**Implementation:**
- [ ] `apps/firmware-teensy/src/ml/groove.cpp`
- [ ] Aplica el perfil de groove al Smart Arpeggiator

**Demo audible:** A/B — arpegio robótico vs humanizado.

### Sprint 6.5 — Velocity Curve Learn (Tier B)

**Theory:**
- Curva de velocity: cómo el MIDI velocity (0-127) mapea a la dinámica del engine
- Calibración por histograma de velocity del usuario en el primer minuto de uso
- Por qué es estadística (calibración), no ML — y por qué eso está bien

**Implementation:**
- [ ] `apps/firmware-teensy/src/ml/velocity_learn.cpp`

**Demo:** dos usuarios con fuerza de toque distinta → el Brain se adapta a cada uno.

> **Genre Fingerprint:** removido de Layer 1 (ver Changelog). Su utilidad real —
> sugerir un pack de presets según estilo — se reubica en Layer 2 (Sprint 7.4), donde
> el análisis de estilo tiene más sentido junto a Community Patches y Genre Profiles.

**HITO FASE 6:** 6+ features de AI on-device funcionando, todas offline e instantáneas.

---

## 7. Fase 7 — Layer 2 Cloud AI (vía ESP32-S3 WiFi)

**Objetivo:** ESP32-S3 conectado a GroovePilot cloud para Layer 2 AI. La nube no tiene
límite de tamaño de modelo — acá viven las features generativas grandes.
**Referencia:** `04-ai-architecture.md §2`.

### Sprint 7.1 — ESP32-S3 WiFi setup

**Theory:** WiFi WPA2 connection flow, mDNS discovery, HTTPS client + certificados.

**Implementation:**
- [ ] `apps/firmware-esp32/src/wifi/manager.cpp`
- [ ] WiFi config via web setup portal
- [ ] Cloud connectivity check

### Sprint 7.2 — GroovePilot Cloud API integration

**Theory:** REST vs WebSocket, auth patterns, rate limiting.

**Implementation:**
- [ ] `apps/firmware-esp32/src/cloud/client.cpp`
- [ ] Patch Search NL (lenguaje natural) endpoint
- [ ] Progression Suggester endpoint
- [ ] Resultados en el display GC9A01

**Demo:** describís un sonido → 5 patches matching aparecen en el display.

### Sprint 7.3 — Cloud generative: Note Continuation + Style Transfer

**Theory:**
- Por qué un modelo generativo *bueno* va en la nube, no en el Teensy (tamaño)
- Note Continuation: melodía reciente → continuación sugerida (el Brain *propone*, no
  auto-toca — el usuario acepta)
- Style Transfer: preset/secuencia → versión modificada según estilo

**Implementation:**
- [ ] Endpoints cloud `/continue` y `/transfer/style`
- [ ] UI de aceptar/rechazar sugerencia generada

**Demo:** tocás 4 compases → la nube sugiere los siguientes 4, los aceptás o no.

### Sprint 7.4 — Preset Pack Suggester (Genre Fingerprint rescatado)

**Theory:**
- El remanente útil del Genre Fingerprint — sin el gimmick de "detectar tu género"
- Server-side: metadata de estilo (key, acorde, densidad, dinámica) → recomendar un
  pack de presets, no un label de género

**Implementation:**
- [ ] Endpoint `/suggest/packs`
- [ ] El Brain manda metadata (nunca audio) → recibe recomendación de pack

**Demo:** tras unos minutos tocando → el Brain sugiere "probá el pack Ambient Pads".

### Sprint 7.5 — OTA updates

**Theory:** OTA partitions en ESP32, rollback strategies, firmware signing.

**Implementation:**
- [ ] OTA update mechanism
- [ ] Update del firmware Teensy via UART + bootloader

**HITO FASE 7:** features AI de nube — generativas, ilimitadas en tamaño de modelo.
Layer 1 sigue 100% funcional sin WiFi (degradación offline limpia).

---

## 8. Fase 8 — Layer 3 DAW Bridge (GroovePilot VST3)

**Objetivo:** integración GroovePilot VST3 via USB-CDC. **Referencia:** `04-ai-architecture.md §3`.

### Sprint 8.1 — USB Audio + MIDI composite
- [ ] Teensy USB type composite — DAW reconoce Brain como audio + MIDI device

### Sprint 8.2 — GroovePilot VST3 communication
- [ ] CDC serial channel + Bridge Protocol over CDC
- [ ] VST3 plugin updates con info del Brain

### Sprint 8.3 — Mix-Aware features
- [ ] VST3 envía mix analysis al Brain
- [ ] Brain ajusta automáticamente (cutoff, level)
- [ ] Frequency conflict detection en el display

**HITO FASE 8:** Brain Nivel 4 — integración profunda con Ableton + GroovePilot.

---

## 9. Fase 9 — PCB + Manufactura

### Sprint 9.1 — Filter ladder discreto 2N3904 (era Sprint 1.4, deferred)
Retomado acá: armado del ladder con pairs matched, calibración 5 pasos, sweep
20Hz-20kHz, self-oscillation a 80% resonance. **Referencia:** `03-filter-design.md`.

### Sprint 9.2 — PCB v0.1 schematic completo en KiCad
### Sprint 9.3 — PCB v0.1 layout + routing
### Sprint 9.4 — Prototype fabrication + hand soldering
### Sprint 9.5 — Enclosure CNC + 3D printing

**HITO FASE 9:** 5 prototipos completos producidos.

---

## 10. Fase 10 — Shipping

### Sprint 10.1 — Beta testing program (10 units a beta testers PTDJA)
### Sprint 10.2 — Manufactura batch 1 (JLCPCB SMT + assembly)
### Sprint 10.3 — Pre-order fulfillment

---

## 11. Tier de features AI — referencia de priorización

Clasificación de los modelos por valor real (funcional × vendible). Guía qué se
marketea como hero feature y qué es soporte/enabler.

| Feature | Fase | Tier | Naturaleza | Nota |
|---|---|---|---|---|
| Scale Lock | 5.3 | **S** | ML (key) + reglas | Buy-reason. "No podés tocar mal." |
| Auto-Harmonization | 6.2 | **S** | ~90% teoría musical | Magia instantánea para no-músicos |
| Smart Arpeggiator | 6.3 | **S** | Markov chain | La gente busca arpegiadores |
| Beat-synced FX | 5.4 | **A** | ML (beat) | Vale si maneja FX, no el número solo |
| Groove Humanizer | 6.4 | **A** | ML + DSP | Feature de connoisseur |
| Chord Recognizer | 5.2 | **B** | ML | Enabler de otros, no hero |
| Velocity Curve Learn | 6.5 | **B** | Estadística | Polish invisible, table stakes |
| Note Continuation | 7.3 | cloud | Generativo | On-device era riesgoso → nube |
| ~~Genre Fingerprint~~ | — | C | — | Removido. Rescatado como 7.4 |

**Integridad de marketing:** de los features de arriba, varios son teoría musical /
estadística / Markov, no redes neuronales. Funcionan excelente — pero marketear un
*conteo* de "modelos de AI" invita escrutinio. Marketear **outcomes** ("nunca toques
mal", "armonía en tiempo real", "FX que siguen tu groove") bajo el paraguas
"AI-powered" es honesto y defendible.

---

## 12. Tracking & Metrics

### 12.1 Por sprint
- [ ] Theory document escrito antes del code
- [ ] Code review (con Claude Code) antes de merge
- [ ] Demo audible/visible/grabable
- [ ] Tests passing
- [ ] Documentation actualizado

### 12.2 Por fase
- Hito tangible (audio output, display showing data, working PCB, etc.)
- Engineering log update con learnings
- Time spent vs estimated
- Decisions log con alternativas consideradas

### 12.3 Métricas AI específicas (Fases 5-7)
- Inference latency on-device (target <20ms p99 — `04-ai-architecture.md §8`)
- Tensor arena footprint (target ≤200KB total — `04-ai-architecture.md §1.2`)
- Accuracy en música real vs sintética (medir el reality gap honestamente)
- CPU del path de audio NO degradado por la inferencia (audio <1ms sagrado)

---

## 13. Anti-patterns que vamos a evitar

- ❌ Implementar varias features en paralelo sin terminar ninguna
- ❌ Saltarse el spec y empezar a codear directo
- ❌ Olvidar el theory document (code without understanding)
- ❌ Optimización prematura
- ❌ Scope creep ("y si agregamos esta otra feature")
- ❌ No grabar demos audibles
- ❌ *(v0.2)* AI que solo cambia la pantalla y se vende como buy-reason
- ❌ *(v0.2)* Refactorizar a multi-task sin una ganancia de CPU medible
- ❌ *(v0.2)* Marketear un conteo de modelos en vez de outcomes

---

## 14. Living document

Este documento se actualiza después de cada sprint con:
- Status real (terminado, en progreso, blocked)
- Time spent vs estimate
- Learnings y decisions tomadas
- Re-prioritización si necesaria

---

*End of Implementation Roadmap v0.2*
*GrooveForge Brain · Spec-first development · Juan Guerrero (GPROG)*

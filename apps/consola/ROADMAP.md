# GroovePush — Roadmap de reconstrucción + contenido

_Estrategia: el prototipo fue desmontado. Reconstruimos **desde cero sobre protoboard**,
validando **cada subsistema** antes de pasar al siguiente. Cada bloque es a la vez un
**hito técnico** y un **episodio de contenido** (build + código explicado)._

> **Cómo leer esto**: los bloques son secuenciales (0 → 12). Cada bloque tiene _Objetivo_,
> _Tareas pequeñas_ (con agente responsable y cómo se **verifica en banco**), y una sección
> **🎬 Contenido** (tema a explicar + tipo de video + gancho). Trazabilidad a
> [`GAPS_AND_IMPROVEMENTS.md`](GAPS_AND_IMPROVEMENTS.md) y a los OpenSpecs (`specs/openspec/`).
>
> **Base de código existente**: aunque "empezamos de 0" para el contenido, reusamos los
> drivers y envs `test_*` que ya existen en `apps/processor` — cada test de bring-up es una
> toma de video lista.

---

## Fase A — Fundaciones y fuentes de verdad (P0)

### Block 0 — Banco de trabajo y fuente de verdad
**Objetivo**: dejar una sola verdad de pines/enlaces y un protoboard listo para validar.
**Depende de**: — · **Gaps**: G-P1, G-P2, G-P3, G-H5 · **Spec**: SPEC-PROC C-2/C-3

- [ ] T-B0-1 (firmware) — Reconciliar el mapa de encoders entre `PIN_DISTRIBUTION.md` y
  `Config.h`; elegir la verdad y actualizar ambos. **Verifica**: `grep` muestra un único
  mapa coherente; compila `pio run -e teensy41`.
- [ ] T-B0-2 (hardware) — Decidir NeoTrellis **I2C** vs **UART-M4** y borrar el camino
  perdedor en docs/`Config.h`/`hardware/`. **Verifica**: `hardware/README.md` y `Config.h`
  coinciden.
- [ ] T-B0-3 (firmware) — Unificar `include/Config.h` vs `include/shared/Config.h`.
  **Verifica**: un solo Config autoritativo; compila.
- [ ] T-B0-4 (industrial-designer) — Diagrama de cableado del protoboard (Teensy + rieles
  3V3/5V/GND). **Verifica**: foto del protoboard alimentado, LED de Teensy parpadea.

**🎬 Contenido — "Anatomía de un controlador MIDI DIY"**
- **Tema**: qué es el Teensy 4.1, buses (USB, I2C, UART, ADC), y por qué un mapa de pines
  limpio es la base de todo.
- **Video**: explicación de concepto + tour del protoboard y las fuentes de alimentación.
- **Gancho**: "Antes de soldar nada, esto es lo que tienes que decidir."
- **Entregable**: diagrama de bloques del sistema (reusar el de `README.md`).

---

## Fase B — MIDI, ADC y sensores (el corazón del contenido educativo)

### Block 1 — MIDI 101: tu primer note-on por USB
**Objetivo**: el Teensy enumera como dispositivo **USB-MIDI class-compliant** y un botón
manda una nota que Ableton escucha.
**Depende de**: B0 · **Gaps**: G-P5 · **Spec**: R-PROC-1

- [ ] T-B1-1 (firmware) — Env mínimo que envía `noteOn/noteOff` con un pulsador.
  **Verifica**: monitor MIDI de Ableton muestra la nota; latencia percibida < 5 ms.
- [ ] T-B1-2 (firmware) — Curva de velocity básica. **Verifica**: distinta velocity según
  fuerza/tiempo.
- [ ] T-B1-3 (qa) — Medir latencia pad→nota. **Verifica**: registro < 5 ms.

**🎬 Contenido — "¿Qué es USB-MIDI? De un botón a una nota en Ableton"**
- **Tema**: MIDI note-on/off, canales, velocity; USB class-compliant vs SysEx.
- **Video**: screencast de código (10-15 líneas) + demo en Ableton.
- **Gancho**: "El 'Hello World' de un instrumento MIDI."
- **Entregable**: gist del sketch mínimo.

### Block 2 — ADC y faders analógicos
**Objetivo**: 4 faders ALPS B50K en A0–A3 → CC, con suavizado y modo pickup.
**Depende de**: B1 · **Gaps**: G-P6 · **Spec**: R-PROC-2 · **Reusa**: `env:test_faders_teensy`, `lib/Faders`

- [ ] T-B2-1 (firmware) — Leer ADC 12-bit, escalar 0–127, `FADER_TOLERANCE`. **Verifica**:
  `pio run -e test_faders_teensy` + monitor imprime valores estables.
- [ ] T-B2-2 (firmware) — Enviar CC por USB-MIDI + modo pickup (`FADER_PICKUP_THRESHOLD`).
  **Verifica**: CC mapeado a un volumen en Ableton, sin saltos.
- [ ] T-B2-3 (qa) — Ruido/jitter del ADC. **Verifica**: valor quieto no oscila > tolerancia.

**🎬 Contenido — "ADC explicado: leyendo faders y mandando CC"**
- **Tema**: qué es un ADC, resolución (10 vs 12 bit), divisor resistivo del potenciómetro,
  suavizado, y "pickup mode" (por qué los faders motorizados no existen aquí).
- **Video**: construcción (soldar/cablear faders) + código del filtro + demo.
- **Gancho**: "Por qué tu fader tiembla y cómo arreglarlo."
- **Entregable**: gráfica antes/después del suavizado.

### Block 3 — Encoders rotatorios y cuadratura
**Objetivo**: 8 encoders → CC relativo; botón de encoder por MCP.
**Depende de**: B2 · **Gaps**: G-P1, G-P6 · **Spec**: R-PROC-2 · **Reusa**: `env:test_encoders_teensy`, `lib/Encoders`

- [ ] T-B3-1 (firmware) — Decodificar cuadratura A/B (lib Encoder). **Verifica**:
  `pio run -e test_encoders_teensy`, giro CW/CCW imprime +1/−1.
- [ ] T-B3-2 (firmware) — CC relativo (two's complement) mapeable en Live. **Verifica**:
  encoder mueve un parámetro de device en Ableton.

**🎬 Contenido — "Encoders y cuadratura: CC relativo vs absoluto"**
- **Tema**: señales A/B, sentido de giro, por qué "relativo" es clave para no saltar
  valores, aceleración.
- **Video**: código de decodificación + osciloscopio/logic-analyzer de las señales A/B.
- **Gancho**: "El truco que usan Push y todos los controladores serios."

### Block 4 — I2C y expansores GPIO (MCP23017)
**Objetivo**: switches de encoder (8, uno por encoder) + botones de transporte (4:
Play/Stop/Record/Loop, con 4 más ya cableados y libres) vía 2× MCP23017 (0x20/0x21).
**Depende de**: B3 · **Spec**: R-PROC-9 · **Reusa**: `env:test_mcp_encoder_buttons_teensy`, `env:test_mcp_extra_buttons_teensy`, `lib/ButtonManager`
**BOM confirmado**: 0 pulsadores extra para encoders (switch integrado EC11), **4 pulsadores
táctiles** para transporte. Asignación exacta en `apps/processor/PIN_MAP.md`.

- [x] T-B4-0 (firmware) — Reconciliar asignación de pines MCP: evitar GPA7/GPB7 (solo-salida,
  erratum), usar lib pins 0-6+8 por chip. **Verifica**: coincide con `hardware/processor.kicad_sch`
  (net lister); `NUM_ENCODER_BUTTONS` = 8. — hecho 2026-07-16 (gap G-P12).
- [ ] T-B4-1 (firmware) — Escanear MCP por I2C, debounce `BUTTON_DEBOUNCE_MS`. **Verifica**:
  `pio run -e test_mcp_extra_buttons_teensy`, cada botón imprime press/release limpio.
- [ ] T-B4-2 (hardware) — Pull-ups I2C 4.7k y direcciones A0/A1/A2. **Verifica**: `i2c scan`
  encuentra 0x20 y 0x21.

**🎬 Contenido — "I2C y expansores: más botones sin gastar pines"**
- **Tema**: bus I2C (SDA/SCL, direcciones, pull-ups), por qué GPA7/GPB7 son solo-salida,
  debounce por software.
- **Video**: construcción (MCP en protoboard) + escaneo I2C + código de matriz de botones.
- **Gancho**: "12 botones con solo 2 cables de datos."

### Block 5 — ❌ REMOVIDO del alcance de GroovePush (2026-07-16)
**Piezos e IR/theremin ya no son parte de este dispositivo.** Decisión del usuario: se
construirán como un **módulo e-drum separado** en el futuro (proyecto aparte, reusando
`lib/Piezo`/`lib/Theremin` como punto de partida cuando llegue ese momento). No se compran
piezos ni sensores IR para GroovePush; `Piezo.cpp`/`Theremin.cpp` quedan en el árbol pero
comentados en `Hardware.cpp` (gap G-P11, ver `GAPS_AND_IMPROVEMENTS.md`).

_Idea de contenido reutilizable para el futuro proyecto e-drum_: "Piezos y velocity: cómo
un pad siente la fuerza" — piezoeléctricos, pico de voltaje, ventana de detección,
protección (bleed + clamp); sensores IR de distancia como controlador gestual.

### Block 6 — El grid de pads (NeoTrellis) — protoboard; LEDs diferidas a PCB
**Objetivo (Fase 1, protoboard)**: grid 8×4 → notas vía I2C. **El feedback RGB propio del
grid del NeoTrellis** (cada pad ya trae su LED, viene con el módulo) sigue funcionando
normal. Lo que se difiere es la **tira WS2812B externa** (encoders/faders/botones) — los
LEDs que tienes son SMD, no aptos para protoboard; se monta cuando pasemos a PCB.
**Depende de**: B4 · **Gaps**: G-P2 (resuelto) · **Spec**: R-PROC-1

- [ ] T-B6-1 (firmware) — Leer pads NeoTrellis por I2C y emitir notas. **Verifica**: pad →
  nota en Ableton, 8×4 mapeado.
- [ ] T-B6-2 (firmware) — Feedback de color nativo del NeoTrellis por estado (clip
  vacío/cargado/reproduciendo). **Verifica**: color correcto por pad sin la tira externa.

**🎬 Contenido — "Grid de pads RGB: NeoTrellis por I2C"**
- **Tema**: matriz de pads, direccionamiento I2C del NeoTrellis (Seesaw), feedback RGB
  nativo por pad.
- **Video**: construcción del grid + código de color + trigger de clips.
- **Gancho**: "El grid que responde a lo que tocas."

> **T-B6-3 (diferido a PCB)** — Render de frames LED + level shifter para tira WS2812B
> externa (28 LEDs, encoders/botones/faders). Ver `hardware/leds.kicad_sch` (ya diseñado
> en Fase 3) — se monta cuando el PCB esté fabricado, no en protoboard.

---

## Fase C — Enlaces, GUI y arquitectura

### Block 7 — Enlace serial Teensy ↔ Raspberry Pi (JSON-lines)
**Objetivo**: bus UART con eventos `enc/btn/fader/pad` arriba y LED/modo abajo.
**Depende de**: B6 · **Gaps**: G-P4, G-P10 · **Spec**: R-PROC-3

- [ ] T-B7-1 (firmware) — Lector de líneas framed + emisor JSON. **Verifica**: RPi (o
  `screen`) recibe `{"ev":"enc","idx":3,"delta":-2}` bien formado.
- [ ] T-B7-2 (firmware) — Handshake + heartbeat/ping (reusar existente). **Verifica**:
  reconexión tras desconectar el cable.

**🎬 Contenido — "Hablar entre micro y Raspberry Pi: UART y protocolos"**
- **Tema**: UART vs USB, baud rate, framing, por qué JSON-lines, handshake/heartbeat.
- **Video**: código de ambos lados + demo de reconexión.
- **Gancho**: "Dos cerebros, un instrumento."

### Block 8 — GUI: Theme singleton + primera pantalla (Qt/QML en RPi 5)
**Objetivo**: montar Qt en la pantalla y el `Theme` con tokens correctos.
**Depende de**: B7 · **Gaps**: G-G1, G-G9 · **Spec**: R-GUI-1, R-GUI-2

- [ ] T-B8-1 (gui) — `Theme` singleton con teal `#00D4AA`, purple `#8B5CF6` (AI-only),
  `scale = Screen.width/800`, fuentes General Sans + JetBrains Mono en `.qrc`. **Verifica**:
  `grep` no encuentra hex/px inline; corre `appPushClone`.
- [ ] T-B8-2 (gui) — Chrome persistente: top bar + soft-key bar de 8 celdas. **Verifica**:
  las 8 celdas reflejan foco de encoder.

**🎬 Contenido — "Qt/QML en Raspberry Pi 5: la pantalla del instrumento"**
- **Tema**: por qué Qt/QML, kiosk/eglfs, design system (teal=audio, purple=IA), fuentes
  embebidas, `scale` para paneles distintos.
- **Video**: setup de la RPi + primera pantalla + el design system.
- **Gancho**: "De pantalla genérica a instrumento con marca."

### Block 9 — Daemon `gp-bridged` + bus ZeroMQ
**Objetivo**: daemon en la RPi dueño de enlaces; GUI como vista pura.
**Depende de**: B8 · **Gaps**: G-I2, G-G4 · **Spec**: R-PUSHCTRL-4, R-GUI-8

- [ ] T-B9-1 (integration) — Scaffold `apps/bridged` (Python 3.12) con bus ZeroMQ pub/sub +
  plugin/backend **fakes**. **Verifica**: pytest headless publica/consume mensajes.
- [ ] T-B9-2 (gui) — Cliente de bus en QML; quitar networking de la vista. **Verifica**:
  pantallas renderizan desde mensajes del bus (mockeado).

**🎬 Contenido — "Arquitectura limpia: separar UI de la red"**
- **Tema**: por qué un daemon, pub/sub, testear headless, degraded modes.
- **Video**: diagrama + pytest en verde + demo de desconexión → estado "offline".
- **Gancho**: "La regla que evita que tu UI se cuelgue."

---

## Fase D — Copilot de IA (features de producto, P2)

### Block 10 — Pairing + backend (mDNS, token, API key, quota)
**Objetivo**: emparejar con el plugin y consumir el backend con `gp_sk_`.
**Depende de**: B9 · **Gaps**: G-I1, G-I4, G-G8 · **Spec**: R-PUSHCTRL-1/5/9

- [ ] T-B10-1 (integration) — Descubrimiento Avahi + código de 6 dígitos + token; API key en
  archivo root `0600`. **Verifica**: cliente `nc` empareja y hace `hello`.
- [ ] T-B10-2 (gui) — Flujo de primer arranque (pairing) + chip de quota + manejo 429/403.
  **Verifica**: 429 muestra toast ámbar, no retry-loop.

**🎬 Contenido — "Emparejar hardware con seguridad: mDNS, tokens y claves"**
- **Tema**: descubrimiento sin IPs, pairing por código, almacenamiento seguro de secretos.
- **Video**: demo de emparejado + dónde NO poner la API key.

### Block 11 — Mix Score, spectrum y one-knob FIX
**Objetivo**: pantalla HOME con score en vivo + FIX con encoder de confianza.
**Depende de**: B10 · **Gaps**: G-G5, G-G6 · **Spec**: R-GUI-5, R-PUSHCTRL-7 (§4.1–4.2)

- [ ] T-B11-1 (gui) — `GpScoreRing` (Shape, count-up 1.5 s) + `GpSpectrum` (7 bandas, gradiente
  teal, `Behavior`). **Verifica**: score y spectrum animan desde `features_frame` a ≥60 fps.
- [ ] T-B11-2 (integration) — FIX: `tools/call` → sugerencia → encoder escala 0–120% →
  `apply_param_batch`. **Verifica**: aplicar mueve el parámetro en Live; UNDO revierte.

**🎬 Contenido — "Un instrumento que puntúa tu mezcla en vivo"**
- **Tema**: features de audio (LUFS, spectrum), el score, y el "one-knob fix".
- **Video**: demo de score subiendo al aplicar un fix + el sweep LED.
- **Gancho**: "El primer controlador con copiloto de mezcla."

### Block 12 — GEN (generación de patrones en el grid) + COPILOT/SOUND
**Objetivo**: generar patrones IA en el grid y escribirlos como clip; chat en pantalla.
**Depende de**: B11 · **Gaps**: G-G5, G-G7, G-P8 · **Spec**: R-GUI-6/7, R-PROC-5/6 (§4.3–4.6)

- [ ] T-B12-1 (firmware) — Secuenciador de audición esclavo de MIDI clock + edición de pasos
  con diff. **Verifica**: preview LED coincide con notas comiteadas.
- [ ] T-B12-2 (integration+gui) — `midi/generate` → preview ghost purple → COMMIT
  `write_midi_clip`; VARIATION (complement/variation/response). **Verifica**: clip aparece en
  Live; ghost purple → teal al comitear.
- [ ] T-B12-3 (gui) — COPILOT (chat SSE token-a-token) + SOUND (recetas). **Verifica**: texto
  fluye por SSE; 429 → toast.

**🎬 Contenido — "Generación de patrones con IA, directo al clip"**
- **Tema**: pad-grid como workstation, preview vs commit (purple→teal), chat en hardware.
- **Video**: generar unos drums, auditarlos y comitearlos a Ableton en vivo.
- **Gancho**: "El feature que ningún controlador comercial tiene."

---

## Cómo se conecta con el harness SDD
Cada bloque → `/spec <componente>` (si falta) → `/tasks <componente>` genera el
`*.tasks.md` con estas tareas y su verificación. Se codifica **una tarea a la vez**, se
**valida en banco**, se marca el ☐, y se graba el contenido de ese bloque.

## Sobre Notion
Este documento está estructurado para copiarse a Notion (cada Block = una página/toggle;
la sección **🎬 Contenido** = una base de datos "Calendario de contenido" con campos
_Tema · Formato · Estado · Video_). Si conectas el conector de Notion (requiere
autorización), puedo crear ahí la base de datos y las páginas por bloque.

## Tabla resumen (calendario de contenido)

| Block | Hito técnico | 🎬 Tema de contenido | Video |
|---|---|---|---|
| 0 | Fuente de verdad + banco | Anatomía de un controlador MIDI | concepto + tour |
| 1 | Primer USB-MIDI note | ¿Qué es USB-MIDI? | screencast + demo |
| 2 | ADC / faders → CC | ADC explicado | construcción + código |
| 3 | Encoders / cuadratura | CC relativo | código + logic analyzer |
| 4 | MCP23017 / I2C | Expansores GPIO | construcción + escaneo I2C |
| 5 | Piezo + IR | Velocity y sensores | construcción drum-pad |
| 6 | Grid NeoTrellis + WS2812B | Pads RGB y level-shift | construcción grid |
| 7 | UART Teensy↔RPi | Protocolos serie | código ambos lados |
| 8 | Qt/QML + Theme | La pantalla del instrumento | setup RPi + design system |
| 9 | gp-bridged + ZeroMQ | Arquitectura limpia | diagrama + pytest |
| 10 | Pairing + backend | mDNS/tokens/claves | demo emparejado |
| 11 | Score + spectrum + FIX | Copiloto de mezcla | demo score + fix |
| 12 | GEN + COPILOT + SOUND | Patrones IA al clip | demo generación |

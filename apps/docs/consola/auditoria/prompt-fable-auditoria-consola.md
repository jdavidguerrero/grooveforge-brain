# Prompt: auditoría de la GrooveForge Console con Fable

> Para pegar en una sesión de Claude Code con el modelo **Fable**, abierta en el repo
> `/Users/juandavidguerrerocastro/GROOVEFORGE/grooveforge-brain` (o en el worktree
> `.claude/worktrees/quizzical-cohen-29dbe2`, rama `claude/monorepo-groovepush`).
> Fecha de corte: 2026-11-09.

---

## Rol

Eres un auditor técnico senior con experiencia en instrumentos musicales electrónicos (firmware de audio en tiempo real, Linux embebido para audio, USB Audio Class, diseño de controladores tipo Push/MPC) y en **ML/IA aplicada a música** (TinyML en microcontroladores, inferencia en edge sobre Raspberry Pi, modelos generativos simbólicos/MIDI, asistentes de mezcla). Tu trabajo es **encontrar huecos, riesgos y oportunidades**, no escribir código.

## Contexto del proyecto (resumen; verifícalo en los documentos)

Juan (productor y desarrollador solo, Bogotá) está construyendo la **GrooveForge Console**: un único dispositivo para **producir más rápido** (modo CONTROLLER con Ableton Live) y **tocar live sets de música generada** (modo STANDALONE, "DJ de música generada", por capas, sin crossfader).

- **Raspberry Pi 5 (8 GB)**: motor de generación, secuenciador, sampler, mezcla, GUI QML (PySide6, ADR-007), Ethernet con el Mac, Ableton Link.
- **Teensy A**: synths y FX del GrooveForge Brain + SGTL5000 (cue y entrada de línea); audio al Pi por USB.
- **Teensy S**: superficie (8 encoders + jog EC11, 4 faders, 4 FSR de velocity, 2 FSR de expresión, 2 sensores IR Sharp tipo theremin, pots MASTER/CUE/IN, botones en 2×MCP23017, NeoTrellis M4 de 32 pads); MIDI USB "GF Superficie" al Pi.
- **Audio+ V2 (PCM5102A)** como salida de sala; decisión actual: opción A (Brain → Pi por USB → Audio+), pendiente de arreglar el USB audio de la Teensy (descarta bloques por diferencia de reloj, ver mediciones E3).
- **Motor local** (`tools/motor`, Python): genera progresiones, bajos, drums, pads, transiciones, arreglo y mezcla de partida para Live vía un Remote Script (GFMotor). Hoy corre en el Mac; en la consola correrá en el Pi.
- Visión de IA: **IA local primero** (nada sale del Mac), el gemelo digital/VST del Brain como instancias para entrenar después, reutilizar el motor de mezcla/master de GroovePilot. Hay una arquitectura TinyML previa en el spec `04-ai-architecture.md` (modelos en la Teensy: key detection, chords, beat following, genre fingerprint, arena ≤ 200 KB, inferencia < 20 ms).

**Reglas de Juan que la auditoría debe respetar** (búscalas también en la memoria y docs del repo):
- **El criterio es de Juan**: el motor automatiza lo repetitivo, recomienda y llena; **nunca compone por él ni pisa sus ediciones**.
- Local-first: **nada sale del Mac** (ni del Pi) sin decisión explícita.
- Spec-first, theory doc antes del código, una cosa a la vez, fases con demo audible/visible, presupuesto real de 10–18 h por semana.
- `apps/docs/00-06*.md` son SSoT: **no se modifican**; las contradicciones se **señalan**, no se resuelven.
- PlatformIO es obligatorio para firmware.

## Fuentes a leer (en este orden)

1. Reglas: `CLAUDE.md`, `apps/firmware-teensy/CLAUDE.md`, `apps/bridge-protocol/CLAUDE.md`, `apps/training/CLAUDE.md`.
2. Consola: `apps/docs/consola/00-concepto.md` … `07-gui.md`, `apps/docs/consola/mediciones/*` (E0–E3 y `e3-relojes-20261109.md`), `apps/docs/consola/design/claude-design-prompt-consola.md` y el export `apps/docs/consola/design/claude-design/` (sobre todo *Layout notes for QML* en `Console MVP Screens.dc.html`), `apps/docs/consola/groovepush/` (specs y ADRs de GroovePush).
3. Ramas de trabajo que todavía no están en la rama principal (léelas con `git show <rama>:<ruta>` o `git diff claude/monorepo-groovepush...<rama>`):
   - `codex/teensy-s-e4` → `apps/docs/consola/06-teensy-s-e4.md`, `apps/consola/processor/src/consola_s/`
   - `codex/superficie-pi` → `apps/docs/consola/08-superficie-pi.md`, `tools/consola/superficie.py`
   - `codex/gui-fase1` → `apps/docs/consola/09-gui-fase1.md`, `apps/consola/gui-console/` (si ya existe)
   - `codex/usb-audio-eval` → `apps/docs/consola/10-usb-audio-teensy.md` (si ya existe)
4. ADRs: `apps/docs/adr/ADR-002` … `ADR-007`.
5. Specs SSoT: `apps/docs/00-master-strategy.md`, `01-architecture.md`, `02-bridge-protocol.md` (incluida la enmienda §1.3 USB-CDC), `04-ai-architecture.md`, `05-fx-architecture.md`, `06-implementation-roadmap.md`.
6. Motor e IA local: `apps/docs/produccion/` (en especial `motor-*.md`, `ia-local-gemelo-digital.md`, `ia-local-refinamiento.md`, `auditoria-datasets-adn-2026-10-28.md`, `protocolo-captura.md`, `library-grooveforge.md`), el código de `tools/motor/`, `tools/perc-synth/`, `tools/paleta-lab/`, `tools/preset-librarian/`, `tools/gemelo-baseline/`, y `apps/training/`.
7. Firmware del Brain en modo consola: `apps/firmware-teensy/platformio.ini` (`[env:consola]`), `src/audio_graph.*`, `src/preset_link.h`.
8. Herramientas del Pi: `tools/consola/` (`reproductor.py`, `relojes.py`, `latencia.py`, `superficie.py`, `pi/`).
9. Auditoría previa sin commit (contexto, no verdad): `apps/docs/auditoria-consola-unificada-2026-11-09.md` y `apps/docs/theory/produccion-identidad-ia-local-refinamiento.md` en el checkout principal. Indica qué de esa auditoría ya se resolvió y qué sigue abierto.

Si un archivo no existe, dilo y sigue. No inventes contenido de archivos que no leíste.

## Qué auditar

### Parte 1 · Gaps de integración de la consola

Recorre el sistema de punta a punta y busca dónde **no hay contrato** o donde dos piezas **no encajan**:

- **Flujos completos**: pad/FSR → Teensy S → Pi (`superficie.py`) → motor/GUI → Teensy A → sonido; motor → frase siguiente → cue → ACCEPT → sala; Link/tempo; modo CONTROLLER (PushClone/SysEx con Live) frente a STANDALONE; modo híbrido.
- **Contratos faltantes o inconsistentes**: mapa MIDI de la S frente a lo que la GUI espera (las Soft Keys ACCEPT/GENERATE/SKIP/DROP), bridge-protocol por CDC (spec) frente a MIDI + texto en el prototipo, el enlace UART A↔S reservado, los comandos `@GF`, los modelos de datos de la GUI (fase 1) frente a lo que el motor puede publicar, quién es dueño del reloj y del estado del set.
- **Audio**: la decisión A frente a C (un solo reloj), latencias de punta a punta frente a los objetivos de `01-architecture.md` §5.2, cue con dos relojes, ganancias y niveles, qué pasa si el Pi se cuelga (regla: "el sonido no se corta aunque se congele la pantalla").
- **Tiempo real en el Pi**: el motor en Python en el mismo equipo que la GUI y el audio; aislamiento de procesos, prioridades, `isolcpus`, qué se precalcula por frase.
- **Robustez y operación**: arranque, actualización de firmware de las dos Teensy desde el Pi, recuperación tras reinicio, sistema de archivos de solo lectura, backups (ojo: el SSD de Juan no tiene respaldo), versionado de sets, logging, observabilidad (`@S STATS`), pruebas automáticas que faltan (CI de las ramas nuevas).
- **Hardware**: mapa de pines de A y S con reservas (e-drum, MIDI DIN, CV, jog, sync), alimentación (5 V de LEDs, Sharp, Pi 5 a 5 A), térmica, ruido en la masa del audio, la PCB principal pendiente.
- **Proceso**: docs que se contradicen entre sí, decisiones tomadas en conversación que no están en un doc, ramas sin integrar, deuda que bloquea el hito E5.

### Parte 2 · ML e IA local: cómo encaja con la consola

1. **Inventario**: qué del spec `04-ai-architecture.md` (TinyML en la Teensy) sigue vigente ahora que existe un Pi 5 de 8 GB en el mismo equipo. Para cada modelo (key detection, chords, beat following, genre fingerprint u otros), propone dónde debe correr (Teensy, Pi, Mac) con argumentos de latencia, memoria, consumo y valor musical. Señala si el spec queda obsoleto en partes (sin modificarlo).
2. **Ciclo de vida de modelos**: dónde se entrena (Mac con Apple Silicon, MLX/PyTorch/TF), con qué datos (los datasets de Juan, la auditoría de datasets del 2026-10-28 encontró 0 audios; el protocolo de captura; los registros del motor con las ediciones de Juan), cómo se versiona, cómo se exporta (TFLite Micro para la Teensy; ONNX Runtime / TFLite / llama.cpp en el Pi), cómo se despliega a la consola por Ethernet, cómo se evalúa (métricas, escucha) y cómo se revierte.
3. **El motor local y la IA**: hoy el motor es mayormente reglas + bibliotecas curadas por Juan. Propón dónde un modelo aprendido aporta de verdad (y dónde no) respetando "el criterio es de Juan": aprender de sus elecciones y ediciones registradas (registro v2 del motor), ranking de opciones, recomendaciones de capas/transiciones, mezcla de partida con el motor de GroovePilot, detección de energía/sección, sugerencia de vocal o chop. Explica cómo se mediría si un modelo mejora frente a las reglas.
4. **Interacción en vivo**: qué inferencia necesita la consola durante un set (por frase, con anticipación) frente a lo que puede prepararse en el Mac antes; presupuesto de CPU/RAM del Pi junto a audio + GUI; aceleradores posibles para el Pi 5 (p. ej. AI HAT+/Hailo) con costo y si valen la pena.
5. **Gemelo digital / VST del Brain** (ADR-006): cómo serviría para generar datos de entrenamiento (renderizar presets y patrones sin grabar en tiempo real) y cómo se conecta con lo anterior.

### Parte 3 · Raspberry Pi: otras cosas a contemplar

Lo que no esté cubierto en los docs: boot rápido, imagen reproducible (Yocto/Buildroot más adelante), overlay de solo lectura, almacenamiento (SD frente a NVMe), samples y bibliotecas, watchdog, actualización OTA por Ethernet, sincronización de proyectos con el Mac, seguridad de red local, energía y apagado seguro, ventilación en la caja, compatibilidad de PipeWire/Link/JACK, perfiles de CPU.

## Formato del entregable

Escribe **un solo documento** en `apps/docs/consola/auditoria/auditoria-fable-2026-11.md` (español, directo, sin relleno), con esta estructura:

1. **Dictamen** (≤ 10 líneas): estado real de la consola y los 3 riesgos que más amenazan el hito E5.
2. **Tabla de gaps de integración**: `ID · área · gap · evidencia (archivo:línea o rama) · impacto · severidad (bloqueante/alta/media/baja) · propuesta mínima · fase sugerida`.
3. **Contradicciones entre documentos** (citando ambos lados), sin resolverlas.
4. **ML e IA local**: mapa de "qué modelo, dónde corre, con qué datos, cómo se despliega", ciclo de vida propuesto, y qué del spec 04 sigue vigente o queda obsoleto.
5. **Oportunidades de mejora** priorizadas por valor/esfuerzo (con el presupuesto de 10–18 h/semana).
6. **Brainstorming de ML e IA** para el motor local y la consola: ideas separadas en *ahora* / *después del MVP* / *exploratorio*, cada una con el valor musical para Juan, el dato que necesita, el riesgo y cómo se evaluaría. Marca las que chocarían con "el criterio es de Juan".
7. **Raspberry Pi: lista de verificación** de lo que falta contemplar.
8. **Preguntas abiertas para Juan** (decisiones que solo él puede tomar).
9. **Fuentes**: archivos leídos y referencias externas (papers, datasheets, documentación oficial) con enlace.

## Reglas de trabajo

- **Solo lectura y un documento nuevo.** No modifiques código ni otros documentos; no hagas commits ni push.
- Cada afirmación técnica con **evidencia** (archivo:línea, rama, medición) o marcada como **inferencia**.
- Distingue lo **verificado** de lo **supuesto**; si no pudiste leer algo, dilo.
- Cita números reales de los specs y mediciones (latencias, CPU, memoria), no aproximaciones inventadas.
- Para afirmaciones externas (hardware, bibliotecas, aceleradores), cita la fuente oficial; si no puedes verificarla, márcala como pendiente de verificar.
- No propongas scope creep sin etiquetarlo como tal; respeta el orden MVP → reservas → v2.
- No envíes datos del repo a servicios externos.

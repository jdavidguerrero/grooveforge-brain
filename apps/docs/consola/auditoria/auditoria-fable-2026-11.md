# Auditoría de la GrooveForge Console · integración, IA local y Raspberry Pi

> **Fecha:** 2026-11-09. **Autor:** Claude (Fable), a pedido de Juan (`prompt-fable-auditoria-consola.md`).
> **Estado:** diagnóstico y propuestas para decidir. **No modifica** `00–06`, ADRs, código ni otros documentos; no hay commits.
> **Base leída:** rama `claude/monorepo-groovepush` (`f0b1792`) más `codex/teensy-s-e4` (`e827339`) y `codex/superficie-pi` (`7da57ca`). Lista completa en §9.
> **Convención:** «verificado» = lo leí en archivo, rama o medición. «Inferencia» = conclusión mía sin medición. Las rutas son relativas a la raíz del repo; `[sp]` = solo existe en `codex/superficie-pi`.

## 1. Dictamen

La consola tiene **tres piezas medidas y sanas por separado** (motor en el Pi: rack de drums en 0,34 s; sala por el Audio+: 0 cortes en 65 min y 61 min; Teensy A visible como audio, MIDI y serie) y **ningún contrato que las una**. El hito E5 (cue → ACCEPT → entra en la frase) necesita cuatro cosas que hoy no existen en código ni en spec: un dueño del estado del set, un motor que funcione sin Live, un reproductor que cambie de patrón en la frase con dos buses (sala y cue), y una acción ACCEPT con un control físico asignado.

Los tres riesgos que más amenazan E5:

1. **El audio USB Teensy → Pi descarta bloques** (21 277 saltos en 60 min). Afecta al Brain en la sala y, hasta separar sentidos, al cue. Sin arreglo, E5 solo es demostrable con drums del Pi y cue con clics.
2. **No hay «núcleo» de la consola.** `Motor` exige un `ClienteLive`; el reproductor tiene el patrón fijo en el código; `superficie.py` produce eventos que nadie consume. Falta el proceso que une motor, reloj, superficie y GUI, y su spec.
3. **Trabajo repartido en ramas y archivos sin commit, con docs que se contradicen en lo que E5 necesita** (qué control es ACCEPT, por dónde habla la S, quién es dueño del reloj). La medición que decide la arquitectura de audio (`e3-relojes-20261109.md`) está sin commit.

## 2. Gaps de integración

Severidad: **B** bloqueante para E5 · **A** alta · **M** media · **b** baja. Fase: E3/E4/E5 = etapa del prototipo; MVP = antes de la placa; v2 = después.

### 2.1 Flujos y contratos

| ID | Área | Gap | Evidencia | Impacto | Sev. | Propuesta mínima | Fase |
|---|---|---|---|---|---|---|---|
| G01 | Motor | El motor no corre sin Live: `Motor.__init__(live: ClienteLive, …)` y sus métodos escriben clips por `GFMotor`. Los generadores (`acordes.py`, `bajo.py`, `drums.py`…) sí son puros. E1 midió generadores y tests, no el flujo | `tools/motor/gf_motor/motor.py:149-150`, `live.py:19-23`; `mediciones/e1-grooveforge-20261109.md:7-12` | En el Pi no hay a quién pedirle «dos opciones de drums para la frase siguiente» | **B** | Theory doc + un backend de set en memoria con la misma interfaz que `ClienteLive` (ya existe uno para tests: `tools/motor/tests/live_simulado.py`). No reescribir el motor | E5 |
| G02 | Núcleo | No existe el proceso que es dueño del estado del set (qué suena por rol, qué está en cue, frase, compás, sección) ni su modelo de datos. El brief de diseño lista «datos disponibles para la UI» que nadie publica (frase n de 8, sección, energía por opción) | `design/claude-design-prompt-consola.md:65-67`; `07-gui.md:54-60`; no hay `apps/consola/gui-console/` ni `09-gui-fase1.md` | La GUI de fase 1 se construiría contra datos inventados | **B** | Spec corto «núcleo de la consola»: estado, comandos (cue, accept, skip, generate, mute, nivel) y eventos. Un solo escritor del estado | E5 |
| G03 | Reproductor | `reproductor.py` es la prueba E2: patrón fijo en `PATRON`, mono duplicado, una salida, sin MIDI hacia la Teensy A, sin cambio en frase, sin bus de cue | `tools/consola/reproductor.py:27-32,146-148`; `04-reproductor-e2.md:20` | No hay con qué sonar la opción B por cue ni entrarla en la frase | **B** | Extender `Secuenciador` a: lista de capas, cola de cambios «en el compás N», dos buses. Mantener la lógica pura con tests | E5 |
| G04 | Superficie → acciones | ACCEPT/GENERATE/SKIP/DROP no tienen control físico asignado de forma única (contradicción C3). `superficie.py` entrega `Boton("ENCODER_n")` y `MCP1_9…`, sin semántica | `07-gui.md:58`; `08-superficie-pi.md:26-36` [sp] | E5 no se puede cablear sin decidir | **B** | Decisión de Juan (P1) y una tabla «acción por modo → control» en un solo doc | E5 |
| G05 | Pi → S (retorno) | La S no tiene camino de vuelta: descarta todo el MIDI entrante y solo manda negro a la M4 al conectar. No hay contrato de LEDs de pads ni de teclas | `apps/consola/processor/src/consola_s/main.cpp:337,182-184` [sp]; `06-teensy-s-e4.md:73` [sp] | Los pads no pueden mostrar SALA / siguiente / cue (`00-concepto.md:90`); la superficie queda ciega | **A** | Para el prototipo: notas/CC de vuelta por el mismo MIDI (nota = pad, velocity = color de paleta). Definirlo junto con G04 | E5 |
| G06 | Modo CONTROLLER | El USB de la S va al Pi; el Mac está por Ethernet. No está definido cómo llega el SysEx de PushClone a Live ni cómo se cambia de firmware/modo (el entorno `consola_s` no enlaza `LiveController`) | `03-arquitectura-capas.md:10,22,59`; `platformio.ini` de `processor` [sp] («no enlaza LiveController») | El modo CONTROLLER no tiene topología. La auditoría previa (H03) lo dejó abierto y sigue abierto | **A** (no bloquea E5) | Tabla de conexiones por modo. Opciones: el Pi reenvía MIDI/SysEx por red (RTP-MIDI), o un segundo USB de la S al Mac. Medir antes de elegir | MVP |
| G07 | Protocolo | Tres formas de hablar con las Teensy conviven: `bridge-protocol` por CDC (spec), MIDI + `@S STATS` (S) y `@GF` texto (A). Los comandos de consola de `02` §1.3 siguen «pendientes»; no hay campo de rol | `02-bridge-protocol.md:64-69`; `05-e3-teensy-a.md:18`; `06-teensy-s-e4.md:70-72` [sp] | Es deuda aceptada para el prototipo, pero cada herramienta nueva del Pi la aumenta | **M** | No migrar ahora. Fijar la condición de salida: «cuando el mapa de controles esté estable» necesita una fecha o un hito (propongo: al cerrar E5) | MVP |
| G08 | Identidad USB | La S se abre por nombre («GF Superficie»), sus STATS por ruta (`/dev/ttyACM1`) y la A por `/dev/ttyACM0`; `02` §1.3 dice que se distinguen por número de serie. El orden de `ttyACM*` cambia entre arranques | `08-superficie-pi.md:36,52` [sp]; `05-e3-teensy-a.md:7`; `02-bridge-protocol.md:66` | Tras un reinicio el Pi puede mandar `@GF` a la S o flashear la Teensy equivocada | **A** | Reglas `udev` por número de serie: `/dev/gf-audio`, `/dev/gf-superficie`. Una sesión | E4 |
| G09 | Reloj | El reloj maestro es el contador de muestras del Audio+ (decidido). Falta: quién manda cuando hay Link (Link ajusta tempo y fase desde fuera), y cómo se alinea la Teensy A, que no recibe marcas de tiempo (USB MIDI no las lleva; el comando con marca de tiempo está «pendiente») | `03-arquitectura-capas.md:16`; `02-bridge-protocol.md:68` | Samples del Pi y synths del Brain desfasados por una cantidad sin medir (USB + bloque de 128 + remuestreo) | **A** | Medir el retardo Brain→sala una vez arreglado G12 y compensarlo como constante (retrasar los samples). Las marcas de tiempo solo si la variación medida lo pide | E5 |
| G10 | Brain | La Teensy A toca **un** sonido o una capa de dos engines; la consola pide BASS, CHORDS, LEAD y PAD a la vez desde el Brain | `adr/ADR-006-plugin-gemelo.md` §Context («un sonido a la vez»); `sketches/49-studio-brain.cpp:29-30,1645-1657` (no hay despacho por canal) | El set por capas de `00-concepto.md` §5.1 no es posible con el firmware actual más allá de drums (Pi) + una parte de synth. Ya señalado como H06 | **A** (E5 solo usa drums) | Decidir fuente por rol (P3): renders del gemelo como audio en el sampler del Pi, multitimbralidad en la Teensy (ADR + medición de CPU), o gemelo en ARM (E6/P4) | MVP |
| G11 | A ↔ S | El enlace UART directo está reservado (pines 7/8, sync 37) y sin implementar; el doc promete pad → sonido < 1 ms | `03-arquitectura-capas.md:61`; `06-teensy-s-e4.md:24` [sp] | Sin él, FSR → sonido pasa por el Pi (ver G14). Además CUE (amplificador del SGTL5000) e IN dependen de ese enlace o de `@GF` vía Pi | **M** | Dejarlo reservado. Para el prototipo, CUE/IN por el Pi con dos comandos `@GF` nuevos | MVP |

### 2.2 Audio

| ID | Gap | Evidencia | Impacto | Sev. | Propuesta mínima | Fase |
|---|---|---|---|---|---|---|
| G12 | Teensy → Pi por USB tira bloques de 128 muestras: el núcleo manda 44 100 muestras/s exactas y la Audio Library produce 44 117,6. La rama `codex/usb-audio-eval` y `10-usb-audio-teensy.md` **no existen** | `mediciones/e3-relojes-20261109.md:8,35-39`; comentario desmentido en `audio/audio_graph.cpp:36` («El FIFO interno absorbe el clock drift») | El Brain en la sala con un hueco de ~3 ms cada ~11 s | **A** (B si E5 incluye synths) | Primero la prueba que el propio doc pide: separar sentidos (`e3-relojes…:49`). Después, evaluar parche de envío 44/45 según nivel del búfer. Existe trabajo de la comunidad sobre USB audio asíncrono en Teensy 4: **pendiente de verificar**, no lo pude confirmar | E3 |
| G13 | Decisión A (USB arreglado) frente a C (un solo reloj) sin criterio de corte | `e3-relojes…:43-47`; `03-arquitectura-capas.md:87` | Riesgo de gastar semanas en el parche | **M** | Presupuesto: 2 sesiones para A. Si no da 0 saltos en 60 min, plan B barato antes de C: **salida de línea del SGTL5000 a una entrada analógica** (hoy el Pi no tiene entrada: es la v2) o aceptar Brain solo por audífonos en el prototipo | E3 |
| G14 | Latencia de punta a punta sin medir. La única medición es inválida (mínimo −4,6 ms). Suma inferida FSR → sample del Pi: 3 ms (ventana) + USB + Python + hasta 5,3 ms de bloque + 5,3 ms de salida ≈ 11–15 ms. El objetivo de `01` §5.2 es < 5 ms p95 para MIDI → audio del Brain | `mediciones/e3-grooveforge-20261109.md:13`; `e2-…md:18`; `06-teensy-s-e4.md:33` [sp]; `01-architecture.md:708` | Tocar los 4 FSR «con dinámica real» sobre samples del Pi puede sentirse tarde. No afecta a lo generativo | **A** | Arreglar `latencia.py` (grabar salida y entrada en el mismo flujo). Definir un objetivo propio de la consola para pad → sala: `01` §5.2 no lo cubre | E4 |
| G15 | Ganancias sin contrato. `lineInLevel(5)` con comentario contrario a PJRC; Brain llega al Pi a −31 dBFS medio / −18 pico; MASTER es volumen digital en el Pi (pierde resolución a volumen bajo); sin limitador en la sala (`np.clip`) | `audio/audio_graph.cpp:113-117`; `05-e3-teensy-a.md:25`; `e3-grooveforge…:9`; `03-arquitectura-capas.md:71`; `reproductor.py:110` | Niveles desparejos entre capas del Pi y del Brain; recorte duro en la sala | **M** | Una tabla de niveles (dBFS por etapa, headroom objetivo) y un limitador simple en el bus de sala | MVP |
| G16 | «El sonido no se corta aunque se congele la pantalla» vale para la GUI, no para el Pi: toda la sala sale del Pi (Audio+). Si muere el proceso de audio o el Pi, hay silencio en la sala; la Teensy A sigue sonando solo por su salida propia | `03-arquitectura-capas.md:11-14`; G12 | El principio «nada del tiempo real depende de Linux» (`03:14`) no se cumple para la sala. Inferencia: es aceptable, pero hay que decirlo | **M** | Proceso de audio separado, sin GUI ni motor dentro, con watchdog de systemd; al reiniciar, retoma el último estado. Documentar el caso «se cuelga el Pi» | MVP |
| G17 | Tres frecuencias: Teensy 44 117,647 Hz (se presenta como 44,1 kHz), Pi/Audio+ 48 kHz, protocolo de captura 48 kHz con tomas reales a 44,1 kHz | `05-e3-teensy-a.md:24`; `produccion/auditoria-datasets-adn-2026-10-28.md` §2 | Remuestreo permanente del Brain en el Pi; datasets mezclados | **b** | Registrar la frecuencia real en cada manifiesto; no cambiar nada ahora | — |

### 2.3 Tiempo real en el Pi

| ID | Gap | Evidencia | Impacto | Sev. | Propuesta mínima | Fase |
|---|---|---|---|---|---|---|
| G18 | El callback de audio es Python (numpy bajo el GIL) y acumula una lista sin límite (`duraciones.append`). Funcionó con carga en **otros procesos**; no se midió con GUI ni con hilos Python en el mismo proceso | `reproductor.py:141-149`; `mediciones/e2-…md:7-9` (máx. 660 µs, 12,4 %) | Un hilo Python que retenga el GIL (GUI PySide6, lector MIDI) en el proceso de audio puede causar cortes. Inferencia | **A** | Regla escrita: el proceso de audio no importa Qt ni el motor; recibe cambios por una cola entre procesos. Repetir E2 con la GUI de la compuerta de ADR-007 corriendo | E5 |
| G19 | Sin aislamiento configurado: `isolcpus`, gobernador `performance`, prioridad de tiempo real y `threadirqs` están propuestos, no aplicados (`cmdline-consola.txt` solo rota la consola) | `03-arquitectura-capas.md:39`; `tools/consola/pi/cmdline-consola.txt` | Las mediciones E2 son sin estas protecciones y aun así pasan; el margen real con GUI es desconocido | **M** | No tocar hasta medir G18. Si aparece un corte, aplicar en este orden: prioridad RT del hilo de audio → gobernador → `isolcpus` | MVP |
| G20 | Qué se precalcula por frase no está escrito. Tiempos medidos: tanda de kicks 0,11 s, rack 0,34 s, 30 clips < 0,01 s. Una frase de 8 compases a 122 BPM dura 15,7 s | `mediciones/e1-…md:9-12` | Hay margen de sobra (inferencia), pero sin regla el motor puede generar en el último compás | **b** | Regla: toda opción está lista (MIDI + samples en RAM) antes del último compás de la frase; si no, no se ofrece | E5 |

### 2.4 Robustez, operación y pruebas

| ID | Gap | Evidencia | Impacto | Sev. | Propuesta mínima | Fase |
|---|---|---|---|---|---|---|
| G21 | CI no cubre nada de la consola: solo `firmware-teensy` (`sketch`, no `sketch49` ni `consola`), `firmware-esp32` y `training`; y solo en PR a `main` | `.github/workflows/ci.yml:3-7,28-31` | 467 + 221 + tests de `tools/consola` + 10 de `consola_s` corren a mano. Ya señalado (H10) | **A** | Añadir jobs: `pio run -e consola`, `pio test -e native` en `apps/consola/processor`, `uv run pytest` en `tools/motor`, `tools/perc-synth`, `tools/consola` | E4 |
| G22 | Ramas `codex/*` solo locales (no están en `origin`); una vive en un worktree bajo `/private/tmp`. Sin commit: `e3-relojes-20261109.md`, su `.txt` y cambios en `relojes.py` | `git branch -a`; `git worktree list`; `git status` del worktree | La medición que decide el audio y todo E4 dependen de un solo disco | **A** | Decisión de Juan: commit de las mediciones y push de las ramas (no lo hice: regla de solo lectura) | Ya |
| G23 | `06-teensy-s-e4.md` declara «sin flash ni mediciones físicas» y parte de la regresión no se pudo compilar | `06-teensy-s-e4.md:3,98` [sp] | E4 no ha pasado; `01-prototipo-mesa.md` §4 exige número medido | **A** | Correr E4 en placa con `@S STATS` (scan ≥ 2 kHz, gap ≤ 500 µs, 0 drops) antes de integrar | E4 |
| G24 | Firmware final ≠ firmware de mesa: 2 MCP en `consola_s`, 3 MCP (0x22) en las placas; el orden de entradas cambia | `PIN_MAP.md:73-91` [sp]; `06-teensy-s-e4.md:67` [sp] | Las notas 80–107 cambian de significado al pasar a PCB. Ya señalado (H05) | **M** | Mapear por nombre lógico en el Pi (`BOTONES` ya lo permite) y generar la tabla de la S desde una sola fuente | MVP |
| G25 | Operación sin definir: arranque automático (servicios), actualización de las dos Teensy desde el Pi (el primer intento de `teensy_loader_cli` falló), recuperación tras reinicio, reconexión USB (`superficie.py` no reconecta), logs, versión visible de firmware (`@GF HELLO` no lleva build) | `mediciones/e3-grooveforge…:3`; `08-superficie-pi.md:42` [sp]; `produccion/ia-local-gemelo-digital.md:198` | Cada sesión de mesa empieza con pasos manuales; en un set, un cable flojo exige reiniciar | **M** | Tres unidades systemd (audio, núcleo, GUI) y un `make flash-a` / `flash-s` que identifique por número de serie (G08) | MVP |
| G26 | Registro y biblioteca atados al Mac: `GF_LIBRARY` por defecto `/Volumes/GROOVE_SSD/…`, eventos en `$GF_DATASET`. No hay plan de dónde escribe el Pi ni cómo vuelve al Mac. El SSD de Juan no tiene respaldo | `tools/motor/gf_motor/motor.py:146`; `tools/paleta-lab/gf_log.py:41`; memoria `ssd_migracion_datos` | Las decisiones del set (el dato más valioso, `motor-asistencia-v2.md` §6) pueden quedar solo en una SD | **A** | El Pi escribe eventos en un `.jsonl` propio por set (solo agregar) y el Mac los **tira** por Ethernet (`rsync`) al conectar. El Pi nunca empuja | E5 |
| G27 | Observabilidad: `@S STATS` existe para la S; la A reporta CPU solo como texto de log; el Pi no guarda cortes ni temperatura fuera de las pruebas | `main.cpp:238-253` [sp]; `ia-local-gemelo-digital.md:194` | Un fallo en un set no deja rastro | **b** | Un log por set: cortes, carga máx., temperatura, STATS cada 10 s | MVP |

### 2.5 Hardware

| ID | Gap | Evidencia | Impacto | Sev. | Propuesta mínima | Fase |
|---|---|---|---|---|---|---|
| G28 | Mapa de pines de la S con conflictos documentales: jog en 29/36 y a la vez «el GPIO 29 sigue sin uso»; 40/41 son INT de MCP en las placas y triggers reservados en la consola; CUE/IN (38/39), jog y sync (37) van por cable directo a pines que la portadora marca NC | `PIN_MAP.md:167,183,189-190` [sp]; `03-arquitectura-capas.md:84` | El mapa «con reservas» que pide `03` §4.2 no existe como documento único; la placa principal no se puede empezar | **A** | Un `PIN_MAP` de consola (A y S) como única fuente, después de E4 | MVP |
| G29 | Mapa de la Teensy A en modo consola sin escribir: qué pines del panel Studio quedan libres, SAI2 reservado, UART a S, CV (29) | `03-arquitectura-capas.md:87,92-94` | Ídem | **M** | Mismo documento que G28 | MVP |
| G30 | Presupuesto de corriente sin hacer. Datos sueltos: Pi 5 a 5,1 V / 5 A; USB limitado a 600 mA con fuente de 3 A; M4 con 32 NeoPixels, 20 SK6812, 2 Sharp a 5 V, dos Teensy, pantalla 7" | `00-concepto.md:231`; `03-arquitectura-capas.md:71,90`; `PIN_MAP.md:156-158` [sp] | Caídas de tensión con LEDs → ruido o reinicios. Ya pedido por la auditoría previa | **M** | Tabla de consumo pico por riel antes de dibujar la placa; decidir si las Teensy se alimentan del USB del Pi o de un riel propio (hoy la portadora corta VUSB–VIN) | MVP |
| G31 | Masa y ruido: Pi, dos Teensy, LEDs y audio comparten tierra; la A se usa con aislador USB frente al Mac pero «directo» al Pi; los Sharp GP2Y0A41 consumen en pulsos | `01-prototipo-mesa.md:29`; `06-teensy-s-e4.md:42` [sp] | Zumbido o ruido de LEDs en cue y en la entrada de línea. Inferencia: no medido | **M** | Medir piso de ruido en cue y en la entrada con LEDs animados y Sharp activos, en la mesa, antes de la PCB | E4 |
| G32 | Pantalla nativa 720×1280 vertical; la GUI se diseñó a 1280×720. La rotación de Qt y del touch no está configurada ni documentada en el repo | `tools/consola/pi/config-consola.txt` («720x1280 nativa (vertical)»), `pi/README.md` («Lo que no está aquí») | Primera hora de la GUI en el Pi perdida en rotación; la rotación por software puede costar cuadros (afecta la compuerta de ADR-007). Inferencia | **M** | Incluir la rotación en la compuerta de rendimiento | MVP |
| G33 | Térmica: 58 °C a los 30 min con carga, a aire libre | `mediciones/e2-…md:8,17` | En caja cerrada con pantalla encima subirá | **b** | Repetir E2 con carga dentro de una caja de cartón con el ventilador activo antes del diseño mecánico | MVP |

### 2.6 Estado de la auditoría previa (`apps/docs/auditoria-consola-unificada-2026-11-09.md`, sin commit, checkout principal)

| Hallazgo | Estado hoy | Evidencia |
|---|---|---|
| H01 contrato de consola unificada | **Parcial.** Hay concepto, capas, GUI y ADR-007. Sigue sin ADR de consola (lo pide también `02-inventario-groovepush.md:56`) | `consola/00…07`, `adr/ADR-007` |
| H02 dos mapas de pines | **Resuelto** como decisión (dos Teensy). Queda G28 | `03-arquitectura-capas.md:60` |
| H03 topología USB con Live | **Abierto** → G06 | — |
| H04 pads no tocan | **Resuelto a medias:** `consola_s` manda notas 48–79 con on/off; nadie las consume | `main.cpp:170-174` [sp] |
| H05 firmware ≠ placas finales | **Abierto** → G24 | — |
| H06 una parte de synth a la vez | **Abierto** → G10. Los drums pasan al sampler del Pi | — |
| H07 dos salidas sin contrato | **Decidido** el reparto (sala Audio+, cue/entrada SGTL5000); **falló** la medición → G12–G15 | `03:69`, `e3-relojes…` |
| H08 estado sincronizado | **Abierto** → G02, G05 | — |
| H09 reloj y recuperación | **Parcial:** reloj decidido; estado del set y recuperación no → G02, G09, G25 | `03:16` |
| H10 Git y CI | **Git resuelto** (importado con historial, `02-inventario…:3`). **CI abierto** → G21 | — |
| H11 jerarquía documental | **Abierto** y más amplio → §3 | — |

`apps/docs/theory/produccion-identidad-ia-local-refinamiento.md` (sin commit): no lo comparé línea a línea con `produccion/ia-local-refinamiento.md`; **pendiente** confirmar si es un duplicado anterior.

## 3. Contradicciones entre documentos (se señalan, no se resuelven)

| # | Un lado | El otro lado |
|---|---|---|
| C1 | `00-master-strategy.md:174`: tras depurar el MCLK del Pi 5 para el SGTL5000, «Linux audio on Pi was not the right path» | La consola pone la sala en el Pi (`03-arquitectura-capas.md:68-69`), con mediciones a favor (`e2`). Usa un PCM5102A, que no necesita MCLK; `config-consola.txt` conserva comentado un `gpclk` de 12,288 MHz de origen no documentado (inferencia: resto de aquel intento). `00-concepto.md:3` declara la contradicción de producto, no esta |
| C2 | `03-arquitectura-capas.md:23`: Teensy ↔ Pi por `bridge-protocol` sobre CDC | `06-teensy-s-e4.md:52,70` [sp] y `05-e3-teensy-a.md:18`: MIDI estándar, `@S` y `@GF` de texto. Ya anotada en `08-superficie-pi.md:62` [sp] y `03:29` |
| C3 | ACCEPT / SKIP / GENERATE: **botones dedicados** (`00-concepto.md:190`) · **fila 3 y 4 del NeoTrellis** (`00-concepto.md:163`) · **un pad del NeoTrellis** (`01-prototipo-mesa.md:85`) | **No son hardware**, son teclas de pantalla con clic de encoder o touch (`07-gui.md:58`) · el export de diseño dice «ACCEPT / GENERATE / SKIP are hardware buttons» y «PRESS ENC TO CUE» (*Layout notes* de `Console MVP Screens.dc.html`) |
| C4 | `03-arquitectura-capas.md:10`: si falla el Pi, la S «sigue mandando MIDI a Live» | El único USB de la S va al Pi (`03:23`, `06:52` [sp]) y el Mac se conecta por Ethernet (`03:59`) |
| C5 | `03-arquitectura-capas.md:61`: enlace A ↔ S «permite pad → sonido en menos de 1 ms» | `06-teensy-s-e4.md:33` [sp]: la velocity del FSR se captura durante 3000 µs antes de emitir la nota |
| C6 | `PIN_MAP.md:183` [sp]: jog A = 29 | `PIN_MAP.md:190` [sp]: «El GPIO 29 sigue sin uso (CV heredado)»; `:167`: 29 «unavailable» en la portadora |
| C7 | `03-arquitectura-capas.md:84`: triggers de e-drum reservados en 25, 27, 40, 41 de la S | `PIN_MAP.md:89` [sp]: INT de los MCP en 40 y 41 en las placas finales |
| C8 | `motor-asistencia-v2.md:26,29`: «sin botón de track completo», «no avanza solo» | `00-concepto.md:80,88,100`: el motor tiene siempre la siguiente opción lista, deriva el track siguiente y ejecuta sola la transición. (Inferencia: es compatible si cada entrada pasa por ACCEPT, pero ningún doc lo dice así) |
| C9 | `motor-asistencia-v2.md:30`: «nada se sube ni se envía fuera del Mac» | El motor y el registro correrán en el Pi (`03:12`); HÍBRIDO manda MIDI por red (`00-concepto.md:65`) |
| C10 | `04-ai-architecture.md` §2: Layer 2 en la nube con API de Anthropic u otros | Prioridad de Juan desde 2026-10-26: IA local (`02-inventario-groovepush.md:47`; `ia-local-refinamiento.md` §11) |
| C11 | `04-ai-architecture.md:102`: arena de ~200 KB | `04-ai-architecture.md:313`: «memory footprint (target <400KB total)». `CLAUDE.md` cita 200 KB |
| C12 | Modos: ESTUDIO / LIVE / SECUENCIADOR / HÍBRIDO (`00-concepto.md` §4) | CONTROLADOR / STANDALONE (`07-gui.md:8`) y «LIVE y MIXER básico» (`03:72`). Falta la tabla de equivalencias |
| C13 | `02-bridge-protocol.md:66`: el Pi distingue las Teensy por número de serie y `GET_STATUS` | `superficie.py` filtra por nombre USB; `Stats` y `@GF` por ruta `/dev/ttyACM*` |
| C14 | `audio_graph.cpp:113-116`: `lineInLevel` 0 = máxima sensibilidad | Documentación de PJRC según `05-e3-teensy-a.md:25`: lo contrario. Abierta desde E3 |
| C15 | `adr/ADR-007`: modelos de la GUI en Python | GroovePush ADR-0003: modelos en C++ (declarada en el propio ADR-007). Además ADR-002: cabeza Pi 3B + LVGL para Studio |
| C16 | El diseño llama «Brain offline» a la pausa de **opciones del motor** («New options pause until the Brain is back») | En el repo «Brain» es la Teensy A (synths). Ambigüedad de nombre en la GUI |
| C17 | `apps/training/CLAUDE.md`: `beat_tracker/`, `export.py`, `auto_harmonizer/`… | En disco: `beat_follower/`, sin `export.py`; solo tres modelos |
| C18 | `06-implementation-roadmap.md`: no tiene track de consola (sí §10B Studio) | La consola es el trabajo activo. `06` es «living», pero nadie lo actualizó |

## 4. ML e IA local

### 4.1 Qué hay hoy (verificado)

- **Modelos TinyML entrenados y exportados** de sprints anteriores: `key_detector` (19,8 KB según su cabecera, frente a 12 KB del spec), `chord_recognizer`, `beat_follower`, como arrays C en `apps/firmware-teensy/src/ml/`. **No están en el firmware que usa la consola:** `49-studio-brain.cpp:33` dice «Fuera de esta prueba: … ML».
- **`apps/training`**: tres modelos con datos públicos (Lakh). No hay nada entrenado con datos de Juan.
- **Datos de Juan:** 1 265 MIDI distintos de 1 713, todos ≈124 BPM; **0 audios** en el dataset; 17 de 118 presets con rol; registro v2 con muy pocas decisiones en formato común (`auditoria-datasets-adn-2026-10-28.md` §2).
- **Motor:** reglas y bibliotecas curadas; registra borradores y ediciones (`motor-asistencia-v2.md` §6). Orden aprobado por Juan: reglas → LLM pequeño → preferencias → CLAP → modelo MIDI → audio generativo (`ia-local-refinamiento.md` §11).
- **Gemelo:** render 0,8–3,6 % de un núcleo en M1 Pro; sin `gf-render`, sin hash repetible probado, sin medición contra el Brain físico (`ia-local-gemelo-digital.md` §3).

### 4.2 Spec `04` frente a la consola (no se modifica)

| Elemento de `04` | En el Brain como producto | En la consola | Dónde correría si hiciera falta |
|---|---|---|---|
| Scale Lock / key detection | Vigente (Nivel 1) | **Redundante para lo generado:** el motor fija la tonalidad. Útil solo para entradas externas (MIDI DIN, marimba, línea) | MIDI: perfil de Krumhansl sin red, en el Pi (microsegundos). Audio: croma en el Pi. Teensy no |
| Chord Recognition | Vigente | Ídem; el motor conoce sus acordes. Útil para mostrar lo que toca un teclado externo | Pi, por reglas sobre notas activas |
| Beat Follower | Vigente | **Sustituido por Link** y por el reloj propio. Útil solo para seguir audio sin Link (CDJ, v2) | Pi, sobre la entrada de línea; v2 |
| Genre Fingerprint | Vigente (v1.2) | **Sin uso:** el género es un dato de la partida del motor | — |
| Velocity Curve Learn | Vigente | Útil como **calibración por pad FSR** (umbral, máximo). Estadística, no red | Teensy S (parámetros) calculados en el Pi |
| Auto-Harmonization | Vigente (v1.1) | Solapa con el motor (acordes, motivo). No duplicar | — |
| Presupuestos (arena 200 KB, < 20 ms p99) | Vigentes para la Teensy | No aplican al Pi | — |
| Layer 2 nube | En el spec | **Choca con local-first** (C10) | Mac, local |
| Layer 3 DAW (mix-aware, sugerencias) | En el spec, vía VST de GroovePilot | Coincide con `GFMotor` + motor de mezcla, en local | Mac |
| Pipeline: datasets públicos, Colab | En el spec | Para la consola el dato es el registro de Juan; Colab choca con local-first | Mac |

**Lectura:** `04` sigue siendo coherente como spec del **Brain producto** (teclado → Teensy, sin computador). Para la **consola**, casi todo Layer 1 queda sin problema que resolver, porque quien genera la música ya sabe tonalidad, acordes, tempo y género. Lo que la consola necesita aprender es otra cosa: **qué elige Juan**. Eso no está en `04`. No propongo mover los modelos TinyML al Pi: propongo no usarlos en la consola hasta que haya una entrada externa que los pida.

### 4.3 Mapa propuesto: qué modelo, dónde, con qué datos

| Modelo | Para qué | Entrena | Corre | Datos | Formato y costo |
|---|---|---|---|---|---|
| **M1 · Ordenador de opciones** | De N candidatos del motor, cuáles 3 mostrar y en qué orden | Mac | Pi (por frase) y Mac | Registro v2: opciones escuchadas, elegidas, incorporadas; en vivo: ACCEPT / SKIP con contexto (rol, sección, energía, lo que suena) | Regresión logística o árboles pequeños sobre rasgos del MIDI y del sonido. Coeficientes en JSON o ONNX; inferencia en microsegundos, sin acelerador |
| **M2 · Fichas medidas de sonidos** | Registro útil, ataque, brillo, choque con el kick; llena `key_range` (0 de 118) | — (medición, no ML) | Mac, con renders del gemelo | Presets × clips de prueba | DSP. Sale una tabla que el Pi solo consulta |
| **M3 · Embeddings de audio (CLAP u otro)** | Buscar sonidos por parecido o descripción | Preentrenado; sin entrenar | Mac calcula; el Pi solo compara vectores | Renders del gemelo + samples propios | Vectores en un archivo; vecino más cercano con numpy. Licencia del modelo: **pendiente de verificar** |
| **M4 · Sugeridor de transición** | Cuál de las 3 maneras proponer primero | Mac | Pi | Transiciones elegidas en sets, con contexto | Como M1. Entra cuando haya decenas de sets |
| **M5 · Modelo de ediciones** | Borradores que piden menos corrección | Mac | Mac (estudio); Pi solo si es pequeño | Pares borrador → edición de varios proyectos | Paso 6 de `ia-local-refinamiento.md` §11; lejos |
| **M6 · LLM local pequeño** | Palabras → cambios acotados | — | **Mac.** En el Pi 5, un modelo de 1–3 B de parámetros en `llama.cpp` responde a pocos tokens por segundo (inferencia; sin medir) y compite con audio y GUI | — | No en el Pi durante un set |
| TinyML de `04` | Entradas externas | Mac | Teensy A | Públicos + renders con etiqueta exacta (uso K del gemelo) | TFLite Micro int8; decisión de Juan pendiente |

### 4.4 Ciclo de vida propuesto

1. **Datos.** Fuente única: el registro de eventos (`gf_log.py`, solo agregar) de Mac y Pi (G26). Partición de entrenamiento y prueba **por proyecto o set**, nunca por clip (`ia-local-gemelo-digital.md` §7, «abundancia falsa»).
2. **Entrenamiento.** En el Mac. Para M1/M4 basta scikit-learn; MLX o PyTorch solo desde M5. `apps/training` queda para TinyML; los modelos de preferencia irían en un directorio hermano (propuesta; requiere spec).
3. **Versionado.** Un directorio por versión: artefacto + `manifest.json` (commit del repo, hash del registro usado, lista de proyectos de prueba, métricas, baseline con el que se comparó). Artefactos fuera de git (pesan y son personales); manifiestos dentro.
4. **Exportación.** Pi: JSON de coeficientes o ONNX Runtime (ruedas para ARM64 y la versión de Python del Pi: **pendiente de verificar**; `python-rtmidi` ya hubo que compilarlo, `e1…:16`). Teensy: TFLite Micro como hoy.
5. **Despliegue.** El Mac copia por Ethernet a `/data/models/<nombre>/<versión>/` y cambia un enlace `current`. El Pi nunca descarga nada por su cuenta.
6. **Evaluación.** Antes de desplegar: repetición fuera de línea sobre proyectos reservados contra la regla «favoritos del mismo rol primero», **con el mismo número de opciones** (`motor-asistencia.md` §11). Después: en uso, una de las tres opciones es siempre la de la regla y otra es de exploración; se comparan tasas de elegida, incorporada y conservada. Decide Juan escuchando.
7. **Reversión.** Enlace `current` a la versión anterior; un interruptor «solo reglas» en Ajustes. El modelo nunca es requisito para tocar: si falta o falla, el motor usa reglas.

### 4.5 Inferencia durante un set y presupuesto del Pi

| Cuándo | Qué | Costo |
|---|---|---|
| Antes, en el Mac | Fichas (M2), embeddings (M3), entrenamiento, renders del gemelo como audio por rol | Sin límite |
| Por frase, en el Pi, con una frase de anticipación | Generar candidatos (reglas: < 0,4 s medidos), ordenarlos (M1), cargar samples | Un núcleo unos cientos de ms cada ~16 s |
| En el golpe | Nada de ML | — |

RAM: 7,9 GB; ninguno de M1–M4 pasa de unos MB. CPU: el callback usa 12,4 % de su bloque en el peor caso con tres núcleos saturados (`e2`). **No hay argumento para un acelerador.** El AI HAT+ (Hailo-8L de 13 TOPS a 70 USD, Hailo-8 de 26 TOPS a 110 USD al lanzamiento) acelera redes convolucionales cuantizadas, pensadas para visión; ocupa el PCIe del Pi 5 (el mismo que usaría un NVMe) y el conector de 40 pines donde va el Audio+ (apilado **a verificar**). Veredicto: no. Sería scope creep.

### 4.6 El gemelo como fuente de datos (ADR-006)

- **Para la consola, antes que para ML:** renderizar las partes de synth de un track «ya hecho» como audio por rol y cargarlas en el sampler del Pi. Resuelve G10 para tracks de biblioteca sin multitimbralidad. Exige fidelidad de nivel 0 (render repetible) y la etiqueta `source: render`.
- **Para M2 y M3:** preset × clip de prueba → WAV + manifiesto. 732 clips en ~8 min de un hilo (estimación del propio doc, §3).
- **Para TinyML de `04` (uso K):** audio con tonalidad y acordes exactos del MIDI. Exige nivel 2 (medición contra el Brain físico) y decisión de Juan.
- **Bloqueos verificados:** no existe `gf-render`, falta probar el hash entre corridas, el plugin no comparte todas las rutas de voz con el firmware. La etapa 1 de `ia-local-gemelo-digital.md` §8 sigue siendo el primer paso.

## 5. Oportunidades priorizadas (10–18 h por semana)

| # | Qué | Valor | Esfuerzo | Por qué en este orden |
|---|---|---|---|---|
| 1 | Commit de mediciones y push de ramas (G22) | Protege lo hecho | < 1 h, decisión de Juan | Riesgo de pérdida |
| 2 | Decidir ACCEPT/SKIP físico y el alcance exacto de E5 (P1, P2) | Desbloquea G04 | Una conversación | Sin esto se programa dos veces |
| 3 | Separar sentidos del USB audio (G12) y fijar el presupuesto de A (G13) | Sabe si el cue sirve | 1 sesión | Lo pide el propio E3 |
| 4 | E5 mínimo **solo con drums del Pi**: backend sin Live (G01), secuenciador con cambio en frase y bus de cue (G03), dos botones | El hito de fase | 3–4 sesiones | No depende del USB audio de la Teensy → Pi |
| 5 | `udev` por número de serie (G08) y E4 en placa (G23) | Mesa repetible | 1–2 sesiones | Antes de integrar la S |
| 6 | CI para consola (G21) | Regresión automática | 1 sesión | Barato |
| 7 | Spec del núcleo y del modelo de datos (G02), después la GUI fase 1 | GUI contra datos reales | 2 sesiones de spec | La GUI sin esto es maqueta |
| 8 | Registro del set en el Pi y recogida desde el Mac (G26) | Empieza a acumular el dato de M1 | 1–2 sesiones | Cada set sin registrar es dato perdido |
| 9 | Mapa de pines único (G28, G29) y presupuesto de corriente (G30) | Permite empezar la placa | 2 sesiones | Después de E4 y E5, como dice `01-prototipo-mesa.md` §5 |
| 10 | M1 (ordenador de opciones) | Primer ML útil | 2–3 sesiones | Solo con datos de varios sets |

Pospuesto a propósito: migración a `bridge-protocol` (G07), enlace A ↔ S (G11), Yocto, PREEMPT_RT (E2 no lo pide), acelerador, LLM en el Pi.

## 6. Brainstorming de ML e IA

⚠ = puede chocar con «el criterio es de Juan» si se implementa sin la condición indicada.

### Ahora (sin entrenar nada, o casi)

| Idea | Valor para Juan | Dato | Riesgo | Cómo se evalúa |
|---|---|---|---|---|
| Registrar ACCEPT / SKIP / GENERATE con contexto completo | Es el dataset de todo lo demás | Eventos del set | Contexto incompleto = dato inútil (lección de la auditoría de datasets) | Se puede reconstruir qué sonaba y qué se ofreció en cada decisión |
| Regla «favoritos del mismo rol primero» en vivo | Opciones mejores desde el primer set; es el baseline | Registro actual | Estrecha el gusto | Reservar 1 de 3 opciones a exploración y medirla aparte |
| Energía por opción y por sección, calculada del MIDI (densidad, registro, velocity) | La GUI ya la muestra («E3», «E4»); permite pedir «más» o «menos» | Ninguno | Que el número no coincida con lo que Juan siente | Juan ordena 10 pares; se ajusta la fórmula |
| Calibración por pad FSR y por sensor de mano (percentiles de uso) | Pads que responden igual | Golpes de calibración | Deriva con el tiempo | 10 suaves / 10 fuertes separados (`01-prototipo-mesa.md:77`) |
| Compatibilidad de tracks (tonalidad, tempo, registro del bajo) por reglas | El «siguiente» siempre mezcla | Fichas de set | — | Tests |
| Fichas medidas con el gemelo (M2) | Recomendaciones de capa con fundamento | Renders | Divergencia con el Brain físico | La decisión se sostiene al oírla en el Brain |

### Después del MVP

| Idea | Valor | Dato | Riesgo | Evaluación |
|---|---|---|---|---|
| M1 · ordenador de opciones | Menos SKIP por cada ACCEPT | ≥ varios sets y proyectos | Sobreajuste a pocos sets; sesgo hacia favoritos | Fuera de línea por set reservado, luego en uso contra la regla |
| M4 · sugeridor de transición | La primera propuesta suele ser la que quería | Transiciones elegidas | Pocos datos por tipo | Tasa de «primera opción aceptada» |
| Búsqueda de sonidos por parecido (M3) | «Otro como este, más oscuro» sin recorrer carpetas | Renders + samples | Licencia; el embedding no entiende «suena a Juan» | Tiempo hasta encontrar frente a carpetas |
| Mezcla de partida con medición de nivel por capa (LUFS) + tabla por rol | Capas del Pi y del Brain parejas desde el inicio | Ninguno | ⚠ pisar un fader movido: mantener la regla 6 | Juan mueve menos los faders al empezar |
| Motor de mezcla de GroovePilot (`mix_analysis`) sobre la grabación del set, en el Mac | Informe después del set | Grabación de sala | Depende de nube hoy (`motor-asistencia-v2.md:63-64`): **solo si corre local** | Juan aplica alguna sugerencia |
| Chop de voz por reglas (cortes por sílaba, a la escala, en los huecos del lead) | Ya previsto (`motor-recomendaciones.md:31`) | Material vocal de Juan (hoy no tiene) | Derechos del material | Juan usa algún chop |
| Aprender microtiming y velocity de lo que Juan toca en los FSR | Borradores con su mano | Tomas con FSR | ⚠ aplicar su «swing» donde no lo pidió: ofrecerlo como opción | A/B contra cuantizado |
| Salud del set: detectar anomalías en STATS, temperatura, cortes | Aviso antes de un fallo en vivo | Logs (G27) | Falsas alarmas | Sets sin sorpresas |

### Exploratorio

| Idea | Valor | Dato | Riesgo | Evaluación |
|---|---|---|---|---|
| M5 · modelo de ediciones | Borradores más cercanos | Ediciones de varios proyectos | «Corregir menos» no prueba «mejor» (`ia-local` §11) | Proyectos no vistos; decide Juan |
| Modelo simbólico pequeño que continúa un motivo de Juan | Variaciones de su material | Sus MIDI (1 265, de pocas recetas) | ⚠ **compone por él** si genera partes enteras sin pedirlo; datos insuficientes | Solo como una de 3 opciones, marcada |
| Lector de arco del set (energía en el tiempo) que sugiere cuándo ir a break | Ayuda en sets largos | Muchos sets | ⚠ **decide el arreglo**: solo sugerir, nunca ejecutar | Juan sigue la sugerencia o no; se registra |
| LLM local en el Mac como agente con render (uso H del gemelo) | «Más cálido» → dos alternativas comprobadas | Gemelo con `gf-render` | Latencia; afirmaciones sin medir | Contrato de `ia-local` §4 |
| Audio → patch (uso J) | Punto de partida desde una referencia | Gemelo nivel 2 | Investigación larga | Audio obtenido, no parámetros |
| TinyML de `04` con datos del gemelo | Mejor precisión en Scale Lock del Brain | Uso K | No aporta a la consola | Métricas de `04` §8 |
| Audio generativo | — | — | ⚠ identidad y licencias; último del orden aprobado | — |
| Generar voces con IA | — | — | Descartado por Juan (`motor-recomendaciones.md:70`) | — |

## 7. Raspberry Pi: lista de verificación

| Tema | Estado en el repo | Qué falta contemplar |
|---|---|---|
| Imagen reproducible | `e0-preparar-pi.sh` cubre paquetes, overlays y cmdline | PipeWire, `linger`, `wireplumber` (`e3-relojes…:31`), udev, servicios y GUI están a mano. Extender el script o un segundo script. Yocto/Buildroot: después de congelar (`03` §3) |
| Almacenamiento | microSD | NVMe por PCIe (HAT M.2) frente a SD: más resistencia y carga rápida de samples; ocupa el PCIe y espacio en la caja. Decidir antes de la mecánica |
| Solo lectura | Mencionado (`03:45`) | Raíz en overlay + partición `/data` aparte, escribible, para sets, registro y modelos. Con overlay activo, lo escrito en la raíz vive en RAM. Hay reportes de que la opción de `raspi-config` no persistía en Bookworm de 64 bits: **verificar en Trixie** |
| Apagado seguro | Nada | El Pi 5 tiene botón de encendido en la placa y pines para uno externo (J2): llevarlo al panel. Con raíz de solo lectura, cortar la luz es tolerable salvo para `/data` (escrituras con `fsync`, solo agregar) |
| Arranque | Nada | Medir tiempo hasta sonido. Orden: audio → núcleo → GUI. Splash propio (`logo.nologo` ya está) |
| Watchdog | Nada | `systemd` `WatchdogSec` por servicio; watchdog de hardware del Pi para cuelgue total |
| Actualización | Flasheo manual de la A desde el Pi (`05-e3` §5) | Un comando desde el Mac por Ethernet que actualice repo, entorno `uv` y las dos Teensy, con versión visible en Ajustes. Sin internet en el Pi: fuentes y ruedas de Python empaquetadas (ADR-007 ya lo pide para fuentes) |
| Sincronía con el Mac | Nada | Mac tira sets y registro; Mac empuja biblioteca, samples y modelos. Nunca automático hacia fuera de la red local (regla local-first) |
| Respaldo | El SSD de Juan no tiene respaldo | La consola no debe ser la única copia de nada, ni el SSD. Como mínimo, el registro en dos discos |
| Red | Ethernet decidido (`03:59`) | IP fija o enlace local; SSH solo con llave; ningún servicio escuchando en Wi-Fi; el servidor del motor hoy solo escucha en `127.0.0.1` (`servidor.py:127`) y así debe seguir. `user-data` con contraseña de Wi-Fi ya se dejó fuera a propósito |
| Link | `aalink` propuesto (`00-concepto.md:221`), sin probar (E6) | Link usa multidifusión: comprobar que funciona en el enlace directo Mac–Pi sin router. Quién manda el tempo cuando Live está en Link (G09) |
| PipeWire / JACK / ALSA | E2 con ALSA directo; E3 con PipeWire 1.4.2 | Elegir uno para el producto. Con dos dispositivos y remuestreo, PipeWire ya está configurado; fijar cuánto y latencia en su config, no por defecto |
| CPU | Kernel 6.18 PREEMPT, sin ajustes | Gobernador `performance`; ver G19. PREEMPT_RT solo si aparece un corte |
| Energía | Fuente oficial de 27 W | Con fuentes que no negocian 5 A el Pi limita los USB (600 mA frente a 1,6 A, `usb_max_current_enable`): **confirmar valores en la documentación oficial**. Presupuesto G30 |
| Térmica | 58 °C con carga al aire | Ventilador activo dentro de la caja, entrada y salida de aire, ruido del ventilador frente a la entrada de línea |
| Pantalla | Overlay Waveshare 7" DSI, vertical | Rotación en Qt (`QT_QPA_EGLFS_ROTATION`) y en el touch (G32); arranque directo en EGLFS sin escritorio |
| RTC | Nada | El Pi 5 tiene RTC con conector para pila. Sin red, las marcas de tiempo del registro salen mal: poner la pila o sincronizar la hora desde el Mac |
| Samples y biblioteca | Sonidos generados al arrancar (`reproductor.py:114-120`) | Carga a RAM antes de la frase; formato y frecuencia (48 kHz); Tribe: licencia sin revisar (`00-concepto.md:235`) |
| Multiproceso | Regla en ADR-007 | Tres procesos y su IPC sin definir (G02, G18) |

## 8. Preguntas abiertas para Juan

- **P1.** ¿Con qué se acepta y se salta? Botones dedicados, pads del NeoTrellis, o clic de encoder bajo la tecla de pantalla (C3). Para E5 basta elegir uno.
- **P2.** ¿E5 puede pasar **solo con drums del Pi** (como dice `01-prototipo-mesa.md:83`), dejando el Brain para después del arreglo del USB audio?
- **P3.** Para los roles de synth (BASS, CHORDS, LEAD, PAD) en STANDALONE: ¿audio renderizado por el gemelo en el sampler del Pi, multitimbralidad en la Teensy A, o una sola parte de Brain en vivo y el resto samples? (G10)
- **P4.** ¿Cuántas sesiones le das al arreglo del USB audio antes de ir al plan B? ¿Aceptas en el prototipo un Brain que solo se oye por audífonos? (G13)
- **P5.** En modo CONTROLLER, ¿el Pi hace de puente hacia Live por red, o la S lleva un segundo cable al Mac? (G06)
- **P6.** «Nada sale del Mac»: ¿el Pi cuenta como «dentro»? ¿El registro del set puede vivir en el Pi hasta que el Mac lo recoja? (C9, G26)
- **P7.** ¿La regla «no avanza solo» del motor v2 vale igual en vivo, entendida como «nada entra a la sala sin ACCEPT»? ¿Y la transición automática, una vez pulsado TRANSICIÓN? (C8)
- **P8.** ¿Commit de las mediciones de E3 y push de las ramas `codex/*`? (G22)
- **P9.** ¿Hace falta ya el ADR de consola (producto personal frente a `00`, y la frase de `00:174` sobre el audio en el Pi)? (C1, H01)
- **P10.** ¿Los modelos TinyML de `04` quedan fuera de la consola hasta que haya una entrada externa que los pida? (§4.2)
- **P11.** ¿NVMe o microSD? Afecta la caja y descarta cualquier otro uso del PCIe.
- **P12.** En la GUI, ¿«Brain» es la Teensy A o el motor? (C16)

## 9. Fuentes

### Leído completo
`CLAUDE.md` (raíz) · `apps/docs/consola/00-concepto.md`, `01-prototipo-mesa.md`, `02-inventario-groovepush.md`, `03-arquitectura-capas.md`, `04-reproductor-e2.md`, `05-e3-teensy-a.md`, `07-gui.md` · `consola/mediciones/` (E0, E1, E2, E3, `e3-relojes-20261109.md` y su `.txt`) · `adr/ADR-007` · `04-ai-architecture.md` · `produccion/motor-asistencia-v2.md`, `motor-recomendaciones.md`, `motor-mezcla.md` · `tools/consola/reproductor.py`, `e0-preparar-pi.sh`, `pi/*` · `.github/workflows/ci.yml` · en `codex/superficie-pi`: `06-teensy-s-e4.md`, `08-superficie-pi.md`, `apps/consola/processor/PIN_MAP.md`, `src/consola_s/main.cpp`, `tools/consola/superficie.py`, y el diff de `platformio.ini`, `Config.h`, `NeoTrellisController.cpp` · `apps/docs/auditoria-consola-unificada-2026-11-09.md` (checkout principal, sin commit).

### Leído en parte (secciones citadas)
`02-bridge-protocol.md` (§1.2–§1.3 y búsqueda de «CDC») · `01-architecture.md` (§5.2–§5.3, índice) · `00-master-strategy.md` y `06-implementation-roadmap.md` (búsquedas) · `produccion/ia-local-gemelo-digital.md` (§1–§9.3) · `ia-local-refinamiento.md` (§1–§3, §8–§11) · `auditoria-datasets-adn-2026-10-28.md` (§1–§2) · `motor-asistencia.md` (§11) · `adr/ADR-006` (cabecera y contexto) · `design/claude-design-prompt-consola.md` (búsquedas) · `design/claude-design/Console MVP Screens.dc.html` (texto extraído: *Layout notes* y teclas) · `apps/firmware-teensy/platformio.ini` (`[env:consola]`), `src/audio/audio_graph.cpp` (líneas 28–75, 105–150, 250–285), `sketches/49-studio-brain.cpp` (cabecera, `note_on`, búsquedas) · `tools/motor/gf_motor/` (firmas de `motor.py`, `live.py`, `servidor.py`), `README.md`, `pyproject.toml` · `tools/paleta-lab/gf_log.py` (firmas) · `tools/consola/relojes.py`, `latencia.py` (cabeceras) · `apps/training/` (estructura y `CLAUDE.md`) · `apps/firmware-teensy/src/ml/` (lista y cabecera de un modelo).

### No leído
`apps/firmware-teensy/CLAUDE.md` y `apps/bridge-protocol/CLAUDE.md` · `05-fx-architecture.md` · ADR-002 a ADR-005 (solo los conozco por citas de otros documentos) · specs y ADRs de GroovePush en `consola/groovepush/` (citados a través de `02-inventario-groovepush.md` y de la auditoría previa) · los demás `motor-*.md`, `protocolo-captura.md`, `library-grooveforge.md` · `tools/perc-synth`, `tools/preset-librarian`, `tools/gemelo-baseline` (solo listados) · KiCad · las otras pantallas del export de diseño · `theory/produccion-identidad-ia-local-refinamiento.md`. **No existen:** ramas `codex/gui-fase1` y `codex/usb-audio-eval`, `09-gui-fase1.md`, `10-usb-audio-teensy.md`, `apps/consola/gui-console/`. Las afirmaciones sobre lo no leído están marcadas como citas de segunda mano o no se hacen.

### Externas
- AI HAT+ (13 y 26 TOPS, 70 y 110 USD al lanzamiento, octubre de 2024; precio actual sin confirmar): [Tom's Hardware](https://tomshardware.com/raspberry-pi/raspberry-pi-release-higher-performance-ai-hat-13-and-26-tops-variants), [Hackster](https://www.hackster.io/news/raspberry-pi-targets-on-device-edge-ai-with-its-26-tops-ai-hat-add-on-d52857d58243).
- Alimentación USB del Pi 5 y `usb_max_current_enable`: [Raspberry Pi, *USB Power Delivery on Raspberry Pi 5* (documento técnico)](https://pip-assets.raspberrypi.com/categories/685-app-notes-guides-whitepapers/documents/RP-009856-WP-1-USB%20Power%20delivery%20on%20Raspberry%20Pi%205.pdf). Los valores de 600 mA y 1,6 A vienen de guías de terceros: pendientes de confirmar en la documentación oficial.
- Overlay de solo lectura: [Adafruit, *Read-Only Raspberry Pi*](https://learn.adafruit.com/read-only-raspberry-pi); problema reportado en 64 bits: [foro Core Electronics](https://forum.core-electronics.com.au/t/pi-5-bookworm-64bit-cannot-set-filesystem-to-read-only/21769). No encontré la página oficial en esta búsqueda.
- USB audio de Teensy y clics: [foro PJRC, *USB Audio Clicking Noise*](https://forum.pjrc.com/threads/54239-USB-Audio-Clicking-Noise?p=191704), [foro PJRC, *USB Audio Frame Sync on Teensy 4.0*](https://forum.pjrc.com/threads/61142-USB-Audio-Frame-Sync-on-Teensy-4-0?p=242039). No confirmé una biblioteca que lo resuelva.
- **Pendientes de verificar, sin fuente consultada:** ruedas de ONNX Runtime para ARM64 con la versión de Python del Pi; rendimiento de `llama.cpp` en Pi 5; apilado del AI HAT+ con un HAT de audio; RTC y conector J2 del Pi 5; licencia de CLAP; multidifusión de Link en enlace directo.
- Referencias ya citadas en los docs del repo (no reverificadas): PJRC SGTL5000 y USB MIDI, Sharp GP2Y0A41, TI ADS1115, Interlink FSR, Gibson, Titze, Haas.

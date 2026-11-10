# Modo SYNTH y touch auxiliar

2026-11-10 · Juan · spec previa al código · ADR-007/008; contratos 07, 14 y 18.

## Propósito y relación con E5

SYNTH convierte la consola en un sintetizador de cuatro partes del host del Pi.
ModeSelect → SYNTH → PARTS → páginas del engine (metadatos del host, bancos de ocho).
Es un hito anterior a E5: no arranca motor, frases ni ACCEPT. No cambia el doc 19,
los modos de demostración existentes ni los specs 00–06.

## Mapa musical

| Control | Acción SYNTH |
|---|---|
| Encoders CC16–23 | Ocho parámetros del banco; relativo → absoluto con step/kind y clamp del host; SHIFT ×0,1 en continuos, discretos conservan step |
| Clic encoder 1–8 | Soft key correspondiente; PARTS abre parte; páginas: anterior/siguiente/banco/fino/escala/—/presets/back |
| Jog CC35 / clic | Presets: desplazar/cargar; PARTS: parte; parámetros: páginas; SHIFT+jog y clic: recorrer/activar cualquier destino touch |
| LEFT/RIGHT, PAGE_LEFT/RIGHT | Foco auxiliar / páginas; BACK vuelve a PARTS; MENU abre ajustes; SEARCH presets |
| NeoTrellis 4×8 | Teclado in-key: grado = columna + 3×fila desde abajo, como cuartas diatónicas Push; raíz y escala seleccionables; OCT −/+ desplaza 12 semitonos |
| FSR notas36–39 | Propuesta inicial implementada: cuatro notas MIDI 36/38/42/46 con velocity en la parte elegida (referencia GM; el engine sigue siendo synth); alternativa pendiente: acordes diatónicos |
| Faders CC24–27 | Nivel partes1–4, −60…+12 dB lineal en dB |
| Sharp CC33/34 + presencia108/109 | OFF por defecto; requiere master + mano ON en Settings. Cutoff / MOVE de parte elegida según descriptor (HYBRID: cutoff de ambas mitades); presencia falsa inhibe cambios; ausencia mantiene último valor |
| Expresión CC28/29 | Mismas macros cutoff/MOVE, sin gate de presencia |
| MASTER / CUE / IN | Sala PipeWire / @GF HP 0–127 / @GF IN 0–15 (misma dirección que firmware18; mayor índice = menor sensibilidad) |
| USB MIDI externo en Pi | Notas, velocity y liberaciones a parte seleccionada; con transporte MIDI también bend y sustain |
| STOP / salir de SYNTH | Liberar notas mantenidas (también al cerrar o perder entrada MIDI) |

Cada note-on recuerda parte y nota hasta note-off: cambiar parte, raíz u octava
no cambia el destino de la liberación. Notas duplicadas de distintas fuentes se
referencian para no cortar otra tecla. No hay interpretación musical del selector
CC36: sólo informa atenuación física.

**Diferencia de contrato explícita:** 18 no tiene `@GF MASTER`. No se inventa ese
comando: MASTER controla un nodo de sala PipeWire por `wpctl set-volume` (nodo
configurable); CUE/IN usan CDC explícito, sin autodetectar otro dispositivo.
CUE/IN ya se aplican localmente en firmware completo; repetir su valor es idempotente.

## Proceso, latencia y LEDs

Se elige un puente en el proceso GUI: comparte parte/banco/metadatos sin duplicarlos
en otro proceso ni añadir IPC. El DSP permanece fuera. Un proceso separado aislaría
mejor un fallo de Python, pero necesitaría sincronizar ese mismo estado. Lector `superficie.decodificar` en callbacks MIDI.
Las notas salen **directamente al puerto MIDI del host**, antes de encolar feedback
visual; no pasan por QTimer, afterAnimating ni QLocalSocket GUI. Es el camino de
menor trabajo: evita JSON/hilo de control; el host respeta los offsets recibidos en el grafo. El puerto
debe estar expuesto por el puente MIDI de PipeWire/ALSA o JACK; nombre configurable.
Por defecto `--note-transport auto` prefiere MIDI y anuncia fallback si ese puerto
no está expuesto; `--note-transport midi` exige MIDI. Alternativa `--note-transport socket`: conexión Unix independiente,
envío inmediato de `note`, lectura continua de ACK fuera del frame (no bend/sustain).
No se promete que socket sea tan rápido como MIDI ni se ocultan errores de puerto.

Parámetros/navegación/medidores entran por SurfaceBatcher, máximo un commit por
frame. MIDI no toca QObject de presentación desde el callback: recibe un snapshot
inmutable de parte/escala. Objetivo USB MIDI→audio p95 <5 ms (CLAUDE.md/01): USB y puente <1 ms,
bloque128/48k ≤2,67 ms y resto de salida <1,33 ms. Ese presupuesto corresponde a128; el default estable actual es256 (5,33ms sólo de quantum), por lo que aún no se certifica p95<5ms. El detector FSR de 18
añade hasta 3 ms de adquisición antes del mensaje; medir pad/FSR por separado. Son presupuestos,
no mediciones; verificar con audio físico y carga de GUI en Pi (ADR-007).

Propuesta LED mínima, compatible con el mapa entrante de 18: **salida canal16**,
CC20/21/22 = RGB de 7 bits, note-on48+índice con velocity1 = aplicar color al pad;
velocity0 = apagar. Raíces blancas; resto color BASS/CHORDS/LEAD/PAD. Deduplicación,
sin animaciones bloqueantes. El puente emite; firmware 18 aún no consume este
formato. No se afirma feedback físico hasta implementar/validar receptor Teensy/M4.

## Touch en todo el registro

Un `TouchTarget` común declara callback y categoría de acción hardware. Tap activa;
arrastre/flick desplaza listas; los knobs sólo seleccionan el encoder/foco (bool
puede conmutar). Pads como notas y teclas de piano no tienen activación musical;
piano roll sólo desplaza/zoom. Controles sin servicio abren detalle local rotulado
como demo/read-only; no simulan una operación de sistema o audio exitosa.

| Categoría de destino touch | Equivalente hardware |
|---|---|
| soft-key | Clic de encoder correspondiente |
| parameter | Encoder correspondiente selecciona/edita; clic para toggle si procede |
| navigation | SHIFT+jog recorre destinos visibles; clic JOG activa (LEFT/RIGHT también recorre) |
| choice | SHIFT+jog + JOG selecciona tab/banco/algoritmo/escala/modo/tile/toggle |
| list | Jog desplaza selección; JOG activa fila; drag/flick touch desplaza |
| text | SHIFT+jog + JOG pulsa tecla del teclado de texto; SAVE/CANCEL también soft keys |
| detail | SHIFT+jog + JOG abre detalle/foco de ajuste demo o read-only |

La prueba recorre cada entrada del registro, comprueba handlers y categorías de
los destinos, y rechaza MouseArea sin el componente común. Las pruebas funcionales
cubren navegación/selección, escala, knobs y browser. La inspección visual completa
sigue necesaria para detectar nuevos elementos que parezcan clicables sin marcar.

## Desarrollo y puerta física

`--surface-port 'GF Consola' --host-midi-port <puerto>`; `--external-midi-port`
repetible: substring explícito, `auto` para todos los teclados externos, `none` para desactivar. `--surface-cdc`
habilita HP/IN. `--fake-surface` no abre hardware: 1–8 / QWERTYUI / ASDFGHJK / ZXCVBNM, (de arriba abajo)
simulan los 32 pads, flechas encoders, SHIFT fino, [/] octava, F3 PARTS. Teclas liberan notas;
no autorrepeat musical; perder foco de ventana libera las notas simuladas.
ALT+flechas/Enter simula recorrer/activar destinos touch. `--preview` es diseño sin sonido.

`pi/run_synth.sh` no instala: touch rotate=0 (identidad, sin invertx) e imagen QML90, socket
$XDG_RUNTIME_DIR/gf-synth.sock, puerto GF Consola. Pasa `--external-midi-port "${GF_EXTERNAL_MIDI_PORT:-auto}"`: sin flags abre todos los inputs externos (p.ej.Arturia MiniLab); excluye GF Consola/GF Superficie, Midi Through, puertos propios RtMidi/GUI y host. Un `--external-midi-port` explícito prevalece sobre el default/env. Resolución deduplicada; un patrón ambiguo sigue siendo error. `GF_EXTERNAL_MIDI_PORT=none` desactiva descubrimiento. Descubrimiento al arrancar, sin promesa de hotplug automático. Dependencias MIDI opcionales
mido/python-rtmidi y pyserial para CDC, instaladas previamente por el usuario.

Preguntas abiertas para Juan: ¿FSR batería o acordes? ¿Aprobar formato LED y
receptor en firmware? ¿Nodo PipeWire fijo para sala? **¿Drag-to-edit en knobs?**
Se propone como opción futura; esta entrega sólo tap para foco. Falta medir Pi,
latencia, xruns, touch físico y receptor LED; capturas desktop no certifican hardware.


## Resultado de escritorio previo a la corrección de hardware

GUI: **305 passed, 13 skipped**; tools/consola: **450 passed**. Los skips existentes
son listener Unix restringido y General Sans local. `uv run pytest` encontró caché
bloqueada; se verificó la misma suite con `uv run --no-project` y Python instalado,
sin descargas. `bash -n` y `git diff --check` sin errores; runner verificado con
intérprete sustituto (sin DRM). [Informe y comandos](../../consola/gui-console/screenshots/modo-synth/REPORT.md),
[capturas y procedencia](../../consola/gui-console/screenshots/modo-synth/index.html).

Dos intentos de staging fallaron con `.git/index.lock: Operation not permitted`:
spec y código quedan **sin commits**, rama `codex/modo-synth`, base `7f106b0`, sin push.
No se modificaron otros checkouts, docs00–06 ni doc19. Puertas físicas y preguntas
abiertas siguen pendientes; no se declara aprobado ADR-007 desde desktop.


## Banco mínimo y RT tras medición del Pi (2026-11-10)

Con `consola_uno` (sin flag diag) probar FSR1/2 en14/15: la superficie envía notas36/37 con velocity y note-off; `synth_surface.py` las transforma en36/38 en la parte seleccionada. Sharp **ADS0x48 AIN2** envía CC33 y nota108 para presencia: la GUI conserva el último cutoff al salir; no controla MOVE (segundo Sharp). AIN0 sigue como fader1 CC24. MASTER22 al aire está **deshabilitado** (`pot_mask=0`), al igual que CUE/IN/selector en mínimo; conectar pot y habilitar `GF_CONSOLA_WIRED_POTS=1` sólo si se quiere probar MASTER. No cambia el cableado.

`run_synth.sh` garantiza GUI SCHED_OTHER. Host/data-loop solicita RT mediante PipeWire y usa256 por defecto; permisos, límites y comprobación **por TID** en [16, sección RT](16-synth-host.md#corrección-pi-con-gui-y-prioridad-rt-2026-11-10). Conservar el quantum persistido256;128 es opción después de verificar RT y repetir xruns con GUI. Las pruebas de escritorio cubren mensajes MIDI reales decodificados de los dos FSR/Sharp, selección de varios teclados con auto y override/env del launcher; no prueban latencia física.

Corrección actual: **308 passed, 13 skipped** en la suite GUI aislada; builds y tests firmware/host en la [medición](mediciones/synth-pi-20261110.md#verificación-de-esta-corrección). No hubo flash ni cambios de configuración en Pi.


## Corrección de soft keys y preferencias de superficie (2026-11-10)

Los clics MCP **notas80–87** activan las ocho soft keys rotuladas; la nota88 es
JOG, no encoder8. Touch y MIDI pasan por `ConsoleState.pressEncoder`. El modelo
de teclas ahora actualiza roles sin recrear los delegates: un evento de reloj o
medidores entre press/release ya no destruye el destino touch. Las teclas
inhabilitadas tampoco ejecutan acciones por MIDI. HYBRID: MORPH abre el foco de
morph en LAYER; LAYER abre presets. Los ajustes comunes regresan al modo activo.
La prueba funcional recorre cada pantalla SYNTH y DRUM, cada tecla rotulada,
touch real sintetizado a **90° QML** con telemetría durante press/release, y MIDI
80–87 decodificado por el puente; verifica navegación, estado o comando del host.

**Settings → Surface / Hands** (MENU, o Settings desde Mode Select) ofrece
Hands master, Left IR y Right IR. **Los tres vienen OFF**: para usar una mano,
activar master y esa mano. Touch o clics de encoder1/2/3 conmutan los mismos
valores; persisten localmente en `~/.config/gf-console/settings.json`
(`$XDG_CONFIG_HOME/gf-console/settings.json` si se define; override de pruebas
`GF_CONSOLE_SETTINGS`). Un error de lectura vuelve a defaults seguros y muestra
el error; un error de escritura se muestra sin simular persistencia.

Con master o mano OFF, **CC33/34 y notas108/109 se ignoran** en SYNTH, DRUM y
LIVE, tanto al recibir MIDI como al aplicar la cola pendiente. No se envían al
host, no mueven macros ni muestran halos. Deshabilitar borra presencia; al
reactivar hace falta una presencia nueva. CC28/29 (pedales de expresión) conserva
su función independiente. No cambia el firmware ni el cableado Sharp.
La fila Pad velocity de esta misma pantalla configura sólo M4 en DRUM (doc22).

Capturas de la corrección y condiciones de escritorio:
[Surface/Hands y rotación90](../../consola/gui-console/screenshots/surface-controls/index.html).
La simulación no certifica touch físico, audio ni latencia del Pi.

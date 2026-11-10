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
| Sharp CC33/34 + presencia108/109 | Cutoff / MOVE de parte elegida según descriptor (HYBRID: cutoff de ambas mitades); presencia falsa inhibe cambios; ausencia mantiene último valor |
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
bloque128/48k ≤2,67 ms y resto de salida <1,33 ms. El detector FSR de 18
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
repetible y explícito, excluyendo la superficie para evitar bucles. `--surface-cdc`
habilita HP/IN. `--fake-surface` no abre hardware: 1–8 / QWERTYUI / ASDFGHJK / ZXCVBNM, (de arriba abajo)
simulan los 32 pads, flechas encoders, SHIFT fino, [/] octava, F3 PARTS. Teclas liberan notas;
no autorrepeat musical; perder foco de ventana libera las notas simuladas.
ALT+flechas/Enter simula recorrer/activar destinos touch. `--preview` es diseño sin sonido.

`pi/run_synth.sh` no instala: touch invertx:rotate=0 e imagen QML90, socket
$XDG_RUNTIME_DIR/gf-synth.sock, puerto GF Consola. Dependencias MIDI opcionales
mido/python-rtmidi y pyserial para CDC, instaladas previamente por el usuario.

Preguntas abiertas para Juan: ¿FSR batería o acordes? ¿Aprobar formato LED y
receptor en firmware? ¿Nodo PipeWire fijo para sala? **¿Drag-to-edit en knobs?**
Se propone como opción futura; esta entrega sólo tap para foco. Falta medir Pi,
latencia, xruns, touch físico y receptor LED; capturas desktop no certifican hardware.


## Resultado de escritorio

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

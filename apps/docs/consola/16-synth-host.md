# gf-synth-host: diseño y verificación

Fecha: 2026-11-10. Contrato autoritativo: `14-synth-host-contrato.md`.
Implementación C++17 custom, sin JUCE ni Python/Qt en audio. Reutiliza las
bibliotecas de `brain-plugin` mediante `add_subdirectory`; una opción temprana
`GF_ENGINE_LIBRARIES_ONLY` evita compilar referencias, pruebas y VST del plugin.
No duplica DSP. `dx7` conserva FM **4-OP** y `dx7_6op` registra el core **6-OP**,
con entradas independientes en hello, presets y list_params.

## Arquitectura y tiempo real

Un hilo de control atiende varios clientes Unix JSON-lines, disco y JSON (json-c).
Un hilo de audio (más el bucle auxiliar de PipeWire cuando corresponde) procesa
cuatro instancias independientes y diez salidas float:
`part1_L/R` … `part4_L/R`, `mix_L/R`. 48 kHz, quantum predeterminado **256**; `--block 128` es opt-in tras comprobar RT; el backend
puede negociar otro tamaño. Cada parte aplica nivel y mute antes de sus salidas;
mix suma esas salidas, sin normalización ni limitador implícito. Inicio: Model D,
Juno, OB-6, HYBRID; roles BASS/CHORDS/LEAD/PAD, −12.0412 dB (ganancia 0.25).

SPSC acotadas control→audio y audio→control, atomics lock-free verificados. ACK y
cambios de parámetros comparten una FIFO: un evento viejo no puede cruzar la
confirmación de un cambio de engine. El estado espejo se actualiza al recibir ACK.
Cambios
aplicados al inicio del bloque, ACK después de aplicar; la respuesta de éxito
confirma aplicación. Preparar/cargar/reemplazar engines ocurre en control; audio
intercambia punteros y devuelve el anterior, que destruye control después del ACK.
No hay `prepare`, disco, JSON, logs, locks ni reserva/liberación de heap en callback.
Voces, delay, plate y buffers se preparan antes. FTZ por callback (FPCR.FZ ARM64;
MXCSR.FTZ/DAZ SSE2). Trabajo por bloque acotado, clientes lentos desconectados,
colas llenas aplican contrapresión; no se pierde un ACK. Los parámetros pendientes
se coalescen hasta que se puedan publicar. Límites: 32 clientes, entrada 64 KiB,
salida pendiente 256 KiB/cliente, 512 parámetros y 2048 eventos MIDI por bloque. El callback mide duración monotónica;
`cpu` informa pared DSP / duración de audio ×100, media y máximo de ventana de 1 s,
sin dividir por cuatro núcleos. `p99_upper_pct` añade una cota de p99 por ventana
(histograma preasignado, resolución 0.5 puntos porcentuales; saturación usa máximo).
No incluye el trabajo de disco/JSON del control ni la adquisición de buffers del
backend. Telemetría RT acotada; pérdidas se contabilizan.
Cambiar engine/preset corta sus voces/colas; editar parámetros conserva voces.

Registro desacoplado (`params.h`, `registry()` en `engine.cpp`): nombre, ID firmware,
fábrica, defaults, catálogo, banco de fábrica, nombre y codecs. Un adaptador nuevo
implementa `Engine` y esos callbacks; una entrada lo anuncia sin cambiar socket,
persistencia ni backend. `Config::six` guarda VCED y máscara de enable como POD; el adaptador 6-OP usa
`Dx7SixPreview` y los codecs compartidos (firmware id 7). `Config::extension`
conserva espacio para adaptadores futuros. La tabla 6-OP se convierte a offsets
POD U8/bit, con clamping y redondeo idénticos al setter de firmware.
Los engines existentes usan los previews y el bus FX/GROOVE del
bench; bloqueador DC de 5 Hz por salida de parte. No certifica fidelidad al hardware.

## Backends y MIDI

**Principal: PipeWire nativo `pw_filter`**, puertos DSP mono y un puerto MIDI del
grafo; menor dependencia operativa que JACK y acceso al reloj/posición del driver.
No crea driver propio: Audio+ PCM5102A debe conducir el grafo a 48 kHz. WirePlumber
no debe conectar simultáneamente mix y partes a sala (doblaría la señal). Ruteo
explícito con `pw-link`; no cambia configuración ni instala servicios.
Alternativa JACK: mismo callback/puertos, vía `pw-jack` en Pi o JACK en Mac.
CMake detecta PipeWire ≥1.4 y JACK por pkg-config; si faltan, queda offline. Petición de backend
no compilado falla, nunca sustituye silenciosamente por offline.

MIDI por el mismo grafo, sin segundo reloj ALSA: canales 1–4 → partes 1–4;
notas/velocity, bend ±2 semitonos, sustain CC64 y CC14 DRIFT, 15 GRIT, 16 POCKET,
19 MOVE, 71 resonancia normalizada, 74 cutoff logarítmico con límites firmware.
CC modifica parámetros al inicio del bloque y publica `param`. Los timestamps de
notas, bend y sustain del puerto se respetan dentro del bloque. Otros canales/mensajes se ignoran.
El backend offline tiene reloj de pared para pruebas del servidor;
`--render` usa reloj de muestras, sin dormir, y escribe WAV fuera de audio.

Referencias API: [pw_filter 1.4](https://docs.pipewire.org/1.4/group__pw__filter.html),
[MIDI PipeWire](https://docs.pipewire.org/page_midi.html),
[JACK API](https://jackaudio.org/api/).

### Entrada USB MIDI / UMP (2026-11-11)

Spec de esta corrección: `midi_in` anuncia `application/control`, preferencia
`SPA_CONTROL_UMP` (`format.dsp = 32 bit raw UMP`) y una alternativa
`SPA_CONTROL_Midi` mediante `SPA_PARAM_EnumFormat` / `SPA_FORMAT_CONTROL_types`.
En PipeWire 1.4 `pw_filter_add_port` deriva la máscara del nombre DSP; anunciar
sólo `8 bit raw midi` restringe la negociación a Midi. El nombre publicado puede
seguir apareciendo como `8 bit raw midi`: comprobar EnumFormat, no sólo la etiqueta.
Cada item de la secuencia selecciona su decoder por `control.type`; el payload
es `SPA_TYPE_Bytes`, UMP contiene words uint32 en orden nativo de CPU, no bytes
MIDI concatenados. Conservamos `control.offset` y orden de eventos.

Conversión custom C++17 independiente del backend, sin heap ni locks:

- MIDI 1.0 crudo: mensajes channel voice completos de 2/3 bytes (sin running status).
  UMP tipo `0x2` (1 word): conserva status, canal y datos MIDI 1.0.
- UMP tipo `0x4` (2 words): Note On/Off, CC, bend, Program Change y Channel Pressure.
  Velocity: bits superiores `velocity16 >> 9`; CC y pressure: `value32 >> 25`;
  bend: `value32 >> 18`, dividido LSB/MSB MIDI de 7 bits. Se conservan extremos
  y centro sin redondeo. Note On MIDI 1.0 velocity 0 sigue siendo off; Note On
  MIDI 2.0 que reduce a 0 usa 1 (en MIDI 2.0 cero también es Note On).
  Program Change extrae program del segundo word; bank select opcional se descarta.
  Los atributos de nota se descartan. Group UMP no crea partes: todos los groups
  comparten el mapa de canales existente.
- Se ignoran utility/JR, system realtime/common, SysEx7/8, flex/stream, mensajes
  reservados y controles MIDI 2.0 por nota (incluido pressure/bend), RPN/NRPN y
  relativos: el motor no ofrece esos destinos ni estado para traducirlos. No se
  ejecuta SysEx/preset ni se cambia transporte en RT. Program/Channel Pressure
  se convierten pero el motor actual no les asigna acción, igual que en MIDI1.
- Canal 1–4 → synth 1–4; canal 10 Note On notas 0..15 → pads 0..15 según
  [22 §5](22-modo-drum.md#5-extensión-del-contrato14). Otros canales se filtran.
  Una lista UMP puede contener varios paquetes: se saltan tipos no soportados
  por su longitud de 1/2/3/4 words; un paquete truncado detiene ese item, nunca
  se reinterpretan words de continuación como nuevas notas. Datos fuera de
  rango o mensajes malformados se rechazan. Límite compartido de 2048 items/
  paquetes escaneados y 2048 eventos por bloque, incluso si se ignoran mensajes.

Prueba nativa: vectores de mensajes/escala/extremos/velocity cero, longitudes y
paquetes inválidos; recorrido mixto UMP, orden/timestamps/canales, disparo de pad
en su muestra exacta y hook RT sin heap. No sustituye negociación real, hotplug
ni latencia en el Pi.

Referencias: [PipeWire 1.4.9 filter.c, add_control_dsp_port_params / pw_filter_add_port](https://raw.githubusercontent.com/PipeWire/pipewire/1.4.9/src/pipewire/filter.c),
[MIDI en PipeWire](https://docs.pipewire.org/page_midi.html),
[UMP y MIDI 2.0, M2-104-UM §3/4 y traducción](https://amei.or.jp/midistandardcommittee/MIDI2.0/MIDI2.0-DOCS/M2-104-UM_v1-0_UMP_and_MIDI_2-0_Protocol_Specification.pdf),
[regla de velocity cero al traducir](https://microsoft.github.io/MIDI/kb/data-translation/#translating-note-on-with-zero-velocity-to-note-off).

### Enlazar teclados USB en el Pi

Con el host PipeWire ya iniciado, desde la raíz del repo, en la misma sesión de
usuario PipeWire (sin sudo):

```bash
# Inspección sin cambiar enlaces:
python3 tools/synth-host/link_midi.py --dry-run
# Enlazar todos los inputs externos físicos MIDI/UMP actualmente presentes:
python3 tools/synth-host/link_midi.py
# O seleccionar los puertos de un controlador por substring:
python3 tools/synth-host/link_midi.py --external-midi-port 'Arturia MiniLab'
pw-link -l
# Alternativa manual, nombre exacto mostrado por pw-link -o:
pw-link 'PUERTO_MIDI_ORIGEN' gf-synth-host:midi_in
```

El helper usa `pw-dump` y `pw-link`, sin ALSA→socket ni dependencias Python extra.
`auto` selecciona outputs físicos MIDI/UMP (USB y otros puertos físicos), excluye
GF Consola/GF Superficie/consola_s_m4, Midi Through y puertos host/GUI/RtMidi.
`none` desactiva enlaces. El substring puede seleccionar varios puertos de un
controlador; nombres personalizados deben revisarse con `--dry-run`. No desconecta
rutas existentes y omite enlaces ya creados. Volver a ejecutarlo tras hotplug o
reinicio del host; no instala reglas ni daemon. `run_on_pi.sh` lo ejecuta al
arrancar con `GF_EXTERNAL_MIDI_PORT=auto` (default), substring o `none`, y guarda
`midi-links.txt`; su patrón socket de benchmark sigue ejecutándose aparte.

Para tocar en directo, detener manualmente el puente temporal `~/midi_to_host.py`
y evitar una segunda ruta para el mismo teclado. En este checkout la opción
`--external-midi-port` existente **es de la GUI** (`pi/run_synth.sh`), no del
binario host. Al enlazar directamente, lanzar la GUI con
`--external-midi-port none` o `GF_EXTERNAL_MIDI_PORT=none` para que no reenvíe
las mismas notas. La superficie sigue entrando a la GUI para su traducción.
La entrada directa conserva el canal enviado por el teclado; no sigue la parte
seleccionada en la GUI. Configurar canales 1–4 para synth y canal 10 notas 0..15
para DRUM. Una fuente USB MIDI1 publicada en UMP normalmente usa tipo `0x2`;
la etiqueta UMP no implica que envíe mensajes MIDI2 tipo `0x4`.

Validación pendiente en Pi: compilar con PipeWire ≥1.4; inspeccionar EnumFormat
con `pw-cli enum-params <id-del-puerto-midi_in> EnumFormat`; enlazar MiniLab,
verificar Note On/Off, sustain/CC74/bend, canales synth y pad, desconectar/reconectar
y repetir el helper. Comparar MIDI1 legacy y UMP sin puente y sin dobles notas,
comprobar xruns/latencia con el audio real. CTest nativo valida la conversión;
no certifica el enlace físico ni el callback PipeWire en macOS.

## Parámetros y presets

`list_params` se genera desde tablas inmutables del adaptador; `get/set` usan esas
mismas tablas. Rangos/defaults OSC/MOD/GROOVE y filtros provienen de `gf_params.h`
y `factory_preset`; los previews no exponen catálogo completo con etiquetas.
El host añade etiquetas, unidades, IDs string, ENV/LFO/FX y controles HYBRID según
sus setters. Es la única tabla del host; la GUI no mantiene copia. Bancos ≤8.
IDs comunes: `cutoff`, `resonance`, `attack`, `drift`, `grit`, `pocket`, `move`;
OSC/MOD: `osc.stop.slot` / `mod.stop.slot` (índices desde 0); FX: `fx.slot.on`,
`mix`, `0/1/2`. En HYBRID se antepone `a.` / `b.` a voz/OSC/MOD y se usan
`layer.morph`, `layer.delay`, `layer.transpose`, `layer.gain_a/b`.
Los enums FX conservan escala GFP 0–1, `step=1/(n−1)`; el índice de opción es
`round((value-min)/step)`. Las asignaciones continuas se recortan; `step` guía al
encoder, no fuerza cuantización de floats. Campos enteros/discretos sí se redondean.
HYBRID expone mitades A/B según engines de la capa de fábrica y controles de capa;
el catálogo estable corresponde a RETRO BRASS (Prophet/Juno). Las capas importadas
con otra pareja requieren catálogo por instancia: gap señalado abajo.

Presets factory en memoria desde los bancos GFP/GFL compartidos, nombres con
prefijo de engine para evitar INIT ambiguo. Importa `.gfp`/`.gfl` de `--repo-presets`
y del directorio de usuario; conserva binarios mediante codecs existentes.
Presets guardados por host son JSON versionado con todos los valores, engine,
nombre, role/tags/favorite. No sobrescribe nombres existentes ni acepta rutas.
Directorio configurable `--presets`, default `$HOME/.local/share/gf-synth/presets`.
Con raíz de solo lectura (ADR-008), usar `--presets /data/gf-synth/presets`
y ajustar ExecStart del servicio; no se cambia el default contractual en silencio.
Estado en `--state` (default junto a presets, `last-state.json`), debounce de 100 ms
(posible pérdida de esa ventana en un corte), reemplazo atómico
con fsync en control, conserva engines, patches, preset, niveles, mute y modified.
Presets JSON guardan un payload GFP/GFL codificado en hex además de metadatos;
no duplican ni reinterpretan el codec. `--repo-presets DIR` importa archivos
recursivamente al arrancar (este checkout no contiene `.gfp` sueltos; el banco
factory está en código). Archivo inválido produce diagnóstico y arranque de fábrica; no se ejecuta contenido.

## Recuperación y cobertura del contrato

`gf-synth-host.service` es unidad de usuario de ejemplo; Restart=on-failure,
WatchdogSec y sd_notify opcional si hay libsystemd. Solo alimenta watchdog si
avanza audio y responde el control; fallo de backend termina con error. Sin
libsystemd usar Type=simple y desactivar WatchdogSec (comandos abajo). Si muere
el Pi hay silencio (G16); reiniciar proceso no conserva notas sostenidas.

Se implementan hello, get_state, list_params, get_params (page por nombre),
set_param, set_engine, list_presets (filtros engine/role/tag), load/save_preset,
set_level, set_mute, note y subscribe. Eventos meters 30 Hz, state/param al cambiar,
cpu 1 Hz (cadencia cuantizada al bloque). Socket por defecto
`$XDG_RUNTIME_DIR/gf-synth.sock`; sin XDG_RUNTIME_DIR, fallback Mac
`/tmp/gf-synth-UID.sock`, modo 0600 y flock contra arranque doble.
IDs enteros de 64 bits, validación de tipos/rangos, tamaño de línea/clientes acotado.

### Ambigüedades / propuestas exactas para el contrato 14 (no lo modifican)

- §3 subscribe enumera meters/state/cpu pero §4 exige param. Propuesta: añadir
  `param` a topics y definir que `state` también incluye eventos `param` para
  compatibilidad. Implementación acepta ambos; no exige cambios a la GUI actual.
- §3 `page?` no define tipo: propuesta `page` = nombre string de página de
  list_params; ausente devuelve todo. Parámetro inexistente devuelve error.
- §3 set_param no nombra su eco: propuesta `{part,param,value}`; mutaciones de
  parte devuelven `{part:{…estado…}}`. `state` usa `{event:"state",part:{…}}`.
- Nombres de preset no son únicos entre engines y load no lleva engine. Propuesta:
  `name` identificador global presentado por list_presets (factory `engine:NAME`).
- Rango de nivel y escala velocity no definidos: propuesta db −60..+12 (clip),
  velocity entero MIDI 0..127, note entero 0..127; part entero 1..4.
- HYBRID arbitrario: propuesta extender list_params con `part?` o fijar catálogo
  por pareja; en esta entrega solo se aceptan capas Prophet/Juno para editar con
  catálogo estable. No se finge que otro catálogo representa sus parámetros.
- `xruns` no define alcance: se informa contador acumulado de incidentes del host
  (deadline/gaps/backend). No equivale a todos los errores del grafo; contrastar
  con `pw-top` durante prueba. Se añaden `deadline_misses`, `backend_xruns` y
  `clock_discontinuities`; la suma puede contar un mismo incidente por más de una
  causa. Propuesta fijar ese alcance y permitir los campos diagnósticos adicionales.
- No hay tempo/transporte en §3. Se usa 120 BPM, transporte corriendo y LFO libre,
  igual que el bench; `lfo_division` conserva el valor pero el sync está desactivado.
  Propuesta futura exacta: `set_transport {bpm,running}` y parámetro `lfo_sync`.
  Los límites del preview (incluido PUMP SOURCE sin disparo externo) se heredan;
  no se anuncia fidelidad completa al firmware.
- ADR-008 reemplaza dos Teensy de `03` y «Linux no interviene»; auditoría G18 dice
  proceso sin motor (generativo), aquí sí DSP C++. No se alteran esos documentos.

## Verificación

Mac: CMake Release, CTest; round-trip real con socket offline, todos los comandos,
clientes múltiples, errores, suscripciones, reinicio/persistencia. Render cuatro
partes (1/6/4/6 notas) repetido con SHA-256 estable por binario; comparar bloques
128/256/512, CPU con 2 s warmup +20 s medidos y política O2, no fast-math ni FMA.
Hash entre toolchains no exige igualdad (medición Pi 20261110).
Hook C++ de asignación/liberación normal y alineada verifica callback y cambios/notas/CC en test. Checklist RT:
inspeccionar setters de previews ante actualizaciones; ninguna llamada a prepare,
resize, assign, new/delete, iostream, filesystem o mutex en callback; revisar cada
backend guardado además en Linux. El hook no detecta syscalls/locks ni malloc C.

Pi: ejecutar 60 min cuatro partes con FX, patrón, GUI QML y PipeWire Audio+ a
48 kHz. Recoger cpu/xruns 1 Hz, temperatura/throttled, `pw-top` y journal; abrir
PARTS/parámetros/presets y editar durante patrón; matar GUI (audio continúa), matar
host (systemd lo recupera con último estado), desconectar/reconectar MIDI.
Puerta: 0 xruns, media ≤50 %, p99 ≤70 %, máximo ≤90 % de un núcleo;
`max_window_p99_upper_pct` del colector es una cota conservadora para esta puerta,
no el p99 de las medias de 1 s; sin throttling;
además compuerta GUI ADR-007 (60 fps, p99 <16.7 ms, CPU GUI <25 %). Medir latencia
MIDI→salida por loopback por separado: el benchmark no la prueba.

Paquetes Pi (instalación manual, herramientas no instalan):
`sudo apt install build-essential cmake pkg-config libjson-c-dev libpipewire-0.3-dev libspa-0.2-dev pipewire pipewire-bin wireplumber libjack-jackd2-dev pipewire-jack libsystemd-dev python3`.

```sh
cmake -S apps/synth-host -B /tmp/gf-synth-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/gf-synth-build -j4
ctest --test-dir /tmp/gf-synth-build --output-on-failure
/tmp/gf-synth-build/gf-synth-host --backend pipewire
```

En otra terminal (sustituir `NODO_AUDIO_PLUS` por el nombre real mostrado por
`pw-link -i`, no por el número de tarjeta ALSA):

```sh
pw-link -i
pw-link gf-synth-host:mix_L 'NODO_AUDIO_PLUS:playback_FL'
pw-link gf-synth-host:mix_R 'NODO_AUDIO_PLUS:playback_FR'
pw-link 'PUERTO_MIDI_ORIGEN' gf-synth-host:midi_in
python3 tools/synth-host/ctl.py '{"id":1,"cmd":"hello","client":"cli"}'
python3 tools/synth-host/ctl.py --subscribe cpu,meters --seconds 10
python3 tools/synth-host/play_pattern.py --minutes 1
```

Alternativa JACK: `pw-jack /tmp/gf-synth-build/gf-synth-host --backend jack`.
Para la prueba aislada, detener manualmente el host anterior; el script rechaza
otro `gf-synth-host` activo, compila en un directorio único de `/tmp`, activa las
cuatro partes wet y recoge JSON. La GUI debe abrirse aparte; el JSON no certifica
que estuviera presente ni mide FPS. No instala ni modifica configuración:

```sh
GF_AUDIO_LEFT='NODO_AUDIO_PLUS:playback_FL' \
GF_AUDIO_RIGHT='NODO_AUDIO_PLUS:playback_FR' \
  bash tools/synth-host/run_on_pi.sh /tmp/gf-host-results 60
```

El servicio usa `%h/.local/bin/gf-synth-host`; copiar binario/unidad y habilitarlos
es operación manual. Sin sd_notify: `systemctl --user edit gf-synth-host` con
`[Service]`, `Type=simple`, `WatchdogSec=0`. Nada se instala automáticamente.

## Resultado Mac anterior al cableado 6-OP (2026-11-10)

AppleClang 17, Darwin arm64, Release O2. Salida real tras compilar:

```text
RT: 7 engines, params, swaps, 4-part notes/CC/sustain/bend; allocations/frees=0
Audio: SPSC 200000 ordered transfers, note timestamps, level/mute/mix, channels/CC and feedback backpressure: PASS
Contract: 13 commands; 711 parameter round-trips; errors, UTF-8, subscriptions, clients, crash recovery: PASS
100% tests passed, 0 tests failed out of 4
Total Test time (real) = 11.62 sec
```

El sandbox bloquea `bind()` Unix: el test detecta EPERM y usa **tres socketpairs
AF_UNIX heredados** (`--control-fd`, solo transporte de pruebas; mismo servidor y
callback). Se verifican flujo UTF-8 fragmentado, pipeline, eventos entre clientes,
importe GFP real, preset duplicado, checkpoint corrupto y recuperación tras SIGKILL.
El listener por ruta y el rechazo de un segundo bind quedan para ejecución sin
sandbox/Pi; el test los ejecuta automáticamente cuando puede hacer bind.

Render húmedo 1/6/4/6 voces, 2 s de warmup +20 s medidos, porcentaje de un núcleo:

| Ejecutable | Bloque | Media | p99 | Máximo | Fuera de plazo |
|---|---:|---:|---:|---:|---:|
| host | 128 | 15.824 % | 21.113 % | 276.509 % | **1** |
| host | 256 | 15.776 % | 19.342 % | 27.804 % | 0 |
| gf_engine_bench wet (mismo Mac/flags) | 128 | 15.425 % | 19.580 % | 70.491 % | 0 |

No se descarta el bloque tardío de 128: pared ~7.37 ms frente a 2.667 ms; causa
sin determinar. El render offline de Mac no tiene prioridad RT y **no pasa la
puerta de deadlines de Pi**. La comparación solo informa costo y variación de
pared; el host usa velocities MIDI enteras, el bench floats, así que no exige el
mismo PCM entre ambos. No hubo muestras no finitas ni clipping.

SHA-256 del PCM de 3 s repetidos y bloques 128/256/512:
`525b02c8281117f3eb64e20c1c280411753b349d153ca20f2db0658b56d1d4c6`.
PCM de 20 s (128 y 256 idénticos):
`8e23bfb4d31b548f484e9a1f81f15f190e97dfcd904424d8377283e821a0a4d1`.
WAV de la medición en `/tmp/gf-host-final-b128.wav` y `...-b256.wav`.
[JSON y comandos](../../synth-host/tests/results/macos-20261110.json),
[salida CTest](../../synth-host/tests/results/macos-ctest-20261110.txt).

Para repetir el render: `/tmp/gf-synth-build/gf-synth-host --render /tmp/four.wav
--seconds 20 --block 128 --report /tmp/four.json` (una sola línea).

**Pendiente físico:** no hay SDK PipeWire/JACK/systemd en este Mac: sus ramas de
C++ están guardadas, consultadas contra API PipeWire 1.4.2 y sin compilar aquí.
SSH al Pi configurado también devuelve `Operation not permitted` por sandbox.
Los paquetes que Juan instaló en Pi (libpipewire-0.3-dev 1.4.2, libspa-0.2-dev,
build-essential) no cambian ese bloqueo de acceso. Faltan compilación Linux,
ruteo/clock Audio+, MIDI real, watchdog systemd y prueba de 60 min con GUI.

Git: el intento de commit documental previo al código falló al crear
`.git/index.lock` (`Operation not permitted`). No se empuja la rama; si el
sandbox sigue bloqueando, doc y código quedan sin commit.

## Cableado DX7 6-OP (2026-11-10)

`registry()` anuncia ocho engines. La fábrica instancia `Dx7SixPreview` con
`prepare(48000)` fuera de audio; la voz recibe note on/off y velocity mediante
el sink del bus compartido. Ediciones usan `edit_patch`, conservan voces/EG y
se aplican al principio del bloque. Sustain llega al core y al scheduler del
bus; el bend convierte el rango nativo ±3 del core a **±2 semitonos** del host.
CC14/15/16/19 editan DRIFT/GRIT/POCKET/MOVE; CC71/74 editan resonancia/cutoff
del filtro post-voz. Todos publican los mismos eventos param que otros engines.

El core no tiene filtro analógico ni ADSR adicional: su salida pasa por un
filtro de bus TPT de dos polos, luego por el mismo `ProphetPreview` usado por
los otros adaptadores para drive/tape/chorus-phaser/delay/reverb y GROOVE. GLOBAL
expone ese filtro por separado (20–20000 Hz, resonancia 0–1); no es un parámetro
Yamaha. DRIFT desplaza detune de operadores sin modificar VCED guardado; GRIT aumenta nivel de moduladores/feedback y resonancia de bus; POCKET acelera
R2/R4, reduce L3 y aumenta sensibilidad de velocity según POCKET VEL; MOVE usa
las funciones de bus compartidas. Las macros son desplazamientos de interpretación
sobre la voz, no parámetros Yamaha adicionales, y nunca reescriben VCED guardado. No se añade otro oscilador.
El pitch de wow/flutter se combina con el bend MIDI antes de llegar al core;
se actualiza al cambiar el multiplicador del bus, sin recalcular log2 por muestra
cuando permanece constante. No se promete identidad de bus con el VST.

### Agrupación y fuente única

`metadata.cpp::params_json()` sirve list_params y `gf-export-metadata`.
Los **151 descriptores reales** de `dx7_6op_params.cpp` conservan todos sus IDs,
rangos, unidades, defaults, kinds y offsets. Sólo cambia su distribución de páginas:

| Página | Contenido |
|---|---|
| OPS | `op1.level` … `op6.level`, seis controles |
| OP 1 … OP 6, banco 1 | mode, coarse, fine, detune, velocity, ams, on (7) |
| OP 1 … OP 6, banco 2 | r1–r4, l1–l4 (8) |
| OP 1 … OP 6, banco 3 | breakpoint, left_depth, right_depth, left_curve, right_curve, rate_scaling (6) |
| ALGO/FB | algorithm, feedback, osc_sync, transpose (4) |
| PITCH EG | pitch.r1–r4 / pitch.l1–l4 (8) |
| LFO | speed, delay, pmd, amd, sync, wave, pms (7) |

No hay IDs duplicados para crear la vista general: el GUI los enlaza desde el
catálogo completo. Un total de **56 controles de host** se añade en GLOBAL,
GROOVE, MACROS y FX Drive/Tape/Chorus/Delay/Reverb: **207 controles**, todos en
bancos ≤8. U8 y bool se redondean/recortan; enum respeta min/step; valores no
finitos se rechazan. Algoritmo crudo 0–31, opciones 1–32; detune 7 = centro,
transpose 24 = centro. FM 4-OP mantiene `osc.*`/`mod.*`, ahora agrupados por operador.

El JSON versionado en `gui-console/synth-metadata.json` se genera desde el binario,
no desde otra tabla Python ni parseando el texto C++. `cmake --build` produce la
exportación en el build; `metadata_snapshot` compara su contenido completo contra
el archivo de repo. El fake carga ese archivo. La GUI de producción consulta el
socket y usa el JSON sólo para tablas gráficas de routing de compilación, con
consistencia probada contra los flags de `dx7_6op_algorithms.cpp`.

### GFP y SysEx sin cambiar el contrato 14

El scanner de `--presets` y `--repo-presets` acepta `.gfp`, `.syx` (también en
mayúsculas), además de los formatos previos. El trabajo de disco/checksum está
en control. VCED single publica una entrada; VMEM de 32 voces publica las 32,
con nombre `dx7_6op:archivo:índice1–32:nombreVCED`. Un banco completo debe ser
válido antes de publicar voces: checksum/formato erróneo no deja entradas parciales.
La GUI usa **list_presets/load_preset**; no necesita import_sysex ni otro comando.
El scan ocurre al arrancar: tras colocar archivos hay que reiniciar el host.

save_preset crea el JSON de host y un **GFP v6/id7 de 162 bytes** de la voz,
`user-<nombre-hex>.gfp`, junto al JSON del mismo stem. JSON conserva engine, nombre,
role y valores de bus; GFP lleva VCED y máscara de operadores. Al importar el
GFP por sí solo, el bus empieza en sus defaults. La pareja se importa una vez,
leyendo el JSON; no hay preset duplicado por el GFP acompañante. No se sobrescribe
un nombre existente. Si falla publicar el JSON, se retira el GFP recién creado.
Checkpoint y user presets conservan también los valores del bus sin truncarlos
al formato Yamaha. GFP 2–5 y GFL conservan sus codecs.

### VST y pruebas

JUCE local **8.0.3**: build VST3 y **14/14 CTest** (incluye MSFA local y plugin smoke).
`Dx7SixView` ya usa directamente los mismos 151 descriptores; su selector ahora
muestra OPS y tres bancos por operador, ALGO/FB, PITCH EG y LFO. Automatización
DAW agrega el namespace existente `dx7_6op.` a cada ID del contrato. El smoke
recorre todos los bancos, comprueba 151 controles visibles y modifica coarse/detune
mediante sus attachments. El VST no añade los controles GLOBAL/GROOVE/FX del host
a esta vista de parámetros Yamaha.

Pruebas de host: **6/6 CTest**, sin asignaciones/liberaciones en audio, tablas reales,
clamping comparado con firmware, SysEx propio (32 VMEM + VCED), rechazo de checksum,
GFP standalone/JSON/reinicio, eventos y render aislado no silencioso/sin NaN.
Firmware nativo: **166/166** en 13 suites. Se usó `~/.platformio/penv/bin/pio test
-e native` con `PLATFORMIO_CORE_DIR=/tmp/gf-host-six-pio`; se copió la plataforma
native local y se reutilizó Unity 2.6.1 del caché local. Sin descargas ni flash.
La caché global es de sólo lectura en el sandbox.

Pruebas GUI: comparación contra exportación C++ recién compilada, cada página y
banco FM 4/6-OP por QLocalSocket al host real, selección repetida del mismo engine,
32 algoritmos, todos los focos, curvas de scaling y capturas. Listener por ruta
sigue bloqueado por sandbox: se usan socketpairs AF_UNIX heredados (mismo servidor,
cliente Qt y audio offline), sin atribuirles cobertura de PipeWire/listener físico.
Capturas y comandos: [15-gui-synth.md](15-gui-synth.md).

No se editó el contrato 14 ni los specs 00–06; no se tocó el checkout USB ni se
hizo push. Quedan las puertas físicas ya descritas: Pi/PipeWire/MIDI/latencia,
xruns con GUI, AMD/AMS y feedback con Yamaha real. La envolvente GUI muestra
control R/L, no una medición del tiempo no lineal del DSP.

Salidas finales guardadas: [host CTest](../../synth-host/tests/results/dx7-wiring-ctest.txt),
[VST CTest](../../synth-host/tests/results/dx7-wiring-vst-ctest.txt),
[firmware native](../../synth-host/tests/results/dx7-wiring-firmware.txt).
Host: 918 round-trips de parámetros; RT: ocho engines, cero asignaciones/liberaciones.
GUI: **204 passed, 13 skipped**, incluidos los tres tests nuevos de wiring y la
regresión de enums FX normalizados. Build VST3:
`/tmp/gf-host-six-vst/GrooveForgeBrainPrototype_artefacts/Release/VST3/GrooveForge Brain Prototype.vst3`.

Entrega Git de este cableado: `git add` volvió a fallar creando `.git/index.lock`
con `Operation not permitted`; cambios y artefactos quedan **sin commit**, sin push.
[Informe de archivos, pruebas y límites](../../synth-host/tests/results/dx7-wiring-report.md).


## Corrección Pi con GUI y prioridad RT (2026-11-10)

La [medición física](mediciones/synth-pi-20261110.md) dio1176xruns con GUI/bloque128 y0 con256. Se conserva256 como default del binario y del servicio. `--block` solicita latencia al grafo, no vence `clock.force-quantum` del servidor. No se modifica desde este checkout el archivo del Pi `~/.config/pipewire/pipewire.conf.d/50-consola.conf` (quantum/min-quantum256).

El backend crea un contexto PipeWire propio. Desactiva únicamente su carga automática de `module.rt` y carga una instancia explícita de **libpipewire-module-rt**, con `rt.prio=70`, `loop.rt-prio=70`, rlimits/rtkit/portal habilitados, antes de conectar el filter con `PW_FILTER_FLAG_RT_PROCESS`. El módulo solicita RT para el **data-loop de audio**; control/socket siguen con política normal. Si falta módulo se informa por stderr; una concesión fallida/clamp de RTKit se diagnostica con PipeWire/chrt: solicitar70 no prueba obtener70. No hay syscalls de scheduling añadidas al callback. [API primaria RT](https://docs.pipewire.org/page_module_rt.html), [client.conf1.4.2](https://github.com/PipeWire/pipewire/blob/1.4.2/src/daemon/client.conf.in), [filter1.4.2](https://github.com/PipeWire/pipewire/blob/1.4.2/src/pipewire/filter.c).

El módulo puede conceder RT mediante RTKit sin privilegios de root; comprobar `systemctl status rtkit-daemon`. Para permiso directo, el usuario del host debe pertenecer a `audio`; configurar manualmente **`/etc/security/limits.d/90-grooveforge-audio.conf`**:

```text
@audio - rtprio 70
@audio - memlock 262144
```

`sudo usermod -aG audio "$USER"`, cerrar sesión completa y volver a entrar (reiniciar Pi también renueva el gestor systemd de usuario). Verificar `id`, `ulimit -r` (≥70) y `ulimit -l` (KiB). La unidad de usuario pide `LimitRTPRIO=70` y `LimitMEMLOCK=268435456` bytes; una unidad de usuario no puede elevar el límite duro del gestor padre. Si no se heredó, comprobar `/proc/$(pgrep -xo gf-synth-host)/limits` y renovar la sesión/gestor. No ejecutar GUI con sudo, nice negativo ni SCHED_FIFO; `run_synth.sh` la inicia explícitamente con **`chrt --other 0`** cuando chrt está disponible.

Verificar **cada hilo**, no sólo el PID principal:

```sh
host_pid=$(pgrep -xo gf-synth-host)
ps -L -p "$host_pid" -o pid,tid,cls,rtprio,comm
chrt -a -p "$host_pid"
# También se puede consultar un TID concreto mostrado por ps:
chrt -p TID_DEL_DATA_LOOP
# El PID principal debe seguir OTHER/0; el data-loop debe tener FIFO/RT concedida.
chrt -p "$host_pid"
pw-top
```

En `pw-top` comprobar nodo `gf-synth-host`, RATE48000, QUANT256, carga y ERR estable con patrón4partes + GUI. Comprobar también política de los hilos de `pipewire`; priorizar sólo el cliente no arregla un driver sin RT. El PID de Python/Qt debe mostrar OTHER/0 con `chrt -p PID_GUI`. RTKit puede limitar la prioridad por debajo de70: anotar la concedida.

Sólo después de confirmar RT y cero errores en256, probar temporalmente128: arrancar host con `--backend pipewire --block 128` (o sustituir ExecStart mediante override del servicio, sin dos hosts simultáneos) y en otra terminal `pw-metadata -n settings 0 clock.force-quantum 128`. Comprobar QUANT real en pw-top; la configuración persistida256 sigue intacta. Volver con `pw-metadata -n settings 0 clock.force-quantum 256` y `--block 256`. No promover128 hasta repetir ensayo con GUI, notas sostenidas y patrón4partes sin xruns. No se declara aprobado RT ni latencia Pi desde CTest de Mac; esta plataforma no dispone de SDK PipeWire/JACK y esas ramas necesitan compilación/ejecución Linux.

## Extensión DRUM (2026-11-10)

[22](22-modo-drum.md) y14§7 incorporan sampler, secuenciador y transporte al mismo
callback. Se mantienen las10salidas previas y se añaden cue_L/R; mix suma ROOM
synth+drums. MIDI canal10 notas0..15 dispara pads, sin tocar synth1..4. El backend
conserva el core PipeWire conectado y quantum256. Discontinuidad real del backend
pide reset mediante atomic; sólo el dueño de audio cambia epoch, detiene DRUM y
rechaza comandos DRUM del epoch anterior, devolviendo memoria retirada a control.
No se reimplementa reloj musical en Python.

Voces64 totales, layers/RR4,256MiB de PCM residente incluyendo preparación y cue.
Hash, rutas contenidas y WAV PCM16/24/32/float se validan antes de swap. Muestras
importadas por el servicio tienen WAVfloat48k canónico (sin timestamp PEAK en el
hash); carga directa PCM tiene remuestreo lineal offline. Parametrización, pasos,
choke, grupos y audición se aplican por SPSC; ningún heap/IO/lock nuevo en audio.
CUE no roba voces ROOM; al saturarse se rechaza audición antes de cortar sala.
REC lleno desarma y cuenta el error; jamás pisa un step ocupado.

Servidor envía deltas pequeños por edición, no los8patrones completos por giro.
JSON completo sólo al cargar/consultar/guardar. Checkpoint de trabajo restaura
rack/params/steps/grupos/patrón seleccionado con transporte y REC apagados, queued
vacío. Espera el ACK inicial antes de responder a clientes. Generación/import son
procesos worker Python vía posix_spawn, sin shell ni ejecución en audio.

Opciones: `--drum-data DIR`, `--drum-kit-python PYTHON`, `--drum-kit-script FILE`.
Default desarrollo ~/.local/share/gf-drum o GF_DRUM_DATA; Pi configurar /data/drum.
No cambiar ExecStart/instalar/flash automáticamente. Resultados, comandos Pi y
límites: [informe DRUM](../../consola/gui-console/screenshots/modo-drum/REPORT.md).

## Tiempo real en la Pi (2026-11-11)

El usuario `juan` se agregó al grupo `pipewire` (`/etc/security/limits.d/25-pw-rlimits.conf`: rtprio 95, memlock
4 MiB) y se reinició la Pi. Resultado: `ulimit -r` = 95; hilo de audio del host `data-loop.0` en **SCHED_FIFO 70**,
PipeWire en FIFO 88. Medición con DRUM cargado y la GUI corriendo: **0 xruns en 60 s**, CPU del host 11 % media,
11,8 % máx. MiniLab enlazado directo por UMP con `tools/synth-host/link_midi.py` (sin el puente provisional).

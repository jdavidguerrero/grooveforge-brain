# gf-synth-host: diseño y verificación

Fecha: 2026-11-10. Contrato autoritativo: `14-synth-host-contrato.md`.
Implementación C++17 custom, sin JUCE ni Python/Qt en audio. Reutiliza las tres
bibliotecas de `brain-plugin` mediante `add_subdirectory`; una opción temprana
`GF_ENGINE_LIBRARIES_ONLY` evita compilar referencias, pruebas y VST del plugin.
No duplica DSP. DX7 es **4-OP**; `dx7_6op` no se anuncia hasta integrar su adaptador.

## Arquitectura y tiempo real

Un hilo de control atiende varios clientes Unix JSON-lines, disco y JSON (json-c).
Un hilo de audio (más el bucle auxiliar de PipeWire cuando corresponde) procesa
cuatro instancias independientes y diez salidas float:
`part1_L/R` … `part4_L/R`, `mix_L/R`. 48 kHz, quantum sugerido 128; el backend
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
persistencia ni backend. `Config::extension` reserva 512 valores float para patches
ajenos a GFP (p. ej. 6-OP). Integración futura: enlazar su biblioteca y registrar
`{"dx7_6op", -1, create, defaults, params, presets, name, encode, decode}`; no se
implementa ese motor aquí. Los engines existentes usan los previews y el bus FX/GROOVE del
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
PipeWire 1.4 puede transportar UMP: el puerto solicita MIDI1 y acepta UMP MIDI1
channel voice. El backend offline tiene reloj de pared para pruebas del servidor;
`--render` usa reloj de muestras, sin dormir, y escribe WAV fuera de audio.

Referencias API: [pw_filter 1.4](https://docs.pipewire.org/1.4/group__pw__filter.html),
[MIDI PipeWire](https://docs.pipewire.org/page_midi.html),
[JACK API](https://jackaudio.org/api/).

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
- `dx7_6op` contractual pero aún no disponible: hello solo anuncia engines
  implementados; set_engine dx7_6op devuelve error explícito.
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

## Resultado Mac y bloqueos (2026-11-10)

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

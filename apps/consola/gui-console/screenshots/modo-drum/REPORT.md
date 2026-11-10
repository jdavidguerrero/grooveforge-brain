# DRUM — entrega y verificación de escritorio

2026-11-10 · clon `grooveforge-codex-host` · rama `codex/modo-drum`, base `c9e26a9`.

## Resultado y archivos

Spec escrito antes del código: [22-modo-drum](../../../../docs/consola/22-modo-drum.md),
con extensión aditiva en14§7 y notas de implementación en16. Sampler, secuenciador
y transporte comparten `gf-synth-host`; no se creó otro proceso de audio.

| Área | Archivos principales | Comportamiento |
|---|---|---|
| Host | `apps/synth-host/src/drum.{h,cpp}`, `drum_files.*`, `drum_service.*`, `rt_queue.h` | 16pads,64voces,4layers/RR,256MiB de PCM residente, DSP por pad,4grupos, cue aislado, patronesA–H/lanes1–32, swing/micro/probabilidad y REC sin pisar pasos |
| Integración host | `audio.*`, `backends.cpp`, `server.*`, `main.cpp`, `CMakeLists.txt` | 12salidas, MIDI canal10, comandos/eventos versionados, ACK/retirada fuera de RT, deltas pequeños y catálogo paginado; PipeWire conserva pw_core conectado/quantum256 |
| Servicio | `tools/drum-kits/drum_kits.py`, README, pyproject y tests | ENGINE con APIs gf_perc, import local con licencia/trim/categorías, WAVfloat48k por hash, variantes worker sin editar el rack |
| GUI | `drum_client.py`, `drum_state.py`, `drum_surface.py`, `fake_drum_host.py`, componentes/vistas Drum*, registry/ModeSelect | Diseños46–55, touch auxiliar, teclado de rutas con minúsculas/SHIFT, tempo auxiliar, browser virtualizado, mismo lector de superficie que SYNTH |
| Metadatos | `drum_metadata_export.cpp`, `gui-console/drum-metadata.json` | Exportación desde C++; comparación CTest; producción consulta list_pad_params, fake usa el snapshot |
| LEDs | `surface/include/consola_s/PadLedReturn.h`, `surface/src/Surface.cpp` | RGB21 canal16, snapshot M4 existente0xA0,≤30Hz, brillo≤31/127, sin eco a MIDI musical |
| Pi/capturas | `pi/run_drum.sh`, `tools/drum_mode_review.py`, este directorio | Touch invertx/rotate0 + QML90, GUI SCHED_OTHER;14capturas1280×720 y manifest de procedencia |

La fuente visual se extrajo leyendo el JSON de `script[type="__bundler/template"]`
y sus Layout notes. El [manifest](manifest.json) conserva SHA256 del HTML original.
No se extrajeron/empaquetaron fuentes de la página. [Galería completa](index.html).

Carga fallida conserva el rack anterior. Audición de pad usa su DSP; la del browser
usa la capa cruda sólo en CUE. CUE no cambia el round-robin ni choke de ROOM y no
le roba voces si el pool está lleno. REC lleno se desarma y reporta el contador.
Un gap del backend cambia epoch y detiene DRUM; comandos preparados en el epoch
anterior se rechazan. Reinicio restaura datos/patrón seleccionado, siempre parado,
REC off y queued vacío. La consulta inicial espera el ACK de esa restauración.

## Pruebas observadas

Logs completos: [directorio de resultados](../../../../synth-host/tests/results/drum/).

| Comando | Resultado |
|---|---|
| CMake Release + ctest host | 10/10 PASS; log final en host-ctest.txt |
| `python3 -m pytest apps/consola/gui-console/tests -q` | 362 passed, 13 skipped; 74.69 s |
| `cd tools/consola && python3 -m pytest -q` | 450 passed |
| `cd tools/perc-synth && python3 -m pytest -q` | 467 passed |
| `python3 -m pytest tools/drum-kits/tests -q` | 4 passed |
| `~/.platformio/penv/bin/pio test -e native` con core temporal | 179/179 PASS,14suites, tests existentes sin editar |
| `pio run -e consola_uno` con core temporal | SUCCESS; sin flash |
| Capturas + py_compile + bash -n + git diff --check | Capturas sin avisos QML; verificaciones sin error |

CMake usó Python3.14 instalado (con numpy/scipy/soundfile); el detector había
seleccionado3.13 sin soundfile en el primer intento. Los intentos `uv run --offline
pytest` fallaron al abrir la caché global de sólo lectura; se usó Python instalado,
sin resolver/descargar dependencias. PlatformIO también falló primero con
`platforms.lock` global. Se copió native/tool-scons a `/tmp/gf-drum-platformio`, se
reutilizaron herramientas/framework locales y Unity del caché del proyecto. Para
el build Teensy se enlazaron los paquetes locales al core temporal; el overlay
USB/Wire permaneció privado en `.pio/build`. No se modificó el framework global.

Las pruebas nuevas verifican muestras exactas a122BPM con bloques1/128/256/511,
polimetría7, microtiming que cruza eventos, probabilidad0/1, fronteras redondeadas
estrictas, layers/RR y aislamiento entre buses, choke,64voces/steal, REC/overflow,
STOP/epoch y memoria. Hook new/delete normal/alineado:0asignaciones/liberaciones
RT (no es un detector de syscalls/malloc C). Socketpairs Unix heredados recorren
el servidor real: PCM16/24/32/float, resampling, hash/rutas, error sin perder rack,
deltas bajo ráfaga de encoders, worker import/licencia y checkpoint detenido.

Qt real + host C++ carga un rack ENGINE recién renderizado, edita parámetros/
grupos/steps, graba FSR por socket inmediato independiente, encola patrón, escucha
CUE y guarda. El fake y las capturas no producen audio. La pasada completa previa
había detectado un cierre de socket por dos snapshots grandes superpuestos: se
corrigió la contrapresión por cliente y se quitó la consulta duplicada. Un timeout
anterior del harness SYNTH pasó al repetir aislado; no se cuenta ese intento como
una pasada completa exitosa. La vista vacía del ListView también detectó una carrera de inicialización de
delegates; se evita adjuntar el modelo anterior cuando el catálogo ya está vacío.
También se cubre salir de SAVE/tempo sin dejar bloqueados los encoders y MUTE +
pad antes del siguiente frame, sin fuga audible del golpe. El resultado definitivo
está arriba y en gui-pytest.txt.

## CPU Mac

[JSON completo](../../../../synth-host/tests/results/drum/macos-64-voices.json).
64voces estéreo simultáneas, filtros y envolventes activos, clock/secuenciador
corriendo con lanes vacías,48k/256,2s de warmup +20s de audio medido. ReleaseO2,
sin fast-math/FMA, FTZ, AppleClang17, arm64. Coste de pared de un núcleo del
sampler (sin las cuatro partes synth, Python, IO del backend ni prioridad RT):

- Media **5.265 %**, p99 **6.911 %**, máximo **8.762 %**.
- **0** bloques fuera de plazo,0steals y0clipping.

Es una estimación de coste en este Mac, no latencia física ni aprobación Pi.
Corridas previas bajo distinta carga tuvieron más dispersión; no se extrapolan
sus picos a un Pi. No hay SDK PipeWire/JACK en este Mac: esas ramas requieren
build/ejecución Linux. El quantum256 dura5.333ms por sí solo: p95<5ms sigue sin
certificar. Mantener256 hasta comprobar RT y0xruns en la máquina real.

## Qué hace Juan en el Pi

Nada de esto se desplegó, instaló ni flasheó automáticamente.

1. Verificar paquetes de16: build-essential, cmake, pkg-config, libjson-c-dev,
   libpipewire-0.3-dev (API≥1.4), libspa-0.2-dev, pipewire/pipewire-bin,
   wireplumber y, si usa watchdog notify, libsystemd-dev. JACK es opcional.
   Python elegido para worker: numpy, scipy, soundfile/libsndfile; GUI conserva
   su PySide6/QtQuick existente, mido/python-rtmidi y pyserial para CDC opcional.
   Usar la instalación/venv que ya funciona, con intérpretes explícitos.
2. Crear `/data/drum` escribible para el usuario del host (raíz del Pi permanece
   de sólo lectura). Conservar la unidad/RT de16, una sola instancia del host.
3. Compilar en este checkout y arrancar con rutas absolutas. Ejemplo desde la raíz:

```sh
cmake -S apps/synth-host -B /tmp/gf-drum-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/gf-drum-build -j4
ctest --test-dir /tmp/gf-drum-build --output-on-failure
export GF_DRUM_DATA=/data/drum
/tmp/gf-drum-build/gf-synth-host --backend pipewire --block 256 \
  --drum-data /data/drum --presets /data/gf-synth/presets \
  --drum-kit-python /usr/bin/python3 \
  --drum-kit-script "$PWD/tools/drum-kits/drum_kits.py"
```

Si utiliza otro Python, sustituir `/usr/bin/python3`. Para systemd, ajustar
manualmente ExecStart con esas opciones/rutas; el servicio anterior debe estar
parado antes de iniciar otro host. Sin libsystemd, Type=simple/WatchdogSec=0 según16.

4. Renderizar propios o importar **sus** samples (ninguno viene en git):

```sh
GF_DRUM_DATA=/data/drum python3 tools/drum-kits/drum_kits.py render \
  --genre organic --root A --mode menor --layers 4
GF_DRUM_DATA=/data/drum python3 tools/drum-kits/drum_kits.py import \
  /media/usb/tribe --licence-note 'Tribe — licence pending' --trim
python3 tools/synth-host/ctl.py '{"cmd":"list_racks"}'
```

También puede usar IMPORT en la GUI: introducir carpeta local y nota, pulsar
IMPORT y volver al jam. Al terminar, escoger rack/sonido en Browser y LOAD.
El import no cambia el rack activo ni inicia transporte; más16archivos producen
varios racks. Los samples importados no se detectan/retocan de afinación por su
cuenta: Juan usa TUNE/elige material compatible. La nota pending conserva esa
incertidumbre y no concede licencia de redistribución.

5. Ruteo explícito (`pw-link -i/-o` para nombres reales): mix_L/R → Audio+;
   cue_L/R → USB Teensy playback1/2 → SGTL5000. Nunca cue→sala ni mix+partes a la
   vez. Mantener el cruce PipeWire48k→USB nominal44.1k. No enlazar GF Consola
   canal1 directamente al host en DRUM; el puente traduce a canal10/pad0..15.
6. `GUI_PYTHON=/ruta/al/python-qt bash apps/consola/gui-console/pi/run_drum.sh`.
   Si hace falta, pasar `--surface-cdc /dev/...` explícito y `--room-node` del Audio+.
   El launcher conserva calibración invertx/rotate0, imagen QML90 y SCHED_OTHER.
   Para LEDs, Juan debe flashear el build consola_uno probado y verificar que la M4
   instalada consume CMD_LED_GRID_UPDATE de96bytes. No se hizo flash aquí.
7. Comprobar prioridades por TID y permisos audio/rtkit según16, quantum real en
   pw-top, touch físico y separación CUE/ROOM. Tras restart debe quedar parado;
   sólo PLAY arranca el patrón.

## Plan de medición Pi

- Ejecutar gf-drum-bench en el Pi para el mismo fixture64voces/clock y guardar JSON.
- Después,60min de host real +4partes synth +GUI +64voces del sampler con un fixture
  propio largo afinado aA (55Hz), nivel bajo y choke0;64disparos y renovar antes
  de que terminen las colas. Registrar cpu1Hz/p99_upper/xruns/RAM/temperatura/
  throttling junto a pw-top; no sustituirlo por el benchmark sin backend.
- Medir por loopback sala/cue, pads M4 y FSR por separado; comprobar muestreo del
  playhead y cambios de patrón dentro de bloque; desconectar/reconectar superficie.
- ADR-007:1000eventos/s,60fps, p99cuadro<16.7ms, GUI<25% de un núcleo. Audio:
  0xruns, media≤50%, p99≤70%, máximo≤90%; memoria PCM≤256MiB.128 sólo después de
  comprobar RT y repetir la prueba, sin cambiar el default persistido256.
- Matar GUI (audio continúa), worker (ediciones/rack intactos), host (silencio;
  systemd recupera datos parado), reiniciar y comprobar que no reaparece PLAY/REC/
  patrón queued viejo. Verificar LEDs físicos, no inferirlos de los mensajes MIDI.

## Preguntas y límites

Siguen en22§9:64voces totales frente a64/bus de la propuesta E5; afinación literal
raíz frente a la regla existente de género/kick quinta; RGB21 frente a paleta19;
SHIFT+encoder en Variations frente al doble uso del clic en el diseño; destino
del envío SEND/FX; y licencia de Tribe. RECORDED, Link y E5/ACCEPT siguen reservados.
SEND se conserva/edita pero no se inventó una reverb ni un retorno sin contrato.
Guardar PATTERN exporta una copia independiente; guardar RACK conserva los ocho
patrones. Las fuentes/documentos contradictorios no se reescribieron.

## Git

El primer staging del spec falló con `.git/index.lock: Operation not permitted`.
Por la instrucción explícita de la tarea, **todo queda sin commits**, incluido el
spec; sin push. No se tocaron otros clones, tests nativos existentes ni docs00–06.
No se añadió audio de terceros ni GeneralSans. Orden lógico preparado para quien
pueda escribir git: spec → host → servicio → GUI/superficie → verificación; cada
mensaje debe terminar con línea vacía y:

```text
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
```

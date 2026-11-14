# Arranque de la Pi · entrega y validación

2026-11-11 · Preparado en macOS, sin SSH, instalación ni cambios en la Pi.
Spec: [26-arranque-pi](../../docs/consola/26-arranque-pi.md).

## Instalar como Juan (en la Pi)

Usar el checkout permanente, no /tmp: los servicios ejecutan helpers, GUI y
worker del repo. Mantener esa ruta y el binario entre reinicios. Primero
retirar del arranque cualquier llamada a ~/host-start.sh, ~/drum-start.sh,
~/synth-start.sh y link_midi.py. No borrar esos archivos hasta validar.
Si se instaló la plantilla vieja: `sudo systemctl disable --now consola-gui.service`.
No ejecutar simultáneamente otra GUI, host, compositor o getty que tome DRM/VT.
Esta entrega no cambia getty, permisos de dispositivos ni boot/config.txt.

```bash
cd /ruta/permanente/grooveforge-brain
# Si aún faltan Qt/EGLFS/fuentes: bash apps/consola/gui-console/pi/setup_pi.sh
# Mantener también los módulos MIDI/superficie de la instalación actual.
cmake -S apps/synth-host -B apps/synth-host/build -DCMAKE_BUILD_TYPE=Release
cmake --build apps/synth-host/build -j4
# En la salida CMake debe aparecer PipeWire=1; no aceptar backend offline.
# Worker de kits (requiere uv/red sólo al preparar el entorno):
(cd tools/drum-kits && uv sync)
ctest --test-dir apps/synth-host/build --output-on-failure

# Obtener node.name y port.name reales; no usar IDs numéricos persistidos.
pw-link -i
pw-dump > /tmp/gf-pw-ports.json
python3 - <<'PY'
import json
for o in json.load(open('/tmp/gf-pw-ports.json')):
    if o['type'] == 'PipeWire:Interface:Node':
        p = o.get('info', {}).get('props', {})
        print(p.get('node.name'), p.get('node.description'), p.get('media.class'))
PY

bash apps/consola/pi/install.sh --dry-run \
  --audio-node 'NOMBRE_EXACTO_AUDIO_PLUS' --cue-node 'NOMBRE_EXACTO_TEENSY'
bash apps/consola/pi/install.sh \
  --audio-node 'NOMBRE_EXACTO_AUDIO_PLUS' --cue-node 'NOMBRE_EXACTO_TEENSY'
# O añadir --host-binary /ruta/absoluta/gf-synth-host si ya se compiló en otra ruta.
# Revisar/editar ~/.config/gf-consola/config antes de arrancar.
systemctl --user start gf-console.target
systemctl --user status gf-console.target gf-synth-host gf-console-gui gf-audio-links gf-midi-link
loginctl show-user juan -p Linger
journalctl --user -u gf-synth-host -u gf-console-gui -u gf-audio-links -u gf-midi-link -b
```

El instalador habilita el target para el próximo boot y linger con sudo; deja
el inicio de esta sesión como comando explícito. No necesita regla udev:
el watcher ve el registro PipeWire ya listo y recupera MIDI1/UMP + audio.
Repetir instalación actualiza unidades/launcher, conserva config y datos.
Si cambia GF_REPO o el binario, editar config; flags de instalación no pisan
config existente. Tras actualizar unidades activas usar
`systemctl --user restart gf-console.target` en un momento sin reproducción.

Config mínima creada: GF_REPO, GF_HOST_BINARY, GF_CONSOLE_MODE=DRUM,
GF_AUDIO_NODE, GF_CUE_NODE, GF_AUDIO_LEFT/RIGHT y GF_CUE_LEFT/RIGHT
(playback_FL/FR), GF_EXTERNAL_MIDI_PORT=auto, touch identidad.
Opcionales: GF_DRUM_DATA, GF_DRUM_KIT_PYTHON, GUI_PYTHON, GUI_KMS_CONFIG,
GF_SURFACE_PORT, GF_HOST_MIDI_PORT. Son asignaciones **bash** confiables:
para espacios usar comillas; `$HOME` se expande al cargar. No se lee como
EnvironmentFile de systemd. DRUM usa ~/.local/share/gf-drum; synth mantiene
~/.local/share/gf-synth/presets y su last-state.json. No se utiliza /data aún.

Para SYNTH: editar `GF_CONSOLE_MODE=SYNTH` y
`systemctl --user restart gf-console-gui.service`. El arranque confirma STOP;
no retoma un transporte anterior. El host MIDI sigue enlazado en ambos modos.

Para desarrollo:

```bash
systemctl --user disable --now gf-console.target
bash apps/consola/pi/systemd/run.sh host
# En otras terminales de la misma sesión, cuando haga falta:
bash apps/consola/pi/systemd/run.sh audio
bash apps/consola/pi/systemd/run.sh midi
bash apps/consola/pi/systemd/run.sh gui
# Reanudar el conjunto (cerrar antes procesos manuales):
systemctl --user enable --now gf-console.target
# Desinstalar, manteniendo datos/config/linger compartido:
bash apps/consola/pi/install.sh --uninstall --dry-run
bash apps/consola/pi/install.sh --uninstall
```

Para una sola reconciliación manual, exportar las variables del config y usar
`python3 apps/consola/pi/systemd/links.py audio --once` (o `midi --once`).
Los watchers no retiran links ajenos, ni programan PLAY, ni cambian drivers.

## Pendiente de validar físicamente

Objetivo **N=60 s**, desde aplicar energía hasta GUI visible y usable, socket
respondiendo y enlaces correctos, **STOPPED / REC apagado / cola vacía**. Es
un objetivo por medir, no un resultado. Ensayar primero con niveles bajos.

1. Confirmar grupos `id juan` (pipewire + video/render/input según imagen),
   permisos KMS/input sin login activo y que no haya otro dueño de DRM. La
   unidad de usuario no recibe ACL de una sesión local por arte de linger.
2. Reboot sin abrir SSH/login: comprobar pantalla DRUM, touch en esquinas,
   host y links. Registrar tiempo con vídeo/reloj; luego consultar journal.
3. `host_pid=$(systemctl --user show gf-synth-host -p MainPID --value)`;
   `ps -L -p "$host_pid" -o pid,tid,cls,rtprio,comm`;
   `cat /proc/"$host_pid"/limits`; `chrt -a -p "$host_pid"`.
   Esperado: data-loop.0 FIFO70, principal OTHER0, GUI OTHER0, PipeWire FIFO88.
   `systemctl --user show gf-synth-host -p LimitRTPRIO -p LimitMEMLOCK` para
   diagnosticar herencia. Renovar gestor/reiniciar si no heredó grupos/límites.
4. `pw-top`: Audio+ driver, RATE48000, QUANT256, ERR estable (0 nuevos xruns)
   con patrón cargado, GUI y notas. El servicio sólo solicita bloque256.
5. Tocar PLAY/REC y elegir patrón en cola, luego
   `systemctl --user kill --signal=SIGKILL gf-synth-host.service`.
   Debe recuperar estado musical persistido y permanecer detenido; GUI
   reconecta sin reenviar PLAY. Probar también SIGKILL de gf-console-gui:
   al volver confirma STOP con el host aún vivo. Probar varias caídas para
   observar backoff 2→30 s y recuperación sin start-limit permanente.
6. Desconectar/reconectar Teensy y teclado: desaparecen y vuelven con otros
   IDs; reaparecen cue FL/FR y MIDI externo; mix sigue en Audio+. Confirmar
   físicamente sala/cue separados y que GF Consola no se duplica hacia host.
   Repetir reiniciando PipeWire/WirePlumber: host sale por fault/watchdog y
   watchers reabren registro. No se promete recuperar un gestor PW que haya
   agotado su propio start-limit; inspeccionar sus unidades/journal si pasa.
7. Tras backup y en banco controlado: PLAY activo, tirar el enchufe, volver a
   alimentar; repetir boot ≤60 s y STOPPED sin intervención. No iniciar PLAY
   desde un script de prueba. Contar al menos tres ciclos, incluido Teensy
   ausente y hotplug posterior. Registrar journal, corrupción, xruns y tiempos.

`journalctl --list-boots` debe mostrar boots previos si journald es persistente.
Para analizar el corte: `journalctl --user -b -1 -u gf-synth-host` (si se conservó).
El instalador no cambia la retención ni garantiza durabilidad de los últimos
mensajes ante un corte. Configurar/verificar persistencia del journal en la Pi
como preparación separada; no equivale a instalar NVMe/raíz de sólo lectura.

## Contradicciones y límites conservados

- 13 conserva una plantilla **de sistema** demo con VT1, grupos suplementarios
  y getty conflict; aquí se pide **usuario con linger**. No se instalan ambas.
  El acceso EGLFS/KMS sin sesión es puerta física pendiente; si la imagen
  exige VT/seat activo, hay que documentar el ajuste específico, no elevar GUI
  a root ni prometer que la plantilla anterior lo resuelva automáticamente.
- 16 primero recomienda grupo `audio`/memlock256MiB, pero el apéndice más
  reciente «Tiempo real en la Pi» mide grupo `pipewire`/rtprio95/memlock4MiB.
  Esta entrega conserva esos límites existentes y solicita RTPRIO70, sin
  instalar límites contradictorios ni exigir 256MiB al gestor de usuario.
- ADR-008 dice watchdog/reanudación del estado; D4 precisa que la recuperación
  es **de estado musical con transporte detenido**. El host ya tiene watchdog
  interno de ACK/audio (5 s); no se inventa heartbeat sd_notify para la GUI.
  systemd reinicia salidas con error; no detecta por sí solo una GUI congelada.
- ADR-008 §5–6 manda NVMe, raíz de sólo lectura, /data y journal persistente
  como dirección; hoy no están instalados. No se toca particionado, fstab,
  journald ni scripts de apagado. La SD sigue expuesta a pérdida/corrupción.
- Ningún test Mac certifica prioridades, Linux/PipeWire, hardware, tiempo de
  boot, sonido ni recuperación del filesystem. No se instaló nada en la Pi.

## Pruebas reproducibles en macOS

```bash
python3 -m unittest discover -s apps/consola/pi/tests -v
bash -n apps/consola/pi/install.sh apps/consola/pi/systemd/run.sh
# Si está instalado:
shellcheck apps/consola/pi/install.sh apps/consola/pi/systemd/run.sh
cmake -S apps/synth-host -B build/systemd-host -DCMAKE_BUILD_TYPE=Release \
  -DGF_HOST_PIPEWIRE=OFF -DGF_HOST_JACK=OFF
cmake --build build/systemd-host -j4
ctest --test-dir build/systemd-host --output-on-failure
GF_SYNTH_HOST_BUILD="$PWD/build/systemd-host" python3 -m pytest apps/consola/gui-console/tests -q
```

En Linux, además: `systemd-analyze --user verify ~/.config/systemd/user/gf-*.service ~/.config/systemd/user/gf-console.target`.
En Mac los tests Python parsean dependencias, restart/backoff, logs y claves
prohibidas; no sustituyen el parser ni ejecución real de systemd. Para kits
los Python de CTest y GUI necesitan numpy/scipy/soundfile; preparar venv antes.

Resultados observados en macOS arm64:

| Verificación | Resultado |
|---|---|
| Helpers/instalador/unidades (unittest) | **13/13 PASS** |
| Build Release host, PipeWire/JACK deshabilitados en Mac | **PASS** |
| CTest host | **13/13 PASS**, 12 en primera pasada + DRUM repetido tras preparar dependencias |
| GUI completa | **523 passed, 13 skipped**, 321,75 s |
| GUI DRUM contra host C++ real, con dependencias de kits | **1 passed**, 5,27 s |
| Startup GUI: STOP + preview-stop + ACK contra C++ real en PLAY/REC | **PASS**, socketpair heredado |
| bash -n / git diff --check | **PASS** |
| shellcheck / systemd-analyze | No están instalados en este Mac; syntax bash + parser Python usados |

Los skips GUI corresponden a restricciones de listener Unix del sandbox y
fuente General Sans local ausente; no representan validación de ese hardware.
La primera pasada DRUM falló por falta de soundfile (y luego typing_extensions)
en el entorno local; se preparó el venv ignorado de tools/drum-kits con paquetes
ya disponibles en caché y la repetición pasó, incluido el crash D4 real.
No se cambiaron manifests de dependencias. La prueba GUI real adicional usó
`PYTHONPATH="$PWD/tools/drum-kits/.venv/lib/python3.14/site-packages"` y
`GF_SYNTH_HOST_BUILD="$PWD/build/systemd-host"` con Python 3.14 del Mac.
Logs locales ignorados en `build/systemd-verification/`: boot-tests.log,
ctest.log, ctest-retry.log, gui-tests.log y gui-real.log. No se atribuye a Mac
ninguna medición del boot/RT/PipeWire de la Pi.

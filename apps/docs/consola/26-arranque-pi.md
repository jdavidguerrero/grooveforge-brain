# 26 · Arranque y recuperación de la Console en la Pi

2026-11-11 · Spec de esta entrega; autorización de implementación: tarea de Juan.
Alcance: Pi 5 / Raspberry Pi OS arm64, usuario `juan`, PipeWire de usuario,
Audio+ como driver 48 kHz / quantum 256. No cambia DSP ni specs 00–06.

## Teoría y decisión

Un gestor de usuario con **linger** vive sin login ni SSH. systemd supervisa
procesos; PipeWire publica los puertos después de arrancar cada dispositivo.
`After=` ordena procesos, pero no demuestra que exista un socket o puerto. Por
eso la GUI espera una respuesta del host y los enlaces se reconcilian contra
el registro real. No se usan sleeps fijos como prueba de disponibilidad.

Se elige un watcher `pw-dump --monitor` por servicio (audio y MIDI): observa
puertos ya publicados por PipeWire, incluidos UMP y reenumeración. Una regla
udev ve hardware antes de que WirePlumber publique puertos y no conoce el bus
ni sesión de Juan. **No se instala regla udev** en esta opción. Cada evento
invalida el snapshot; como máximo se consulta/reconcilia una vez por segundo,
y un reintento cada 5 s cubre carreras al crear links. Sin trabajo en audio.
La alternativa udev + timer requeriría igualmente descubrir puertos después.

## Secuencia y grafo de unidades

```text
boot → user@UID (linger) → default.target → gf-console.target
                         ├─ pipewire.service + wireplumber.service (Wants/After)
                         ├─ gf-synth-host.service
                         ├─ gf-audio-links.service → mix L/R a Audio+, cue L/R a Teensy
                         ├─ gf-midi-link.service → MIDI físico externo a midi_in
                         └─ gf-console-gui.service → espera socket/hello → STOP → DRUM/SYNTH
```

Los cuatro servicios son `PartOf=gf-console.target`: detener el target los
para juntos. Los helpers y GUI no dependen de la vida de un PID del host:
permanecen/reconectan tras su reinicio. No usar `BindsTo=host` que los dejaría
parados al caer éste. No se depende de red, escritorio ni sesión gráfica.

`run.sh host|gui|audio|midi` es también la entrada manual. Configuración shell
local confiable en `~/.config/gf-consola/config`, permisos 0600; rutas absolutas,
variables GF_* y GUI_* exportadas. Modo `GF_CONSOLE_MODE=DRUM` por defecto,
o `SYNTH`. Reutiliza run_drum/run_synth: EGLFS, QML 90°, touch identidad,
GUI SCHED_OTHER. Un solo cliente DRM; quitar la antigua unidad de sistema y
scripts de login antes de habilitar ésta. Se necesitan grupos video/render/input
para acceso sin sesión activa; no se conceden permisos globales ni root al GUI.

Los destinos de audio son **node.name exactos configurados**, nunca card N ni
IDs efímeros. Descubrir con `pw-dump`/`pw-link -i`; el instalador deja vacíos
GF_AUDIO_NODE/GF_CUE_NODE para no adivinar hardware. Puertos por defecto
playback_FL/FR; configurables. Un destino ambiguo o ausente no se enlaza;
los otros pares siguen funcionando y se informa en journal. El watcher no
borra enlaces ajenos. El plan MIDI reutiliza exclusiones de link_midi.py
(superficie, through, host y GUI) y soporta MIDI1/UMP; `auto`, `none` o substring.

## Fallos, RT y disponibilidad

`Restart=on-failure`, retraso inicial 2 s, 5 pasos hasta 30 s (systemd ≥254,
trixie). `StartLimitIntervalSec=0`: reintenta indefinidamente incluso si el
hardware tarda. Stop explícito no reinicia. PipeWire perdido: los watchers
fallan y reabren el monitor; el host vuelve si su backend sale con error.
No se anuncia WatchdogSec de systemd sin heartbeat implementado. El host ya
tiene watchdog interno de audio/ACK (5 s); una GUI bloqueada requiere intervención.

El host solicita bloque 256, conserva el grafo de 48 kHz y los datos DRUM
`~/.local/share/gf-drum`, worker del venv tools/drum-kits y checkpoints synth
existentes. No usa benchmark, render ni play_pattern. No se fuerza quantum
ni se reinstala configuración PipeWire/WirePlumber.

El grupo `pipewire` ya medido en 16 («Tiempo real en la Pi») concede rtprio 95.
La unidad pide `LimitRTPRIO=70`; no eleva el límite duro del gestor padre ni
pone el proceso entero en FIFO. El módulo RT del host conserva data-loop FIFO70;
control y GUI siguen OTHER. `LimitMEMLOCK` se hereda: no pedir 256 MiB con el
límite de grupo actual de 4 MiB. Renovar gestor/reboot si cambian grupos/límites.

## Estado y D4

19 §12 D4 manda: **tras reinicio, detenido hasta PLAY**. El host restaura
presets/params/niveles synth sin notas; DRUM restaura rack, pasos, grupos y
patrón con running=false, armed=false, queued=-1. No serializa voces activas.
La GUI sólo consulta/suscribe al conectar; no reenvía PLAY ni armado pendiente.
Cada arranque del servicio GUI además confirma hello, envía STOP y apaga preview antes de
mostrarla: un reinicio sólo de GUI tampoco continúa transporte anterior.
El helper de disponibilidad usa ACK, no sólo la existencia de un socket viejo.
Ni los enlaces ni un hotplug envían MIDI de reproducción. Notas nuevas de un
teclado son acciones explícitas del músico, no restauración de transporte.

Sobrevive lo ya confirmado/persistido por el host; no se promete salvar una
edición aún no escrita al cortar energía ni inmunidad de la SD a corrupción.
El socket de XDG_RUNTIME_DIR y links se recrean; config, samples y checkpoints
viven hoy en home. No migrar a /data todavía.

## Instalación, desarrollo y logs

Ver `apps/consola/pi/REPORT.md` para comandos. El instalador idempotente copia
unidades de usuario y launcher, conserva config existente, habilita linger y
el target; no arranca servicios en la sesión actual (reboot o start explícito).
`--dry-run` no escribe ni ejecuta sudo/systemctl. `--uninstall` deshabilita/para
el target, quita unidades/launcher y conserva datos/config y linger (compartido).

`journalctl --user -u gf-synth-host -u gf-console-gui -u gf-audio-links -u gf-midi-link -b`
muestra stdout/stderr y decisiones de enlace. Para el boot anterior usar `-b -1`
si journald es persistente; comprobar `journalctl --list-boots`. Esta entrega no
cambia política de journald ni flash. Para desarrollo:
`systemctl --user disable --now gf-console.target`; luego lanzar run.sh a mano.
`enable --now gf-console.target` vuelve a autostart. Parar sólo un servicio
habilita pruebas de crash sin competir con un segundo host o GUI.

## Validación y puerta física

Antes del test: pruebas unitarias del plan y watcher con eventos agrupados/reintentos, puertos tardíos, links existentes,
duplicados y IDs USB cambiados; tests estáticos de unidades y dry-run/uninstall.
Prueba D4 mata un host con DRUM PLAY+REC+patrón en cola, restaura checkpoint y
comprueba estado detenido. GUI prueba handshake/reconexión sin replay.
Regresión: CTest host y pytest GUI en Mac; no valida PipeWire, RT ni EGLFS.

Objetivo físico propuesto: **≤60 s desde alimentar hasta GUI visible, host
respondiendo, enlaces presentes y transporte detenido**, sin login. Medir,
no asumir aprobado. Verificar kill -KILL del host/GUI, hotplug Teensy/teclado,
FIFO70, 48k/256 y cero xruns con carga; finalmente corte real en banco seguro.

## NVMe / raíz de solo lectura (fuera de alcance)

ADR-008 §5–6 es plan, no estado instalado. La migración futura moverá datos
escribibles y journal a /data, ordenará arranque tras montaje y definirá
recuperación de corrupción/UPS. Mantener binario/venv/config legibles; renovar
rutas del config y volver a medir boot/RT. No particionar, editar fstab ni
habilitar overlay/read-only desde este instalador.

Referencias: [systemd.service](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html),
[systemd.exec](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html),
[pw-dump monitor](https://docs.pipewire.org/page_man_pw-dump_1.html),
[16](16-synth-host.md), [13](13-gui-pi.md), [19 D4](19-nucleo-e5-spec.md),
[ADR-008](../adr/ADR-008-consola-arquitectura.md).

# GUI en Raspberry Pi 5 y compuerta ADR-007

2026-11-09 · Preparación y medición; **no certifica el Pi desde macOS**.
Sin motor, audio, MIDI ni IPC real hasta el spec del núcleo. UI en inglés.

## Ejecución y dependencias

Objetivo: Raspberry Pi OS Lite arm64 / Debian 13 trixie, kernel 6.18,
Python 3.13, Mesa/V3D + EGLFS/KMS, sin X ni Wayland. `pi/setup_pi.sh` instala
bindings `python3-pyside6.{qtcore,qtgui,qtqml,qtquick,qtopengl}` y módulos QML
QtQuick/Window/Layouts, EGLFS (`libqt6opengl6`, no qt6-qpa-plugins), GBM, DRM, libinput, fontconfig y fuentes fallback.
Se elige **apt**: trixie publica PySide6 6.8.2.1 para arm64/Python del sistema;
Qt y plugins pertenecen al mismo conjunto. El setup comprueba imports y plugins,
no instala escritorio, no inicia servicios y no modifica boot ni grupos.

Alternativa no seleccionada: `python3 -m venv .venv-pi`, luego
`.venv-pi/bin/pip install --only-binary=:all: 'PySide6>=6.8'`; requiere una rueda
compatible con CPython 3.13/aarch64 y su glibc. No mezclar plugins apt con el Qt
de la rueda. Verificar EGLFS/eglfs_kms también en esa instalación; una rueda
instalable no demuestra soporte KMS. `GUI_PYTHON` permite elegir intérprete.

Setup descarga las fuentes con `fonts/fetch_fonts.sh`, incluida General Sans
de Fontshare y su licencia. GeneralSans.ttf y fonts/general-sans/ son locales,
ignorados y **jamás se commitean**. Después, todas las fuentes son locales y
ninguna ejecución necesita internet. El setup puede repetirse; requiere red.

`pi/run_gui.sh` fija `QT_QPA_PLATFORM=eglfs`,
`QT_QPA_EGLFS_INTEGRATION=eglfs_kms`, `QSG_RHI_BACKEND=opengl`, escala 1 y cursor
oculto. No forzar software ni desactivar vsync. `GUI_KMS_CONFIG=/ruta/kms.json`
selecciona tarjeta/salida; `GUI_TOUCH_DEVICE=/dev/input/by-path/...` fija Goodix.
Usar `modetest -c` y `libinput list-devices` para descubrir nombres reales.
Ejemplo KMS (ajustar card y conectores, no asumir card0):

```json
{"device":"/dev/dri/card1","hwcursor":false,"outputs":[{"name":"DSI-1","mode":"preferred","primary":true},{"name":"HDMI-A-1","mode":"off"},{"name":"HDMI-A-2","mode":"off"}]}
```

Ejecutar desde VT local con permisos sobre /dev/dri y /dev/input (grupos
video/render/input según la imagen); volver a iniciar sesión tras cambiarlos.
El servicio de sistema incluye esos grupos. Nunca ejecutar dos clientes DRM
simultáneos. No cambiar config.txt/cmdline.txt desde estos scripts.

## Rotación de imagen y touch

Panel nativo **720×1280**, contenido lógico **1280×720**. `--rotate 0|90|180|270`
y `--rotation-method auto|qml|eglfs`. Auto usa transformación del Item raíz QML;
90/270 intercambian las dimensiones de ventana manteniendo el lienzo lógico.
En EGLFS la ventana sigue el tamaño físico de QScreen; el lienzo se centra y
escala conservando proporción. Para este montaje probar primero 90, después 270
si la orientación física exige el sentido contrario. Medir ambas rutas por separado.

**Contradicción G32 / requisito:** según Qt, `QT_QPA_EGLFS_ROTATION` solo rota
QWidget/software, **no Qt Quick/OpenGL ni el touch**. tools/consola/pi/README.md
afirma que rota la GUI. El modo explícito `eglfs` fija esa variable (270→-90),
pero reporta `eglfs-env-unverified`, avisa y nunca certifica orientación/gate.
No se inventa soporte: en Qt Quick usar `qml`; una rotación KMS externa requiere
validación física aparte. `fbcon=rotate:1` solo afecta texto de la VT.

Con QML, el touch debe permanecer en coordenadas **físicas sin rotar**: Qt Quick
invierte la transformación del Item para hit testing. Rotar además evdev daría
doble rotación. El runner prefiere evdev (`QT_QPA_EGLFS_NO_LIBINPUT=1`) y usa
`QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=<device>:rotate=0` por defecto.
`GUI_TOUCH_ROTATION=90|180|270` permite calibrar una transformación externa,
independiente de `--rotate`; no se deduce automáticamente del panel.
Alternativa `GUI_INPUT_BACKEND=libinput`: `GUI_TOUCH_MATRIX` fija
`QT_QPA_LIBINPUT_TOUCH_MATRIX` (identidad por defecto); alternativamente regla udev con
`ENV{LIBINPUT_CALIBRATION_MATRIX}="0 -1 1 1 0 0"` para 90°, o identidad
`"1 0 0 0 1 0"` para QML sin calibración previa. No aplicar ambas rutas.
Verificar las cuatro esquinas, centro, arrastre y diez contactos en el Pi;
no puede verificarse físicamente aquí.

## Cola por cuadro: interfaz para fase 3

`SurfaceBatcher.enqueue(event)` acepta dataclasses de superficie.py por nombre
y campos (Pad, Fsr, Boton, Encoder, Jog, Fader, Expresion, Perilla, Mano,
ManoPresencia), sin importar ni abrir hardware. Una cola FIFO protegida permite
productor externo. `QQuickWindow.afterAnimating` drena una vez antes del sync;
se aplican eventos en orden, acumulando deltas contra el estado pendiente.
La primera inserción despierta la ventana por señal queued (sin polling inactivo).
Cada modelo emite como máximo un dataChanged por cuadro; último valor de cada
rol gana, sin perder eventos de la cola. Los modelos de una ventana dejan de
usar timers independientes. Navegación/touch demo conservan sus comandos locales.
El adaptador visual es provisional; **no define el protocolo/comandos del núcleo**.

## Harness y lectura del resultado

```bash
cd apps/consola/gui-console
bash pi/setup_pi.sh
bash pi/run_gui.sh --rotate 90 --rotation-method qml --screen LiveSession
bash pi/run_perf.sh --rotate 90 --rotation-method qml --duration 120
bash pi/run_perf.sh --rotate 90 --rotation-method eglfs --duration 120
bash pi/run_perf.sh --rotate 90 --rotation-method qml --duration 1800 --rate 1000
```

`tools/perf_gate.py` carga **ambas pantallas reales simultáneamente**, cada una
con su contenido a 1280×568 en textura propia, vistas lado a lado a media escala.
Chrome sigue a 1280×720; dos texturas añaden costo y las vistas reducidas cambian
fill-rate: comparación conservadora de algunas cargas, **no equivalencia exacta
con dos pantallas físicas**. Es una interpretación explícita de «LIVE y MIXER
animando» del ADR; falta fijar oficialmente simultáneas vs alternadas.
Animators continuos en ambas vistas, sin blur vivo. `--cycle-screens` alterna
además el catálogo; no sustituye la carga doble.

Productor sintético en hilo separado, tasa configurable (1000 ev/s por defecto),
mezcla determinista de encoders/faders/FSR/manos/pads y controles restantes.
Usa exactamente la misma cola que recibirá el adaptador real. No se inyectan
updates directamente en los modelos. Calentamiento excluido (`--warmup`, 2 s);
medición por defecto 120 s. `--duration 3 --warmup 0 --offscreen` es solo smoke.

JSON + tabla PASS/FAIL/UNVERIFIED: fps globales y mínimo por ventanas de 1 s,
intervalos frameSwapped, beforeRendering→afterRendering, QSG render total p99,
latencia enqueue→frameSwapped p95, CPU media/máxima por segundo como porcentaje
de **un núcleo** (/proc/self/stat Linux; resource macOS), contadores FIFO y
rotación/método/backend/plataforma/fuentes. Se guardan series por cuadro y QSG.
El probe visual del lote participa en el sync; se reconoce el lote sincronizado
al swap, no al vaciar la cola. Valores intermedios coalescidos no aparecen uno a
uno: la latencia corresponde al cuadro con el estado final del lote. frameSwapped
es envío/presentación de Qt, **no fotodiodo ni tiempo de scanout del panel**.

Límites literales ADR: 60 fps sostenidos, p99 QSG <16.7 ms, p95 <32.667 ms,
CPU <25%. No se relaja 60 a 59 por redondeo: un panel de 59.94 Hz puede fallar;
el ADR no especifica tolerancia ni ventana de «sostenidos». Reportar ese caso.
La tasa real, cobertura de eventos, fuentes, warnings, duración, renderer y QSG
forman controles adicionales para evitar PASS con prueba incompleta.

`QSG_RENDER_TIMING=1` y `qt.scenegraph.time.renderloop=true` habilitan logs.
Se parsea `syncAndRender: frame rendered in ...ms` (total de Qt 6.8)
y `Frame rendered with ... renderloop in ...ms` (observado en Qt 6.11), sin sumar categorías
solapadas ni confundir tiempo GPU/CPU con intervalo entre swaps. Sin formato
reconocido: p99 QSG null y UNVERIFIED, nunca usar render CPU como sustituto.
`pi/run_perf.sh` guarda JSON, log Qt y vcgencmd measure_temp/get_throttled antes
y después. La CPU incluye generador e instrumentación: costo real de la prueba.
Los totales QSG de Qt 6.8 tienen resolución de 1 ms; conservar el log original.
Código de salida 0=gate certificado, 1=FAIL/UNVERIFIED, 2=error de argumentos.
Los logs software también se miden, pero no certifican hardware.
Offscreen/software/macOS, método EGLFS no verificado o duración <120 s nunca
certifican Pi. Revisar razones de elegibilidad aunque los números den PASS.

## systemd (instalación manual)

`pi/consola-gui.service` es plantilla de sistema: sustituir @USER@ y @GUI_DIR@
por usuario y ruta absoluta. Type=simple, VT1, Restart=on-failure; sin watchdog
porque no hay sd_notify ni dependencia adicional. Ejemplo desde el directorio GUI:

```bash
sed -e "s|@USER@|$(id -un)|g" -e "s|@GUI_DIR@|$PWD|g" pi/consola-gui.service > /tmp/consola-gui.service
sudo install -m 644 /tmp/consola-gui.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl start consola-gui
journalctl -u consola-gui -b
sudo systemctl stop consola-gui
```

No enable automático; si getty ocupa VT1, elegir otra VT o detener ese getty
manualmente antes de arrancar. El servicio no necesita red. Arranque en frío,
reinicio tras fallo, permisos KMS y funcionamiento offline pendientes en Pi.

## Naming y límites pendientes

C16 (audit solicitado **ausente** en este clone; no se reconstruye su contenido):
BRAIN = Teensy/synth; ENGINE = generador. Cambios de texto previstos:
`BRAIN OFFLINE`→`ENGINE OFFLINE`; `PAUSED · BRAIN OFFLINE`→`PAUSED · ENGINE OFFLINE`;
`No computer. The Brain generates every layer; you play it like a DJ set.`→
`No computer. The engine generates every layer; you play it like a DJ set.`;
`Brain      ready · 6/8 models`→`Engine     ready · 6/8 models`.
BRAIN status dot, boot firmware, presets y páginas synth conservan BRAIN.
Offline del motor no debe cambiar el dot del Teensy.

No se certifican Pi, touch físico, consumo térmico sostenido, diez contactos,
audio ni rendimiento con el núcleo real. El audit G32/C16/§7 no se pudo leer;
la preparación usa los hechos suministrados por el usuario. Diferencia de
versiones: Mac PySide6 6.11.1/Python 3.14 frente a Pi apt 6.8.2.1/Python 3.13;
compatibilidad debe comprobarse con el setup y la ejecución real. Contradicciones
previas de 11-gui-fase2 siguen abiertas (incluida FM 6-OP vs ADR-005 4-OP).

Referencias: [ADR-007](../adr/ADR-007-gui-consola-pyside6.md),
[Qt EGLFS y rotación](https://doc.qt.io/qt-6/embedded-linux.html),
[Qt input](https://doc.qt.io/qt-6/inputs-linux-device.html),
[Debian PySide6 Quick arm64](https://packages.debian.org/trixie/python3-pyside6.qtquick),
[EGLFS en libqt6opengl6](https://packages.debian.org/trixie/arm64/libqt6opengl6/filelist),
[formato QSG Qt 6.8](https://github.com/qt/qtdeclarative/blob/6.8/src/quick/scenegraph/qsgthreadedrenderloop.cpp).

## Verificación observada en Mac

`python3 -m pytest apps/consola/gui-console/tests -q`: **122 passed in 23.39 s**.
Los 110 casos previos siguen presentes; se corrige la expectativa del dot BRAIN
para que offline del ENGINE no simule desconexión del Teensy. Bash syntax y
`git diff --check` sin errores. Se inspeccionó una captura QML a 720×1280 con
ambas vistas y rotación 90°; no demuestra orientación/touch físico.

Smoke Darwin arm64, Python 3.14.3, Qt/PySide6 6.11.1, **offscreen/software**,
3 s medidos + 1 s de calentamiento, 1000 ev/s pedidos (no Pi, no gate aprobado):

| Ruta 90° | FPS medio / mínimo 1 s | QSG p99 ms | Latencia p95 ms | CPU un núcleo |
|---|---|---|---|---|
| mac-qml-90 | 43.00 / 30 | 150.00 | 182.03 | 86.14% |
| mac-eglfs-env-90 | 34.00 / 18 | 122.00 | 299.00 | 71.05% |

Ambas ejecuciones terminaron con exit 1, FAIL numérico y elegibilidad UNVERIFIED.
Todos los eventos encolados se aplicaron y reconocieron sin pendientes; cero
warnings Qt. El renderer software del Mac mostró variabilidad entre ejecuciones;
no inferir rendimiento del Pi ni costo GPU de rotación de estas cifras. JSON y logs
locales en `apps/consola/gui-console/perf-results/mac-*.{json,qt.log}` (ignorados).

Commit documental intentado antes del código: `.git/index.lock: Operation not
permitted`. Todo queda sin commit en codex/gui-pi; no push. No se tocó el checkout
USB ni archivos fuera del alcance solicitado.

## Calibración medida en el Pi (2026-11-10)

Panel Waveshare 7" DSI montado en vertical nativo: la imagen se rota con `--rotate 90 --rotation-method qml` (queda
derecha) y el touch con `QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=invertx:rotate=0` (con `run_gui.sh`:
`GUI_TOUCH_DEVICE=invertx GUI_TOUCH_ROTATION=0`). Probado por Juan: 90 → invertido, 270 → espejo, 270+invertx → mal,
**0+invertx → correcto**. Faltaban en el Pi los módulos QML (`qml6-module-qtquick`, `-qtquick-window`, `-qtqml`,
`-qtqml-workerscript`) que ya lista `setup_pi.sh`: correr el script completo antes de la primera prueba.

### Corrección (2026-11-10, medida con coordenadas crudas)

La calibración anterior (`invertx:rotate=0`) estaba mal; en SYNTH no se notó porque las cards tocadas estaban al
centro. Medido en `/dev/input/event1` (Goodix): esquina superior izquierda de la imagen = crudo (685, 65), inferior
derecha = (53, 1216). Con la imagen rotada 90° en QML, esas esquinas caen en (720, 0) y (0, 1280) del panel vertical:
**el crudo ya coincide, sin inversión ni rotación** (`QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=rotate=0`). `run_synth.sh` y
`run_drum.sh` usan ese valor por defecto.


## Respuesta touch (2026-11-11)

TouchTarget muestra press inmediato. Menú/soft keys/tabs actúan al presionar;
listas y cualquier destino dentro de Flickable/ListView esperan release y ceden
al scroll. `QStyleHints.startDragDistance=25` equivale a~3mm al DPI del panel7”.
No se añade pressDelay ni se bloquea stealing. Conserva calibración identidad
`rotate=0`, imagen QML90 y TouchController/jog/clic hardware. Cargas muestran aviso
desde la petición, hasta ACK/error/desconexión. Clock/meters tienen notificación
independiente de modelos estructurales. Harness reproducible y comparación de
CPU/ms por evento en [REPORT](../../consola/gui-console/screenshots/touch/REPORT.md).
Las cifras son de macOS offscreen; resta confirmar sensación, orientación y
CPU en el Pi con EGLFS+evdev y el mismo browse TRIBE.

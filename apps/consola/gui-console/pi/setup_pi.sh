#!/usr/bin/env bash
# Manual setup only. apt keeps Python 3.13, Qt and platform plugins ABI-compatible.
set -euo pipefail
GUI_DIR=$(cd "$(dirname "$0")/.." && pwd)
if [[ $(uname -s) != Linux ]] || [[ $(dpkg --print-architecture) != arm64 ]]; then
  echo 'Expected Raspberry Pi OS Lite / Debian trixie arm64.' >&2
  exit 1
fi
. /etc/os-release
if [[ ${VERSION_CODENAME:-} != trixie ]]; then
  echo 'Expected trixie; review package compatibility before setup.' >&2
  exit 1
fi
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  python3 python3-pyside6.qtcore python3-pyside6.qtgui python3-pyside6.qtqml \
  python3-pyside6.qtquick python3-pyside6.qtnetwork python3-pyside6.qtopengl python3-pytest \
  qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-layouts \
  qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript \
  libqt6opengl6 libegl1 libegl-mesa0 libgles2 libgbm1 libdrm2 libgl1-mesa-dri \
  libinput10 libinput-tools libudev1 libxkbcommon0 libdrm-tests \
  fontconfig fonts-dejavu-core ca-certificates
# Raspberry Pi repositories provide vcgencmd via raspi-utils; plain Debian may not.
if apt-cache show raspi-utils >/dev/null 2>&1; then
  sudo apt-get install -y --no-install-recommends raspi-utils
fi
/usr/bin/python3 - <<'PY'
from pathlib import Path
import sys
import PySide6
from PySide6.QtCore import QLibraryInfo, qVersion
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickWindow
from PySide6.QtNetwork import QLocalSocket
import shiboken6
plugins = Path(QLibraryInfo.path(QLibraryInfo.PluginsPath))
for relative in ('platforms/libqeglfs.so', 'egldeviceintegrations/libqeglfs-kms-integration.so'):
    if not (plugins/relative).is_file():
        raise SystemExit(f'Missing {plugins/relative}; inspect apt Qt installation, do not mix pip plugins')
print(f'Python {sys.version.split()[0]}, PySide6 {PySide6.__version__}, Qt {qVersion()}, plugins {plugins}')
PY
bash "$GUI_DIR/fonts/fetch_fonts.sh"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software /usr/bin/python3 - "$GUI_DIR" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, sys.argv[1])
from PySide6.QtGui import QGuiApplication
from main import create_engine
app = QGuiApplication([])
engine, state = create_engine()
if not engine.rootObjects():
    raise SystemExit('QML import smoke failed')
print('QML imports OK (offscreen smoke; KMS still requires a physical run).')
import shiboken6
shiboken6.delete(engine)
PY
printf '%s\n' 'Setup complete. No service installed/started; no boot or group changes.' \
  'Run from a local VT with access to /dev/dri and /dev/input.'

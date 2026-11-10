#!/usr/bin/env bash
set -euo pipefail
GUI_DIR=$(cd "$(dirname "$0")/.." && pwd)
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_INTEGRATION=eglfs_kms
export QSG_RHI_BACKEND=opengl
export QT_SCALE_FACTOR=1
export QT_QPA_EGLFS_HIDECURSOR=1
unset DISPLAY WAYLAND_DISPLAY QT_QUICK_BACKEND
if [[ -n ${GUI_KMS_CONFIG:-} ]]; then
  [[ -r $GUI_KMS_CONFIG ]] || { echo 'GUI_KMS_CONFIG is not readable' >&2; exit 2; }
  export QT_QPA_EGLFS_KMS_CONFIG=$GUI_KMS_CONFIG
fi
case ${GUI_INPUT_BACKEND:-evdev} in
  evdev)
    export QT_QPA_EGLFS_NO_LIBINPUT=1
    case ${GUI_TOUCH_ROTATION:-0} in 0|90|180|270) ;; *) echo 'Invalid GUI_TOUCH_ROTATION' >&2; exit 2 ;; esac
    export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS="${GUI_TOUCH_DEVICE:+$GUI_TOUCH_DEVICE:}rotate=${GUI_TOUCH_ROTATION:-0}"
    ;;
  libinput)
    unset QT_QPA_EGLFS_NO_LIBINPUT QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS
    export QT_QPA_LIBINPUT_TOUCH_MATRIX="${GUI_TOUCH_MATRIX:-1 0 0 0 1 0}"
    ;;
  *) echo 'GUI_INPUT_BACKEND must be evdev or libinput' >&2; exit 2 ;;
esac
# Shared environment for the harness; neither path contacts the motor.
if [[ ${GUI_RUN_PERF:-0} == 1 ]]; then
  exec "${GUI_PYTHON:-/usr/bin/python3}" "$GUI_DIR/tools/perf_gate.py" "$@"
fi
exec "${GUI_PYTHON:-/usr/bin/python3}" "$GUI_DIR/main.py" --frameless "$@"

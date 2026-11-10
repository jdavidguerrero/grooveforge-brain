#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export GUI_TOUCH_DEVICE="${GUI_TOUCH_DEVICE-}"  # medido 2026-11-10: sin inversión (crudo = imagen con rotación QML 90)
export GUI_TOUCH_ROTATION="${GUI_TOUCH_ROTATION:-0}"
: "${XDG_RUNTIME_DIR:?Set XDG_RUNTIME_DIR for the host socket}"
scheduler=(env)
if command -v chrt >/dev/null 2>&1; then scheduler=(chrt --other 0); fi
exec "${scheduler[@]}" bash "$HERE/run_gui.sh" --screen DrumRack --drum-mode \
    --rotate 90 --rotation-method qml \
    --drum-socket "$XDG_RUNTIME_DIR/gf-synth.sock" \
    --surface-port "${GF_SURFACE_PORT:-GF Consola}" \
    --host-midi-port "${GF_HOST_MIDI_PORT:-gf-synth-host}" "$@"

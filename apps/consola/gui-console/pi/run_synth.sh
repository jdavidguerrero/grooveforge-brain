#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export GUI_TOUCH_DEVICE="${GUI_TOUCH_DEVICE:-invertx}"
export GUI_TOUCH_ROTATION="${GUI_TOUCH_ROTATION:-0}"
: "${XDG_RUNTIME_DIR:?Set XDG_RUNTIME_DIR for the synth host socket}"
exec bash "$HERE/run_gui.sh" --screen SynthParts --synth-mode \
    --rotate 90 --rotation-method qml \
    --synth-socket "$XDG_RUNTIME_DIR/gf-synth.sock" \
    --surface-port "${GF_SURFACE_PORT:-GF Consola}" \
    --host-midi-port "${GF_HOST_MIDI_PORT:-gf-synth-host}" "$@"

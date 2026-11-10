#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export GUI_TOUCH_DEVICE="${GUI_TOUCH_DEVICE:-invertx}"
export GUI_TOUCH_ROTATION="${GUI_TOUCH_ROTATION:-0}"
: "${XDG_RUNTIME_DIR:?Set XDG_RUNTIME_DIR for the synth host socket}"
# An explicit CLI selection overrides the environment/default auto discovery.
explicit_external=0
for arg in "$@"; do
    case "$arg" in --external-midi-port|--external-midi-port=*) explicit_external=1 ;; esac
done
if (( ! explicit_external )); then
    set -- --external-midi-port "${GF_EXTERNAL_MIDI_PORT:-auto}" "$@"
fi
# The GUI always runs SCHED_OTHER, even if this launcher inherited RT policy.
scheduler=(env)
if command -v chrt >/dev/null 2>&1; then scheduler=(chrt --other 0); fi
exec "${scheduler[@]}" bash "$HERE/run_gui.sh" --screen SynthParts --synth-mode \
    --rotate 90 --rotation-method qml \
    --synth-socket "$XDG_RUNTIME_DIR/gf-synth.sock" \
    --surface-port "${GF_SURFACE_PORT:-GF Consola}" \
    --host-midi-port "${GF_HOST_MIDI_PORT:-gf-synth-host}" "$@"

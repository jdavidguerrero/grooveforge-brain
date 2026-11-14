#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
GF_REPO=${GF_REPO:-$(cd -- "$HERE/../../../.." && pwd)}
config=${GF_CONSOLE_CONFIG:-$HOME/.config/gf-consola/config}
if [[ -f $config ]]; then
    set -a
    # Local, trusted shell assignments; the installer writes shell-quoted paths.
    # shellcheck source=/dev/null
    source "$config"
    set +a
fi
: "${XDG_RUNTIME_DIR:?Run in the PipeWire user session (XDG_RUNTIME_DIR required)}"
case ${1:-} in
    host)
        exec "${GF_HOST_BINARY:-$GF_REPO/apps/synth-host/build/gf-synth-host}" \
            --backend pipewire --block 256 --socket "$XDG_RUNTIME_DIR/gf-synth.sock" \
            --drum-data "${GF_DRUM_DATA:-$HOME/.local/share/gf-drum}" \
            --drum-kit-python "${GF_DRUM_KIT_PYTHON:-$GF_REPO/tools/drum-kits/.venv/bin/python}" \
            --drum-kit-script "$GF_REPO/tools/drum-kits/drum_kits.py"
        ;;
    gui)
        case ${GF_CONSOLE_MODE:-DRUM} in
            DRUM) runner=run_drum.sh ;;
            SYNTH) runner=run_synth.sh ;;
            *) echo 'GF_CONSOLE_MODE must be DRUM or SYNTH' >&2; exit 2 ;;
        esac
        python3 "$GF_REPO/apps/consola/pi/systemd/wait_host.py"
        exec bash "$GF_REPO/apps/consola/gui-console/pi/$runner"
        ;;
    audio|midi)
        exec python3 "$GF_REPO/apps/consola/pi/systemd/links.py" "${1}"
        ;;
    *) echo 'Usage: run.sh host|gui|audio|midi' >&2; exit 2 ;;
esac

#!/usr/bin/env bash
# Install as juan, never through sudo; only linger needs elevated privileges.
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$HERE/../../.." && pwd)
dry_run=0
uninstall=0
host_binary=$repo/apps/synth-host/build/gf-synth-host
audio_node=
cue_node=
while (( $# )); do
    case $1 in
        --dry-run) dry_run=1; shift ;;
        --uninstall) uninstall=1; shift ;;
        --host-binary|--audio-node|--cue-node)
            (( $# >= 2 )) || { echo "Missing value for $1" >&2; exit 2; }
            case $1 in
                --host-binary) host_binary=$2 ;;
                --audio-node) audio_node=$2 ;;
                --cue-node) cue_node=$2 ;;
            esac
            shift 2 ;;
        --help)
            echo 'Usage: install.sh [--dry-run] [--uninstall] [--host-binary /absolute/path] [--audio-node NAME] [--cue-node NAME]'
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
run() {
    if (( dry_run )); then printf '%q ' "$@"; printf '\n'; else "$@"; fi
}
units=$HOME/.config/systemd/user
launcher=$HOME/.local/lib/gf-consola
config=$HOME/.config/gf-consola/config
names=(gf-console.target gf-synth-host.service gf-audio-links.service gf-midi-link.service gf-console-gui.service)
if (( ! dry_run )); then
    [[ $(uname -s) == Linux ]] || { echo 'Install requires Linux; use --dry-run on macOS.' >&2; exit 1; }
    (( EUID != 0 )) || { echo 'Run as juan, without sudo.' >&2; exit 1; }
fi
if (( uninstall )); then
    run systemctl --user disable --now gf-console.target
    for unit in "${names[@]}"; do run rm -f -- "$units/$unit"; done
    run rm -f -- "$launcher/run.sh"
    run systemctl --user daemon-reload
    echo 'Uninstalled. Config, musical data, PipeWire and shared linger preserved.'
    exit 0
fi
[[ $host_binary == /* ]] || { echo '--host-binary must be absolute' >&2; exit 2; }
if (( ! dry_run )); then
    [[ -x $host_binary ]] || { echo "Build the host first: $host_binary" >&2; exit 1; }
    for tool in systemctl loginctl pw-dump pw-link python3; do
        command -v "$tool" >/dev/null || { echo "Missing dependency: $tool" >&2; exit 1; }
    done
fi
run install -d -m 755 "$units" "$launcher"
run install -d -m 700 "${config%/*}"
if [[ ! -e $config ]]; then
    if (( dry_run )); then
        printf 'Create config (0600): %s; repo=%q host=%q audio=%q cue=%q\n' "$config" "$repo" "$host_binary" "$audio_node" "$cue_node"
    else
        # %q produces shell assignments; the config is sourced by run.sh only.
        (
            umask 077
            {
                printf 'GF_REPO=%q\nGF_HOST_BINARY=%q\n' "$repo" "$host_binary"
                printf 'GF_CONSOLE_MODE=DRUM\nGF_AUDIO_NODE=%q\nGF_CUE_NODE=%q\n' "$audio_node" "$cue_node"
                printf '%s\n' 'GF_AUDIO_LEFT=playback_FL' 'GF_AUDIO_RIGHT=playback_FR' \
                    'GF_CUE_LEFT=playback_FL' 'GF_CUE_RIGHT=playback_FR' \
                    'GF_EXTERNAL_MIDI_PORT=auto' 'GUI_TOUCH_DEVICE=""' 'GUI_TOUCH_ROTATION=0'
            } > "$config"
        )
    fi
else
    echo "Preserving $config (CLI defaults do not overwrite existing config)."
fi
run install -m 755 "$HERE/systemd/run.sh" "$launcher/run.sh"
for unit in "${names[@]}"; do run install -m 644 "$HERE/systemd/$unit" "$units/$unit"; done
run sudo loginctl enable-linger "$(id -un)"
run systemctl --user daemon-reload
run systemctl --user enable gf-console.target
echo "Installed. Review $config, then: systemctl --user start gf-console.target"
echo 'No udev rule: audio/MIDI registry watchers handle hotplug. Reboot to renew group limits if needed.'

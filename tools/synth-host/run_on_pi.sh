#!/usr/bin/env bash
# Nothing is installed; only /tmp build and explicit result directory are written.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
result_dir=${1:-/tmp/gf-synth-results}
minutes=${2:-60}
python3 -c 'import sys; n=float(sys.argv[1]); assert 0 < n <= 1440' "$minutes"
if pgrep -x gf-synth-host >/dev/null; then
    echo 'An existing gf-synth-host is running; stop it manually before this isolated test.' >&2
    exit 1
fi
mkdir -p -- "$result_dir"
if [[ -e "$result_dir/run.json" ]]; then
    echo 'Results directory already contains run.json; choose a new directory.' >&2
    exit 1
fi
result_dir=$(cd -- "$result_dir" && pwd)
build_dir=$(mktemp -d /tmp/gf-synth-build.XXXXXX)
run_dir=$(mktemp -d /tmp/gf-synth-run.XXXXXX)
host_pid= pattern_pid= collector_pid=
cleanup() {
    for pid in "$pattern_pid" "$collector_pid" "$host_pid"; do
        if [[ -n "$pid" ]]; then kill -TERM "$pid" 2>/dev/null || true; fi
    done
    for pid in "$pattern_pid" "$collector_pid" "$host_pid"; do
        if [[ -n "$pid" ]]; then wait "$pid" 2>/dev/null || true; fi
    done
}
trap cleanup EXIT INT TERM
cmake -S "$repo/apps/synth-host" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DGF_HOST_PIPEWIRE=ON >"$result_dir/build.log" 2>&1
cmake --build "$build_dir" -j4 >>"$result_dir/build.log" 2>&1
ctest --test-dir "$build_dir" --output-on-failure >"$result_dir/tests.log" 2>&1
"$build_dir/gf-synth-host" --backend pipewire --benchmark --no-state --socket "$run_dir/gf-synth.sock" --presets "$run_dir/presets" >"$result_dir/host.log" 2>&1 &
host_pid=$!
for ((i=0;i<100;i++)); do
    if [[ -S "$run_dir/gf-synth.sock" ]]; then break; fi
    if ! kill -0 "$host_pid" 2>/dev/null; then cat "$result_dir/host.log"; exit 1; fi
    sleep .1
done
# Provide exact sink ports from `pw-link -i`; never guess a hardware device.
if [[ -n "${GF_AUDIO_LEFT:-}" && -n "${GF_AUDIO_RIGHT:-}" ]]; then
    pw-link gf-synth-host:mix_L "$GF_AUDIO_LEFT"
    pw-link gf-synth-host:mix_R "$GF_AUDIO_RIGHT"
else
    echo 'Set GF_AUDIO_LEFT and GF_AUDIO_RIGHT to the Audio+ playback ports (pw-link -i).' >&2
    exit 1
fi
seconds=$(python3 -c 'import sys; print(float(sys.argv[1])*60)' "$minutes")
python3 "$repo/tools/synth-host/collect.py" --socket "$run_dir/gf-synth.sock" --seconds "$seconds" --output "$result_dir/run.json" &
collector_pid=$!
python3 "$repo/tools/synth-host/play_pattern.py" --socket "$run_dir/gf-synth.sock" --minutes "$minutes" &
pattern_pid=$!
wait "$pattern_pid"
pattern_pid=
wait "$collector_pid"
collector_pid=
kill -0 "$host_pid"
printf 'Results: %s\nBuild retained: %s\n' "$result_dir" "$build_dir"

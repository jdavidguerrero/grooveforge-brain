#!/usr/bin/env bash
# Offline runner. Pass an optional results directory and audio seconds per case.
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
results_dir="${1:-$PWD/gemelo-pi-results-$(date -u +%Y%m%dT%H%M%SZ)}"
audio_seconds="${2:-20}"
for command_name in cmake c++ tee diff; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf 'Missing prerequisite: %s (nothing was installed).\n' "$command_name" >&2
        exit 1
    fi
done
mkdir -p -- "$results_dir"
results_dir="$(cd -- "$results_dir" && pwd)"
# Refuse to mix runs from different source/flag states in a single folder.
if [[ -e "$results_dir/system.txt" ]]; then
    printf 'Results folder already used: %s\n' "$results_dir" >&2
    exit 1
fi
build_root="$(mktemp -d /tmp/gf-gemelo-pi-build.XXXXXX)"
printf 'Source: %s\nBuild: %s\nResults: %s\n' "$repo_dir" "$build_root" "$results_dir"

diagnostics() {
    printf '\n--- %s: %s ---\n' "$1" "$(date -u +%FT%TZ)"
    if command -v vcgencmd >/dev/null 2>&1; then
        vcgencmd measure_temp || true
        vcgencmd get_throttled || true
    else
        printf 'vcgencmd unavailable; temperature/throttling gate needs external evidence.\n'
    fi
    local entry
    for entry in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/class/thermal/thermal_zone*/temp; do
        if [[ -r "$entry" ]]; then printf '%s: ' "$entry"; cat -- "$entry"; fi
    done
}
trap 'diagnostics after_exit | tee -a "$results_dir/system.txt"' EXIT
{
    date -u +%FT%TZ
    uname -a
    c++ --version
    cmake --version
    if command -v lscpu >/dev/null 2>&1; then lscpu; fi
    if [[ -r /etc/os-release ]]; then cat /etc/os-release; fi
    if command -v git >/dev/null 2>&1; then
        git -C "$repo_dir" rev-parse HEAD
        git -C "$repo_dir" status --short
        git -C "$repo_dir" diff -- apps/brain-plugin tools/gemelo-pi
    fi
    diagnostics before
} | tee "$results_dir/system.txt"

for optimization in O2 O3; do
    build_dir="$build_root/$optimization"
    output_dir="$results_dir/$optimization"
    mkdir -p -- "$output_dir"
    cmake -S "$repo_dir/apps/brain-plugin" -B "$build_dir" \
        -DGF_BUILD_VST3=OFF -DGF_BUILD_ENGINE_BENCH=ON -DCMAKE_BUILD_TYPE=Release \
        -DGF_BENCH_OPT="$optimization" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        2>&1 | tee "$output_dir/configure.txt"
    cmake --build "$build_dir" --parallel 4 2>&1 | tee "$output_dir/build.txt"
    ctest --test-dir "$build_dir" --output-on-failure 2>&1 | tee "$output_dir/ctest.txt"
    cp -- "$build_dir/compile_commands.json" "$output_dir/compile_commands.json"
    cp -- "$build_dir/bench/source-sha256.txt" "$output_dir/source-sha256.txt"
    cp -- "$build_dir/CMakeCache.txt" "$output_dir/CMakeCache.txt"
    for repeat in 1 2 3; do
        run_dir="$output_dir/run-$repeat"
        mkdir -p -- "$run_dir"
        diagnostics "before $optimization run-$repeat" | tee -a "$results_dir/system.txt"
        "$build_dir/gf_engine_bench" --seconds "$audio_seconds" --threads 1 \
            --json "$run_dir/results.json" 2>&1 | tee "$run_dir/table.txt"
        diagnostics "after $optimization run-$repeat" | tee -a "$results_dir/system.txt"
        if [[ "$repeat" != 1 ]]; then
            diff -u -- "$output_dir/run-1/pcm-sha256.txt" "$run_dir/pcm-sha256.txt" \
                > "$run_dir/hash-comparison.txt"
        fi
    done
    printf '%s: PCM hashes stable over three runs.\n' "$optimization" | tee -a "$results_dir/summary.txt"
done
printf 'Results saved to %s\nBuild retained at %s\nReview JSON timing gates and system.txt thermal evidence; this does not approve hardware.\n' \
    "$results_dir" "$build_root" | tee -a "$results_dir/summary.txt"

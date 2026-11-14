#!/usr/bin/env bash
set -euo pipefail
GUI_DIR=$(cd "$(dirname "$0")/.." && pwd)
RESULT_DIR=${GUI_PERF_DIR:-$GUI_DIR/perf-results}
mkdir -p "$RESULT_DIR"
RUN_PATH="$RESULT_DIR/$(date -u +%Y%m%dT%H%M%SZ)-$$"
snapshot() {
  date -u +%FT%TZ
  if command -v vcgencmd >/dev/null 2>&1; then
    vcgencmd measure_temp || true
    vcgencmd get_throttled || true
  else
    echo 'vcgencmd unavailable; thermal/throttling status unverified'
  fi
}
snapshot > "$RUN_PATH.before.txt"
trap 'snapshot > "$RUN_PATH.after.txt"' EXIT
export GUI_RUN_PERF=1 QSG_RENDER_TIMING=1
# The harness enables and parses Qt's version-specific renderloop category.
set +e
bash "$GUI_DIR/pi/run_gui.sh" --qsg-timing "$@" --output "$RUN_PATH.json" 2>&1 | tee "$RUN_PATH.console.log"
status=${PIPESTATUS[0]}
set -e
printf 'Saved run: %s (exit %s)\n' "$RUN_PATH" "$status"
exit "$status"

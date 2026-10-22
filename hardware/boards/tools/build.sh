#!/bin/bash
# Rebuild one panel board from placement to DRC (same order as the Brain's build_pcb_a.sh).
#   bash hardware/boards/tools/build.sh pcb-f
set -euo pipefail
cd "$(dirname "$0")"
B=$1
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
KC=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
run() { "$KPY" "$@" 2>&1 | grep -v -e "assert" -e "leak" -e "^$" | tail -2; }
"$KPY" - "$B" <<'PY' 2>/dev/null
import sys, pcbnew
p = f"../{sys.argv[1]}/{sys.argv[1]}.kicad_pcb"
b = pcbnew.LoadBoard(p)
for t in list(b.GetTracks()): b.Delete(t)
for z in list(b.Zones()): b.Delete(z)
for g in list(b.Groups()): b.Delete(g)
pcbnew.SaveBoard(p, b)
PY
run "place_${B//-/_}.py"
run sync_from_netlist.py "$B"
run sync_fp_fields.py "$B"
run trim_silk_outside.py "$B"
run add_gnd_escapes.py "$B"
for pass in $([ "${SKIP_FREEROUTING:-0}" = 1 ] || echo 1 2); do
  rm -f "../$B/$B.ses" "../$B/${B}_best.ses"
  run route_board.py "$B" 60 || echo "Freerouting pass $pass gave no result"
done
# Nets that need their own lane end to end go first (PCB-A: the 0.5 mm +5V from J1 to J4, 200 mm,
# then longest first: everything fans out of the tab's IDC along the strip).
case "$B" in pcb-a) export ROUTE_FIRST=/+5V ;; pcb-k2) export ROUTE_FIRST=/LED_K2_RET,/LED_K2_IN ;; esac
run finish_routing.py "$B"
run repair_routes.py "$B"
run clean_vias.py "$B"
run add_gnd_pours.py "$B"
for _ in 1 2 3 4; do   # each tie can expose another stranded GND group; repeat until none is routed
  TIE=$("$KPY" tie_loose_gnd.py "$B" 2>/dev/null || true); echo "$TIE" | grep -v "^$" || true
  if echo "$TIE" | grep -q "ruteado"; then run add_gnd_pours.py "$B"; else break; fi
done
run sync_fp_fields.py "$B"
"$KPY" check_via_in_pad.py "$B" 2>/dev/null | head -12 || true
"$KC" pcb drc --schematic-parity --severity-all -o "/tmp/$B-drc.rpt" "../$B/$B.kicad_pcb" >/dev/null 2>&1 || true
grep -E "Found" "/tmp/$B-drc.rpt"
grep -E "^\[" "/tmp/$B-drc.rpt" | sed -E 's/\]:.*/]/' | sort | uniq -c | sort -rn

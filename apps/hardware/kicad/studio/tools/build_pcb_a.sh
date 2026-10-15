#!/bin/bash
# Reconstruye PCB-A desde la ubicación hasta los planos de GND (rev. 2026-10-15, sin PCB-K).
#   bash apps/hardware/kicad/studio/tools/build_pcb_a.sh
set -euo pipefail
cd "$(dirname "$0")/.."
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3
KC=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
run() { "$KPY" "$@" 2>&1 | grep -v -e "assert" -e "leak" | tail -1; }
"$KPY" - <<'PY' 2>/dev/null
import pcbnew
b = pcbnew.LoadBoard("pcb-a/pcb-a.kicad_pcb")
for t in list(b.GetTracks()): b.Delete(t)
for z in list(b.Zones()): b.Delete(z)
for g in list(b.Groups()): b.Delete(g)
pcbnew.SaveBoard("pcb-a/pcb-a.kicad_pcb", b)
PY
run tools/place_pcb_a.py
run tools/sync_fp_fields.py pcb-a
run tools/trim_silk_outside.py pcb-a
run tools/add_gnd_escapes.py pcb-a
for pass in 1 2; do
  rm -f pcb-a/pcb-a.ses pcb-a/pcb-a_best.ses
  run tools/route_board.py pcb-a 40 || echo "pasada $pass de Freerouting sin resultado"
done
run tools/finish_routing.py pcb-a
run tools/clean_vias.py pcb-a
run tools/add_gnd_pours.py pcb-a
run tools/sync_fp_fields.py pcb-a
"$KC" pcb drc --schematic-parity -o /tmp/pcb-a-drc.rpt pcb-a/pcb-a.kicad_pcb >/dev/null 2>&1 || true
grep -E "Found" /tmp/pcb-a-drc.rpt

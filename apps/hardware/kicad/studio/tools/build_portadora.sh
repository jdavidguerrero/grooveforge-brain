#!/bin/bash
# Reconstruye la placa portadora desde la ubicación hasta los planos de GND, en orden.
#   bash apps/hardware/kicad/studio/tools/build_portadora.sh
# Requiere KiCad (Python de pcbnew y kicad-cli) y ~/.kicad-mcp/freerouting.jar.
set -euo pipefail
cd "$(dirname "$0")/.."
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3
KC=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
run() { "$KPY" "$@" 2>&1 | grep -v -e "assert" -e "leak" | tail -1; }

# 0. Placa sin pistas, zonas ni grupos (la ubicación se rehace igual).
"$KPY" - <<'PY' 2>/dev/null
import pcbnew
b = pcbnew.LoadBoard("portadora/portadora.kicad_pcb")
for t in list(b.GetTracks()): b.Delete(t)
for z in list(b.Zones()): b.Delete(z)
for g in list(b.Groups()): b.Delete(g)
pcbnew.SaveBoard("portadora/portadora.kicad_pcb", b)
PY
# 1. Ubicación, campos del esquemático y serigrafía fuera de la placa.
run tools/place_portadora.py
run tools/sync_fp_fields.py portadora
run tools/trim_silk_outside.py portadora
# 2. Vía propia para cada pad SMD de GND.
run tools/add_gnd_escapes.py portadora
# 3. Conexiones que Freerouting deja encerradas: van primero y bloqueadas.
run tools/fix_portadora_gnd.py                              # espina de GND: J5, J6, J10 y TVS
# El orden de pines de J5 sigue la x de los pines de la Teensy (set_idc_pinout.py): el bus baja casi
# derecho y ya no hacen falta rutas previas para las líneas que antes quedaban encerradas.
# 4. El resto con Freerouting (GND por planos), 2 pasadas.
for pass in 1 2; do
  rm -f portadora/portadora.ses portadora/portadora_best.ses
  run tools/route_board.py portadora 40 || echo "pasada $pass de Freerouting sin resultado"
done
# 5. Lo que Freerouting no cerró y vías sueltas.
run tools/finish_routing.py portadora
run tools/clean_vias.py portadora
run tools/grid_route.py portadora U1.3 C2.2 0.3 0.26        # GND de U1 (entre las filas de la Teensy)
# 6. Planos de GND con costura, campos y DRC.
run tools/add_gnd_pours.py portadora
# 6b. Retorno del bus I2S: GND junto a cada cambio de capa y en los huecos del plano (§10K D04).
run tools/stitch_returns.py portadora /I2S_MCLK,/I2S_BCLK,/I2S_LRCLK,/I2S_RX,/I2S_TX
run tools/sync_fp_fields.py portadora
"$KC" pcb drc --schematic-parity -o /tmp/portadora-drc.rpt portadora/portadora.kicad_pcb >/dev/null 2>&1 || true
grep -E "Found" /tmp/portadora-drc.rpt
# Sin vías dentro de pads SMD (§10K D05). Si falla, sacar la vía del pad con un cuello corto a mano.
"$KPY" tools/check_via_in_pad.py portadora 2>&1 | grep -v -e assert -e leak | tail -3
# Nota (2026-10-22): el ruteo entregado incluye ediciones locales de §10K (D05: GND de U1/C2 desde sus vías de
# escape). Una corrida completa vuelve a rutear con Freerouting: repetir las comprobaciones de §10K.

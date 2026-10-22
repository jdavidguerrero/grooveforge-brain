"""Falla si hay una vía con el centro dentro de un pad SMD (theory/02 §10K, auditoría delta D05).

  $KPY apps/hardware/kicad/studio/tools/check_via_in_pad.py portadora

Una vía abierta dentro de un pad SMD se lleva la pasta al barril: unión pobre y asimetría térmica. JLC solo lo
acepta con vía rellena y tapada (proceso aparte). El DRC de KiCad no lo marca con las reglas de este proyecto.
"""
import os
import sys

import pcbnew as p

HERE = os.path.dirname(os.path.abspath(__file__))
name = sys.argv[1]
b = p.LoadBoard(os.path.join(HERE, "..", name, f"{name}.kicad_pcb"))
vias = [t for t in b.GetTracks() if t.GetClass() == "PCB_VIA"]
bad = [(f.GetReference(), pd.GetNumber(), round(p.ToMM(v.GetPosition().x), 2), round(p.ToMM(v.GetPosition().y), 2))
       for f in b.GetFootprints() for pd in f.Pads() if pd.GetAttribute() == p.PAD_ATTRIB_SMD
       for v in vias if pd.HitTest(v.GetPosition())]
for ref, num, x, y in bad:
    print(f"VÍA EN PAD: {ref}.{num} en ({x}, {y})")
print(f"{name}: {len(bad)} vías en pads SMD")
sys.exit(1 if bad else 0)

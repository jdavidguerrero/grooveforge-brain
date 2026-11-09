"""Borra la serigrafía de los footprints que cae fuera del contorno de la placa.

  $KPY apps/hardware/kicad/studio/tools/trim_silk_outside.py portadora

Caso típico: la nariz de un jack de panel sobresale del borde (atraviesa la pared) y su dibujo en la
serigrafía queda en el aire (DRC "silk_edge_clearance"). Edita el .kicad_pcb como texto: en algunos
footprints GraphicalItems() de la API de Python devuelve un puntero SWIG sin envolver.
"""
import math
import os
import re
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
MARGIN = 0.2


def blocks(text, head):
    """(inicio, fin) de cada bloque '(head ...' de primer nivel dentro de text."""
    out = []
    for m in re.finditer(r"\(" + head + r"\b", text):
        depth = 0
        for j in range(m.start(), len(text)):
            if text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
                if depth == 0:
                    out.append((m.start(), j + 1))
                    break
    return out


def main():
    name = sys.argv[1]
    path = os.path.join(HERE, "..", name, f"{name}.kicad_pcb")
    bb = pcbnew.LoadBoard(path).GetBoardEdgesBoundingBox()
    x0, y0 = pcbnew.ToMM(bb.GetLeft()) + MARGIN, pcbnew.ToMM(bb.GetTop()) + MARGIN
    x1, y1 = pcbnew.ToMM(bb.GetRight()) - MARGIN, pcbnew.ToMM(bb.GetBottom()) - MARGIN

    text = open(path).read()
    removed = 0
    for fa, fb in reversed(blocks(text, "footprint")):
        fp = text[fa:fb]
        at = re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)", fp)
        if not at:
            continue
        ox, oy = float(at.group(1)), float(at.group(2))
        a = math.radians(float(at.group(3) or 0))
        new = fp
        for ga, gb in reversed(blocks(fp, r"fp_(?:line|rect|poly|arc|circle)")):
            g = fp[ga:gb]
            if '"F.SilkS"' not in g and '"B.SilkS"' not in g:
                continue
            pts = [(float(px), float(py)) for px, py in re.findall(r"\((?:start|end|xy|mid|center) ([-\d.]+) ([-\d.]+)\)", g)]
            # Local → placa (ángulo positivo = antihorario en pantalla, con y hacia abajo).
            world = [(ox + px * math.cos(a) + py * math.sin(a), oy - px * math.sin(a) + py * math.cos(a)) for px, py in pts]
            if any(not (x0 <= wx <= x1 and y0 <= wy <= y1) for wx, wy in world):
                new = new[:ga] + new[gb:]
                removed += 1
        text = text[:fa] + new + text[fb:]
    open(path, "w").write(text)
    print(f"{name}: {removed} trazos de serigrafía fuera de la placa borrados")


main()

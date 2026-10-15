"""Genera los archivos para pedir las tres placas a JLCPCB: Gerbers + taladros (zip), BOM y CPL.

  $KPY apps/hardware/kicad/studio/tools/export_fab.py            # las tres
  $KPY apps/hardware/kicad/studio/tools/export_fab.py portadora  # una

Salida en fab/<placa>/: <placa>-gerbers.zip, <placa>-bom.csv, <placa>-cpl.csv.
BOM y CPL incluyen solo las piezas con campo LCSC (los SMD que ensambla JLC; ver set_lcsc.py).

Rotaciones: KiCad y JLC no usan el mismo cero para algunos encapsulados. ROT_FIX aplica las correcciones
conocidas (base de datos de kicad-jlcpcb-tools); igual hay que revisar el visor de ubicación de JLC antes
de pagar, sobre todo los LEDs SK6812 de PCB-K y los diodos.
"""
import csv
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
LAYERS = "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts"
# Rev. 2026-10-15 (prototipo económico): PCB-K no se pide (sin LEDs, los Choc van cableados a J1 de PCB-A)
# y PCB-A va sin ensamble. Solo la portadora lleva BOM y CPL.
BOARDS = ["pcb-a", "portadora"]
ASSEMBLE = {"pcb-k": True, "pcb-a": False, "portadora": True}
ROT_FIX = [(r"^SOT-23", 180), (r"^SOIC-", 270), (r"^CP_Elec_", 180),
           (r"^IDC-Header_", -90)]   # el modelo de JLC del box header está acostado a 0°


def cli(*args):
    subprocess.run([KICAD_CLI, *args], check=True, capture_output=True)


def gerbers(name, pcb, out):
    with tempfile.TemporaryDirectory() as tmp:
        cli("pcb", "export", "gerbers", "--layers", LAYERS, "--subtract-soldermask", "-o", tmp + "/", pcb)
        cli("pcb", "export", "drill", "--format", "excellon", "--drill-origin", "absolute",
            "--excellon-units", "mm", "--excellon-separate-th", "-o", tmp + "/", pcb)
        files = sorted(os.listdir(tmp))
        with zipfile.ZipFile(os.path.join(out, f"{name}-gerbers.zip"), "w", zipfile.ZIP_DEFLATED) as z:
            for f in files:
                z.write(os.path.join(tmp, f), f)
    return files


def part_center(fp):
    """Centro que espera JLC. Los SMD tienen el origen en el centro; los conectores THT de KiCad lo tienen
    en el pin 1 (el IDC 2×15 quedaba corrido 17.8 mm), así que para ellos se usa el centro de los pads."""
    pads = [p for p in fp.Pads() if p.GetNumber()]
    if not any(p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH for p in pads):
        return fp.GetPosition()
    xs = [p.GetPosition().x for p in pads]
    ys = [p.GetPosition().y for p in pads]
    return pcbnew.VECTOR2I((min(xs) + max(xs)) // 2, (min(ys) + max(ys)) // 2)


def bom_and_cpl(name, pcb, out):
    board = pcbnew.LoadBoard(pcb)
    groups, rows = {}, []
    for fp in board.GetFootprints():
        if not fp.HasField("LCSC") or not fp.GetFieldText("LCSC") or fp.IsExcludedFromBOM():
            continue
        ref, lcsc = fp.GetReference(), fp.GetFieldText("LCSC")
        package = str(fp.GetFPID().GetLibItemName())
        groups.setdefault((fp.GetValue(), package, lcsc), []).append(ref)
        rot = fp.GetOrientationDegrees()
        for pattern, delta in ROT_FIX:
            if re.match(pattern, package):
                rot += delta
                break
        x, y = pcbnew.ToMM(part_center(fp))
        rows.append([ref, f"{x:.4f}mm", f"{-y:.4f}mm", "Bottom" if fp.IsFlipped() else "Top",
                     f"{rot % 360:.1f}"])
    natural = lambda r: [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", r)]
    with open(os.path.join(out, f"{name}-bom.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (value, package, lcsc), refs in sorted(groups.items(), key=lambda g: natural(g[1][0])):
            w.writerow([value, ",".join(sorted(refs, key=natural)), package, lcsc])
    with open(os.path.join(out, f"{name}-cpl.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writerows(sorted(rows, key=lambda r: natural(r[0])))
    return len(groups), len(rows), {r[3] for r in rows}


def main():
    names = sys.argv[1:] or BOARDS
    for name in names:
        pcb = os.path.join(ROOT, name, f"{name}.kicad_pcb")
        out = os.path.join(ROOT, "fab", name)
        shutil.rmtree(out, ignore_errors=True)
        os.makedirs(out)
        files = gerbers(name, pcb, out)
        if not ASSEMBLE.get(name, True):
            print(f"{name}: {len(files)} archivos Gerber/taladro · sin ensamble (solo PCB)")
            continue
        lines, parts, sides = bom_and_cpl(name, pcb, out)
        print(f"{name}: {len(files)} archivos Gerber/taladro · BOM {lines} líneas · CPL {parts} piezas "
              f"({', '.join(sorted(sides))})")


main()

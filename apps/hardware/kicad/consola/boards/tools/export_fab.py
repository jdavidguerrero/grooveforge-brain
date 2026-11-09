"""Gerbers + drill (zip), BOM and CPL for ordering the GroovePush panel boards at JLCPCB.
Adapted from the GrooveForge Brain tool of the same name.

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
# GroovePush: the four panel boards, all with single-sided (B.Cu) Economic assembly. THT parts
# (EC11, RS6011 faders, Choc V2, the IDC box header) carry no LCSC field and are hand-soldered.
BOARDS = ["pcb-a", "pcb-f", "pcb-k1", "pcb-k2", "pcb-s"]
ASSEMBLE = {b: True for b in BOARDS}
ASSEMBLE["pcb-s"] = False   # shield adapter: two pin headers + two JSTs, hand-soldered
ROT_FIX = [(r"^SOT-23", 180), (r"^SOIC-", 270), (r"^SSOP-", 270), (r"^CP_Elec_", 180),
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
    groups, rows, alt_rows = {}, [], []
    for fp in board.GetFootprints():
        if not fp.HasField("LCSC") or not fp.GetFieldText("LCSC") or fp.IsExcludedFromBOM():
            continue
        ref, lcsc = fp.GetReference(), fp.GetFieldText("LCSC")
        package = str(fp.GetFPID().GetLibItemName())
        # One line per part: the LEDs carry their key in the value ("SK6812MINI-EA (PLAY, idx 11)").
        value = re.sub(r"\s*\(.*\)$", "", fp.GetValue())
        m = re.match(r"JST_SH_(SM\d+B-SRSS-TB)", package)   # the cable note ("to PCB-K2 J1") isn't a part name
        if m:
            value = m.group(1)
        groups.setdefault((package, lcsc), {"value": value, "refs": []})
        g = groups[(package, lcsc)]
        g["refs"].append(ref)
        if len(value) < len(g["value"]):
            g["value"] = value
        rot = fp.GetOrientationDegrees()
        for pattern, delta in ROT_FIX:
            if re.match(pattern, package):
                rot += delta
                break
        x, y = pcbnew.ToMM(part_center(fp))
        rows.append([ref, f"{x:.4f}mm", f"{-y:.4f}mm", "Bottom" if fp.IsFlipped() else "Top",
                     f"{rot % 360:.1f}"])
        # Alternative bottom-side convention, from the community tool that tracks JLC's behaviour
        # (JLCKicadTools, cpl_fix_rotations.py: "-rotation + 180" for bottom parts). It only differs from the
        # file above for parts at 0 or 180 degrees (LEDs, some JSTs); 90/270 come out the same.
        alt = (-rot + 180) % 360 if fp.IsFlipped() else rot % 360
        alt_rows.append([ref, f"{x:.4f}mm", f"{-y:.4f}mm", "Bottom" if fp.IsFlipped() else "Top", f"{alt:.1f}"])
    natural = lambda r: [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", r)]
    with open(os.path.join(out, f"{name}-bom.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (package, lcsc), g in sorted(groups.items(), key=lambda g: natural(sorted(g[1]["refs"], key=natural)[0])):
            w.writerow([g["value"], ",".join(sorted(g["refs"], key=natural)), package, lcsc])
    with open(os.path.join(out, f"{name}-cpl.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writerows(sorted(rows, key=lambda r: natural(r[0])))
    with open(os.path.join(out, f"{name}-cpl-alt-bottom.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writerows(sorted(alt_rows, key=lambda r: natural(r[0])))
    differ = sorted((a[0] for a, b in zip(alt_rows, rows) if a[4] != b[4]), key=natural)
    if differ:
        print(f"   {name}: {len(differ)} parts differ between the two CPL conventions: {', '.join(differ)}")
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

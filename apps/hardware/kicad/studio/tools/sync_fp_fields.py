"""Copia los campos Description y Datasheet de cada símbolo al footprint de la placa (lo que hace F8 en KiCad).

  $KPY apps/hardware/kicad/studio/tools/sync_fp_fields.py pcb-k
"""
import os
import re
import subprocess
import sys
import tempfile

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"


def main():
    name = sys.argv[1]
    folder = os.path.join(HERE, "..", name)
    with tempfile.TemporaryDirectory() as tmp:
        net = os.path.join(tmp, "n.net")
        subprocess.run([KICAD_CLI, "sch", "export", "netlist", "--format", "kicadsexpr", "-o", net,
                        os.path.join(folder, f"{name}.kicad_sch")], check=True, capture_output=True)
        text = open(net).read()
    fields = {}
    for comp in re.split(r"\(comp\s", text)[1:]:
        ref = re.search(r'\(ref "([^"]+)"\)', comp).group(1)
        d = re.search(r'\(description "([^"]*)"\)', comp)
        ds = re.search(r'\(datasheet "([^"]*)"\)', comp)
        fields[ref] = {"Description": d.group(1) if d else "", "Datasheet": ds.group(1) if ds else ""}
    unconnected = {}
    for chunk in re.split(r"\(net\s*\(code", text)[1:]:
        net_name = re.search(r'\(name "([^"]+)"', chunk).group(1)
        if net_name.startswith("unconnected-"):
            unconnected[net_name] = re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', chunk)
    path = os.path.join(folder, f"{name}.kicad_pcb")
    board = pcbnew.LoadBoard(path)
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        for field, value in fields.get(ref, {}).items():
            fp.SetField(field, value)
        if ref.startswith(("TP", "JP")):
            # Puntos de prueba y solder jumpers: cobre de la placa, no piezas que JLC monte.
            fp.SetExcludedFromBOM(True)
            fp.SetExcludedFromPosFiles(True)
    # Pines sin conexión: el esquemático les da una red "unconnected-(…)" propia y la paridad la exige.
    for net_name, nodes in unconnected.items():
        net = board.FindNet(net_name)
        if net is None:
            net = pcbnew.NETINFO_ITEM(board, net_name)
            board.Add(net)
        for ref, pin in nodes:
            fp = board.FindFootprintByReference(ref)
            if fp:
                for pad in fp.Pads():
                    if pad.GetNumber() == pin and not pad.GetNetname():
                        pad.SetNet(net)
    pcbnew.SaveBoard(path, board)
    print(f"{name}: campos sincronizados")


main()

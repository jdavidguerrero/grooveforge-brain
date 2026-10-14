"""Rutea una placa de Studio con Freerouting (DSN → Freerouting → SES) y guarda el resultado.

  KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3
  $KPY apps/hardware/kicad/studio/tools/route_board.py pcb-k [pasadas]

GND no se rutea con pistas: se saca del DSN y después lo unen los planos (add_gnd_pours.py). Si el plano
va en el DSN, Freerouting lo trata como cobre sólido y pierde esa capa para las demás redes.

Freerouting usa la holgura de la clase de red también contra el borde de la placa, y no lee las reglas
de JLC ni el .kicad_dru: durante el ruteo se exporta con ROUTE_CLEARANCE para que respete los 0.25 mm al
borde (incluidas las ventanas de los LEDs). En la placa quedan las holguras de la clase (0.2 mm).
Requiere ~/.kicad-mcp/freerouting.jar (Freerouting 2.1.0).
"""
import os
import re
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
JAR = os.path.expanduser("~/.kicad-mcp/freerouting.jar")
SKIP_NETS = ["/GND"]
ROUTE_CLEARANCE = 0.25
TIMEOUT = 420


def strip_nets(dsn, nets):
    """Saca redes del DSN: sus pads quedan como obstáculos y Freerouting no intenta unirlos."""
    text = open(dsn).read()
    for net in nets:
        # Bloque (net /GND (pins ...)) de la sección network.
        text = re.sub(r"\(net " + re.escape(net) + r"\s*\(pins[^)]*\)\s*\)", "", text)
        # Pistas y vías ya existentes de la red (se guardan aparte y se reponen después del SES).
        text = re.sub(r"^\s*\((?:wire|via)\b.*\(net " + re.escape(net) + r"\).*$\n?", "", text,
                      flags=re.MULTILINE)
        # La red dentro de las listas (class ...).
        text = re.sub(r"(?<=\s)" + re.escape(net) + r"(?=[\s)])", "", text)
    open(dsn, "w").write(text)


def main():
    name = sys.argv[1]
    passes = sys.argv[2] if len(sys.argv) > 2 else "100"
    folder = os.path.join(HERE, "..", name)
    pcb_path = os.path.join(folder, f"{name}.kicad_pcb")
    dsn = os.path.join(folder, f"{name}.dsn")
    ses = os.path.join(folder, f"{name}.ses")

    board = pcbnew.LoadBoard(pcb_path)
    kept = [t.Duplicate() for t in board.GetTracks() if t.GetNetname() in SKIP_NETS]
    ds = board.GetDesignSettings()
    saved = {}
    for nc_name, nc in ds.m_NetSettings.GetNetclasses().items():
        saved[nc_name] = nc.GetClearance()
        nc.SetClearance(pcbnew.FromMM(ROUTE_CLEARANCE))
    default = ds.m_NetSettings.GetDefaultNetclass()
    saved["__default__"] = default.GetClearance()
    default.SetClearance(pcbnew.FromMM(ROUTE_CLEARANCE))
    for z in list(board.Zones()):
        board.Delete(z)
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        raise SystemExit("no se pudo exportar el DSN")
    strip_nets(dsn, SKIP_NETS)

    # Freerouting a veces se queda optimizando sin fin con 1–2 conexiones pendientes: se corta a los
    # TIMEOUT segundos y se importa el mejor SES que haya escrito.
    try:
        subprocess.run(["java", "-jar", JAR, "-de", dsn, "-do", ses, "--gui.enabled=false",
                        "-mp", passes, "-oit", "100", "--router.optimizer.max_passes=1",
                        "--router.save_intermediate_stages=true", "-mt", "1"], check=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        print(f"Freerouting cortado a los {TIMEOUT} s")
    if not os.path.exists(ses):
        best = ses.replace(".ses", "_best.ses")
        if not os.path.exists(best):
            raise SystemExit("Freerouting no escribió ningún SES")
        ses = best

    board = pcbnew.LoadBoard(pcb_path)   # recargar: las holguras de la clase vuelven a las del proyecto
    if not pcbnew.ImportSpecctraSES(board, ses):
        raise SystemExit("no se pudo importar el SES")
    for t in kept:                       # el SES reemplaza todas las pistas: reponer las de SKIP_NETS
        t.SetParent(board)
        board.Add(t)
    pcbnew.SaveBoard(pcb_path, board)
    print(f"{name}: ruteo importado ({len(board.GetTracks())} pistas y vías)")


main()

"""Ubica los componentes de la portadora en el piso del enclosure (rev. 2026-10-14).

  KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3
  $KPY apps/hardware/kicad/studio/tools/place_portadora.py

Coordenadas de PLANTA (no del panel inclinado): X desde el borde izquierdo, Y horizontal desde el frente,
en mm. En la placa: x_pcb = X + 50, y_pcb = 200 - Y (mismo sistema que PCB-K y PCB-A).
Fuente: theory/02-studio-pcb.md §4.3–4.5 y design/README.md.

Franjas (de atrás hacia adelante):
  Y 138–162  hilera de jacks contra la pared trasera (cara interior en Y 162). Desde atrás se lee:
             IN L, IN R, OUT L, OUT R, PHONES, CV OUT.
  Y  92–137  Audio Shield a la izquierda · bloque analógico (MCP6004) a la derecha: bajo los USB/Ethernet
             de la Pi solo piezas bajas (theory/02 §4.5).
  Y  72– 92  Teensy 4.1 a lo largo, USB hacia la izquierda.
  Y  61– 72  J6 (cable a la Pi) · alimentación (J10, F1, TVS) · J5 (IDC desde PCB-A), corrido
             respecto del J2 de PCB-A para que el cable haga una S.
Placa: X 44–141, Y 62–162 (97 × 100 mm). Después correr trim_silk_outside.py (serigrafía de las narices
de los jacks, que atraviesan la pared).
"""
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, "..", "portadora", "portadora.kicad_pcb")

WALL_Y = 162.0                 # cara interior de la pared trasera (PETG 3 mm)
OUTLINE = (44.0, 62.0, 141.0, 162.0)   # borde trasero = cara de la pared (los jacks apoyan ahí)
MOUNT_HOLES = [(48.0, 100.0), (48.0, 128.0), (138.0, 128.0), (138.0, 77.0)]
MOUNT_HOLE_D = 3.2

# Jacks: X del eje. NRJ3HF-1: nariz hacia +x local, cara del panel en x = 16.95, eje en y = 7.23.
# SJ1-3533NG: nariz hacia −y local, cara del panel en y = −1.2, eje en x = 0.9.
JACKS_635 = {"J7": 133.0, "J8": 115.0, "J11": 97.0, "J12": 79.0}      # IN L, IN R, OUT L, OUT R
JACKS_35 = {"J13": 62.0, "J14": 50.0}                                 # PHONES, CV OUT

# Teensy 4.1 (USB hacia −X): pin 1 de cada fila a 1.27 mm del borde del USB.
TEENSY_PIN1_X = 51.3
TEENSY_ROWS_Y = {"J1": 75.0, "J2": 90.24}                             # fila 0–12 adelante, 13–23 atrás
# Audio Shield: filas a 15.24 mm, pin 1 (12 y 13) a la izquierda; su borde no toca el de la Teensy.
SHIELD_PIN1_X = 56.5
SHIELD_ROWS_Y = {"J16": 100.4, "J15": 115.64}

# (ref, X, Y, rot) en planta; centro del footprint salvo los conectores ubicados por sus pads.
PARTS = [
    # Alimentación al frente, entre J6 y J5: la Pi (J6) toma hasta 2.5 A y queda a ~20 mm del fusible
    # (con el bloque a la derecha eran 115 mm y una vía). J10 recibe el cable de la USB-C de panel
    # (pared trasera, x 22). El 100 µF queda a la derecha como reserva para los LEDs (J5).
    ("J10", 72.0, 66.5, 0), ("F1", 82.5, 64.8, 0), ("D1", 82.8, 69.6, 0), ("C3", 70.5, 71.6, 0),
    ("C1", 128.0, 82.0, 0), ("C4", 136.0, 73.5, 0),
    # Buffer de LEDs bajo la Teensy, entre sus filas (pin 28 = fila delantera, X 99.6)
    ("U1", 98.0, 82.0, 0), ("C2", 98.0, 85.6, 0), ("JP1", 103.0, 82.0, 90), ("R3", 107.0, 82.0, 90),
    # Entrada analógica (L arriba, R abajo de U2) y VREF
    ("U2", 118.0, 112.0, 0), ("C7", 118.0, 105.5, 0),
    ("C5", 100.0, 121.0, 0), ("R10", 104.0, 121.0, 0), ("D2", 108.0, 121.0, 0), ("R12", 112.0, 121.0, 0),
    ("C6", 100.0, 103.0, 0), ("R11", 104.0, 103.0, 0), ("D3", 108.0, 103.0, 0), ("R13", 112.0, 103.0, 0),
    ("C8", 126.0, 118.0, 0), ("C9", 126.0, 106.0, 0),
    ("R14", 129.5, 112.0, 90), ("R15", 132.0, 112.0, 90), ("C10", 135.0, 112.0, 90),
    ("JP2", 122.0, 121.5, 0), ("JP3", 122.0, 102.5, 0), ("JP4", 130.0, 104.0, 0), ("JP5", 130.0, 120.0, 0),
    ("TP1", 137.0, 106.0, 0), ("TP2", 137.0, 118.0, 0), ("TP3", 137.0, 100.0, 0), ("TP4", 137.0, 124.0, 0),
    ("TP5", 97.8, 112.0, 0),
    # Escalera del pad junto a los jacks de entrada y header de la palanca PAD
    ("R4", 133.0, 135.0, 0), ("R5", 133.0, 132.5, 0), ("R6", 133.0, 130.0, 0),
    ("R7", 115.0, 135.0, 0), ("R8", 115.0, 132.5, 0), ("R9", 115.0, 130.0, 0),
    # Salidas (10 µF + 47k) junto a OUT L / OUT R
    ("C11", 97.0, 134.5, 0), ("R16", 97.0, 131.5, 0), ("C12", 79.0, 134.5, 0), ("R17", 79.0, 131.5, 0),
    # CV OUT: RC de 2 polos + 1k junto a su jack
    ("R18", 53.0, 135.0, 0), ("C13", 53.0, 132.5, 0), ("R19", 57.0, 135.0, 0), ("C14", 57.0, 132.5, 0),
    ("R20", 57.0, 130.0, 0),
]
# Conectores ubicados por el centro de sus pads: (ref, X, Y, rot)
BY_PADS = [("J5", 112.0, 67.2, 90), ("J6", 57.0, 67.2, 90), ("J9", 93.5, 106.2, 0), ("J3", 46.5, 117.0, 0),
           ("SW1", 124.0, 131.5, 0)]


def pcb(x, y):
    return x + 50.0, 200.0 - y


def vec(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def place(board, ref, x, y, rot=0.0):
    fp = board.FindFootprintByReference(ref)
    if fp.IsFlipped():
        fp.Flip(fp.GetPosition(), False)
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(vec(x, y))
    return fp


def place_by_pads(board, ref, x, y, rot):
    fp = place(board, ref, 0, 0, rot)
    xs = [pcbnew.ToMM(p.GetPosition().x) for p in fp.Pads() if p.GetNumber()]
    ys = [pcbnew.ToMM(p.GetPosition().y) for p in fp.Pads() if p.GetNumber()]
    fp.SetPosition(vec(x - (min(xs) + max(xs)) / 2, y - (min(ys) + max(ys)) / 2))


def edge(board, shape, **kw):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(shape)
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(pcbnew.FromMM(0.1))
    if shape == pcbnew.SHAPE_T_SEGMENT:
        s.SetStart(vec(*kw["a"]))
        s.SetEnd(vec(*kw["b"]))
    else:
        s.SetCenter(vec(*kw["c"]))
        s.SetEnd(vec(kw["c"][0] + kw["d"] / 2, kw["c"][1]))
    board.Add(s)


def main():
    board = pcbnew.LoadBoard(BOARD)

    for d in list(board.GetDrawings()):
        if d.GetLayer() == pcbnew.Edge_Cuts:
            board.Delete(d)
    x0, y0, x1, y1 = OUTLINE
    corners = [pcb(x0, y0), pcb(x1, y0), pcb(x1, y1), pcb(x0, y1)]
    for a, b in zip(corners, corners[1:] + corners[:1]):
        edge(board, pcbnew.SHAPE_T_SEGMENT, a=a, b=b)
    for p in MOUNT_HOLES:
        edge(board, pcbnew.SHAPE_T_CIRCLE, c=pcb(*p), d=MOUNT_HOLE_D)

    wall = 200.0 - WALL_Y
    for ref, x in JACKS_635.items():
        # rot 90: la nariz (+x local) mira a la pared (−y en la placa); la cara del panel queda en la pared.
        place(board, ref, x + 50.0 - 7.23, wall + 16.95, rot=90)
    for ref, x in JACKS_35.items():
        place(board, ref, x + 50.0 - 0.9, wall + 1.2, rot=0)

    for ref, y in TEENSY_ROWS_Y.items():
        place(board, ref, *pcb(TEENSY_PIN1_X, y), rot=90)
    for ref, y in SHIELD_ROWS_Y.items():
        place(board, ref, *pcb(SHIELD_PIN1_X, y), rot=90)

    for ref, x, y, rot in PARTS:
        place(board, ref, *pcb(x, y), rot=rot)
    for ref, x, y, rot in BY_PADS:
        place_by_pads(board, ref, *pcb(x, y), rot)

    # Designadores de los SMD chicos ocultos (JLC ensambla desde el CPL); quedan en Fab.
    for fp in board.GetFootprints():
        name = fp.GetReference()
        if name[0] in "CRDF" or name.startswith(("JP", "TP")):
            fp.Reference().SetVisible(False)
        elif name == "SW1":            # al lado del header: arriba choca con la serigrafía de J7
            fp.Reference().SetTextPos(fp.GetPosition() + pcbnew.VECTOR2I(pcbnew.FromMM(-4.0), pcbnew.FromMM(3.8)))
            fp.Reference().SetTextAngle(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T))

    for net in board.GetNetInfo().NetsByName().values():
        name = net.GetNetname()
        if name.startswith("/unconnected-"):
            net.SetNetname(name[1:])
        elif name and not name.startswith(("/", "unconnected-")) and name != "PWR_FLAG":
            net.SetNetname("/" + name)

    pcbnew.SaveBoard(BOARD, board)

    print("Portadora: componentes ubicados y contorno dibujado")


main()

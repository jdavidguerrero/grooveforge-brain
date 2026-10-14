"""Ubica los componentes de PCB-K en las coordenadas del enclosure de Studio.

Correr con el Python de KiCad (trae pcbnew):
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
      apps/hardware/kicad/studio/tools/place_pcb_k.py

Coordenadas del panel (enclosure): x desde el borde izquierdo, y desde el borde frontal, en mm.
En la placa: x_pcb = x + 50, y_pcb = 200 - y (el frente queda abajo en pantalla, vista desde arriba).
Fuente: apps/docs/variants/studio/design/README.md y theory/02-studio-pcb.md.
"""
import math
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, "..", "pcb-k", "pcb-k.kicad_pcb")
LIB = os.path.join(HERE, "..", "lib", "studio.pretty")

LED_OFFSET = 4.93       # ventana de LED del Choc V2, hacia atrás del centro
RING_R = 13.0           # radio del anillo de NAV (difusor de Ø32)
RING_CAP_R = 13.5      # entre LEDs vecinos; afuera del anillo el C7 tocaba el borde frontal
BUSHING_HOLE = 8.0      # buje del EC11 (Ø6.8) + tolerancia de alineación
STANDOFF_HOLE = 6.5     # paso del separador M3 de PCB-A

KEYS = {"SW1": (60, 62), "SW2": (80, 62), "SW3": (100, 62), "SW4": (120, 62), "SW5": (60, 32), "SW6": (120, 32)}
KEY_LEDS = {"D13": "SW1", "D14": "SW2", "D15": "SW3", "D16": "SW4", "D17": "SW5", "D18": "SW6"}
ENCODERS = {"ENC L": (32, 62), "ENC R": (148, 62), "P1": (32, 32), "P2": (148, 32), "NAV": (90, 32)}
STANDOFFS = [(20, 47), (70, 47), (110, 47), (160, 47)]
OUTLINE = (12, 14, 168, 76)  # x0, y0, x1, y1 en coordenadas del panel


def pcb(x, y):
    return x + 50.0, 200.0 - y


def place(board, ref, x, y, rot=0.0, back=False):
    fp = board.FindFootprintByReference(ref)
    if fp is None:
        raise SystemExit(f"no encuentro {ref}")
    if back and not fp.IsFlipped():
        fp.Flip(fp.GetPosition(), False)
    fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
    fp.SetOrientationDegrees(rot)


def refresh_from_lib(board, ref):
    """Reemplaza el footprint de la placa por la versión actual de studio.pretty, conservando redes."""
    old = board.FindFootprintByReference(ref)
    new = pcbnew.FootprintLoad(LIB, old.GetFPID().GetLibItemName().wx_str())
    new.SetParent(board)
    new.SetFPID(old.GetFPID())
    new.SetReference(ref)
    new.SetValue(old.GetValue())
    new.SetPath(old.GetPath())
    new.SetSheetname(old.GetSheetname())
    new.SetSheetfile(old.GetSheetfile())
    nets = {p.GetNumber(): p.GetNet() for p in old.Pads()}
    board.Delete(old)
    board.Add(new)
    for p in new.Pads():
        if p.GetNumber() in nets:
            p.SetNet(nets[p.GetNumber()])


def radius(board, ref, pad_number, cx, cy):
    """Distancia (mm) del pad al centro del anillo."""
    pad = next(p for p in board.FindFootprintByReference(ref).Pads() if p.GetNumber() == pad_number)
    x, y = pcbnew.ToMM(pad.GetPosition())
    return math.hypot(x - cx, y - cy)


def edge_line(board, x0, y0, x1, y1):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(pcbnew.FromMM(0.1))
    s.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(x0), pcbnew.FromMM(y0)))
    s.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(x1), pcbnew.FromMM(y1)))
    board.Add(s)


def edge_circle(board, x, y, d):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_CIRCLE)
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(pcbnew.FromMM(0.1))
    s.SetCenter(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
    s.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(x + d / 2), pcbnew.FromMM(y)))
    board.Add(s)


def main():
    board = pcbnew.LoadBoard(BOARD)

    # Borra lo dibujado antes en Edge.Cuts (el script se puede volver a correr).
    for d in list(board.GetDrawings()):
        if d.GetLayer() == pcbnew.Edge_Cuts:
            board.Delete(d)

    x0, y0 = pcb(OUTLINE[0], OUTLINE[3])
    x1, y1 = pcb(OUTLINE[2], OUTLINE[1])
    for a, b in [((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))]:
        edge_line(board, *a, *b)
    for x, y in ENCODERS.values():
        edge_circle(board, *pcb(x, y), BUSHING_HOLE)
    for x, y in STANDOFFS:
        edge_circle(board, *pcb(x, y), STANDOFF_HOLE)

    # Choc y sus LEDs (LED en la cara inferior, bajo la ventana del switch).
    for ref, (x, y) in KEYS.items():
        refresh_from_lib(board, ref)
        place(board, ref, *pcb(x, y))
    for i, (led, sw) in enumerate(KEY_LEDS.items()):
        x, y = pcb(*KEYS[sw])
        place(board, led, x, y - LED_OFFSET, back=True)
        # Capacitor a la derecha, pasando la guía del Choc (x+5): a la izquierda tapaba el DIN del LED.
        place(board, f"C{13 + i}", x + 7.5, y - LED_OFFSET, rot=0, back=True)

    # Anillo de NAV: D1 arriba (atrás) y en sentido horario, = índices 0–11 del firmware.
    cx, cy = pcb(*ENCODERS["NAV"])
    for i in range(12):
        a = math.radians(i * 30)
        # Pads en dirección radial: así 12 LEDs caben en el anillo sin tocarse. Con 270° (y no 90°) el
        # DOUT de cada LED queda del lado del LED siguiente y su DIN del lado del anterior: cada salto de
        # datos cruza un solo hueco. VDD queda adentro (anillo de +5V) y GND afuera (vías al plano).
        place(board, f"D{i + 1}", cx + RING_R * math.sin(a), cy - RING_R * math.cos(a), rot=270 - i * 30, back=True)
        # Cap en el hueco hacia el LED siguiente (2.9 mm libres a r=13), orientado radial.
        ac = math.radians(i * 30 + 15)
        ref = f"C{i + 1}"
        x, y = cx + RING_CAP_R * math.sin(ac), cy - RING_CAP_R * math.cos(ac)
        place(board, ref, x, y, rot=90 - (i * 30 + 15), back=True)
        # Pad 1 (+5V) hacia adentro, junto al VDD del LED siguiente; pad 2 (GND) hacia afuera.
        if radius(board, ref, "1", cx, cy) > radius(board, ref, "2", cx, cy):
            place(board, ref, x, y, rot=270 - (i * 30 + 15), back=True)

    # Conector al borde trasero; 47 µF sobre el bus de +5V; puntos de prueba sobre sus pistas
    # (TP1 en LED_DIN entre SW2 y SW3, TP2 en LED_DOUT junto a D18). Ver route_pcb_k_keys.py.
    place(board, "J1", 140.0, y0 + 3.5, rot=180, back=True)
    place(board, "C19", 159.0, 136.8, back=True)
    place(board, "TP1", 139.7, 140.0, back=True)
    place(board, "TP2", 178.0, 160.9, back=True)

    # Designadores: en 0402/LED no caben sin pisar pads. JLC ensambla desde el CPL; quedan en Fab.
    for fp in board.GetFootprints():
        ref = fp.Reference()
        name = ref.GetText()
        if name[0] in "CD":
            ref.SetVisible(False)
        elif name.startswith("SW"):
            ref.SetTextPos(fp.GetPosition() + pcbnew.VECTOR2I(0, pcbnew.FromMM(-8.6)))
        elif name.startswith("TP"):
            ref.SetTextPos(fp.GetPosition() + pcbnew.VECTOR2I(0, pcbnew.FromMM(-1.5)))
        elif name == "J1":
            ref.SetTextPos(fp.GetPosition() + pcbnew.VECTOR2I(pcbnew.FromMM(0), pcbnew.FromMM(5.0)))

    # El esquemático usa etiquetas locales: KiCad nombra esas redes con el prefijo "/".
    for net in board.GetNetInfo().NetsByName().values():
        name = net.GetNetname()
        if name and not name.startswith("/") and name not in ("PWR_FLAG",):
            net.SetNetname("/" + name)

    pcbnew.SaveBoard(BOARD, board)
    print("PCB-K: componentes ubicados y contorno dibujado")


main()

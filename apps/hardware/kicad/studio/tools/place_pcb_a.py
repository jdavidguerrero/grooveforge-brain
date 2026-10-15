"""Ubica los componentes de PCB-A (encoders, pots, conectores) en las coordenadas del enclosure.

  KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3
  $KPY apps/hardware/kicad/studio/tools/place_pcb_a.py

Mismo sistema que place_pcb_k.py: panel (x desde la izquierda, y desde el frente) → placa
x_pcb = x + 50, y_pcb = 200 - y. Fuente: theory/02-studio-pcb.md §4.2 y §6.

PCB-A queda a −12.0 mm (cara superior). Por arriba tiene 7 mm de aire hasta PCB-K: J2 (IDC 2×15, caja
de ~9 mm) va en la cara inferior, apuntando a la portadora en el piso. J1 (header 1×10 acodado, rev.
2026-10-15: sin PCB-K) va arriba, con los pines hacia los Choc: 7 hilos Dupont a los switches (B1–B6 y GND)
y +5V/LED_DIN libres para LEDs futuros.

PROVISIONAL: los brazos de los pots (x 6–24 y 156–174 hasta y 106) asumen un módulo Freenove de
~121 mm de ancho centrado en x 90. Medir el módulo y el cable DSI antes del pedido (tarea 44.61).
"""
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, "..", "pcb-a", "pcb-a.kicad_pcb")

ENCODERS = {"ENC1": (32, 62), "ENC2": (148, 62), "ENC3": (90, 32), "ENC4": (32, 32), "ENC5": (148, 32)}
POTS = {"RV1": (15, 96), "RV2": (165, 96)}
# 4 separadores bajo los pasos de PCB-K + 1 por brazo (el par del pot no queda en voladizo).
STANDOFFS = [(20, 47), (70, 47), (110, 47), (160, 47), (15, 84), (165, 84)]
STANDOFF_HOLE = 3.2            # M3
# Contorno en coordenadas del panel: franja + brazos.
OUTLINE = [(6, 106), (24, 106), (24, 78), (156, 78), (156, 106), (174, 106), (174, 18), (6, 18)]
# Capacitores de 10 nF por encoder: (A, B, SW). El pin común (C) y S2 van a GND.
ENC_CAPS = {"ENC1": ("C1", "C2", "C3"), "ENC2": ("C4", "C5", "C6"), "ENC3": ("C7", "C8", "C9"),
            "ENC4": ("C10", "C11", "C12"), "ENC5": ("C13", "C14", "C15")}
POT_CAPS = {"RV1": "C16", "RV2": "C17"}


def pcb(x, y):
    return x + 50.0, 200.0 - y


def vec(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def shaft_offset(fp):
    """Centro del eje respecto del origen: punto medio de las dos patas de montaje (EC11 y RV09)."""
    tabs = [p for p in fp.Pads() if p.GetNumber() in ("MP", "") and p.GetDrillSize().x > 1.05 * 1e6]
    tabs.sort(key=lambda p: p.GetSize().x * p.GetSize().y, reverse=True)
    a, b = tabs[0].GetPosition(), tabs[1].GetPosition()
    o = fp.GetPosition()
    return (pcbnew.ToMM((a.x + b.x) // 2 - o.x), pcbnew.ToMM((a.y + b.y) // 2 - o.y))


def place_center(board, ref, cx, cy, rot=0.0, back=False, by="shaft"):
    """Ubica el footprint para que su eje (o el centro de sus pads) caiga en (cx, cy)."""
    fp = board.FindFootprintByReference(ref)
    if back and not fp.IsFlipped():
        fp.Flip(fp.GetPosition(), False)
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(vec(0, 0))
    if by == "shaft":
        ox, oy = shaft_offset(fp)
    else:
        xs = [pcbnew.ToMM(p.GetPosition().x) for p in fp.Pads() if p.GetNumber() not in ("", "MP")]
        ys = [pcbnew.ToMM(p.GetPosition().y) for p in fp.Pads() if p.GetNumber() not in ("", "MP")]
        ox, oy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    fp.SetPosition(vec(cx - ox, cy - oy))


def place(board, ref, x, y, rot=0.0, back=False):
    fp = board.FindFootprintByReference(ref)
    if back != fp.IsFlipped():
        fp.Flip(fp.GetPosition(), False)
    fp.SetPosition(vec(x, y))
    fp.SetOrientationDegrees(rot)


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
    pts = [pcb(*p) for p in OUTLINE]
    for a, b in zip(pts, pts[1:] + pts[:1]):
        edge(board, pcbnew.SHAPE_T_SEGMENT, a=a, b=b)
    for p in STANDOFFS:
        edge(board, pcbnew.SHAPE_T_CIRCLE, c=pcb(*p), d=STANDOFF_HOLE)

    # Encoders: A/C/B al oeste del eje, switch al este. Capacitores en la cara SUPERIOR (con J1): la placa
    # queda de una sola cara para el ensamble Economic de JLCPCB (rev. 2026-10-14).
    for ref, (x, y) in ENCODERS.items():
        cx, cy = pcb(x, y)
        place_center(board, ref, cx, cy)
        c_a, c_b, c_sw = ENC_CAPS[ref]
        place(board, c_a, cx - 11.0, cy - 2.5, rot=180)                # pad 1 (señal) hacia el pin
        place(board, c_b, cx - 11.0, cy + 2.5, rot=180)
        place(board, c_sw, cx + 11.0, cy + 2.5, rot=0)

    # Pots: pines hacia la franja (abajo en la placa) para bajar por el brazo.
    for ref, (x, y) in POTS.items():
        cx, cy = pcb(x, y)
        place_center(board, ref, cx, cy, rot=90)
    for ref, cap in POT_CAPS.items():
        fp = board.FindFootprintByReference(ref)
        wiper = next(p for p in fp.Pads() if p.GetNumber() == "2").GetPosition()
        wx, wy = pcbnew.ToMM(wiper.x), pcbnew.ToMM(wiper.y)
        # Arriba no cabe bajo el cuerpo del pot: va al costado de los pines, lejos del separador del brazo.
        place(board, cap, wx - 6.0, wy + 3.0, rot=0)

    # J1 (Choc + LEDs futuros): cara superior, header acodado con los pines hacia +x (los Choc).
    place_center(board, "J1", 100.0, 150.0, rot=0, by="pads")
    # J2 (a la portadora): cara inferior, eje largo horizontal, junto al borde trasero.
    place_center(board, "J2", 163.0, 128.0, rot=90, back=True, by="pads")
    j2 = board.FindFootprintByReference("J2")
    p30 = next(p for p in j2.Pads() if p.GetNumber() == "30").GetPosition()
    # Fuera de la caja de la IDC (la caja ocupa ±5 mm alrededor de y 128 en esta cara).
    place(board, "C18", pcbnew.ToMM(p30.x), 135.5, rot=0)

    # El JST-SH de 1 mm necesitaba vías de escape propias para GND; el header de 2.54 no.
    for g in list(board.Groups()):
        if g.GetName() == "escapes_gnd_j1":
            for item in list(g.GetItems()):
                board.Delete(item)
            board.Delete(g)

    # Designadores de los 0603 ocultos (JLC ensambla desde el CPL); quedan en Fab.
    for fp in board.GetFootprints():
        if fp.GetReference().startswith("C"):
            fp.Reference().SetVisible(False)

    for net in board.GetNetInfo().NetsByName().values():
        name = net.GetNetname()
        if name and not name.startswith("/") and name != "PWR_FLAG":
            net.SetNetname("/" + name)

    pcbnew.SaveBoard(BOARD, board)
    print("PCB-A: componentes ubicados y contorno dibujado")


main()

"""Espina de GND por el borde frontal de la portadora, ruteada ANTES que Freerouting y bloqueada.

Une los GND de J5 (IDC desde PCB-A), J6 (Pi), J10 (entrada de 5 V) y el del TVS D1. Sin ella, el abanico
de pistas de J5 encierra sus pines de GND en una isla y el TVS queda lejos del plano. Va por F.Cu en
y 137.35 (0.5 mm al borde, 0.3 mm a los pads del fusible), franja que casi ninguna señal necesita.
"""
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, "..", "portadora", "portadora.kicad_pcb")
SPINE_Y, W = 137.35, 0.4
PATHS = [
    [(146.76, 134.07), (146.76, SPINE_Y), (112.08, SPINE_Y), (112.08, 134.07)],   # J5.3 → J6.9
    [(146.76, 131.53), (146.76, 134.07)],                                         # J5.4
    [(149.3, 131.53), (147.9, 132.8), (147.9, SPINE_Y)],                          # J5.6
    [(124.5, 133.5), (124.5, SPINE_Y)],                                           # J10.2
    [(134.95, 130.4), (136.6, 130.4), (136.6, SPINE_Y)],                          # D1.2 (TVS)
]


def main():
    board = pcbnew.LoadBoard(BOARD)
    gnd = board.FindNet("/GND")
    for g in list(board.Groups()):
        if g.GetName() == "espina_gnd":
            for item in list(g.GetItems()):
                board.Delete(item)
            board.Delete(g)
    group = pcbnew.PCB_GROUP(board)
    group.SetName("espina_gnd")
    board.Add(group)
    for path in PATHS:
        for a, b in zip(path, path[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(a[0]), pcbnew.FromMM(a[1])))
            t.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(b[0]), pcbnew.FromMM(b[1])))
            t.SetWidth(pcbnew.FromMM(W))
            t.SetLayer(pcbnew.F_Cu)
            t.SetNet(gnd)
            t.SetLocked(True)
            board.Add(t)
            group.AddItem(t)
    pcbnew.SaveBoard(BOARD, board)
    print("portadora: espina de GND por el borde frontal")


main()

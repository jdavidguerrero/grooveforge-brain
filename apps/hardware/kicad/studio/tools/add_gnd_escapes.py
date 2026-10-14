"""Da a cada pad SMD de GND su propia vía al plano, antes de rutear.

  $KPY apps/hardware/kicad/studio/tools/add_gnd_escapes.py portadora

Sin esto, las pistas que pasan alrededor de una pieza chica (sobre todo entre las filas de un zócalo)
encierran su pad de GND en una isla del plano que no se une a nada. La vía va pegada al pad, del lado
contrario al centro de la pieza, en la primera de 4 direcciones donde no toca otro cobre ni agujero.
Quedan bloqueadas y en el grupo "escapes_gnd" (se borra y rehace en cada corrida); route_board.py las
conserva porque GND no pasa por Freerouting.
"""
import math
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
GROUP = "escapes_gnd"
VIA_D, VIA_DRILL, W = 0.6, 0.3, 0.3
CLEAR = 0.3                     # a cobre de otra red (mm)
HOLE_GAP = 0.5                  # agujero a agujero (JLCPCB)


def main():
    name = sys.argv[1]
    path = os.path.join(HERE, "..", name, f"{name}.kicad_pcb")
    board = pcbnew.LoadBoard(path)
    gnd = board.FindNet("/GND")

    for g in list(board.Groups()):
        if g.GetName() == GROUP:
            for item in list(g.GetItems()):
                board.Delete(item)
            board.Delete(g)
    group = pcbnew.PCB_GROUP(board)
    group.SetName(GROUP)
    board.Add(group)

    pads = [p for fp in board.GetFootprints() for p in fp.Pads()]
    holes = [(pcbnew.ToMM(p.GetPosition()), pcbnew.ToMM(max(p.GetDrillSize().x, p.GetDrillSize().y)) / 2)
             for p in pads if p.GetDrillSize().x > 0]
    edge = board.GetBoardEdgesBoundingBox()
    ex0, ey0 = pcbnew.ToMM(edge.GetLeft()) + 1.0, pcbnew.ToMM(edge.GetTop()) + 1.0
    ex1, ey1 = pcbnew.ToMM(edge.GetRight()) - 1.0, pcbnew.ToMM(edge.GetBottom()) - 1.0
    placed = []

    def free(x, y, own_pad):
        r = VIA_D / 2
        for p in pads:
            if p is own_pad:
                continue
            bb = p.GetBoundingBox()
            px0, py0 = pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop())
            px1, py1 = pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom())
            gap = 0 if p.GetNetname() == "/GND" else CLEAR
            dx = max(px0 - x, 0, x - px1)
            dy = max(py0 - y, 0, y - py1)
            if math.hypot(dx, dy) < r + gap:
                return False
        for (hx, hy), hr in holes:
            if math.hypot(x - hx, y - hy) < hr + VIA_DRILL / 2 + HOLE_GAP:
                return False
        for (vx, vy) in placed:
            if math.hypot(x - vx, y - vy) < VIA_DRILL + HOLE_GAP:
                return False
        return ex0 <= x <= ex1 and ey0 <= y <= ey1

    count, failed = 0, []
    for fp in board.GetFootprints():
        cx, cy = pcbnew.ToMM(fp.GetPosition())
        for pad in fp.Pads():
            if pad.GetNetname() != "/GND" or pad.GetDrillSize().x > 0:
                continue
            x, y = pcbnew.ToMM(pad.GetPosition())
            sx, sy = pcbnew.ToMM(pad.GetBoundingBox().GetWidth()), pcbnew.ToMM(pad.GetBoundingBox().GetHeight())
            ax, ay = x - cx, y - cy
            # Primero hacia afuera de la pieza; después las otras tres direcciones.
            dirs = [(1 if ax >= 0 else -1, 0), (0, 1 if ay >= 0 else -1)] if abs(ax) >= abs(ay) else \
                   [(0, 1 if ay >= 0 else -1), (1 if ax >= 0 else -1, 0)]
            dirs += [(-d[0], -d[1]) for d in dirs]
            for dx, dy in dirs:
                ok = False
                for extra in (0.25, 0.6, 1.0):
                    vx = x + dx * (sx / 2 + VIA_D / 2 + extra)
                    vy = y + dy * (sy / 2 + VIA_D / 2 + extra)
                    if free(vx, vy, pad):
                        ok = True
                        break
                if ok:
                    break
            if not ok:
                failed.append(f"{fp.GetReference()}.{pad.GetNumber()}")
                continue
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(pad.GetPosition())
            t.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(vx), pcbnew.FromMM(vy)))
            t.SetWidth(pcbnew.FromMM(W))
            t.SetLayer(pcbnew.F_Cu if not fp.IsFlipped() else pcbnew.B_Cu)
            t.SetNet(gnd)
            t.SetLocked(True)
            board.Add(t)
            group.AddItem(t)
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(vx), pcbnew.FromMM(vy)))
            v.SetWidth(pcbnew.FromMM(VIA_D))
            v.SetDrill(pcbnew.FromMM(VIA_DRILL))
            v.SetNet(gnd)
            v.SetLocked(True)
            board.Add(v)
            group.AddItem(v)
            placed.append((vx, vy))
            count += 1

    pcbnew.SaveBoard(path, board)
    print(f"{name}: {count} vías de escape de GND" + (f"; sin lugar: {', '.join(failed)}" if failed else ""))


main()

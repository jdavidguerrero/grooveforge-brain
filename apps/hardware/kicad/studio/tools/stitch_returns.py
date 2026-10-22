"""Cose el retorno de GND de un bus rápido: vía de GND junto a cada cambio de capa y vías en los huecos del plano.

  $KPY apps/hardware/kicad/studio/tools/stitch_returns.py portadora /I2S_MCLK,/I2S_BCLK,/I2S_LRCLK,/I2S_RX,/I2S_TX

Theory/02 §10K (auditoría delta D04). En dos capas, la señal que viaja por B.Cu usa como retorno el plano de
F.Cu que tiene encima (y viceversa). Donde ese plano se corta, el retorno da la vuelta y el lazo crece.

1. **Transiciones:** por cada vía de señal del bus, una vía de GND lo más cerca posible (objetivo ≤ 1 mm entre
   centros + diámetros), para que el retorno también cambie de capa ahí.
2. **Huecos:** en una grilla sobre el corredor del bus (bus inflado `MARGIN` mm), se agrega una vía de GND donde
   una de las dos caras no tiene plano relleno y la vía cabe con holgura. KiCad borra las islas de plano sin
   conexión; con la vía, la isla queda unida al otro plano y se conserva. Se repite tras cada relleno.

No mueve pistas ni piezas. Las vías quedan bloqueadas en el grupo "costura_retorno" (se rehace en cada corrida).
Correr después del ruteo y antes del DRC; vuelve a rellenar las zonas.
"""
import math
import os
import sys

import pcbnew as p

HERE = os.path.dirname(os.path.abspath(__file__))
GROUP = "costura_retorno"
VIA_D, VIA_DRILL = 0.6, 0.3
CLEAR = 0.3            # a cobre de otra red (mm)
HOLE_GAP = 0.5         # agujero a agujero
EDGE = 0.6             # al borde de la placa
MARGIN = 2.5           # ancho del corredor a cada lado del bus (mm)
STEP = 1.0             # paso de la grilla de huecos (mm)
NEAR_TARGET = 1.6      # distancia máxima centro a centro buscada para la vía junto a la transición (mm)


def mm(v):
    return p.ToMM(v.x), p.ToMM(v.y)


def main():
    name, nets = sys.argv[1], sys.argv[2].split(",")
    path = os.path.join(HERE, "..", name, f"{name}.kicad_pcb")
    b = p.LoadBoard(path)
    gnd = b.FindNet("/GND")

    for g in list(b.Groups()):
        if g.GetName() == GROUP:
            for it in list(g.GetItems()):
                b.Delete(it)
            b.Delete(g)
    group = p.PCB_GROUP(b)
    group.SetName(GROUP)
    b.Add(group)

    edge = b.GetBoardEdgesBoundingBox()
    ex0, ey0 = mm(edge.GetOrigin())
    ex1, ey1 = mm(edge.GetEnd())
    holes = [(mm(pd.GetPosition()), p.ToMM(pd.GetDrillSize().x) / 2) for f in b.GetFootprints() for pd in f.Pads()
             if pd.GetDrillSize().x > 0]
    holes += [(mm(d.GetCenter()), p.ToMM(d.GetRadius())) for d in b.GetDrawings()
              if d.GetLayer() == p.Edge_Cuts and d.GetShape() == p.SHAPE_T_CIRCLE]
    courtyards = [f.GetCourtyard(p.F_CrtYd) for f in b.GetFootprints()]

    def fits(x, y):
        if not (ex0 + EDGE + VIA_D / 2 <= x <= ex1 - EDGE - VIA_D / 2 and ey0 + EDGE + VIA_D / 2 <= y <= ey1 - EDGE - VIA_D / 2):
            return False
        pt = p.VECTOR2I(p.FromMM(x), p.FromMM(y))
        acc = p.FromMM(VIA_D / 2 + CLEAR)
        for t in b.GetTracks():
            if t.GetNetCode() != gnd.GetNetCode() and t.HitTest(pt, acc):
                return False
            if t.GetClass() == "PCB_VIA" and math.dist(mm(t.GetPosition()), (x, y)) < VIA_DRILL + HOLE_GAP:
                return False
        for f in b.GetFootprints():
            for pd in f.Pads():
                if pd.GetNetCode() != gnd.GetNetCode() and pd.HitTest(pt, acc):
                    return False
        for (hx, hy), r in holes:
            if math.dist((hx, hy), (x, y)) < r + VIA_DRILL / 2 + HOLE_GAP:
                return False
        return not any(c.Contains(pt) for c in courtyards if c.OutlineCount())

    def add_via(x, y):
        v = p.PCB_VIA(b)
        v.SetPosition(p.VECTOR2I(p.FromMM(x), p.FromMM(y)))
        v.SetDrill(p.FromMM(VIA_DRILL))
        v.SetWidth(p.FromMM(VIA_D))
        v.SetNet(gnd)
        v.SetLocked(True)
        b.Add(v)
        group.AddItem(v)

    sig = [t for t in b.GetTracks() if t.GetNetname() in nets]

    # 1. Junto a cada transición de capa
    near_report = []
    for v in [t for t in sig if t.GetClass() == "PCB_VIA"]:
        vx, vy = mm(v.GetPosition())
        best = None
        for r10 in range(8, int(NEAR_TARGET * 10) + 1):
            r = r10 / 10
            for k in range(24):
                a = 2 * math.pi * k / 24
                x, y = vx + r * math.cos(a), vy + r * math.sin(a)
                if fits(x, y):
                    best = (x, y, r)
                    break
            if best:
                break
        if best:
            add_via(best[0], best[1])
            near_report.append(f"{v.GetNetname()} ({vx:.2f}, {vy:.2f}): GND a {best[2]:.2f} mm")
        else:
            near_report.append(f"{v.GetNetname()} ({vx:.2f}, {vy:.2f}): SIN LUGAR a ≤ {NEAR_TARGET} mm")

    # 2. Huecos del plano a lo largo del corredor
    pts = []
    for t in sig:
        if t.GetClass() == "PCB_VIA":
            continue
        (x1, y1), (x2, y2) = mm(t.GetStart()), mm(t.GetEnd())
        n = max(1, math.ceil(math.dist((x1, y1), (x2, y2)) / STEP))
        pts += [(x1 + (x2 - x1) * i / n, y1 + (y2 - y1) * i / n) for i in range(n + 1)]
    xs, ys = [q[0] for q in pts], [q[1] for q in pts]
    added = 0
    for _ in range(3):
        p.ZONE_FILLER(b).Fill(b.Zones())
        filled = {lay: [z.GetFilledPolysList(lay) for z in b.Zones() if z.GetNetCode() == gnd.GetNetCode()
                        and z.IsOnLayer(lay)] for lay in (p.F_Cu, p.B_Cu)}
        new = 0
        y = min(ys) - MARGIN
        while y <= max(ys) + MARGIN:
            x = min(xs) - MARGIN
            while x <= max(xs) + MARGIN:
                if any(math.dist((x, y), q) <= MARGIN for q in pts):
                    pt = p.VECTOR2I(p.FromMM(x), p.FromMM(y))
                    void = any(not any(poly.Contains(pt) for poly in filled[lay]) for lay in (p.F_Cu, p.B_Cu))
                    if void and fits(x, y):
                        add_via(x, y)
                        new += 1
                x += STEP
            y += STEP
        added += new
        if not new:
            break
    p.ZONE_FILLER(b).Fill(b.Zones())
    p.SaveBoard(path, b)
    print("\n".join(near_report))
    print(f"vías de costura en huecos: {added}")


if __name__ == "__main__":
    main()

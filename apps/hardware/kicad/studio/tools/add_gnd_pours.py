"""Agrega un plano de GND en F.Cu y B.Cu sobre todo el contorno de la placa y lo rellena.

  KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3
  $KPY apps/hardware/kicad/studio/tools/add_gnd_pours.py pcb-k [F|B|FB]

Por defecto ambas caras, más vías de costura (cada STITCH_PITCH mm donde los dos planos tienen cobre)
para que ninguna isla de GND quede suelta. Con "F" antes de Freerouting: el DSN exporta el plano de GND y Freerouting une
GND con vías al plano en vez de tender pistas. Re-ejecutable: borra los planos de GND anteriores antes de crear los nuevos.
"""
import math
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
STITCH_PITCH = 6.0
STITCH_GROUP = "costura_gnd"
VIA_D, VIA_DRILL = 0.6, 0.3


def stitch(board, gnd, bbox):
    """Vías de GND en una grilla, solo donde la vía entera cae dentro del relleno de las dos caras."""
    for g in list(board.Groups()):
        if g.GetName() == STITCH_GROUP:
            for item in list(g.GetItems()):
                board.Delete(item)
            board.Delete(g)
    group = pcbnew.PCB_GROUP(board)
    group.SetName(STITCH_GROUP)
    board.Add(group)

    fills = [z.GetFilledPolysList(z.GetLayer()) for z in board.Zones() if z.GetNetname() == "/GND"]
    # Agujeros existentes (pads con taladro y vías): una vía nueva respeta 0.5 mm agujero-agujero (JLCPCB).
    holes = [(p.GetPosition(), max(p.GetDrillSize().x, p.GetDrillSize().y) // 2, p.GetNetname())
             for fp in board.GetFootprints() for p in fp.Pads() if p.GetDrillSize().x > 0]
    holes += [(t.GetPosition(), t.GetDrill() // 2, t.GetNetname())
              for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    gap = pcbnew.FromMM(0.5 + VIA_DRILL / 2)

    def free(x, y):
        return all(math.hypot(x - c.x, y - c.y) >= r + gap for c, r, _ in holes)

    def fits(x, y):
        return free(x, y) and all(f.Contains(pcbnew.VECTOR2I(int(x + dx), int(y + dy)))
                                  for f in fills for dx, dy in ring)

    def add_via(x, y):
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I(int(x), int(y)))
        v.SetWidth(pcbnew.FromMM(VIA_D))
        v.SetDrill(pcbnew.FromMM(VIA_DRILL))
        v.SetNet(gnd)
        board.Add(v)
        group.AddItem(v)
        holes.append((v.GetPosition(), pcbnew.FromMM(VIA_DRILL) // 2, "/GND"))
    # El relleno ya guarda su holgura con el resto del cobre: si el círculo de la vía (más 0.1 mm) cae
    # adentro, la vía respeta la holgura.
    r = pcbnew.FromMM(VIA_D / 2 + 0.1)
    ring = [(r * math.cos(a * math.pi / 4), r * math.sin(a * math.pi / 4)) for a in range(8)] + [(0, 0)]
    step = pcbnew.FromMM(STITCH_PITCH)
    count = 0
    y = bbox.GetTop() + step // 2
    while y < bbox.GetBottom():
        x = bbox.GetLeft() + step // 2
        while x < bbox.GetRight():
            if fits(x, y):
                add_via(x, y)
                count += 1
            x += step
        y += step

    # Islas sin ninguna vía adentro (quedan sueltas aunque toquen pads): buscar un lugar en grilla fina.
    fine = pcbnew.FromMM(0.5)
    for f in fills:
        for i in range(f.OutlineCount()):
            island = pcbnew.SHAPE_POLY_SET()
            island.AddOutline(f.Outline(i))
            # Ya unida a la otra cara si tiene adentro una vía o un pad pasante de GND.
            if any(n == "/GND" and island.Contains(c) for c, _, n in holes):
                continue
            bb = f.Outline(i).BBox()
            done = False
            yy = bb.GetTop()
            while yy < bb.GetBottom() and not done:
                xx = bb.GetLeft()
                while xx < bb.GetRight() and not done:
                    if island.Contains(pcbnew.VECTOR2I(int(xx), int(yy))) and fits(xx, yy):
                        add_via(xx, yy)
                        count += 1
                        done = True
                    xx += fine
                yy += fine
    return count


def main():
    name = sys.argv[1]
    sides = sys.argv[2] if len(sys.argv) > 2 else "FB"
    layers = [l for s_, l in (("F", pcbnew.F_Cu), ("B", pcbnew.B_Cu)) if s_ in sides]
    path = os.path.join(HERE, "..", name, f"{name}.kicad_pcb")
    board = pcbnew.LoadBoard(path)
    gnd = board.FindNet("/GND")

    for z in list(board.Zones()):
        if z.GetNetname() == "/GND":
            board.Delete(z)

    # Pads de GND de los conectores: unión sólida al plano. Entre pines de 1–2.54 mm las pistas vecinas
    # dejan los alivios térmicos con un solo radio (DRC "starved thermal"); el retorno de GND del cable
    # pasa por acá, así que conviene la unión llena.
    for fp in board.GetFootprints():
        if fp.GetReference().startswith("J"):
            for p in fp.Pads():
                if p.GetNetname() == "/GND":
                    p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

    bbox = board.GetBoardEdgesBoundingBox()
    for layer in layers:
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(gnd)
        z.SetIsRuleArea(False)
        outline = z.Outline()
        outline.NewOutline()
        # Rectángulo un poco mayor que la placa: KiCad recorta el relleno al contorno y a los agujeros.
        m = pcbnew.FromMM(1)
        for x, y in [(bbox.GetLeft() - m, bbox.GetTop() - m), (bbox.GetRight() + m, bbox.GetTop() - m),
                     (bbox.GetRight() + m, bbox.GetBottom() + m), (bbox.GetLeft() - m, bbox.GetBottom() + m)]:
            outline.Append(x, y)
        z.SetLocalClearance(pcbnew.FromMM(0.3))
        z.SetMinThickness(pcbnew.FromMM(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(pcbnew.FromMM(0.3))
        z.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.4))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        board.Add(z)

    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    if len(layers) == 2:
        n = stitch(board, gnd, bbox)
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        print(f"{name}: {n} vías de costura")
    pcbnew.SaveBoard(path, board)
    print(f"{name}: plano de GND rellenado en {sides}")


main()

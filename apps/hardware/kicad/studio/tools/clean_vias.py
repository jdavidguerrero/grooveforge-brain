"""Borra vías de señal que sobran: sueltas, o que solo tocan pistas de una misma cara (el ruteador a veces
deja una vía donde dos tramos de la misma capa se juntan). GND no se toca (costura y escapes).

  $KPY apps/hardware/kicad/studio/tools/clean_vias.py portadora
"""
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    name = sys.argv[1]
    path = os.path.join(HERE, "..", name, f"{name}.kicad_pcb")
    board = pcbnew.LoadBoard(path)
    # Pistas y vías duplicadas (mismo net, capa y extremos): se deja una sola.
    seen, dups = set(), 0
    for t in list(board.GetTracks()):
        if t.GetClass() == "PCB_VIA":
            k = ("v", t.GetNetname(), t.GetPosition().x, t.GetPosition().y)
        else:
            a_, b_ = sorted([(t.GetStart().x, t.GetStart().y), (t.GetEnd().x, t.GetEnd().y)])
            k = ("t", t.GetNetname(), t.GetLayer(), a_, b_, t.GetWidth())
        if k in seen:
            board.Delete(t)
            dups += 1
        else:
            seen.add(k)
    print(f"{name}: {dups} duplicados borrados")
    def touching(pos, radius, net):
        """Capas de las pistas de la red cuyo extremo cae dentro de la vía, y si hay un pad debajo."""
        layers, pad = set(), False
        for t in board.GetTracks():
            if t.GetClass() != "PCB_TRACK" or t.GetNetname() != net:
                continue
            for e in (t.GetStart(), t.GetEnd()):
                if (e.x - pos.x) ** 2 + (e.y - pos.y) ** 2 <= radius ** 2:
                    layers.add(t.GetLayer())
        for fp in board.GetFootprints():
            for p in fp.Pads():
                if p.GetNetname() == net and p.HitTest(pos):
                    pad = True
        return layers, pad

    gone = 0
    for v in list(board.GetTracks()):
        if v.GetClass() != "PCB_VIA" or v.GetNetname() == "/GND":
            continue
        layers, pad = touching(v.GetPosition(), v.GetWidth() // 2, v.GetNetname())
        if len(layers) < 2 and not pad:
            board.Delete(v)
            gone += 1

    # Tramos colgantes: un extremo que no toca pad, vía ni otra pista de su red. Hasta que no quede ninguno.
    stubs = 0
    while True:
        loose = []
        tracks = [t for t in board.GetTracks() if t.GetNetname() != "/GND"]
        for t in tracks:
            if t.GetClass() != "PCB_TRACK":
                continue
            for e in (t.GetStart(), t.GetEnd()):
                hit = False
                for o in tracks:
                    if o is t or o.GetNetname() != t.GetNetname():
                        continue
                    if o.GetClass() == "PCB_VIA":
                        r = o.GetWidth() // 2
                        hit = (e.x - o.GetPosition().x) ** 2 + (e.y - o.GetPosition().y) ** 2 <= r * r
                    elif o.GetLayer() == t.GetLayer():
                        # Unión real: el extremo cae sobre la línea central de la otra pista (como el DRC).
                        hit = o.HitTest(e, -o.GetWidth() // 2 + pcbnew.FromMM(0.01))
                    if hit:
                        break
                if not hit:
                    hit = any(p.GetNetname() == t.GetNetname() and p.IsOnLayer(t.GetLayer()) and p.HitTest(e)
                              for fp in board.GetFootprints() for p in fp.Pads())
                if not hit:
                    loose.append(t)
                    break
        if not loose:
            break
        for t in loose:
            board.Delete(t)
            stubs += 1
    pcbnew.SaveBoard(path, board)
    print(f"{name}: {gone} vías sobrantes y {stubs} tramos/vías colgantes borrados")


main()

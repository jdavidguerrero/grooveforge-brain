"""Tie GND copper that the pours left stranded back to the plane.

  $KPY tie_loose_gnd.py <board>    (after add_gnd_pours.py; build.sh re-pours afterwards)

A GND pin fenced in by signal tracks gets a fill island (sometimes one per layer, joined by its escape
via) that touches nothing else, and the stitcher finds no room for a via in it: DRC reports
"Zone [/GND] ↔ Zone [/GND]" unconnected. This builds the GND connectivity graph (fill islands, tracks,
vias, pads, thermal spokes), and for every group not joined to the main plane grid-routes a 0.3 mm
track from one of its pads to the nearest GND pad that is.
"""
import math
import os
import subprocess
import sys

import pcbnew

import finish_routing as fr

HERE = os.path.dirname(os.path.abspath(__file__))


def loose_groups(b):

    mm = pcbnew.ToMM
    nodes, parent = [], {}


    def add(n):
        parent[n] = n
        nodes.append(n)


    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a


    def union(a, c):
        parent[find(a)] = find(c)


    isl = []
    for z in b.Zones():
        if z.GetNetname() != "/GND":
            continue
        L = z.GetLayer()
        f = z.GetFilledPolysList(L)
        for i in range(f.OutlineCount()):
            n = ("isl", L, i)
            add(n)
            isl.append((n, L, f.Outline(i), abs(f.Outline(i).Area()) / 1e12))
    tr = [t for t in b.GetTracks() if t.GetNetname() == "/GND"]
    pads = [(fp.GetReference(), p) for fp in b.GetFootprints() for p in fp.Pads() if p.GetNetname() == "/GND"]
    for t in tr:
        add(("t", t.m_Uuid.AsString()))
    for r, p in pads:
        add(("p", p.m_Uuid.AsString()))


    def inside(o, pt):
        return o.PointInside(pt)


    for t in tr:
        tn = ("t", t.m_Uuid.AsString())
        if t.GetClass() == "PCB_VIA":
            for n, L, o, _ in isl:
                if inside(o, t.GetPosition()):
                    union(tn, n)
            ends = [t.GetPosition()]
        else:
            ends = [t.GetStart(), t.GetEnd()]
            for n, L, o, _ in isl:
                if L == t.GetLayer() and any(inside(o, e) for e in ends):
                    union(tn, n)
        for t2 in tr:
            if t2 is t:
                continue
            e2 = [t2.GetPosition()] if t2.GetClass() == "PCB_VIA" else [t2.GetStart(), t2.GetEnd()]
            if any((a - c).EuclideanNorm() < 1000 for a in ends for c in e2):
                union(tn, ("t", t2.m_Uuid.AsString()))
        for r, p in pads:
            if any(p.HitTest(e) for e in ends):
                union(tn, ("p", p.m_Uuid.AsString()))
    for r, p in pads:
        pn = ("p", p.m_Uuid.AsString())
        bb = p.GetBoundingBox()
        cx, cy = p.GetPosition().x, p.GetPosition().y
        hw, hh = bb.GetWidth() / 2, bb.GetHeight() / 2
        pts = []
        for k in range(16):
            a = k * math.pi / 8
            pts.append(pcbnew.VECTOR2I(int(cx + math.cos(a) * (hw + 60000)), int(cy + math.sin(a) * (hh + 60000))))
        for n, L, o, _ in isl:
            if p.IsOnLayer(L) and any(inside(o, q) for q in pts):
                union(pn, n)
    main = find(max(isl, key=lambda x: x[3])[0])
    loose = {}
    for n, L, o, a in isl:
        if find(n) != main:
            bb = o.BBox()
            loose.setdefault(find(n), []).append((b.GetLayerName(L), round(a, 2),
                                                  [round(mm(v), 2) for v in (bb.GetX(), bb.GetY(), bb.GetRight(), bb.GetBottom())]))
    groups = {g: [(r + "." + p.GetNumber(), p.GetPosition()) for r, p in pads if find(("p", p.m_Uuid.AsString())) == g]
              for g in loose}
    mains = [(r + "." + p.GetNumber(), p.GetPosition()) for r, p in pads if find(("p", p.m_Uuid.AsString())) == main]
    return groups, mains, loose


def force_route(name, a, b):
    os.environ["GRID_FORCE"] = "1"
    try:
        return fr.route(name, a, b, "0.3")
    finally:
        del os.environ["GRID_FORCE"]


def main():
    name = sys.argv[1]
    path = os.path.join(HERE, "..", name, f"{name}.kicad_pcb")
    groups, mains, info = loose_groups(pcbnew.LoadBoard(path))
    tied = 0
    for g, members in groups.items():
        print(f"{name}: grupo de GND suelto {info[g]} con pads {[s for s, _ in members]}")
        cands = sorted(((m, t, (mp - tp).EuclideanNorm()) for m, mp in members for t, tp in mains),
                       key=lambda c: c[2])[:6]
        for s, t, _ in cands:
            out = subprocess.run([sys.executable, os.path.join(HERE, "grid_route.py"), name, s, t, "0.3"],
                                 capture_output=True, text=True, env={**os.environ, "GRID_FORCE": "1"})
            msg = [l for l in (out.stdout + out.stderr).splitlines() if "ruteado" in l or "camino" in l]
            print(f"  GND suelto {s} → {t}: {msg[-1] if msg else 'error'}")
            if msg and "ruteado" in msg[-1]:
                tied += 1
                break
        else:
            # Encerrado: levantar de a una las redes que pasan cerca, unir GND y volver a rutear la otra.
            s, t, _ = cands[0]
            backup = open(path).read()
            for other in fr.blockers(path, s, t, "/GND", margin=1.0)[:8]:
                fr.rip_up(path, other)
                msg, ok = force_route(name, s, t)
                print(f"  levantando {other}: {msg}")
                if ok and fr.reroute_net(path, name, other, "0.2"):
                    print(f"  GND suelto {s} → {t}: ruteado levantando y re-ruteando {other}")
                    tied += 1
                    break
                open(path, "w").write(backup)
    print(f"{name}: {tied}/{len(groups)} grupos de GND sueltos unidos")


main()

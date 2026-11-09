"""Close the connections finish_routing.py left open by clearing their ideal path.

  $KPY repair_routes.py <board>      (after finish_routing.py, before clean_vias / pours)

finish_routing rips up one neighbouring net at a time; a long line squeezed out by several
parallel ones (PCB-A: J1 on the tab → the leftmost encoder) needs more than one out of the way.
For each open pad pair: route it on a copy of the board without any other signal copper (its
ideal path), rip up every net whose tracks come within 0.6 mm of that path, route the pair on the
real board, then route the ripped nets again, shortest first. If any of them doesn't fit, the
board goes back to how it was and the next pair is tried.
"""
import itertools
import os
import shutil
import subprocess
import sys

import pcbnew

import finish_routing as fr

HERE = os.path.dirname(os.path.abspath(__file__))
KEEP = ("/GND",)


def ideal_path(name, path, net, a, b, width):
    """Segments of the route a → b on a copy with only pads, outline and GND escapes."""
    tmp_name = f"{name}-ideal"
    tmp_dir = os.path.join(HERE, "..", tmp_name)
    os.makedirs(tmp_dir, exist_ok=True)
    tmp = os.path.join(tmp_dir, f"{tmp_name}.kicad_pcb")
    board = pcbnew.LoadBoard(path)
    for t in list(board.GetTracks()):
        if t.GetNetname() not in KEEP:
            board.Delete(t)
    pcbnew.SaveBoard(tmp, board)
    try:
        msg, ok = fr.route(tmp_name, a, b, width)
        if not ok:
            return []
        board = pcbnew.LoadBoard(tmp)
        return [(pcbnew.ToMM(t.GetStart()), pcbnew.ToMM(t.GetEnd())) for t in board.GetTracks()
                if t.GetNetname() == net]
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


def crossing_nets(path, segs, net, margin=0.6):
    board = pcbnew.LoadBoard(path)
    hit = set()
    for t in board.GetTracks():
        n = t.GetNetname()
        if n in KEEP or n == net or t.IsLocked():
            continue
        pts = [pcbnew.ToMM(t.GetPosition())] if t.GetClass() == "PCB_VIA" else \
              [pcbnew.ToMM(t.GetStart()), pcbnew.ToMM(t.GetEnd())]
        if t.GetClass() != "PCB_VIA":    # sample the track
            (sx, sy), (ex, ey) = pts
            pts = [(sx + (ex - sx) * k / 8, sy + (ey - sy) * k / 8) for k in range(9)]
        for (px, py) in pts:
            if any(fr._seg_dist(px, py, ax, ay, bx, by) < margin for (ax, ay), (bx, by) in segs):
                hit.add(n)
                break
    return sorted(hit)


def pocket_nets(path, spec, net, radius=2.6):
    """Nets with copper within `radius` mm of a pad: what walls it in when it can't even escape."""
    board = pcbnew.LoadBoard(path)
    ref, num = spec.split(".")
    pad = next(p for p in board.FindFootprintByReference(ref).Pads() if p.GetNumber() == num)
    cx, cy = pcbnew.ToMM(pad.GetPosition())
    near = {}
    for t in board.GetTracks():
        n = t.GetNetname()
        if n in KEEP or n == net or t.IsLocked():
            continue
        if t.GetClass() == "PCB_VIA":
            (vx, vy) = pcbnew.ToMM(t.GetPosition())
            d = ((vx - cx) ** 2 + (vy - cy) ** 2) ** 0.5
        else:
            (sx, sy), (ex, ey) = pcbnew.ToMM(t.GetStart()), pcbnew.ToMM(t.GetEnd())
            d = fr._seg_dist(cx, cy, sx, sy, ex, ey)
        if d < radius:
            near[n] = min(near.get(n, 99), d)
    return sorted(near, key=lambda n: near[n])


def free_pocket(name, path, net, a, b):
    """A pad walled in by a few neighbours (PCB-A: J1.5 inside the IDC fan-out): rip them all, route
    this net first, then the neighbours in every order until one fits."""
    for end in (a, b):
        others = pocket_nets(path, end, net)
        if not others or len(others) > 4:
            continue
        backup = open(path).read()
        for order in itertools.permutations(others):
            for o in others:
                fr.rip_up(path, o)
            fr.rip_up(path, net)
            if fr.reroute_net(path, name, net, width_of(net)) and \
                    all(fr.reroute_net(path, name, o, width_of(o)) for o in order):
                print(f"{net}: {end} encerrado; ruteado despejando {', '.join(order)}")
                return True
            open(path, "w").write(backup)
    return False


def width_of(net):
    return "0.5" if net in ("/+5V", "/VBUS_IN") else "0.2"


def main():
    name = sys.argv[1]
    path = os.path.abspath(os.path.join(HERE, "..", name, f"{name}.kicad_pcb"))
    pairs, partial = fr.unconnected_pairs(path)
    # Nets reported as pad ↔ track: join their two closest copper groups.
    for net in sorted(partial - {p[0] for p in pairs}):
        groups = fr.clusters(path, net)
        if len(groups) < 2:
            continue
        best = min(((abs(ax - bx) + abs(ay - by), a, b) for g in groups[1:] for a, (ax, ay) in groups[0]
                    for b, (bx, by) in g), default=None)
        if best:
            pairs.append((net, best[1], best[2]))
    fixed = 0
    for net, a, b in pairs:
        if free_pocket(name, path, net, a, b):
            fixed += 1
            continue
        segs = ideal_path(name, path, net, a, b, width_of(net))
        if not segs:
            print(f"{net}: {a} → {b} sin camino ni con la placa vacía")
            continue
        others = crossing_nets(path, segs, net)
        backup = open(path).read()
        for o in others:
            fr.rip_up(path, o)
        msg, ok = fr.route(name, a, b, width_of(net))
        good = ok
        if ok:
            for o in others:
                good &= fr.reroute_net(path, name, o, width_of(o))
                if not good:
                    break
        if good:
            fixed += 1
            print(f"{net}: {a} → {b} ruteado despejando {', '.join(others)}")
        else:
            open(path, "w").write(backup)
            print(f"{net}: {a} → {b} no se pudo (despejando {', '.join(others)})")
    print(f"{name}: {fixed}/{len(pairs)} conexiones reparadas")


main()

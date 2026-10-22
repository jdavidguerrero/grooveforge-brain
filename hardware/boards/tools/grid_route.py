"""Rutea una conexión que Freerouting no cerró: búsqueda de camino en grilla sobre F.Cu y B.Cu.

  $KPY apps/hardware/kicad/studio/tools/grid_route.py portadora J2.17 J5.11 [ancho_mm] [holgura_mm]

Rasteriza a 0.1 mm todo el cobre de otras redes (pistas, vías, pads, agujeros y el borde) inflado por la
holgura + medio ancho de pista; busca el camino más corto (Dijkstra, 8 vecinos) con un costo extra por
vía, lo simplifica en tramos rectos y lo agrega bloqueado. No mira los planos de GND (se rellenan
después). Correr antes de add_gnd_pours.py.
"""
import heapq
import math
import os
import sys

import numpy as np
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
RES = 0.1                      # mm por celda
CLEAR = 0.26                   # holgura a otras redes (mm): cubre la clase Power (0.25)
EDGE = 0.4                     # al borde (mm)
VIA_D, VIA_DRILL, VIA_COST = 0.6, 0.3, 40.0
# Bloquear lo ruteado (como en el Brain, donde sólo lo de Freerouting se levanta). Sin Freerouting todo
# sale de acá, y bloqueado el rip-up de finish_routing no tendría nada que levantar.
LOCK = os.environ.get("GRID_LOCK", "0") == "1"



def edge_segments(d):
    """Straight pieces of an Edge.Cuts drawing: a segment, the four sides of a rectangle (rounded or not)
    or an arc sampled every ~0.2 mm. (A rectangle used to be read as ONE start->end line: a diagonal
    wall across the whole board.)"""
    t = d.GetShape()
    (sx, sy), (ex, ey) = pcbnew.ToMM(d.GetStart()), pcbnew.ToMM(d.GetEnd())
    if t == pcbnew.SHAPE_T_RECT:
        return [((sx, sy), (ex, sy)), ((ex, sy), (ex, ey)), ((ex, ey), (sx, ey)), ((sx, ey), (sx, sy))]
    if t == pcbnew.SHAPE_T_ARC:
        arc = pcbnew.SHAPE_ARC(d.GetStart(), d.GetArcMid(), d.GetEnd(), 0)
        chain = arc.ConvertToPolyline(pcbnew.FromMM(0.05))
        pts = [pcbnew.ToMM(chain.CPoint(i)) for i in range(chain.PointCount())]
        return list(zip(pts, pts[1:]))
    return [((sx, sy), (ex, ey))]

def main():
    board_name, a_ref, b_ref = sys.argv[1:4]
    width = float(sys.argv[4]) if len(sys.argv) > 4 else 0.2
    global CLEAR
    if len(sys.argv) > 5:
        CLEAR = float(sys.argv[5])
    path = os.path.join(HERE, "..", board_name, f"{board_name}.kicad_pcb")
    board = pcbnew.LoadBoard(path)

    def pad_of(spec):
        ref, num = spec.split(".")
        fp = board.FindFootprintByReference(ref)
        return next(p for p in fp.Pads() if p.GetNumber() == num)

    pa, pb = pad_of(a_ref), pad_of(b_ref)
    net = pa.GetNetname()
    assert net == pb.GetNetname(), "los pads no son de la misma red"
    # Si ya están unidos por cobre (pistas y vías, sin contar planos), no hay nada que hacer.
    board.BuildConnectivity()
    conn = board.GetConnectivity()
    seen, todo = set(), ([] if os.environ.get("GRID_FORCE") else [pa])  # GRID_FORCE: rutear igual
    while todo:
        item = todo.pop()
        key = item.m_Uuid.AsString()
        if key in seen:
            continue
        seen.add(key)
        if item.m_Uuid.AsString() == pb.m_Uuid.AsString():
            print(f"{net}: {a_ref} y {b_ref} ya están conectados")
            return
        todo.extend(i for i in conn.GetConnectedItems(item) if i.GetClass() != "ZONE")

    bb = board.GetBoardEdgesBoundingBox()
    x0, y0 = pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop())
    nx = int(pcbnew.ToMM(bb.GetWidth()) / RES) + 1
    ny = int(pcbnew.ToMM(bb.GetHeight()) / RES) + 1
    layers = [pcbnew.F_Cu, pcbnew.B_Cu]
    blocked = np.zeros((2, ny, nx), dtype=bool)
    via_blocked = np.zeros((ny, nx), dtype=bool)
    gx = x0 + np.arange(nx) * RES
    gy = y0 + np.arange(ny) * RES

    def mark_disk(mask, cx, cy, r):
        i0, i1 = max(0, int((cy - r - y0) / RES)), min(ny, int((cy + r - y0) / RES) + 2)
        j0, j1 = max(0, int((cx - r - x0) / RES)), min(nx, int((cx + r - x0) / RES) + 2)
        if i0 >= i1 or j0 >= j1:
            return
        X, Y = np.meshgrid(gx[j0:j1], gy[i0:i1])
        mask[i0:i1, j0:j1] |= (X - cx) ** 2 + (Y - cy) ** 2 <= r * r

    def mark_seg(mask, ax, ay, bx, by, r):
        i0 = max(0, int((min(ay, by) - r - y0) / RES)); i1 = min(ny, int((max(ay, by) + r - y0) / RES) + 2)
        j0 = max(0, int((min(ax, bx) - r - x0) / RES)); j1 = min(nx, int((max(ax, bx) + r - x0) / RES) + 2)
        if i0 >= i1 or j0 >= j1:
            return
        X, Y = np.meshgrid(gx[j0:j1], gy[i0:i1])
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy or 1e-12
        t = np.clip(((X - ax) * dx + (Y - ay) * dy) / L2, 0, 1)
        mask[i0:i1, j0:j1] |= (X - ax - t * dx) ** 2 + (Y - ay - t * dy) ** 2 <= r * r

    def mark_rect(mask, cx0, cy0, cx1, cy1, r):
        # Exacto: sólo las celdas cuyo centro cae dentro del rectángulo inflado. El redondeo hacia
        # afuera (±2 celdas) tapaba el canal entre pads de un SSOP (paso 0.65 mm).
        i0, i1 = max(0, math.ceil((cy0 - r - y0) / RES - 1e-9)), min(ny, math.floor((cy1 + r - y0) / RES + 1e-9) + 1)
        j0, j1 = max(0, math.ceil((cx0 - r - x0) / RES - 1e-9)), min(nx, math.floor((cx1 + r - x0) / RES + 1e-9) + 1)
        if i0 < i1 and j0 < j1:
            mask[i0:i1, j0:j1] = True

    half = width / 2
    for t in board.GetTracks():
        if t.GetNetname() == net:
            if t.GetClass() == "PCB_VIA":  # misma red: igual respeta agujero a agujero (0.5 mm)
                (cx, cy) = pcbnew.ToMM(t.GetPosition())
                mark_disk(via_blocked, cx, cy, VIA_DRILL + 0.55)
            continue
        if t.GetClass() == "PCB_VIA":
            (cx, cy), r = pcbnew.ToMM(t.GetPosition()), pcbnew.ToMM(t.GetWidth()) / 2
            for k in range(2):
                mark_disk(blocked[k], cx, cy, r + CLEAR + half)
            mark_disk(via_blocked, cx, cy, r + CLEAR + VIA_D / 2)
        else:
            k = layers.index(t.GetLayer()) if t.GetLayer() in layers else None
            if k is None:
                continue
            (ax, ay), (bx, by) = pcbnew.ToMM(t.GetStart()), pcbnew.ToMM(t.GetEnd())
            r = pcbnew.ToMM(t.GetWidth()) / 2
            mark_seg(blocked[k], ax, ay, bx, by, r + CLEAR + half)
            mark_seg(via_blocked, ax, ay, bx, by, r + CLEAR + VIA_D / 2)
    for fp in board.GetFootprints():
        for p in fp.Pads():
            box = p.GetBoundingBox()
            bx0, by0 = pcbnew.ToMM(box.GetLeft()), pcbnew.ToMM(box.GetTop())
            bx1, by1 = pcbnew.ToMM(box.GetRight()), pcbnew.ToMM(box.GetBottom())
            same = p.GetNetname() == net and p.GetNetname() != ""
            drill = pcbnew.ToMM(max(p.GetDrillSize().x, p.GetDrillSize().y)) / 2
            if drill > 0:
                (cx, cy) = pcbnew.ToMM(p.GetPosition())
                mark_disk(via_blocked, cx, cy, drill + VIA_DRILL / 2 + 0.5)
            elif same:
                # Nunca una vía dentro (ni pegada) a un pad SMD, ni siquiera de su propia red: el barril se
                # lleva la pasta de soldar (auditoría 2026-10-22, D01). Centro ≥ 0.5 mm del borde del pad.
                # SAME_NET_VIA_MARGIN: via centre to pad edge (default 0.5; 0.4 still leaves the drill 0.25 mm out)
                mark_rect(via_blocked, bx0, by0, bx1, by1, float(os.environ.get("SAME_NET_VIA_MARGIN", "0.5")))
            if same:
                continue
            for k, layer in enumerate(layers):
                if p.IsOnLayer(layer):
                    mark_rect(blocked[k], bx0, by0, bx1, by1, CLEAR + half)
            mark_rect(via_blocked, bx0, by0, bx1, by1, CLEAR + VIA_D / 2)
    # Borde de la placa y agujeros del contorno (separadores).
    for d in board.GetDrawings():
        if d.GetLayer() != pcbnew.Edge_Cuts:
            continue
        if d.GetShape() == pcbnew.SHAPE_T_CIRCLE:
            (cx, cy), r = pcbnew.ToMM(d.GetCenter()), pcbnew.ToMM(d.GetRadius())
            for k in range(2):
                mark_disk(blocked[k], cx, cy, r + EDGE + half)
            mark_disk(via_blocked, cx, cy, r + EDGE + VIA_D / 2)
        else:
            for (ax, ay), (bx, by) in edge_segments(d):
                for k in range(2):
                    mark_seg(blocked[k], ax, ay, bx, by, EDGE + half)
                mark_seg(via_blocked, ax, ay, bx, by, EDGE + VIA_D / 2)

    # Ventanas Edge.Cuts dentro de huellas (SK6812 de montaje inverso): el hueco entero bloquea.
    for fp in board.GetFootprints():
        cuts = [g for g in fp.GraphicalItems() if g.GetLayer() == pcbnew.Edge_Cuts]
        if not cuts:
            continue
        box = cuts[0].GetBoundingBox()
        for g in cuts[1:]:
            box.Merge(g.GetBoundingBox())
        bx0, by0 = pcbnew.ToMM(box.GetLeft()), pcbnew.ToMM(box.GetTop())
        bx1, by1 = pcbnew.ToMM(box.GetRight()), pcbnew.ToMM(box.GetBottom())
        for k in range(2):
            mark_rect(blocked[k], bx0, by0, bx1, by1, EDGE + half)
        mark_rect(via_blocked, bx0, by0, bx1, by1, EDGE + VIA_D / 2)

    if os.environ.get("GRID_DUMP"):              # máscaras como PGM (F, B, vías) para depurar
        for k, m in (("F", blocked[0]), ("B", blocked[1]), ("V", via_blocked)):
            with open(f"{os.environ['GRID_DUMP']}-{k}.pgm", "wb") as fh:
                fh.write(b"P5 %d %d 255\n" % (nx, ny))
                fh.write((np.where(m, 0, 255)).astype(np.uint8).tobytes())

    def cell(x, y):
        return int(round((y - y0) / RES)), int(round((x - x0) / RES))

    sa, sb = pcbnew.ToMM(pa.GetPosition()), pcbnew.ToMM(pb.GetPosition())
    start_cell, goal_cell = cell(*sa), cell(*sb)
    starts = [(k,) + start_cell for k, l in enumerate(layers) if pa.IsOnLayer(l)]
    goals = {(k,) + goal_cell for k, l in enumerate(layers) if pb.IsOnLayer(l)}
    for k, i, j in starts + list(goals):
        blocked[k, i, j] = False

    dist = {s: 0.0 for s in starts}
    prev = {}
    pq = [(0.0, s) for s in starts]
    steps = [(-1, 0, 1), (1, 0, 1), (0, -1, 1), (0, 1, 1), (-1, -1, 1.414), (-1, 1, 1.414), (1, -1, 1.414), (1, 1, 1.414)]
    end = None
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist.get(u, 1e18):
            continue
        if u in goals:
            end = u
            break
        k, i, j = u
        nbrs = [((k, i + di, j + dj), c) for di, dj, c in steps]
        if not via_blocked[i, j]:
            nbrs.append(((1 - k, i, j), VIA_COST))
        for v, c in nbrs:
            kk, ii, jj = v
            if not (0 <= ii < ny and 0 <= jj < nx) or blocked[kk, ii, jj]:
                continue
            nd = d + c
            if nd < dist.get(v, 1e18):
                dist[v] = nd
                prev[v] = u
                heapq.heappush(pq, (nd, v))
    if end is None:
        if os.environ.get("GRID_DEBUG"):
            # Alcance desde cada extremo: dónde se cierra el paso.
            for label, seeds in ((a_ref, starts), (b_ref, list(goals))):
                seen = set(seeds); todo = list(seeds)
                while todo:
                    k, i, j = todo.pop()
                    nb = [(k, i + di, j + dj) for di, dj, _ in steps]
                    if not via_blocked[i, j]:
                        nb.append((1 - k, i, j))
                    for kk, ii, jj in nb:
                        if 0 <= ii < ny and 0 <= jj < nx and not blocked[kk, ii, jj] and (kk, ii, jj) not in seen:
                            seen.add((kk, ii, jj)); todo.append((kk, ii, jj))
                xs = [x0 + c[2] * RES for c in seen]; ys = [y0 + c[1] * RES for c in seen]
                print(f"  alcance desde {label}: {len(seen)} celdas, x {min(xs):.1f}–{max(xs):.1f}, "
                      f"y {min(ys):.1f}–{max(ys):.1f}, capas {sorted({c[0] for c in seen})}")
        raise SystemExit(f"{net}: no hay camino entre {a_ref} y {b_ref}")

    pts = [end]
    while pts[-1] in prev:
        pts.append(prev[pts[-1]])
    pts.reverse()
    # Simplificar: cortar en cambios de dirección o de capa.
    nodes = [pts[0]]
    for p_prev, p, p_next in zip(pts, pts[1:], pts[2:]):
        d1 = (p[0] - p_prev[0], p[1] - p_prev[1], p[2] - p_prev[2])
        d2 = (p_next[0] - p[0], p_next[1] - p[1], p_next[2] - p[2])
        if d1 != d2:
            nodes.append(p)
    nodes.append(pts[-1])

    netinfo = board.FindNet(net)
    added = 0

    def xy(n):
        return pcbnew.VECTOR2I(pcbnew.FromMM(x0 + n[2] * RES), pcbnew.FromMM(y0 + n[1] * RES))

    # Extremos exactos en el centro de los pads.
    first, last = pcbnew.VECTOR2I(pa.GetPosition()), pcbnew.VECTOR2I(pb.GetPosition())
    for idx, (n1, n2) in enumerate(zip(nodes, nodes[1:])):
        if n1[0] != n2[0]:
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(xy(n1))
            v.SetWidth(pcbnew.FromMM(VIA_D))
            v.SetDrill(pcbnew.FromMM(VIA_DRILL))
            v.SetNet(netinfo)
            v.SetLocked(LOCK)
            board.Add(v)
            added += 1
            continue
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(first if idx == 0 else xy(n1))
        t.SetEnd(last if idx == len(nodes) - 2 else xy(n2))
        t.SetWidth(pcbnew.FromMM(width))
        t.SetLayer(layers[n1[0]])
        t.SetNet(netinfo)
        t.SetLocked(LOCK)
        board.Add(t)
        added += 1
    pcbnew.SaveBoard(path, board)
    print(f"{net}: {a_ref} → {b_ref} ruteado ({added} tramos/vías, costo {dist[end]:.0f})")


main()

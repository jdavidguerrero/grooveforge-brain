"""Shared helpers for the GroovePush panel boards (run with KiCad's bundled Python, which has pcbnew).

Coordinate system: every board uses the panel's flat coordinates from
../panel/panel-coordinates.csv (x from the left edge, y from the front edge, mm) mapped to KiCad
as x_pcb = x, y_pcb = Y0 - y, so all four boards share one frame and can be checked against the
enclosure. The front of the instrument is at the bottom of the screen.
"""
import csv
import math
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
BOARDS = os.path.dirname(HERE)
COORDS = os.path.join(BOARDS, "..", "panel", "panel-coordinates.csv")
Y0 = 290.0


def panel():
    with open(COORDS, newline="") as f:
        return {r["ref"]: r for r in csv.DictReader(f)}


def pcb(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(Y0 - y))


def load(name):
    path = os.path.join(BOARDS, name, f"{name}.kicad_pcb")
    return path, pcbnew.LoadBoard(path)


def fp(board, ref):
    f = board.FindFootprintByReference(ref)
    if f is None:
        raise SystemExit(f"footprint {ref} not found")
    return f


def place(board, ref, x, y, rot=0.0, back=False):
    """Place footprint `ref` at panel (x, y). `rot` is the on-screen rotation in degrees (CCW)."""
    f = fp(board, ref)
    if back != f.IsFlipped():
        f.Flip(f.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
    f.SetPosition(pcb(x, y))
    f.SetOrientationDegrees(rot)
    return f


def pad_panel(board, ref, num):
    """Panel coordinates of the first pad numbered `num` of footprint `ref`."""
    for p in fp(board, ref).Pads():
        if p.GetNumber() == num:
            pos = p.GetPosition()
            return pcbnew.ToMM(pos.x), Y0 - pcbnew.ToMM(pos.y)
    raise SystemExit(f"{ref} has no pad {num}")


def jst_opening(board, ref):
    """Unit vector (panel frame) pointing out of a horizontal JST-SH connector's mating face.

    In the KiCad footprint the signal pads sit behind the mounting pads (MP); the plug enters
    from the MP side, so the opening points from the signal-pad row towards the MP pads.
    """
    f = fp(board, ref)
    sig = [p.GetPosition() for p in f.Pads() if p.GetNumber() not in ("MP", "")]
    mp = [p.GetPosition() for p in f.Pads() if p.GetNumber() == "MP"]
    sx = sum(p.x for p in sig) / len(sig); sy = sum(p.y for p in sig) / len(sig)
    mx = sum(p.x for p in mp) / len(mp); my = sum(p.y for p in mp) / len(mp)
    dx, dy = mx - sx, -(my - sy)  # back to panel frame (y up)
    n = math.hypot(dx, dy)
    return round(dx / n, 3), round(dy / n, 3)


def outline(board, x0, y0, x1, y1, r=1.0):
    """Rectangular Edge.Cuts outline in panel coordinates with rounded corners of radius r."""
    for d in list(board.GetDrawings()):
        if d.GetLayer() == pcbnew.Edge_Cuts:
            board.Delete(d)
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_RECT)
    s.SetStart(pcb(x0, y1))
    s.SetEnd(pcb(x1, y0))
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(pcbnew.FromMM(0.1))
    if hasattr(s, "SetCornerRadius"):
        s.SetCornerRadius(pcbnew.FromMM(r))
    board.Add(s)


def jlc_rules(board):
    """JLCPCB 2-layer rules used on the Brain boards (lecciones-para-groovepush.md §1)."""
    ds = board.GetDesignSettings()
    ds.m_TrackMinWidth = pcbnew.FromMM(0.2)
    ds.m_MinClearance = pcbnew.FromMM(0.2)
    ds.m_ViasMinSize = pcbnew.FromMM(0.5)
    ds.m_ViasMinAnnularWidth = pcbnew.FromMM(0.13)
    ds.m_MinThroughDrill = pcbnew.FromMM(0.3)
    ds.m_HoleToHoleMin = pcbnew.FromMM(0.5)
    ds.m_CopperEdgeClearance = pcbnew.FromMM(0.3)
    ds.m_HoleClearance = pcbnew.FromMM(0.25)
    ds.SetCopperLayerCount(2)
    nc = ds.m_NetSettings.GetDefaultNetclass()
    nc.SetTrackWidth(pcbnew.FromMM(0.25))
    nc.SetClearance(pcbnew.FromMM(0.2))
    nc.SetViaDiameter(pcbnew.FromMM(0.6))
    nc.SetViaDrill(pcbnew.FromMM(0.3))


def _boxes(board, skip):
    """Rectangles (mm, PCB frame) a new SMD part must stay out of: every pad of the other footprints
    (inflated by their clearance) and the courtyard of the SMD ones (their body)."""
    out = []
    for f in board.FootprintsList() if hasattr(board, "FootprintsList") else board.GetFootprints():
        if f.GetReference() in skip:
            continue
        for p in f.Pads():
            bb = p.GetBoundingBox()
            m = 0.45 if p.GetDrillSize().x == 0 else 0.55
            out.append((pcbnew.ToMM(bb.GetLeft()) - m, pcbnew.ToMM(bb.GetTop()) - m,
                        pcbnew.ToMM(bb.GetRight()) + m, pcbnew.ToMM(bb.GetBottom()) + m))
        if all(p.GetDrillSize().x == 0 for p in f.Pads() if p.GetNumber()):
            cy = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd)
            if cy.OutlineCount():
                bb = cy.BBox()
                out.append((pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()),
                            pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom())))
    return out


def _outline_segments(board):
    out = []
    for d in board.GetDrawings():
        if d.GetLayer() != pcbnew.Edge_Cuts:
            continue
        (sx, sy), (ex, ey) = pcbnew.ToMM(d.GetStart()), pcbnew.ToMM(d.GetEnd())
        t = d.GetShape()
        if t == pcbnew.SHAPE_T_RECT:
            out += [((sx, sy), (ex, sy)), ((ex, sy), (ex, ey)), ((ex, ey), (sx, ey)), ((sx, ey), (sx, sy))]
        elif t == pcbnew.SHAPE_T_SEGMENT:
            out.append(((sx, sy), (ex, ey)))
    return out


def _edge_gap(f, segs):
    """Smallest distance (mm) from the footprint's body+pads box to the board outline."""
    bb = f.GetBoundingBox(False)
    pts = [(pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop())), (pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetTop())),
           (pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom())), (pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetBottom()))]
    best = 1e9
    for px, py in pts:
        for (ax, ay), (bx, by) in segs:
            dx, dy = bx - ax, by - ay
            t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy or 1e-12)))
            best = min(best, math.hypot(px - ax - t * dx, py - ay - t * dy))
    return best


def _own_courtyard(f):
    cy = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd)
    if not cy.OutlineCount():
        return None
    bb = cy.BBox()
    return (pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()), pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom()))


def _hits(r, boxes):
    return any(r[0] < b[2] and r[2] > b[0] and r[1] < b[3] and r[3] > b[1] for b in boxes)


def place_decap(board, cref, uref, vdd="9", vss="10", rmax=9.0, step=0.25, edge_keepout=1.0, skip=()):
    """Put decoupling capacitor `cref` as close as possible to pins `vdd`/`vss` of `uref` (audit D06).

    Tries a grid of centres and the four rotations, keeps the ones whose pads clear every other pad and
    body and the board outline, and takes the one with the shortest pad1->VDD + pad2->VSS distance
    (pad 1 is the +3V3 side). Returns that distance sum in mm. The part stays on the side it is on."""
    c, u = fp(board, cref), fp(board, uref)
    pin = {p.GetNumber(): pcbnew.ToMM(p.GetPosition()) for p in u.Pads()}
    (vx, vy), (gx, gy) = pin[vdd], pin[vss]
    pos3 = {p.GetNumber(): p.GetNetname() for p in c.Pads()}
    vdd_net = next(p.GetNetname() for p in u.Pads() if p.GetNumber() == vdd)
    pad_v = "1" if pos3.get("1") == vdd_net else "2"
    pad_g = "2" if pad_v == "1" else "1"
    boxes = _boxes(board, {cref} | set(skip))
    e = board.GetBoardEdgesBoundingBox()
    outline = [(pcbnew.ToMM(d.GetStart()), pcbnew.ToMM(d.GetEnd())) for d in board.GetDrawings()
               if d.GetLayer() == pcbnew.Edge_Cuts and d.GetShape() == pcbnew.SHAPE_T_SEGMENT]

    def seg_dist(px, py, ax, ay, bx, by):
        dx, dy = bx - ax, by - ay
        t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy or 1e-12)))
        return math.hypot(px - ax - t * dx, py - ay - t * dy)

    best, cx0, cy0 = None, (vx + gx) / 2, (vy + gy) / 2
    n = int(rmax / step)
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            x, y = cx0 + i * step, cy0 + j * step
            if math.hypot(x - cx0, y - cy0) > rmax:
                continue
            for rot in (0, 90, 180, 270):
                c.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
                c.SetOrientationDegrees(rot)
                rects, ok = {}, True
                for p in c.Pads():
                    bb = p.GetBoundingBox()
                    r = tuple(pcbnew.ToMM(v) for v in (bb.GetLeft(), bb.GetTop(), bb.GetRight(), bb.GetBottom()))
                    rects[p.GetNumber()] = (r, pcbnew.ToMM(p.GetPosition()))
                    for b in boxes:
                        if r[0] < b[2] and r[2] > b[0] and r[1] < b[3] and r[3] > b[1]:
                            ok = False
                            break
                    if not ok:
                        break
                    if outline and min(seg_dist((r[0] + r[2]) / 2, (r[1] + r[3]) / 2, *a, *b2) for a, b2 in
                                       [((s[0], s[1]), (t[0], t[1])) for s, t in outline]) < edge_keepout + 0.5:
                        ok = False
                        break
                if not ok:
                    continue
                cyb = _own_courtyard(c)
                if cyb and _hits(cyb, boxes):
                    continue
                (pvx, pvy), (pgx, pgy) = rects[pad_v][1], rects[pad_g][1]
                cost = math.hypot(pvx - vx, pvy - vy) + math.hypot(pgx - gx, pgy - gy)
                if best is None or cost < best[0]:
                    best = (cost, x, y, rot)
    if best is None:
        raise SystemExit(f"{cref}: no free spot within {rmax} mm of {uref}.{vdd}/{vss}")
    _, x, y, rot = best
    c.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
    c.SetOrientationDegrees(rot)
    return best[0]


def place_free(board, ref, candidates, rot=None, skip=(), back=True, edge_min=2.5):
    """Place `ref` at the first panel-frame (x, y) candidate where its pads clear everything else."""
    f = fp(board, ref)
    if back != f.IsFlipped():
        f.Flip(f.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
    boxes = _boxes(board, {ref} | set(skip))
    segs = _outline_segments(board)
    for x, y in candidates:
        f.SetPosition(pcb(x, y))
        if rot is not None:
            f.SetOrientationDegrees(rot)
        ok = True
        for p in f.Pads():
            bb = p.GetBoundingBox()
            r = tuple(pcbnew.ToMM(v) for v in (bb.GetLeft(), bb.GetTop(), bb.GetRight(), bb.GetBottom()))
            if any(r[0] < b[2] and r[2] > b[0] and r[1] < b[3] and r[3] > b[1] for b in boxes):
                ok = False
                break
        cyb = _own_courtyard(f)
        if ok and not (cyb and _hits(cyb, boxes)) and (not segs or _edge_gap(f, segs) >= edge_min):
            return x, y
    raise SystemExit(f"{ref}: none of the {len(candidates)} candidate spots is free")


def save(board, path):
    pcbnew.SaveBoard(path, board)


def refresh_lib_footprints(board, lib="groovepush"):
    """Replace every footprint from our own library with its current version on disk, keeping
    reference, value, schematic link and pad nets (the board stores its own copy)."""
    libpath = os.path.join(BOARDS, "lib", f"{lib}.pretty")
    # GetFootprints() can hand back unwrapped SWIG pointers; go by reference instead.
    import re as _re
    text = open(board.GetFileName()).read()
    refs = _re.findall(r'\(footprint "' + lib + r':[^"]+".*?\(property "Reference" "([^"]+)"', text, _re.S)
    n = 0
    for ref in refs:
        old = board.FindFootprintByReference(ref)
        fpid = old.GetFPID()
        new = pcbnew.FootprintLoad(libpath, fpid.GetLibItemName().wx_str())
        new.SetParent(board)
        new.SetFPID(fpid)
        new.SetReference(old.GetReference())
        new.SetValue(old.GetValue())
        new.SetPath(old.GetPath())
        new.SetSheetname(old.GetSheetname())
        new.SetSheetfile(old.GetSheetfile())
        new.SetPosition(old.GetPosition())
        if old.IsFlipped():
            new.Flip(new.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
        new.SetOrientation(old.GetOrientation())
        nets = {}
        for p in old.Pads():
            nets.setdefault(p.GetNumber(), p.GetNet())
        board.Delete(old)
        board.Add(new)
        for p in new.Pads():
            if p.GetNumber() in nets:
                p.SetNet(nets[p.GetNumber()])
        n += 1
    return n


def hide_refs(board, refs):
    """Hide the silkscreen reference of large parts whose designator would overlap their neighbours
    (the reference stays on the Fab layer for assembly drawings)."""
    for r in refs:
        f = fp(board, r)
        f.Reference().SetVisible(False)


def outline_poly(board, pts):
    """Closed Edge.Cuts polygon through panel-coordinate points (for L-shaped boards)."""
    for d in list(board.GetDrawings()):
        if d.GetLayer() == pcbnew.Edge_Cuts:
            board.Delete(d)
    for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(pcbnew.FromMM(0.1))
        s.SetStart(pcb(x0, y0))
        s.SetEnd(pcb(x1, y1))
        board.Add(s)


def drop(board, refs):
    """Delete footprints that were removed from the schematic."""
    for r in refs:
        f = board.FindFootprintByReference(r)
        if f is not None:
            board.Delete(f)


def place_center(board, ref, x, y, rot=0.0, back=False, pads=None):
    """Place `ref` so the centre of its pads (or of the pads numbered in `pads`, e.g. ['MP'] for an
    EC11's shaft) lands on panel (x, y)."""
    f = place(board, ref, x, y, rot, back)
    sel = [p for p in f.Pads() if pads is None or p.GetNumber() in pads]
    xs = [p.GetPosition().x for p in sel]; ys = [p.GetPosition().y for p in sel]
    cx, cy = (min(xs) + max(xs)) // 2, (min(ys) + max(ys)) // 2
    pos = f.GetPosition()
    want = pcb(x, y)
    f.SetPosition(pcbnew.VECTOR2I(pos.x + (want.x - cx), pos.y + (want.y - cy)))
    return f

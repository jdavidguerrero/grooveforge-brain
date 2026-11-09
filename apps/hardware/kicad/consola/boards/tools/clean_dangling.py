"""Remove track segments with a free end (Freerouting leftovers). Usage: clean_dangling.py <board>"""
import sys

import pcbnew

import boardlib as bl

path, b = bl.load(sys.argv[1])
removed = 0
while True:
    tracks = [t for t in b.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]
    anchors = []
    for t in b.GetTracks():
        if t.Type() == pcbnew.PCB_VIA_T:
            anchors.append((t.GetPosition(), t.GetNetCode(), None, t.GetWidth(pcbnew.F_Cu) // 2))
    for f in b.GetFootprints():
        for p in f.Pads():
            anchors.append((p.GetPosition(), p.GetNetCode(), p, 0))
    dead = []
    for t in tracks:
        for end in (t.GetStart(), t.GetEnd()):
            ok = False
            for o in tracks:
                if o is not t and o.GetNetCode() == t.GetNetCode() and o.GetLayer() == t.GetLayer() and \
                        (o.HitTest(end, t.GetWidth() // 2)):
                    ok = True; break
            if not ok:
                for pos, net, pad, r in anchors:
                    if net != t.GetNetCode():
                        continue
                    if pad is not None:
                        if pad.IsOnLayer(t.GetLayer()) and pad.HitTest(end):
                            ok = True; break
                    elif (pos - end).EuclideanNorm() <= r:
                        ok = True; break
            if not ok:
                dead.append(t); break
    if not dead:
        break
    for t in dead:
        b.Remove(t); removed += 1
bl.save(b, path)
print("dangling segments removed:", removed)

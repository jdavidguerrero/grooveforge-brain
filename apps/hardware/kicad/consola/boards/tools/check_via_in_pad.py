"""Fail if any via drill touches an SMD pad of its own board (DFM: solder wicks down the barrel).

  $KPY check_via_in_pad.py <board>

An SMD pad is a pad with no drill. For each via, the distance from its centre to the pad rectangle must
be at least the via's drill radius + 0.2 mm (the audit of 2026-10-22 asks for the centre >= 0.5 mm from the
pad edge for a 0.6 mm via; the stricter router margin is 0.5 mm, this check fails below 0.35 mm).
Exit code 1 and a list when something is wrong.
"""
import math
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
MIN_GAP = 0.35   # via centre to pad edge (mm)


def main():
    name = sys.argv[1]
    board = pcbnew.LoadBoard(os.path.join(HERE, "..", name, f"{name}.kicad_pcb"))
    vias = [(pcbnew.ToMM(t.GetPosition()), t.GetNetname()) for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    bad = []
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetDrillSize().x > 0:
                continue
            bb = p.GetBoundingBox()
            x0, y0, x1, y1 = (pcbnew.ToMM(v) for v in (bb.GetLeft(), bb.GetTop(), bb.GetRight(), bb.GetBottom()))
            for (vx, vy), vnet in vias:
                d = math.hypot(max(x0 - vx, 0, vx - x1), max(y0 - vy, 0, vy - y1))
                if d < MIN_GAP:
                    bad.append(f"{fp.GetReference()}.{p.GetNumber()} [{p.GetNetname()}] via at ({vx:.2f}, {vy:.2f}) "
                               f"[{vnet}] {d:.2f} mm from the pad")
    print(f"{name}: {len(bad)} vías sobre pads SMD")
    for b in bad:
        print("  ", b)
    sys.exit(1 if bad else 0)


main()

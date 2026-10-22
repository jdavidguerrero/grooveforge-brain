"""Place PCB-K2 (5 mode keys beside the display) from the panel coordinate table.

Kailh Choc V2 on F.Cu; each SK6812MINI-E reverse-mounted on B.Cu under the key's LED window,
4.93 mm towards the back of the key centre (same as the Brain's PCB-K). The keys clip into the
aluminium plate, so the two M2.5 holes only steady the rear end. J1 faces the front, towards PCB-K1.
"""
import boardlib as bl

LED_OFFSET = 4.93
path, b = bl.load("pcb-k2")
print("refreshed", bl.refresh_lib_footprints(b), "library footprints")
P = bl.panel()
bl.jlc_rules(b)

KEYS = ["BROWSE", "MIX", "DEVICE", "NOTE", "SESSION"]  # SW1..SW5 / D1..D5 / C10..C14
for i, k in enumerate(KEYS):
    x, y = float(P[k]["x_mm"]), float(P[k]["y_mm"])
    bl.place(b, f"SW{i+1}", x, y)
    bl.place(b, f"D{i+1}", x, y + LED_OFFSET, back=True)
    # Audit D04: long axis across the board (perpendicular to the bending axis) and >= 2.5 mm from the edge
    bl.place_free(b, f"C{10+i}", [(x - dx, y + LED_OFFSET) for dx in (6.0, 6.5, 7.0, 7.5)], rot=0)

X0, X1, Y0f, Y1 = 11.5, 30.0, 120.5, 224.0
bl.place(b, "J1", 21.0, Y0f + 3.3, rot=0, back=True)
dx, dy = bl.jst_opening(b, "J1")
if dy > -0.9:
    bl.place(b, "J1", 21.0, Y0f + 3.3, rot=180, back=True)
    dx, dy = bl.jst_opening(b, "J1")
assert dy < -0.9, f"J1 opening points {dx, dy}"
bl.place(b, "C1", 26.5, Y0f + 9.0, rot=90, back=True)
bl.place(b, "H1", 15.5, 220.5)
bl.place(b, "H2", 26.0, 220.5)

bl.hide_refs(b, [f"D{i+1}" for i in range(5)] + [f"C{10+i}" for i in range(5)] + ["C1", "H1", "H2", "J1"])
bl.outline(b, X0, Y0f, X1, Y1)
bl.save(b, path)
print("PCB-K2 placed; J1 opening", (dx, dy))

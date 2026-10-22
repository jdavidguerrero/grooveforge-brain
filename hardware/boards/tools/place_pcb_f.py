"""Place PCB-F (4 ALPS RS6011 faders) from the panel coordinate table.

Faders run along the panel's y axis. Rotation 90 puts terminal 3 (+3V3) at the far end, so the
wiper reads high with the cap pushed away from the player. Every SMD part sits on B.Cu (single-sided
JLC assembly); the faders are hand-soldered on F.Cu.
"""
import boardlib as bl

path, b = bl.load("pcb-f")
print('refreshed', bl.refresh_lib_footprints(b), 'library footprints')
P = bl.panel()
bl.jlc_rules(b)

FADERS = ["F1", "F2", "F3", "F4"]
for i, name in enumerate(FADERS):
    x, y = float(P[name]["x_mm"]), float(P[name]["y_mm"])
    bl.place(b, f"RV{i+1}", x, y, rot=90)
    # wiper filter cap on the back, under the fader body next to the wiper pad
    wx, wy = bl.pad_panel(b, f"RV{i+1}", "2")
    bl.place(b, f"C{i+1}", wx + 1.0, wy + 5.0, rot=90, back=True)

X0, X1, Y0f, Y1 = 228.0, 274.0, 26.5, 118.5
# JST-SH 6 at the rear-left corner, mating face towards PCB-A (left)
j = bl.place(b, "J1", X0 + 3.5, 113.5, rot=90, back=True)
dx, dy = bl.jst_opening(b, "J1")
if dx > -0.9:  # opening must point to -x
    bl.place(b, "J1", X0 + 3.5, 113.5, rot=270, back=True)
    dx, dy = bl.jst_opening(b, "J1")
assert dx < -0.9, f"J1 opening points {dx, dy}"
# Audit D04: the old spot (X0+9, 113.5) was 3.2 mm from H3 (M3 screw head up to ~5.2 mm on the same face)
bl.place_free(b, "C5", [(x, y) for y in (105.0, 103.0, 101.0, 99.0) for x in (232.0, 233.5, 231.0)], rot=90)

for ref, (x, y) in {"H1": (240.0, 30.5), "H2": (262.0, 30.5), "H3": (240.0, 114.5), "H4": (262.0, 114.5)}.items():
    bl.place(b, ref, x, y)

bl.hide_refs(b, ['RV1', 'RV2', 'RV3', 'RV4', 'H1', 'H2', 'H3', 'H4', 'C5'])
bl.outline(b, X0, Y0f, X1, Y1)
bl.save(b, path)
print("PCB-F placed; J1 opening", (dx, dy))
for i in range(4):
    print(f"RV{i+1}", "pad1", bl.pad_panel(b, f"RV{i+1}", "1"), "pad3", bl.pad_panel(b, f"RV{i+1}", "3"))

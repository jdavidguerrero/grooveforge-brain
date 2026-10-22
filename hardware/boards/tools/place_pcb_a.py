"""Place PCB-A (8 EC11 encoders + MCP23017 @0x20 + the carrier IDC) from the panel table.

Shape: an encoder strip (y 97.5-119.6, rear edge kept 1.3 mm off the display module at y 120.9) plus a
tab towards the front between the M4 (x <= 186) and the faders (PCB-F at x >= 228), where the
panel has no controls. The 43 mm IDC 2x15 doesn't fit between encoders (9.5 mm gaps), so it goes on
the tab with the pull-ups, the I2C-in JST and the fader JST (the MCP sits between E4 and E5). J4 (to
PCB-K1) sits on the tab's left edge next to J1, so its bus stays off the strip. EC11 rotated 0: pin columns left/right of the shaft, so the
footprint is 14.2 mm deep. All SMD on B.Cu; EC11 and the IDC are hand-soldered.
"""
import boardlib as bl

FRONT = 97.5                            # strip front edge (was 99)
path, b = bl.load("pcb-a")
P = bl.panel()
bl.jlc_rules(b)

for i in range(8):
    x, y = float(P[f"E{i+1}"]["x_mm"]), float(P[f"E{i+1}"]["y_mm"])
    bl.place_center(b, f"RE{i+1}", x, y, rot=0, pads=["MP"])
    ax, ay = bl.pad_panel(b, f"RE{i+1}", "A")
    bx, by = bl.pad_panel(b, f"RE{i+1}", "B")
    side = 1 if ax < x else -1          # caps go between the A/B column and the shaft
    bl.place(b, f"C{10+2*i}", ax + 3.0 * side, ay, rot=0, back=True)
    bl.place(b, f"C{11+2*i}", bx + 3.0 * side, by, rot=0, back=True)

# --- tab: x 187-227, y 50-99
bl.place_center(b, "J1", 217.0, 75.0, rot=0, back=True)
# U1 (SSOP-28) in the front band between E4 and E5: the 8 switch lines stay short (GPB0-3 fan out
# left to ENC1-4, GPA0-3 right to ENC5-8); only I2C, INT_A and +3V3 run to the tab. The SOIC on the
# tab put all 8 switch lines through the strip/tab junction and the board didn't route. Between the
# encoders' pin columns there are only 9.5 mm, so the SSOP sits in front of their pads (y < 107.5),
# which needs the strip's front edge at 97.5 (M4 PCB ends at y 97, and it sits higher anyway).
bl.place(b, "U1", 125.75, 103.1, rot=0, back=True)
bl.place(b, "C1", 125.75, 110.1, rot=0, back=True)
bl.place(b, "C2", 203.5, 88.0, rot=90, back=True)
bl.place(b, "R1", 201.0, 72.0, rot=90, back=True)
bl.place(b, "R2", 203.5, 72.0, rot=90, back=True)
for ref, x in (("J2", 194.0), ("J3", 205.5)):
    for rot in (0, 180):
        bl.place(b, ref, x, 50.0 + 3.3, rot=rot, back=True)
        if bl.jst_opening(b, ref)[1] < -0.9:
            break
    assert bl.jst_opening(b, ref)[1] < -0.9, f"{ref} must open to the front"
# --- J4 to PCB-K1 on the tab's left edge, next to J1, opening towards the M4 (-x). At the strip's
# left end its six lines (+5V 0.5 mm, +3V3, SDA, SCL, LED_DIN, INT_K) ran 200 mm along the strip
# from J1 and left no lane for the leftmost encoder's A/B; the cable to PCB-K1 is ~20-25 cm instead.
for rot in (90, 270):
    bl.place(b, "J4", 187.0 + 3.3, 80.0, rot=rot, back=True)
    if bl.jst_opening(b, "J4")[0] < -0.9:
        break
assert bl.jst_opening(b, "J4")[0] < -0.9, "J4 must open towards -x"

# H4 lives on the tab: between E6 and E7 it sat in the junction every J1 → encoder line goes through.
for ref, (x, y) in {"H1": (18.0, 114.0), "H2": (74.0, 111.0), "H3": (100.0, 111.0), "H4": (194.0, 64.0)}.items():
    bl.place(b, ref, x, y)

# Audit D06: C1 as close as it gets to U1's VDD (pin 9) / VSS (pin 10), after everything else is placed.
print("C1 decoupling distance to U1.9/U1.10: %.2f mm (sum of both)" % bl.place_decap(b, "C1", "U1"))

bl.hide_refs(b, [f"C{n}" for n in range(10, 26)] + ["C1", "C2", "R1", "R2", "U1", "J1", "J2", "J3", "J4",
                                                    "H1", "H2", "H3", "H4"])
bl.outline_poly(b, [(10, FRONT), (187, FRONT), (187, 50), (227, 50), (227, 119.6), (10, 119.6)])
for r in ("RE1", "RE8"):
    print(r, "A", bl.pad_panel(b, r, "A"), "S1", bl.pad_panel(b, r, "S1"))
print("J1 pin1", bl.pad_panel(b, "J1", "1"), "pin30", bl.pad_panel(b, "J1", "30"))
bl.save(b, path)
print("PCB-A placed")

"""Place PCB-S, the shield adapter that plugs into the Brain carrier's empty Audio Shield sockets.

GroovePush has no Audio Shield, and its sockets J15/J16 are the only place the carrier brings out
I2C (Teensy 18/19) and Serial5 (20/21). PCB-S re-wires them to two keyed connectors: J1 (JST-SH 4,
Qwiic order) to PCB-A J2 for the MCP23017s, J2 (JST-PH 4) to the NeoTrellis M4's CONN1 (UART).
No other parts: the I2C pull-ups are on PCB-A, and the M4's own 3.3 V (CONN1 pin 2) stays unconnected.

Unlike the panel boards this one uses the carrier's coordinates (portadora.kicad_pcb), so every
header pin sits exactly over its socket pin: J15 row at y 99.6 and J16 at y 84.36, pin 1 at x 106.5,
2.54 mm pitch towards +x. The male headers go on the bottom (B side, long pins down into the sockets);
the two vertical JSTs on top, between the rows, so the cables leave upwards. The outline is the
Audio Shield's footprint: J9 (x >= 141.7), the Teensy sockets (y >= 108) and the audio jacks
(y <= 62.8) stay clear.
"""
import pcbnew

import boardlib as bl

J15_Y, J16_Y, PIN1_X, PITCH = 99.6, 84.36, 106.5, 2.54
X0, Y0, X1, Y1 = 104.3, 82.4, 141.2, 101.6

path, b = bl.load("pcb-s")
bl.jlc_rules(b)


def at(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def pad_xy(f, num):
    p = next(p for p in f.Pads() if p.GetNumber() == num)
    return pcbnew.ToMM(p.GetPosition())


def place_header(ref, row_y):
    """Pin 1 over socket pin 1, pin 14 over socket pin 14, body on the bottom."""
    f = b.FindFootprintByReference(ref)
    if not f.IsFlipped():
        f.Flip(f.GetPosition(), pcbnew.FLIP_DIRECTION_TOP_BOTTOM)
    for rot in (0, 90, 180, 270):
        f.SetOrientationDegrees(rot)
        f.SetPosition(at(0, 0))
        (x1, y1), (x14, y14) = pad_xy(f, "1"), pad_xy(f, "14")
        f.SetPosition(at(PIN1_X - x1, row_y - y1))
        (x1, y1), (x14, y14) = pad_xy(f, "1"), pad_xy(f, "14")
        if abs(x14 - (PIN1_X + 13 * PITCH)) < 0.01 and abs(y14 - row_y) < 0.01:
            return
    raise SystemExit(f"{ref}: no orientation puts pin 14 at the socket's pin 14")


def place_top(ref, x, y, rot):
    f = b.FindFootprintByReference(ref)
    if f.IsFlipped():
        f.Flip(f.GetPosition(), pcbnew.FLIP_DIRECTION_TOP_BOTTOM)
    f.SetOrientationDegrees(rot)
    f.SetPosition(at(x, y))


place_header("P1", J15_Y)          # J15: SDA .6, SCL .7, TX5 .8, RX5 .9, +3V3 .12, GND .13
place_header("P2", J16_Y)          # J16: GND .14
place_top("J1", 114.0, 92.0, 0)    # JST-SH 4 vertical, under SDA/SCL
place_top("J2", 129.5, 92.0, 0)    # JST-PH 4 vertical, under TX5/RX5

# Verify the mapping against the carrier, pin by pin.
for ref, row in (("P1", J15_Y), ("P2", J16_Y)):
    f = b.FindFootprintByReference(ref)
    for n in range(1, 15):
        x, y = pad_xy(f, str(n))
        assert abs(x - (PIN1_X + (n - 1) * PITCH)) < 0.01 and abs(y - row) < 0.01, (ref, n, x, y)

for d in list(b.GetDrawings()):
    if d.GetLayer() == pcbnew.Edge_Cuts or (isinstance(d, pcbnew.PCB_TEXT) and d.GetLayer() == pcbnew.F_SilkS):
        b.Delete(d)
r = 1.0
corners = [(X0 + r, Y0), (X1 - r, Y0), (X1, Y0 + r), (X1, Y1 - r), (X1 - r, Y1), (X0 + r, Y1), (X0, Y1 - r), (X0, Y0 + r)]
for (ax, ay), (cx, cy) in zip(corners[::2], corners[1::2]):
    s = pcbnew.PCB_SHAPE(b)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(at(ax, ay)); s.SetEnd(at(cx, cy))
    s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(pcbnew.FromMM(0.1))
    b.Add(s)
for (cx, cy), (sx, sy), (ex, ey) in (((X1 - r, Y0 + r), (X1 - r, Y0), (X1, Y0 + r)),
                                     ((X1 - r, Y1 - r), (X1, Y1 - r), (X1 - r, Y1)),
                                     ((X0 + r, Y1 - r), (X0 + r, Y1), (X0, Y1 - r)),
                                     ((X0 + r, Y0 + r), (X0, Y0 + r), (X0 + r, Y0))):
    a = pcbnew.PCB_SHAPE(b)
    a.SetShape(pcbnew.SHAPE_T_ARC)
    a.SetCenter(at(cx, cy)); a.SetStart(at(sx, sy)); a.SetEnd(at(ex, ey))
    a.SetLayer(pcbnew.Edge_Cuts); a.SetWidth(pcbnew.FromMM(0.1))
    b.Add(a)


def label(text, x, y, size=1.0):
    t = pcbnew.PCB_TEXT(b)
    t.SetText(text)
    t.SetPosition(at(x, y))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(size), pcbnew.FromMM(size)))
    t.SetTextThickness(pcbnew.FromMM(0.15))
    b.Add(t)


label("GroovePush PCB-S  shield adapter", 122.75, 87.2, 0.9)
label("I2C > PCB-A J2", 114.0, 96.2, 0.8)
label("UART > M4", 129.5, 96.2, 0.8)
# The rows are symmetric, so the board also fits turned 180° (harmless, but nothing works): name each
# row after the carrier socket it belongs on.
label("J15", PIN1_X + 0.4, J15_Y - 2.3, 0.8)
label("J16", PIN1_X + 0.4, J16_Y + 2.3, 0.8)

bl.hide_refs(b, ["P1", "P2"])     # they'd sit on the edge; the silk labels say what each JST is
bl.save(b, path)
print("PCB-S placed; headers match carrier J15/J16 pin by pin")

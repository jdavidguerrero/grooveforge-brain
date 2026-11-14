#!/usr/bin/env python3
"""Build PCB-M (186 × 80) from its exported netlist: outline, M3 holes, edge connectors and purpose islands.

Run with the KiCad bundled Python (pcbnew). Placement only: no tracks or zones (route_m does that).
Coordinates are KiCad mm; panel = (x + 44, 371 − y). Rear edge (jacks) is y = 35, front edge y = 115.
Islands follow doc 24 D44/D45 (Juan, 2026-11-13): Teensy + Audio Shield on collinear rails, analog on the left near
the LINE jacks, every function in its own framed block.
"""
from pathlib import Path
import sys, csv, json, math, xml.etree.ElementTree as ET
sys.path.append('/Applications/KiCad/KiCad.app/Contents/SharedSupport/scripting')
import pcbnew as p

ROOT = Path(__file__).resolve().parents[6]
CON = ROOT / 'apps/hardware/kicad/consola'
STD = Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints')
FOLDER = CON / 'boards/pcb-m'
X0, Y0, X1, Y1 = 96.0, 35.0, 282.0, 115.0
MARGIN = 0.35            # courtyard to courtyard
EDGE = 0.8               # courtyard to board edge


def v(x, y):
    return p.VECTOR2I(round(x * 1e6), round(y * 1e6))


# Teensy 4.1 rails (24 pins) and Audio Shield Rev D rails (14 pins) on the same two rows, 15,24 mm apart.
ROW_R, ROW_L = 65.0, 80.24
TEENSY_X = 110.0
SHIELD_X = TEENSY_X + 23 * 2.54 + 5.08          # 2 mm between module bodies
# Module bodies seen from above: low SMD only under them; nothing placed by the packer.
KEEPOUTS = [
    (TEENSY_X - 3.0, ROW_R - 2.0, TEENSY_X + 23 * 2.54 + 1.5, ROW_L + 2.0),   # Teensy 4.1 + USB plug side
    (96.0, 62.0, TEENSY_X - 1.3, 83.0),                                         # micro-USB plug and cable
    (SHIELD_X - 1.5, ROW_R - 2.0, SHIELD_X + 13 * 2.54 + 1.5, ROW_L + 2.0),     # Audio Shield rails
]

# Rear wall jacks (pcb-outlines-console-v5.csv, panel x − 44): one JST-PH per jack, at the rear edge (D15).
REAR = {'J_LINE_IN_L1': 104, 'J_LINE_IN_R1': 128, 'J_LINE_OUT_L1': 152, 'J_LINE_OUT_R1': 176,
        'J_PEDAL1': 200, 'J_CV1': 221, 'J_CV2': 239, 'J_MIDI_IN1': 257, 'J_MIDI_OUT1': 275}
# Front cables to panel controls, left to right as on the panel.
FRONT = ['J_PAD_SEL1', 'J_POT_IN1', 'J7_EXP1', 'J8_EXP2', 'J_PCB_K1', 'J3_M4', 'J_FSR1', 'J_FSR2', 'J_FSR3',
         'J_FSR4', 'J2_I2C1', 'J_JOG1', 'J_POT_CUE1', 'J_POT_MASTER1', 'J_HP1']
FRONT_X0, FRONT_X1 = 104.0, 262.5          # between the front M3 holes and the PCB-A IDC
RIGHT = ['J5_SHARP1', 'J6_SHARP2']          # right edge, between H6 and H3
LEFT = ['J_5V_LED_IN1']       # left edge (the Pi is on that side), below H1
IDC_X = 268.5                               # J1_PCBA1 standing along the right side

# Islands: (x0, y0, x1, y1). The rear connector row ends at y 41.4, the front row starts at y 108.6.
ISLANDS = {
    'POWER A': (97.0, 42.5, 114.0, 55.0),
    'AUDIO IN': (114.0, 42.5, 171.0, 62.0),
    'SHIELD': (171.0, 42.5, 197.0, 55.4),
    'SENSORS': (197.0, 42.5, 213.0, 62.0),
    'CV OUT': (213.0, 42.5, 248.0, 62.0),
    'MIDI': (248.0, 42.5, 281.0, 59.3),
    'ADS': (210.0, 62.5, 236.0, 84.0),
    'I2C A': (236.0, 62.5, 262.5, 84.0),
    'LEDS': (106.5, 83.0, 126.0, 108.0),
    'I2C K': (126.0, 83.0, 148.0, 108.0),
    'E-DRUM': (148.0, 83.0, 172.0, 108.0),
    'FRONT RC': (172.0, 83.0, 210.0, 108.0),
    'DIAG': (210.0, 84.0, 262.5, 108.0),
}


def island_of(ref, sheet):
    if ref.startswith(('U_LDO', 'C_LDO', 'NT', 'TP_AGND', 'TP_3V3A', 'D_IND_3V3A', 'R_IND_3V3A')):
        return 'POWER A'
    if ref.startswith('J_EDRUM'):
        return 'E-DRUM'
    if ref.startswith('J_SHIELD_'):
        return 'SHIELD'
    if sheet == '02-analog-front':
        return 'AUDIO IN'
    if ref.startswith(('J_5V_BKUP', 'F1', 'Q_LED', 'D_LED_TVS', 'U_LED_BUF', 'C_LED_BUF', 'R_LED_BIAS', 'SJ_LED', 'TP_LED',
                       'TP_5V_LED', 'D_IND_5V_LED', 'R_IND_5V_LED', 'TP_5V_RAW', 'SJ_FRONT', 'TP_FRONT')):
        return 'LEDS'
    if ref.startswith(('R_TRIG', 'D_TRIG', 'C_TRIG')):
        return 'E-DRUM'
    if sheet == '05-cv-dac' or ref.startswith(('TP_CV', 'SJ_CV', 'TP_5V_CV')):
        return 'CV OUT'
    if sheet == '07-midi' or ref.startswith(('R_OPT_MIDI', 'TP_MIDI')):
        return 'MIDI'
    if sheet == '03-adc' or ref.startswith(('C_SHARP', 'R_SHARP', 'SJ_SHARP', 'R_EXP')):
        return 'ADS'
    if ref.startswith(('R_PEDAL', 'C_PEDAL', 'D_PEDAL')):
        return 'SENSORS'
    if sheet == '06-surface' or ref.startswith(('R_M4',)):
        return 'FRONT RC'
    if ref.endswith(('_K1', '_K_22', '_K_47')) or '_K_' in ref or ref.startswith(('U_I2C_K', 'R_EN_K', 'SJ_EN_K')):
        return 'I2C K'
    if sheet == '12-i2c-branches':
        return 'I2C A'
    if ref.startswith(('R_OPT_BIAS',)):
        return 'AUDIO IN'
    if ref.startswith(('R_SDA', 'R_SCL', 'SJ_PU_S', 'TP_SDA', 'TP_SCL')):
        return 'SHIELD'
    return 'DIAG'


class Board:
    def __init__(self):
        root = ET.parse(FOLDER / 'docs/pcb-m-netlist.xml').getroot()
        b = p.BOARD()
        b.SetCopperLayerCount(2)
        b.GetDesignSettings().SetBoardThickness(p.FromMM(1.6))
        b.GetTitleBlock().SetTitle('GrooveForge PCB-M — Teensy 4.1 + Audio Shield, 186 × 80')
        b.GetTitleBlock().SetRevision('layout-2')
        self.b, self.fps, self.sheet, self.rects = b, {}, {}, []
        pinmap = {}
        self.nets = {}
        for nn in root.findall('./nets/net'):
            name = nn.attrib['name']
            if name.startswith('unconnected-('):
                name = 'unconnected-(' + name[len('unconnected-('):].replace('/', '{slash}')
            n = p.NETINFO_ITEM(b, name, int(nn.attrib['code']))
            b.Add(n)
            self.nets[n.GetNetname()] = n
            for node in nn.findall('node'):
                pinmap[(node.attrib['ref'], node.attrib['pin'])] = n
        for c in root.findall('./components/comp'):
            ref = c.attrib['ref']
            lib, fp = c.findtext('footprint').split(':')
            libpath = CON / 'lib/gf_consola.pretty' if lib == 'gf_consola' else STD / (lib + '.pretty')
            f = p.FootprintLoad(str(libpath), fp)
            assert f, (ref, libpath, fp)
            f.SetReference(ref)
            f.SetValue(c.findtext('value'))
            f.SetFPID(p.LIB_ID(lib, fp))
            stamp, sheet = c.findtext('tstamps'), c.find('sheetpath')
            if stamp and sheet is not None:
                f.SetPath(p.KIID_PATH(sheet.attrib['tstamps'] + stamp))
            props = {x.get('name'): x.get('value') for x in c.findall('property')}
            if 'dnp' in props:
                f.SetDNP(True)
            f.SetExcludedFromBOM('exclude_from_bom' in props)
            for field in c.findall('./fields/field'):
                name = field.attrib['name']
                if name == 'Footprint':
                    continue
                f.SetField(name, field.text or '')
                f.GetField(name).SetVisible(False)
            b.Add(f)
            for pad in f.Pads():
                n = pinmap.get((ref, pad.GetNumber()))
                if n:
                    pad.SetNet(n)
            f.Value().SetVisible(False)
            f.Reference().SetTextSize(v(0.8, 0.8))
            f.Reference().SetTextThickness(p.FromMM(0.15))
            self.fps[ref] = f
            self.sheet[ref] = sheet.attrib['names'].strip('/') if sheet is not None else ''

    # ------------------------------------------------------------------ geometry helpers
    def crt(self, f):
        poly = f.GetCourtyard(p.F_CrtYd)
        bb = poly.BBox() if poly.OutlineCount() else f.GetBoundingBox(False, False)
        return (p.ToMM(bb.GetX()), p.ToMM(bb.GetY()), p.ToMM(bb.GetRight()), p.ToMM(bb.GetBottom()))

    def free(self, r, own=None):
        x0, y0, x1, y1 = r
        if x0 < X0 + EDGE or y0 < Y0 + EDGE or x1 > X1 - EDGE or y1 > Y1 - EDGE:
            return False
        for a in self.rects:
            if a[4] is not own and x0 < a[2] + MARGIN and x1 > a[0] - MARGIN and y0 < a[3] + MARGIN and y1 > a[1] - MARGIN:
                return False
        return True

    def fix(self, ref, x, y, angle=0.0):
        f = self.fps[ref]
        f.SetOrientationDegrees(angle)
        f.SetPosition(v(x, y))
        r = self.crt(f)
        assert self.free(r, f), (ref, x, y, r)
        self.rects.append((*r, f))
        return r

    def block(self, rect):
        self.rects.append((*rect, None))

    # ------------------------------------------------------------------ packing
    def target(self, ref):
        pts = []
        f = self.fps[ref]
        for pad in f.Pads():
            n = pad.GetNetname()
            if not n or n in ('GND', 'AGND', '+3.3V', '+3.3VA', '+5V_TEENSY'):
                continue
            for other in self.placed:
                if other == ref:
                    continue
                for q in self.fps[other].Pads():
                    if q.GetNetname() == n:
                        pts.append((p.ToMM(q.GetPosition().x), p.ToMM(q.GetPosition().y)))
        if not pts:
            return None
        return sum(x for x, _ in pts) / len(pts), sum(y for _, y in pts) / len(pts)

    def pack(self, ref, rect):
        f = self.fps[ref]
        x0, y0, x1, y1 = rect
        t = self.target(ref) or ((x0 + x1) / 2, (y0 + y1) / 2)
        tx, ty = min(max(t[0], x0), x1), min(max(t[1], y0), y1)
        best = None
        step = 0.25
        for angle in (0.0, 90.0):
            f.SetOrientationDegrees(angle)
            f.SetPosition(v(0, 0))
            cx0, cy0, cx1, cy1 = self.crt(f)
            nx = int((x1 - x0) / step) + 1
            ny = int((y1 - y0) / step) + 1
            for i in range(nx):
                x = math.ceil((x0 - cx0) / step) * step + i * step
                if x + cx1 > x1:
                    break
                for j in range(ny):
                    y = math.ceil((y0 - cy0) / step) * step + j * step
                    if y + cy1 > y1:
                        break
                    r = (x + cx0, y + cy0, x + cx1, y + cy1)
                    d = (x - tx) ** 2 + (y - ty) ** 2 + (0.5 if angle else 0)
                    if best and d >= best[0]:
                        continue
                    if self.free(r):
                        best = (d, x, y, angle)
        assert best, ('island full', ref, rect)
        _, x, y, angle = best
        self.fix(ref, x, y, angle)
        self.placed.append(ref)


def outline(b):
    pts = [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)]
    for a, z in zip(pts, pts[1:] + pts[:1]):
        e = p.PCB_SHAPE(b)
        e.SetShape(p.SHAPE_T_SEGMENT)
        e.SetLayer(p.Edge_Cuts)
        e.SetStart(v(*a))
        e.SetEnd(v(*z))
        e.SetWidth(p.FromMM(0.05))
        b.Add(e)


def holes(bd):
    rows = list(csv.DictReader((CON / 'panel/pcb-outlines-console-v5.csv').open()))
    lib = STD / 'MountingHole.pretty'
    out = []
    for row in rows:
        if row['board'] != 'PCB-M' or row['item'] != 'agujero':
            continue
        x, y = float(row['x_mm']) - 44, 371 - float(row['y_mm'])
        f = p.FootprintLoad(str(lib), 'MountingHole_3.2mm_M3_Pad')
        f.SetReference(f'H{len(out) + 1}')
        f.SetValue('M3 to floor tower')
        f.SetFPID(p.LIB_ID('MountingHole', 'MountingHole_3.2mm_M3_Pad'))
        f.SetPosition(v(x, y))
        f.SetExcludedFromBOM(True)
        f.SetExcludedFromPosFiles(True)
        f.SetBoardOnly(True)
        bd.b.Add(f)
        for pad in f.Pads():
            pad.SetNet(bd.nets['GND'])
        f.Reference().SetVisible(False)
        f.Value().SetVisible(False)
        bd.fps[f.GetReference()] = f
        # Screw head Ø5.5 + washer clearance.
        bd.block((x - 3.3, y - 3.3, x + 3.3, y + 3.3))
        out.append((x, y))
    return out


def frame(b, label, rect):
    x0, y0, x1, y1 = rect
    s = p.PCB_SHAPE(b)
    s.SetShape(p.SHAPE_T_RECT)
    s.SetLayer(p.F_SilkS)
    s.SetStart(v(x0, y0))
    s.SetEnd(v(x1, y1))
    s.SetWidth(p.FromMM(0.15))
    b.Add(s)
    t = p.PCB_TEXT(b)
    t.SetText(label)
    t.SetLayer(p.F_SilkS)
    t.SetTextSize(v(1.0, 1.0))
    t.SetTextThickness(p.FromMM(0.15))
    t.SetHorizJustify(p.GR_TEXT_H_ALIGN_LEFT)
    t.SetPosition(v(x0 + 0.6, y0 - 0.9))
    b.Add(t)


def build():
    bd = Board()
    b = bd.b
    outline(b)
    hole_list = holes(bd)
    for r in KEEPOUTS:
        bd.block(r)
    # Teensy and shield rails: pins along +x (rotation 90 puts the 1×N socket pads along +x from pin 1).
    for ref, x, y, n in (('J_T41L1', TEENSY_X, ROW_L, 24), ('J_T41R1', TEENSY_X, ROW_R, 24),
                         ('J_SHD_L1', SHIELD_X, ROW_L, 14), ('J_SHD_R1', SHIELD_X, ROW_R, 14)):
        f = bd.fps[ref]
        f.SetOrientationDegrees(90)
        f.SetPosition(v(x, y))
        pad1 = [q for q in f.Pads() if q.GetNumber() == '1'][0]
        padn = [q for q in f.Pads() if q.GetNumber() == str(n)][0]
        assert abs(p.ToMM(padn.GetPosition().x) - (x + (n - 1) * 2.54)) < 0.01, (ref, p.ToMM(padn.GetPosition().x))
        assert abs(p.ToMM(pad1.GetPosition().y) - y) < 0.01
    # Rear edge connectors at their jacks.
    for ref, x in REAR.items():
        f = bd.fps[ref]
        f.SetOrientationDegrees(180)
        f.SetPosition(v(0, 0))
        cx0, cy0, cx1, cy1 = bd.crt(f)
        bd.fix(ref, x - (cx0 + cx1) / 2, Y0 + EDGE - cy0, 180)
    # Front edge connectors, spread evenly left to right between the front hole and the IDC.
    widths = []
    for ref in FRONT:
        f = bd.fps[ref]
        f.SetOrientationDegrees(0)
        f.SetPosition(v(0, 0))
        widths.append(bd.crt(f))
    gap = (FRONT_X1 - FRONT_X0 - sum(c[2] - c[0] for c in widths)) / (len(FRONT) - 1)
    assert gap >= MARGIN + 0.1, ('front row too long', gap)
    x = FRONT_X0
    for ref, (cx0, cy0, cx1, cy1) in zip(FRONT, widths):
        bd.fix(ref, x - cx0, Y1 - EDGE - cy1, 0)
        x += (cx1 - cx0) + gap
    # PCB-A IDC standing along the right side, pin 1 toward the rear.
    f = bd.fps['J1_PCBA1']
    f.SetOrientationDegrees(0)
    f.SetPosition(v(0, 0))
    cx0, cy0, cx1, cy1 = bd.crt(f)
    bd.fix('J1_PCBA1', IDC_X - (cx0 + cx1) / 2, Y1 - EDGE - cy1, 0)
    # Sharp sensors on the right edge between H6 and H3; 5 V LED input on the left edge below H1.
    for refs, y, side in ((RIGHT, 67.0, 'right'), (LEFT, 95.0, 'left')):
        for ref in refs:
            f = bd.fps[ref]
            f.SetOrientationDegrees(90 if side == 'right' else 270)
            f.SetPosition(v(0, 0))
            cx0, cy0, cx1, cy1 = bd.crt(f)
            x = X1 - EDGE - cx1 if side == 'right' else X0 + EDGE - cx0
            bd.fix(ref, x, y - cy0, f.GetOrientationDegrees())
            y += (cy1 - cy0) + 1.0
    fixed = (set(REAR) | set(FRONT) | set(RIGHT) | set(LEFT) |
             {'J_T41L1', 'J_T41R1', 'J_SHD_L1', 'J_SHD_R1', 'J1_PCBA1'})
    # Shield harness headers next to the shield pads (D28), above the shield rails.
    bd.placed = list(fixed)
    groups = {}
    for ref in bd.fps:
        if ref in fixed or ref.startswith('H') and ref[1:].isdigit():
            continue
        groups.setdefault(island_of(ref, bd.sheet[ref]), []).append(ref)
    order = lambda r: (0 if r.startswith(('U', 'J', 'SW', 'Q', 'F')) else 1,
                       -len(list(bd.fps[r].Pads())), r)
    for name, rect in ISLANDS.items():
        for ref in sorted(groups.get(name, []), key=order):
            bd.pack(ref, rect)
        frame(b, name, rect)
    left = [r for g, refs in groups.items() if g not in ISLANDS for r in refs]
    assert not left, left
    b.SetFileName(str(FOLDER / 'pcb-m.kicad_pcb'))
    p.SaveBoard(b.GetFileName(), b)
    placements = {r: (round(p.ToMM(f.GetPosition().x), 3), round(p.ToMM(f.GetPosition().y), 3),
                      f.GetOrientationDegrees()) for r, f in bd.fps.items()}
    (FOLDER / 'placements-layout.json').write_text(json.dumps(placements, indent=1, sort_keys=True) + '\n')
    print('placed', len(bd.fps), 'footprints,', len(hole_list), 'holes', flush=True)


if __name__ == '__main__':
    build()

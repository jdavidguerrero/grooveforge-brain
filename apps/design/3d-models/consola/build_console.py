"""GrooveForge Console v1 — modelo paramétrico en FreeCAD desde el CSV de coordenadas.

Uso (consola de Python de FreeCAD o MCP):
    exec(open('<repo>/apps/design/3d-models/consola/build_console.py').read())
    build(repo='<repo>')

Coordenadas: X desde el borde izquierdo, Y desde el frente, Z hacia arriba desde la base del cuerpo (sin patas).
Fuente: apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv y el diseño v5 de Claude Design
(apps/docs/consola/design/claude-design-cuerpo-v5/) con sus contornos de placas, torres y jacks
(apps/hardware/kicad/consola/panel/pcb-outlines-console-v5.csv).
"""
import csv
import os

import FreeCAD as App
import Part

W = D = 340.0          # panel y cuerpo en planta
BODY_H = 83.0          # cuerpo impreso (v5): la chapa va encima
PANEL_T = 2.0
CORNER_R = 5.0
WALL = 3.2
FLOOR = 3.0
TOP = BODY_H + PANEL_T  # cara superior del aluminio

# Cortes CNC (v5 §1)
CHOC_CUT, CHOC_POCKET, CHOC_POCKET_D = 14.0, 16.0, 0.8
ROUND_HOLE = 7.1
PAD_CUT, PAD_R = 11.0, 1.0
FADER_SLOT, FADER_R = (3.0, 63.0), 1.5
FSR_CUT, FSR_R = 25.6, 3.0
SHARP_CUT, SHARP_R = (31.0, 14.5), 1.0
SEL_CUT, SEL_R = (10.0, 4.0), 1.0
WINDOW, WINDOW_R = (158.0, 91.0), 1.0


def rounded_rect(cx, cy, w, h, r, z0, height):
    """Prisma de planta rectangular con esquinas verticales redondeadas."""
    box = Part.makeBox(w, h, height, App.Vector(cx - w / 2, cy - h / 2, z0))
    if r <= 0:
        return box
    r = min(r, w / 2 - 0.01, h / 2 - 0.01)
    edges = [e for e in box.Edges if abs(e.Vertexes[0].Point.z - e.Vertexes[1].Point.z) > 1e-6]
    return box.makeFillet(r, edges)


def cylinder(cx, cy, d, z0, height):
    return Part.makeCylinder(d / 2, height, App.Vector(cx, cy, z0))


def load_csv(repo):
    path = os.path.join(repo, 'apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv')
    with open(path, newline='') as f:
        return list(csv.DictReader(f))


def panel_cuts(rows):
    """Sólidos a restar de la chapa: pasantes + rebajes de Choc por debajo."""
    z0, h = BODY_H - 1, PANEL_T + 2
    cuts = []
    for r in rows:
        k, x, y = r['kind'], float(r['x_mm']), float(r['y_mm'])
        if k == 'key':
            cuts.append(rounded_rect(x, y, CHOC_CUT, CHOC_CUT, 0, z0, h))
            cuts.append(rounded_rect(x, y, CHOC_POCKET, CHOC_POCKET, 0, BODY_H - 1, 1 + CHOC_POCKET_D))
        elif k in ('encoder', 'pot'):
            cuts.append(cylinder(x, y, ROUND_HOLE, z0, h))
        elif k == 'pad':
            cuts.append(rounded_rect(x, y, PAD_CUT, PAD_CUT, PAD_R, z0, h))
        elif k == 'fader':
            cuts.append(rounded_rect(x, y, FADER_SLOT[0], FADER_SLOT[1], FADER_R, z0, h))
        elif k == 'fsr':
            cuts.append(rounded_rect(x, y, FSR_CUT, FSR_CUT, FSR_R, z0, h))
        elif k == 'sensor':
            cuts.append(rounded_rect(x, y, SHARP_CUT[0], SHARP_CUT[1], SHARP_R, z0, h))
        elif k == 'switch':
            cuts.append(rounded_rect(x, y, SEL_CUT[0], SEL_CUT[1], SEL_R, z0, h))
        elif k == 'outline' and r['ref'] == 'MOD':
            cuts.append(rounded_rect(x, y, WINDOW[0], WINDOW[1], WINDOW_R, z0, h))
    return cuts


def make_panel(rows):
    plate = rounded_rect(W / 2, D / 2, W, D, CORNER_R, BODY_H, PANEL_T)
    return plate.cut(Part.makeCompound(panel_cuts(rows)))


def make_body():
    """Cuerpo macizo de una pieza con bahía de la Pi (v5 §3); torres y jacks en body_details()."""
    outer = rounded_rect(W / 2, D / 2, W, D, CORNER_R, 0, BODY_H)
    inner = rounded_rect(W / 2, D / 2, W - 2 * WALL, D - 2 * WALL, CORNER_R - WALL / 2, FLOOR, BODY_H)
    body = outer.cut(inner)
    wall_top = 62.0
    body = body.fuse(Part.makeBox(2, D - WALL - 211, wall_top, App.Vector(136, 211, 0)))   # tabique x 136–138
    body = body.fuse(Part.makeBox(136 - WALL, 2, wall_top, App.Vector(WALL, 211, 0)))     # tabique y 211–213
    body = body.cut(Part.makeBox(94, 68, FLOOR + 2, App.Vector(5, 213, -1)))             # tapa de la bahía (aire)
    return body.removeSplitter()


PCB_T = 1.6
# Cara superior de cada placa, bajo la cara superior del aluminio (v5 §2) o desde la base (PCB-M)
PCB_TOP = {'PCB-K': TOP - 3.4, 'PCB-P': TOP - 6.5, 'PCB-F': TOP - 9.5, 'PCB-M': 8.0 + PCB_T}
TOWER_TOP = BODY_H - 10.0   # separador PEM de 10 que cuelga de la chapa
JACK_AXIS_Z = 8.0 + PCB_T + 7.0
JACK_HOLE = {'1/4': 9.5, '3.5': 6.2}


def load_outlines(repo):
    path = os.path.join(repo, 'apps/hardware/kicad/consola/panel/pcb-outlines-console-v5.csv')
    with open(path, newline='') as f:
        return list(csv.DictReader(f))


def board_face(rows, board):
    """Cara plana (z = 0) de una placa: contorno menos ventanas y agujeros. Sirve para el sólido y para el DXF."""
    mine = [r for r in rows if r['board'] == board]
    verts = [(float(r['x_mm']), float(r['y_mm'])) for r in mine if r['item'].startswith('vértice')]
    if verts:
        pts = [App.Vector(x, y, 0) for x, y in verts] + [App.Vector(*verts[0], 0)]
        face = Part.Face(Part.makePolygon(pts))
    else:
        r = next(r for r in mine if r['item'] == 'contorno')
        x, y, w, h = (float(r[k]) for k in ('x_mm', 'y_mm', 'w_mm', 'h_mm'))
        face = Part.makePlane(w, h, App.Vector(x - w / 2, y - h / 2, 0))
    holes = []
    for r in mine:
        x, y = float(r['x_mm']), float(r['y_mm'])
        if r['item'] == 'agujero' or (r['item'] == 'ventana' and r['notes'].startswith('redonda')):
            holes.append(Part.Face(Part.Wire(Part.makeCircle(float(r['w_mm']) / 2, App.Vector(x, y, 0)))))
        elif r['item'] == 'ventana':
            w, h = float(r['w_mm']), float(r['h_mm'])
            holes.append(Part.makePlane(w, h, App.Vector(x - w / 2, y - h / 2, 0)))
    for hole in holes:
        face = face.cut(hole)
    return face


def make_boards(rows):
    solids = {}
    for board, top in PCB_TOP.items():
        face = board_face(rows, board)
        solid = face.extrude(App.Vector(0, 0, PCB_T))
        solid.translate(App.Vector(0, 0, top - PCB_T))
        solids[board.replace('-', '_')] = solid
    return solids


def body_details(rows, body):
    """Torres de la chapa, torres de PCB-M y agujeros de los jacks traseros (pcb-outlines-console-v5.csv)."""
    adds, cuts = [], []
    for r in rows:
        x, y = float(r['x_mm']), float(r['y_mm'])
        if r['board'] == 'CUERPO' and r['item'].startswith('pozo'):
            adds.append(cylinder(x, y, 10.0, 0, TOWER_TOP))
            cuts.append(cylinder(x, y, 6.5, -1, TOWER_TOP - 6))          # pozo para el destornillador
            cuts.append(cylinder(x, y, 3.4, -1, TOWER_TOP + 2))          # paso del M3
        elif r['board'] == 'PCB-M' and r['item'] == 'agujero':
            adds.append(cylinder(x, y, 7.0, 0, 8.0))                     # torre de 5 sobre el piso de 3
            cuts.append(cylinder(x, y, 4.0, FLOOR, 6.0))                 # inserto M3 Ø4 × 6
        elif r['board'] == 'PCB-M' and r['item'] == 'jack':
            kind = '1/4' if any(k in r['notes'] for k in ('LINE', 'PEDAL')) else '3.5'
            hole = Part.makeCylinder(JACK_HOLE[kind] / 2, WALL + 2, App.Vector(x, D - WALL - 1, JACK_AXIS_Z),
                                     App.Vector(0, 1, 0))
            cuts.append(hole)
    body = body.fuse(Part.makeCompound(adds))
    return body.cut(Part.makeCompound(cuts)).removeSplitter()


def export_board_dxf(rows, folder):
    """Contorno + agujeros de cada placa en z = 0 para importar como Edge.Cuts en KiCad (coordenadas del panel)."""
    import importDXF
    doc = App.ActiveDocument
    for board in PCB_TOP:
        obj = doc.addObject('Part::Feature', 'tmp_' + board.replace('-', '_'))
        obj.Shape = Part.makeCompound(board_face(rows, board).Edges)
        importDXF.export([obj], os.path.join(folder, board.lower() + '-contorno-v5.dxf'))
        doc.removeObject(obj.Name)


def make_placeholders():
    """Volúmenes de referencia (no se fabrican): apilado de la Pi, módulo de pantalla y PCB-A."""
    return {
        'Pi_stack': Part.makeBox(85, 56, 60, App.Vector(9, 219, 6)),
        'Display_module': rounded_rect(170, 268, 189.32, 120.24, 8, BODY_H - 14.8, 14.8),
        'PCB_A': Part.makeBox(200, 30, PCB_T, App.Vector(70, 177, TOP - 13.6)),
    }


def build(repo, doc_name='GrooveForgeConsole'):
    rows = load_csv(repo)
    outlines = load_outlines(repo)
    doc = App.newDocument(doc_name)
    parts = {'Panel_aluminio': make_panel(rows), 'Cuerpo_PETG': body_details(outlines, make_body())}
    parts.update(make_boards(outlines))
    parts.update(make_placeholders())
    colors = {'Panel_aluminio': (0.15, 0.15, 0.17), 'Cuerpo_PETG': (0.08, 0.08, 0.08)}
    for name, shape in parts.items():
        obj = doc.addObject('Part::Feature', name)
        obj.Shape = shape
        if App.GuiUp:
            obj.ViewObject.ShapeColor = colors.get(name, (0.36, 0.29, 0.72))
            if name not in colors:
                obj.ViewObject.Transparency = 60
    doc.recompute()
    return doc

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


# Variante láser (2026-11-12): sin rebajes de Choc (piden fresado por la otra cara); corte pasante que deja pasar
# la carcasa superior de la tecla, que queda sostenida solo por PCB-K. Ajustar con el cupón de prueba.
CHOC_CUT_LASER = 15.4
# Agujeros de montaje de los PEM (hoja PEM: FH-M2.5 → 2,5; BSO-M2.5 / BSO-M3 → 4,2). Se prensan a mano en tornillo de
# banco con un yunque plano; verificar diámetros con la hoja del proveedor antes de cortar.
PEM_HOLE = {'PCB-K': 2.5, 'PCB-P': 4.2, 'PCB-F': 4.2, 'PCB-A': 4.2, 'CUERPO': 4.2}


def pem_holes(outlines):
    """Agujeros de la chapa para los espárragos y separadores PEM de las placas y de las torres del cuerpo."""
    holes = []
    for r in outlines:
        if r['board'] in PEM_HOLE and (r['item'] == 'agujero' or r['item'].startswith('pozo')):
            holes.append((float(r['x_mm']), float(r['y_mm']), PEM_HOLE[r['board']]))
    return holes


def panel_cuts(rows, outlines=(), laser=True):
    """Sólidos a restar de la chapa. laser=True: solo pasantes (corte 2D); False: CNC con rebajes de Choc por debajo."""
    z0, h = BODY_H - 1, PANEL_T + 2
    cuts = [cylinder(x, y, d, z0, h) for x, y, d in pem_holes(outlines)]
    for r in rows:
        k, x, y = r['kind'], float(r['x_mm']), float(r['y_mm'])
        if k == 'key' and laser:
            cuts.append(rounded_rect(x, y, CHOC_CUT_LASER, CHOC_CUT_LASER, 0.5, z0, h))
        elif k == 'key':
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


def make_panel(rows, outlines=(), laser=True):
    plate = rounded_rect(W / 2, D / 2, W, D, CORNER_R, BODY_H, PANEL_T)
    return plate.cut(Part.makeCompound(panel_cuts(rows, outlines, laser)))


def make_coupon():
    """Cupón de prueba de 120 × 60 para el taller láser: tecla en 5 medidas, EC11, fader, PEM. Cortar antes del panel."""
    plate = rounded_rect(60, 30, 120, 60, 3, 0, PANEL_T)
    cuts = [rounded_rect(12 + 22 * i, 44, s, s, 0.5, -1, PANEL_T + 2) for i, s in enumerate((14.0, 14.6, 15.0, 15.4, 16.0))]
    cuts += [cylinder(12, 18, ROUND_HOLE, -1, PANEL_T + 2), cylinder(30, 18, 7.4, -1, PANEL_T + 2)]
    cuts += [cylinder(48 + 9 * i, 18, d, -1, PANEL_T + 2) for i, d in enumerate((2.5, 2.6, 4.2, 4.3))]
    cuts.append(rounded_rect(100, 18, FADER_SLOT[0], 24, FADER_R, -1, PANEL_T + 2))
    return plate.cut(Part.makeCompound(cuts))


def export_profile(shape, z, path):
    """Perfil 2D de una chapa (corte a media altura) a DXF para el láser."""
    import importDXF
    doc = App.ActiveDocument
    obj = doc.addObject('Part::Feature', 'tmp_perfil')
    obj.Shape = Part.makeCompound(shape.slice(App.Vector(0, 0, 1), z))
    importDXF.export([obj], path)
    doc.removeObject(obj.Name)


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
# MIDI por hardware sin poblar (Juan, 2026-11-12): MIDI va por un conversor USB en la Pi; la pared queda ciega.
MIDI_HOLES = False


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
        elif r['board'] == 'PCB-M' and r['item'] == 'jack' and (MIDI_HOLES or 'MIDI' not in r['notes']):
            kind = '1/4' if any(k in r['notes'] for k in ('LINE', 'PEDAL')) else '3.5'
            hole = Part.makeCylinder(JACK_HOLE[kind] / 2, WALL + 2, App.Vector(x, D - WALL - 1, JACK_AXIS_Z),
                                     App.Vector(0, 1, 0))
            cuts.append(hole)
    body = body.fuse(Part.makeCompound(adds))
    return body.cut(Part.makeCompound(cuts)).removeSplitter()


# Partido para la Ender 3 V3 (cama 220 × 220): piezas de 160/180 × 165/175 × 83, impresas con el piso abajo.
# x = 160 pasa entre los jacks LINE IN L/R (148 y 172) y las torres de PCB-M; y = 165 deja libre la torre de (332, 178).
SPLIT_X, SPLIT_Y = 160.0, 165.0
BOSS_T, BOSS_L = 8.0, 16.0        # cada mitad del tope: 8 hacia su lado de la junta, 16 a lo largo
INSERT_D, INSERT_DEPTH, BOLT_D = 4.0, 6.0, 3.4

# (eje, posición a lo largo, z0, alto, lado del inserto). Lado del inserto = -1 (x o y menores) o +1; la cabeza del M3
# queda del otro lado, por donde entra la llave. Alturas fuera de PCB-M (≤ 30), faders (≥ 58) y pantalla (≥ 68).
# Topes de x = 160 en y 215 y 240: PCB-M (80 de alto desde 2026-11-13) empieza en y 256 a z 8 y los topes llegan a z 13.
SEAM_BOSSES = [
    ('x', 40, FLOOR, 10, -1), ('x', 110, FLOOR, 10, -1), ('x', 215, FLOOR, 10, -1), ('x', 240, FLOOR, 10, -1),
    ('x', WALL + BOSS_L / 2, 12, 14, -1), ('x', WALL + BOSS_L / 2, 50, 14, -1),
    ('x', D - WALL - BOSS_L / 2, 45, 13, -1),
    ('y', 40, FLOOR, 10, +1), ('y', 110, FLOOR, 10, +1), ('y', 210, FLOOR, 10, +1), ('y', 280, FLOOR, 10, +1),
    ('y', WALL + BOSS_L / 2, 12, 14, +1), ('y', WALL + BOSS_L / 2, 50, 14, +1),
    ('y', W - WALL - BOSS_L / 2, 12, 14, +1), ('y', W - WALL - BOSS_L / 2, 36, 14, +1),
]


def seam_joints(body):
    """Topes atornillados a cada lado de las juntas: M3 × 16 a inserto Ø4 × 6, invisibles desde fuera."""
    adds, cuts = [], []
    for axis, along, z0, h, side in SEAM_BOSSES:
        if axis == 'x':
            adds.append(Part.makeBox(2 * BOSS_T, BOSS_L, h, App.Vector(SPLIT_X - BOSS_T, along - BOSS_L / 2, z0)))
            c, d = App.Vector(SPLIT_X - BOSS_T - 1, along, z0 + h / 2), App.Vector(1, 0, 0)
            ins = App.Vector(SPLIT_X, along, z0 + h / 2) if side > 0 else App.Vector(SPLIT_X - INSERT_DEPTH, along, z0 + h / 2)
        else:
            adds.append(Part.makeBox(BOSS_L, 2 * BOSS_T, h, App.Vector(along - BOSS_L / 2, SPLIT_Y - BOSS_T, z0)))
            c, d = App.Vector(along, SPLIT_Y - BOSS_T - 1, z0 + h / 2), App.Vector(0, 1, 0)
            ins = App.Vector(along, SPLIT_Y, z0 + h / 2) if side > 0 else App.Vector(along, SPLIT_Y - INSERT_DEPTH, z0 + h / 2)
        cuts.append(Part.makeCylinder(BOLT_D / 2, 2 * BOSS_T + 2, c, d))
        cuts.append(Part.makeCylinder(INSERT_D / 2, INSERT_DEPTH, ins, d))
    body = body.fuse(Part.makeCompound(adds)).removeSplitter()
    return body.cut(Part.makeCompound(cuts))


def split_body(body):
    """Cuatro piezas: FI (frente izq.), FD, TI (trasera izq.), TD."""
    quads = {'FI': (0, 0, SPLIT_X, SPLIT_Y), 'FD': (SPLIT_X, 0, W - SPLIT_X, SPLIT_Y),
             'TI': (0, SPLIT_Y, SPLIT_X, D - SPLIT_Y), 'TD': (SPLIT_X, SPLIT_Y, W - SPLIT_X, D - SPLIT_Y)}
    return {k: body.common(Part.makeBox(w, h, BODY_H + 2, App.Vector(x, y, -1))).removeSplitter()
            for k, (x, y, w, h) in quads.items()}


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
        # Tira recta (x 54–271, y 178–201 aprox.) con su cara superior a TOP−20: el EC11 de Juan mide 28 mm de la
        # base a la punta del eje (2026-11-13), así el eje asoma 8 mm sobre el aluminio. Encoders EC11 como cilindros.
        'PCB_A': Part.makeBox(217, 23, PCB_T, App.Vector(54, 178, TOP - 20 - PCB_T)),
        'EC11_bodies': Part.makeCompound([Part.makeBox(12.4, 13.4, 6.5, App.Vector(x - 6.2, 192 - 6.7, TOP - 20))
                                          for x in [79 + 26 * i for i in range(8)]]),
        'EC11_shafts': Part.makeCompound([Part.makeCylinder(3.0, 28 - 6.5, App.Vector(x, 192, TOP - 20 + 6.5))
                                          for x in [79 + 26 * i for i in range(8)]]),
    }


def build(repo, doc_name='GrooveForgeConsole'):
    rows = load_csv(repo)
    outlines = load_outlines(repo)
    doc = App.newDocument(doc_name)
    body = seam_joints(body_details(outlines, make_body()))
    parts = {'Panel_aluminio': make_panel(rows, outlines, laser=True)}
    parts.update({'Cuerpo_' + k: v for k, v in split_body(body).items()})
    parts.update(make_boards(outlines))
    parts.update(make_placeholders())
    shades = {'FI': .07, 'FD': .11, 'TI': .11, 'TD': .07}
    colors = {'Panel_aluminio': (0.15, 0.15, 0.17)}
    colors.update({'Cuerpo_' + k: (s, s, s) for k, s in shades.items()})
    for name, shape in parts.items():
        obj = doc.addObject('Part::Feature', name)
        obj.Shape = shape
        if App.GuiUp:
            obj.ViewObject.ShapeColor = colors.get(name, (0.36, 0.29, 0.72))
            if name not in colors:
                obj.ViewObject.Transparency = 60
    doc.recompute()
    return doc

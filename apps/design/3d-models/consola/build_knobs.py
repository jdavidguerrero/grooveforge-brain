"""Perillas de la consola para imprimir en FDM: Ø18 (encoders y pots) y jog Ø40.

Uso en FreeCAD: exec(open(path).read(), ns); ns['build'](out_dir).
Eje: Alps EC11 de Juan, estriado Ø6 (18 dientes), asoma 8 mm sobre el aluminio con PCB-A a −20 mm (doc 25 §7).
La perilla deja 1 mm de aire sobre la chapa (carrera del clic 0,5 mm + margen) y el agujero entra 7 mm.
"""
import math
import os

import FreeCAD as App
import Part

SHAFT_D = 6.0
TEETH = 18
TOOTH_H = 0.35          # dientes internos que muerden el estriado; el plástico cede al entrar
BORE_CLEAR = 0.10       # holgura radial de impresión; ajustar con la pieza de prueba
BORE_DEPTH = 7.0
SLOT_W = 0.8            # ranura vertical que deja flexar el cubo al encajar


def bore(depth):
    """Agujero estriado: círculo de Ø(6 + 2·holgura) con 18 dientes triangulares hacia adentro."""
    r = SHAFT_D / 2 + BORE_CLEAR
    hole = Part.makeCylinder(r, depth)
    teeth = []
    for i in range(TEETH):
        a = 2 * math.pi * i / TEETH
        da = math.pi / TEETH * 0.6
        pts = [App.Vector(r * math.cos(a - da), r * math.sin(a - da), 0),
               App.Vector((r - TOOTH_H) * math.cos(a), (r - TOOTH_H) * math.sin(a), 0),
               App.Vector(r * math.cos(a + da), r * math.sin(a + da), 0)]
        tri = Part.Face(Part.makePolygon(pts + [pts[0]]))
        teeth.append(tri.extrude(App.Vector(0, 0, depth)))
    return hole.cut(Part.makeCompound(teeth))


def knob(d, height, skirt_h=0.0, pointer=True, grip=0):
    """Perilla con agujero estriado, ranura de flexión, chaflán arriba, línea indicadora y estrías de agarre."""
    body = Part.makeCylinder(d / 2, height)
    top_edges = [e for e in body.Edges if abs(e.BoundBox.ZMin - height) < 1e-6]
    body = body.makeChamfer(0.8, top_edges)
    if grip:
        cuts = [Part.makeCylinder(0.9, height, App.Vector((d / 2 + 0.35) * math.cos(2 * math.pi * i / grip),
                                                          (d / 2 + 0.35) * math.sin(2 * math.pi * i / grip), 1.5))
                for i in range(grip)]
        body = body.cut(Part.makeCompound(cuts))
    if pointer:
        body = body.cut(Part.makeBox(1.0, d / 2 - 2, 0.8, App.Vector(-0.5, 2, height - 0.8)))
    hub = bore(BORE_DEPTH + 0.01)
    slot = Part.makeBox(SLOT_W, SHAFT_D / 2 + 1.5, BORE_DEPTH, App.Vector(-SLOT_W / 2, 0, 0))
    return body.cut(hub).cut(slot).removeSplitter()


def coupon():
    """Pieza de prueba: tres agujeros con holgura 0,05 / 0,10 / 0,15 para elegir BORE_CLEAR antes de imprimir 13."""
    global BORE_CLEAR
    base = Part.makeBox(36, 14, BORE_DEPTH, App.Vector(-18, -7, 0))
    keep = BORE_CLEAR
    for i, c in enumerate((0.05, 0.10, 0.15)):
        BORE_CLEAR = c
        h = bore(BORE_DEPTH + 0.01)
        h.translate(App.Vector(-11 + 11 * i, 0, 0))
        base = base.cut(h)
    BORE_CLEAR = keep
    return base


def build(out_dir):
    import MeshPart
    doc = App.newDocument('Perillas')
    parts = {
        'perilla_18': knob(18.0, 15.0, grip=24),           # 8 EC11 + MASTER/CUE/IN
        'jog_40': knob(40.0, 14.0, pointer=False, grip=40),  # rueda jog; añadir hoyuelo para el dedo
        'cupon_agujero': coupon(),
    }
    dimple = Part.makeSphere(9.0, App.Vector(12, 0, 14.0 + 7.0))
    parts['jog_40'] = parts['jog_40'].cut(dimple)
    for name, shape in parts.items():
        obj = doc.addObject('Part::Feature', name)
        obj.Shape = shape
        mesh = MeshPart.meshFromShape(Shape=shape, LinearDeflection=0.02, AngularDeflection=0.2)
        mesh.write(os.path.join(out_dir, name + '.stl'))
    doc.recompute()
    doc.saveAs(os.path.join(out_dir, 'perillas.FCStd'))
    return doc

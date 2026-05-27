# =============================================================================
# GrooveForge Brain — 02_Top_Panel — SOLO LED RING
#
# Corre este script si el panel ya tiene:
#   ✓ Body (180×100×2mm con fillets)
#   ✓ Agujeros circulares (ENC shafts, VOL, display, MTG holes)
#   ✓ Botones 14×14mm (B1–B4)
#   PENDIENTE: LED ring Ø24..Ø32mm en ENC NAV
#
# Estado típico antes de este script: body.faces ≈ 30
# Estado esperado después:            body.faces ≈ 34
# (+ 2 cilindros ring exterior/interior + cara fondo anular + cara top modificada)
# =============================================================================

import adsk.core
import adsk.fusion
import traceback


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui  = app.userInterface
        des = adsk.fusion.Design.cast(app.activeProduct)
        root = des.rootComponent

        P  = adsk.core.Point3D
        OC = adsk.core.ObjectCollection
        VI = adsk.core.ValueInput
        DD = adsk.fusion.DistanceExtentDefinition
        FO = adsk.fusion.FeatureOperations
        ED = adsk.fusion.ExtentDirections

        # Localizar componente del top panel
        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '02' in n or 'Top_Panel' in n:
                comp = occ.component
                break
        if comp is None:
            ui.messageBox('ERROR: No se encontró el componente 02_Top_Panel_Aluminum')
            return

        body = comp.bRepBodies.item(0)
        print(f'Panel body: {body.name}  faces={body.faces.count}')

        exts   = comp.features.extrudeFeatures
        combos = comp.features.combineFeatures

        # ENC NAV: X_panel=90 → X_world=0cm, Y_panel=22 → Z_world=-2.2cm
        center = P.create(0.0, 0.0, -2.2)

        sk = comp.sketches.add(comp.xZConstructionPlane)
        sk.name = 'Ring_Annulus'
        circ = sk.sketchCurves.sketchCircles
        circ.addByCenterRadius(center, 1.6)   # Ø32mm exterior → r=1.6cm
        circ.addByCenterRadius(center, 1.2)   # Ø24mm interior → r=1.2cm

        # Seleccionar el perfil del anillo (≈3.519 cm² = 351.9 mm²)
        TARGET = 3.519
        annulus = None
        best = 1e9
        for i in range(sk.profiles.count):
            a = sk.profiles.item(i).areaProperties().area
            if abs(a - TARGET) < best:
                best = abs(a - TARGET)
                annulus = sk.profiles.item(i)

        area_mm2 = annulus.areaProperties().area * 100
        print(f'Anillo seleccionado: {area_mm2:.1f} mm²  (esperado 351.9 mm²)')

        # Crear tool body: extruir el anillo 3mm en +Y (cruza el panel completo)
        inp_ring = exts.createInput(annulus, FO.NewBodyFeatureOperation)
        inp_ring.setOneSideExtent(
            DD.create(VI.createByReal(0.03)),   # 3mm
            ED.PositiveExtentDirection
        )
        feat_ring = exts.add(inp_ring)
        ring_body = feat_ring.bodies.item(0)
        print(f'Ring tool body: health={feat_ring.healthState}')

        # Combine CUT: restar el anillo del panel
        ring_coll = OC.create()
        ring_coll.add(ring_body)
        comb_inp = combos.createInput(body, ring_coll)
        comb_inp.operation = FO.CutFeatureOperation
        comb_inp.isKeepToolBodies = False
        feat_comb = combos.add(comb_inp)

        print(f'Ring CUT: health={feat_comb.healthState}  faces={body.faces.count}')
        ui.messageBox(
            f'✓ LED Ring cortado\n'
            f'health={feat_comb.healthState}  (0=OK, 1=warning)\n'
            f'Panel faces: {body.faces.count}\n\n'
            f'Si faces subió de ~30 a ~34, el anillo quedó OK.'
        )

    except Exception:
        if ui:
            ui.messageBox(traceback.format_exc())

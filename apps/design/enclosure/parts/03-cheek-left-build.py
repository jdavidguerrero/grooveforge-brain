# =============================================================================
# GrooveForge Brain — 03_Cheek_Left_Guadua (Audio IN)
# Script Fusion 360 (Python API) — v0.6 rectangular body
#
# SISTEMA DE COORDENADAS (consistente con todos los demás scripts):
#   X: ±90mm desde el centro (ancho 180mm, izquierda negativa)
#   Y: 0..38mm hacia arriba (altura, Y=0 = base)
#   Z: 0..100mm (profundidad: Z=0 = cara trasera/Pogo, Z=100 = cara frontal/usuario)
#
# POSICIÓN DEL CHEEK IZQUIERDO:
#   X: -90mm a -102mm (CHEEK_T=12mm, exterior al PETG body)
#   = X: -9.0 a -10.2 cm en Fusion
#
# DIFERENCIA vs spec 03-cheek-left-guadua.md (v0.1):
#   El spec referencia BODY_H_F=42mm / BODY_H_R=30mm (diseño inclinado).
#   Este script usa BODY_H=38mm rectangular (v0.6) para alinearse con el
#   PETG body actualizado (01-petg-body-build.py v0.6).
#
# CÓMO USAR:
#   Shift+S → Scripts → "+" → Python → pegar → Run
#   El componente '03_Cheek_Left_Guadua' debe estar vacío.
# =============================================================================

import adsk.core, adsk.fusion, traceback


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

        # Parámetros (v0.6 rectangular)
        BODY_H  = 3.8   # 38mm en cm
        CHEEK_T = 1.2   # 12mm en cm
        BODY_D  = 10.0  # 100mm en cm
        X_INNER = -9.0  # cara interior del cheek (borde exterior del PETG)
        X_OUTER = -10.2 # cara exterior visible (-9.0 - 1.2)

        # Alturas Y de elementos
        JACK_Y  = 2.1   # JACK_Z = 21mm → Y=2.1cm (centro de jacks)

        # Posiciones Z de los jacks (Z desde cara trasera)
        Z_TRS_L  = 3.0  # TRS IN L: 30mm
        Z_TRS_R  = 5.5  # TRS IN R: 55mm
        Z_AUX    = 7.8  # AUX 3.5mm: 78mm

        # Radios de cutout en guadua (con tolerancia +0.5mm)
        R_TRS    = 0.55   # Ø11.0mm → r=5.5mm=0.55cm
        R_AUX    = 0.375  # Ø7.5mm  → r=3.75mm=0.375cm

        # Heat inserts M3 (agujeros de fijación ciegos)
        R_FIX    = 0.21   # Ø4.2mm → r=2.1mm=0.21cm
        DEPTH_FIX = 0.7   # 7mm profundidad

        # Localizar o crear componente
        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '03' in n or 'Cheek_Left' in n:
                comp = occ.component
                break
        if comp is None:
            occ2 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
            comp = occ2.component
            comp.name = '03_Cheek_Left_Guadua'

        exts   = comp.features.extrudeFeatures
        combos = comp.features.combineFeatures
        planes = comp.constructionPlanes

        # ==============================================================
        # STEP 1 — CUERPO PRINCIPAL  12 × 100 × 38 mm
        # ==============================================================
        # Footprint en xZConstructionPlane (Y=0):
        # X: -10.2 a -9.0cm  |  Z: 0 a 10cm
        # Extrude 3.8cm en +Y

        sk1 = comp.sketches.add(comp.xZConstructionPlane)
        sk1.name = 'CheekL_Footprint'
        ln = sk1.sketchCurves.sketchLines
        ln.addByTwoPoints(P.create(X_OUTER, 0, 0.0),   P.create(X_INNER, 0, 0.0))
        ln.addByTwoPoints(P.create(X_INNER, 0, 0.0),   P.create(X_INNER, 0, BODY_D))
        ln.addByTwoPoints(P.create(X_INNER, 0, BODY_D),P.create(X_OUTER, 0, BODY_D))
        ln.addByTwoPoints(P.create(X_OUTER, 0, BODY_D),P.create(X_OUTER, 0, 0.0))

        inp1 = exts.createInput(sk1.profiles.item(0), FO.NewBodyFeatureOperation)
        inp1.setOneSideExtent(DD.create(VI.createByReal(BODY_H)), ED.PositiveExtentDirection)
        feat1 = exts.add(inp1)
        feat1.bodies.item(0).name = 'Cheek_Left_Guadua'
        body = comp.bRepBodies.item(0)
        print(f'Step 1 — Cheek body: health={feat1.healthState}  faces={body.faces.count}')

        # ==============================================================
        # STEP 2 — FILLETS R2mm EN ARISTAS VERTICALES (4 esquinas)
        # ==============================================================
        fillet_edges = OC.create()
        for edge in body.edges:
            v1 = edge.startVertex.geometry
            v2 = edge.endVertex.geometry
            if (abs(v1.x - v2.x) < 1e-4 and
                abs(v1.z - v2.z) < 1e-4 and
                abs(v1.y - v2.y) > 0.005):
                fillet_edges.add(edge)
        print(f'Step 2 — Fillet edges: {fillet_edges.count}  (esperado 4)')
        if fillet_edges.count > 0:
            fi = comp.features.filletFeatures.createInput()
            fi.addConstantRadiusEdgeSet(fillet_edges, VI.createByReal(0.2), True)
            comp.features.filletFeatures.add(fi)

        # ==============================================================
        # STEP 3 — CUTOUTS DE JACKS (cara exterior X=-10.2cm)
        # ==============================================================
        # Técnica: NewBody desde plano exterior → Combine CUT
        # Plano exterior en X = X_OUTER = -10.2cm
        # (offset negativo desde yZConstructionPlane que tiene normal en +X)

        pin_out = planes.createInput()
        pin_out.setByOffset(comp.yZConstructionPlane, VI.createByReal(X_OUTER))
        outer_plane = planes.add(pin_out)
        outer_plane.name = 'CheekL_OuterFace'

        jack_tools = []
        jack_defs = [
            (Z_TRS_L, R_TRS,  'TRS_INL'),
            (Z_TRS_R, R_TRS,  'TRS_INR'),
            (Z_AUX,   R_AUX,  'AUX_35'),
        ]
        for z_cm, r_cm, jname in jack_defs:
            sk_j = comp.sketches.add(outer_plane)
            sk_j.name = f'CheekL_{jname}'
            # Punto en coordenadas mundo: X=X_OUTER, Y=JACK_Y, Z=z_cm
            sk_j.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(X_OUTER, JACK_Y, z_cm), r_cm)
            inp_j = exts.createInput(sk_j.profiles.item(0), FO.NewBodyFeatureOperation)
            # Extruir en +X (PositiveExtentDirection = hacia el interior del cheek)
            inp_j.setOneSideExtent(DD.create(VI.createByReal(CHEEK_T)), ED.PositiveExtentDirection)
            jack_tools.append(exts.add(inp_j).bodies.item(0))

        jack_coll = OC.create()
        for tb in jack_tools: jack_coll.add(tb)
        ci_j = combos.createInput(body, jack_coll)
        ci_j.operation = FO.CutFeatureOperation
        ci_j.isKeepToolBodies = False
        combos.add(ci_j)
        print(f'Step 3 — Jack cutouts: faces={body.faces.count}')

        # ==============================================================
        # STEP 4 — AGUJEROS DE FIJACIÓN M3 (cara interior X=-9.0cm)
        # ==============================================================
        # 2 agujeros ciegos Ø4.2mm × 7mm profundidad
        # Para heat inserts M3 (tornillos desde el interior del PETG)
        # Posiciones v0.6: Y=BODY_H/2=19mm=1.9cm (cheek rectangular)

        FIX_Y = BODY_H / 2  # 1.9cm — centrado en altura

        pin_in = planes.createInput()
        pin_in.setByOffset(comp.yZConstructionPlane, VI.createByReal(X_INNER))
        inner_plane = planes.add(pin_in)
        inner_plane.name = 'CheekL_InnerFace'

        fix_tools = []
        fix_defs = [
            (2.5, FIX_Y, 'FIX1'),   # Z=25mm
            (7.5, FIX_Y, 'FIX2'),   # Z=75mm
        ]
        for z_cm, y_cm, fname in fix_defs:
            sk_f = comp.sketches.add(inner_plane)
            sk_f.name = f'CheekL_{fname}'
            sk_f.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(X_INNER, y_cm, z_cm), R_FIX)
            inp_f = exts.createInput(sk_f.profiles.item(0), FO.NewBodyFeatureOperation)
            # Extruir en -X (NegativeExtentDirection = hacia el interior del cheek)
            inp_f.setOneSideExtent(DD.create(VI.createByReal(DEPTH_FIX)), ED.NegativeExtentDirection)
            fix_tools.append(exts.add(inp_f).bodies.item(0))

        fix_coll = OC.create()
        for tb in fix_tools: fix_coll.add(tb)
        ci_f = combos.createInput(body, fix_coll)
        ci_f.operation = FO.CutFeatureOperation
        ci_f.isKeepToolBodies = False
        combos.add(ci_f)
        print(f'Step 4 — Fixing holes: faces={body.faces.count}')

        # ==============================================================
        # RESUMEN
        # ==============================================================
        bb = body.boundingBox
        summary = (
            f'✓ 03_Cheek_Left_Guadua — BUILD COMPLETO (v0.6 rectangular)\n\n'
            f'BBox X: {(bb.maxPoint.x-bb.minPoint.x)*10:.1f}mm  '
            f'(esperado: 12mm)\n'
            f'BBox Z: {(bb.maxPoint.z-bb.minPoint.z)*10:.1f}mm  '
            f'(esperado: 100mm)\n'
            f'BBox Y: {(bb.maxPoint.y-bb.minPoint.y)*10:.1f}mm  '
            f'(esperado: 38mm)\n\n'
            f'Faces: {body.faces.count}\n\n'
            f'Cutouts: TRS IN L (Ø11mm @ Z=30mm) + TRS IN R (Ø11mm @ Z=55mm)\n'
            f'          AUX 3.5mm (Ø7.5mm @ Z=78mm)\n'
            f'Fixings: 2× Ø4.2mm blind 7mm (heat insert M3) en cara interior\n\n'
            f'NOTA: la guadua se fabrica artesanalmente — este modelo es\n'
            f'referencia de verificación de clearances, no guía de CNC.'
        )
        print(summary)
        ui.messageBox(summary)

    except:
        if ui: ui.messageBox(traceback.format_exc())

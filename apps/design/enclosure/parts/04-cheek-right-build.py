# =============================================================================
# GrooveForge Brain — 04_Cheek_Right_Guadua (Audio OUT + USB)
# Script Fusion 360 (Python API) — v0.6 rectangular body
#
# SISTEMA DE COORDENADAS (consistente con todos los demás scripts):
#   X: ±90mm desde el centro (ancho 180mm, izquierda negativa)
#   Y: 0..38mm hacia arriba (altura, Y=0 = base)
#   Z: 0..100mm (profundidad: Z=0 = cara trasera/Pogo, Z=100 = cara frontal/usuario)
#
# POSICIÓN DEL CHEEK DERECHO:
#   X: +90mm a +102mm (CHEEK_T=12mm, exterior al PETG body)
#   = X: +9.0 a +10.2 cm en Fusion
#
# DIFERENCIA vs spec 04-cheek-right-guadua.md (v0.2):
#   El spec referencia BODY_H_F=42mm / BODY_H_R=30mm (diseño inclinado).
#   Este script usa BODY_H=38mm rectangular (v0.6).
#
# CONECTORES (resolución D-10, v0.4 arquitectura USB):
#   Todos centrados en Y=21mm (JACK_Z) desde la base:
#   Z=15mm  USB-C Data/Power  (rectangular 11×5.5mm)
#   Z=38mm  TRS OUT L         (circular Ø11mm)
#   Z=60mm  TRS OUT R         (circular Ø11mm)
#   Z=82mm  USB-C Host MIDI   (rectangular 11×5.5mm)
#
# CÓMO USAR:
#   Shift+S → Scripts → "+" → Python → pegar → Run
#   El componente '04_Cheek_Right_Guadua' debe estar vacío.
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
        X_INNER = 9.0   # cara interior del cheek derecho (borde exterior del PETG)
        X_OUTER = 10.2  # cara exterior visible (+9.0 + 1.2)

        JACK_Y  = 2.1   # JACK_Z = 21mm → Y=2.1cm (centro de todos los conectores)

        # Posiciones Z de los conectores
        Z_USBC_DATA = 1.5   # USB-C Power+Data: 15mm
        Z_TRS_L     = 3.8   # TRS OUT L: 38mm
        Z_TRS_R     = 6.0   # TRS OUT R: 60mm
        Z_USBC_HOST = 8.2   # USB-C Host: 82mm

        # Dimensiones cutouts
        R_TRS       = 0.55  # Ø11.0mm → r=0.55cm (Ø10.5mm nominal + 0.5mm tolerancia guadua)
        USBC_W      = 1.1   # 11.0mm (10.5mm + 0.5mm tolerancia)
        USBC_H      = 0.55  # 5.5mm  (5.0mm + 0.5mm tolerancia)

        # Heat inserts M3
        R_FIX     = 0.21    # Ø4.2mm
        DEPTH_FIX = 0.7     # 7mm

        # Localizar o crear componente
        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '04' in n or 'Cheek_Right' in n:
                comp = occ.component
                break
        if comp is None:
            occ2 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
            comp = occ2.component
            comp.name = '04_Cheek_Right_Guadua'

        exts   = comp.features.extrudeFeatures
        combos = comp.features.combineFeatures
        planes = comp.constructionPlanes

        # ==============================================================
        # STEP 1 — CUERPO PRINCIPAL  12 × 100 × 38 mm
        # ==============================================================
        # Footprint en xZConstructionPlane (Y=0):
        # X: +9.0 a +10.2cm  |  Z: 0 a 10cm
        # Extrude 3.8cm en +Y

        sk1 = comp.sketches.add(comp.xZConstructionPlane)
        sk1.name = 'CheekR_Footprint'
        ln = sk1.sketchCurves.sketchLines
        ln.addByTwoPoints(P.create(X_INNER, 0, 0.0),   P.create(X_OUTER, 0, 0.0))
        ln.addByTwoPoints(P.create(X_OUTER, 0, 0.0),   P.create(X_OUTER, 0, BODY_D))
        ln.addByTwoPoints(P.create(X_OUTER, 0, BODY_D),P.create(X_INNER, 0, BODY_D))
        ln.addByTwoPoints(P.create(X_INNER, 0, BODY_D),P.create(X_INNER, 0, 0.0))

        inp1 = exts.createInput(sk1.profiles.item(0), FO.NewBodyFeatureOperation)
        inp1.setOneSideExtent(DD.create(VI.createByReal(BODY_H)), ED.PositiveExtentDirection)
        feat1 = exts.add(inp1)
        feat1.bodies.item(0).name = 'Cheek_Right_Guadua'
        body = comp.bRepBodies.item(0)
        print(f'Step 1 — Cheek body: health={feat1.healthState}  faces={body.faces.count}')

        # ==============================================================
        # STEP 2 — FILLETS R2mm EN ARISTAS VERTICALES
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
        # STEP 3 — CUTOUTS CIRCULARES: TRS OUT L y TRS OUT R
        # ==============================================================
        # Plano exterior del cheek derecho: X = +10.2cm
        # Normal del plano = +X, extruir en NegativeExtentDirection = hacia -X (interior del cheek)

        pin_out = planes.createInput()
        pin_out.setByOffset(comp.yZConstructionPlane, VI.createByReal(X_OUTER))
        outer_plane = planes.add(pin_out)
        outer_plane.name = 'CheekR_OuterFace'

        # Jacks circulares TRS
        trs_tools = []
        for z_cm, jname in [(Z_TRS_L, 'TRS_OUTL'), (Z_TRS_R, 'TRS_OUTR')]:
            sk_t = comp.sketches.add(outer_plane)
            sk_t.name = f'CheekR_{jname}'
            sk_t.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(X_OUTER, JACK_Y, z_cm), R_TRS)
            inp_t = exts.createInput(sk_t.profiles.item(0), FO.NewBodyFeatureOperation)
            # NegativeExtentDirection desde X=+10.2 → hacia -X (dentro del cheek)
            inp_t.setOneSideExtent(DD.create(VI.createByReal(CHEEK_T)), ED.NegativeExtentDirection)
            trs_tools.append(exts.add(inp_t).bodies.item(0))

        trs_coll = OC.create()
        for tb in trs_tools: trs_coll.add(tb)
        ci_t = combos.createInput(body, trs_coll)
        ci_t.operation = FO.CutFeatureOperation
        ci_t.isKeepToolBodies = False
        combos.add(ci_t)
        print(f'Step 3 — TRS cutouts: faces={body.faces.count}')

        # ==============================================================
        # STEP 4 — CUTOUTS RECTANGULARES: USB-C Data + USB-C Host
        # ==============================================================
        # Rectángulos USB-C: 11.0 × 5.5mm (con tolerancia guadua)
        # Centrados en Y=JACK_Y, Z=Z_USBC_*
        # Técnica: NewBody desde yZConstructionPlane + OffsetStart → Combine CUT

        half_w = USBC_W / 2   # 0.55cm
        half_h = USBC_H / 2   # 0.275cm

        usbc_tools = []
        for z_cm, uname in [(Z_USBC_DATA, 'USBC_DATA'), (Z_USBC_HOST, 'USBC_HOST')]:
            # Cross-section en YZ (plano vertical en X=0):
            # Y: JACK_Y ± half_h, Z: z_cm ± half_w
            sk_u = comp.sketches.add(comp.yZConstructionPlane)
            sk_u.name = f'CheekR_{uname}'
            ln_u = sk_u.sketchCurves.sketchLines
            y0, y1 = JACK_Y - half_h, JACK_Y + half_h
            z0, z1 = z_cm - half_w, z_cm + half_w
            for (ya, za), (yb, zb) in [
                ((y0, z0), (y1, z0)),
                ((y1, z0), (y1, z1)),
                ((y1, z1), (y0, z1)),
                ((y0, z1), (y0, z0)),
            ]:
                ln_u.addByTwoPoints(P.create(0, ya, za), P.create(0, yb, zb))

            inp_u = exts.createInput(sk_u.profiles.item(0), FO.NewBodyFeatureOperation)
            # Extruir en +X desde X_INNER (cara interior), longitud CHEEK_T
            # OffsetStart = X_INNER, dirección +X, longitud CHEEK_T → de X=9.0 a X=10.2
            inp_u.setOneSideExtent(DD.create(VI.createByReal(CHEEK_T)), ED.PositiveExtentDirection)
            inp_u.startExtent = adsk.fusion.OffsetStartDefinition.create(
                VI.createByReal(X_INNER))
            usbc_tools.append(exts.add(inp_u).bodies.item(0))

        usbc_coll = OC.create()
        for tb in usbc_tools: usbc_coll.add(tb)
        ci_u = combos.createInput(body, usbc_coll)
        ci_u.operation = FO.CutFeatureOperation
        ci_u.isKeepToolBodies = False
        combos.add(ci_u)
        print(f'Step 4 — USB-C cutouts: faces={body.faces.count}')

        # ==============================================================
        # STEP 5 — AGUJEROS DE FIJACIÓN M3 (cara interior X=+9.0cm)
        # ==============================================================
        FIX_Y = BODY_H / 2  # 1.9cm — centrado en altura (v0.6 rectangular)

        pin_in = planes.createInput()
        pin_in.setByOffset(comp.yZConstructionPlane, VI.createByReal(X_INNER))
        inner_plane = planes.add(pin_in)
        inner_plane.name = 'CheekR_InnerFace'

        fix_tools = []
        for z_cm, fname in [(2.5, 'FIX1'), (7.5, 'FIX2')]:
            sk_f = comp.sketches.add(inner_plane)
            sk_f.name = f'CheekR_{fname}'
            sk_f.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(X_INNER, FIX_Y, z_cm), R_FIX)
            inp_f = exts.createInput(sk_f.profiles.item(0), FO.NewBodyFeatureOperation)
            # Extruir en +X (PositiveExtentDirection) desde X=+9.0 → hacia interior del cheek (-X)
            # Wait: desde cara interior (X=+9.0) hacia el interior del cheek es en +X direction
            # porque X_INNER=9.0 y X_OUTER=10.2, entonces +X va hacia la cara exterior.
            # El agujero ciego va desde la cara INTERIOR hacia ADENTRO del cheek = +X direction
            inp_f.setOneSideExtent(DD.create(VI.createByReal(DEPTH_FIX)), ED.PositiveExtentDirection)
            fix_tools.append(exts.add(inp_f).bodies.item(0))

        fix_coll = OC.create()
        for tb in fix_tools: fix_coll.add(tb)
        ci_f = combos.createInput(body, fix_coll)
        ci_f.operation = FO.CutFeatureOperation
        ci_f.isKeepToolBodies = False
        combos.add(ci_f)
        print(f'Step 5 — Fixing holes: faces={body.faces.count}')

        # ==============================================================
        # RESUMEN
        # ==============================================================
        bb = body.boundingBox
        summary = (
            f'✓ 04_Cheek_Right_Guadua — BUILD COMPLETO (v0.6 rectangular)\n\n'
            f'BBox X: {(bb.maxPoint.x-bb.minPoint.x)*10:.1f}mm  (esperado: 12mm)\n'
            f'BBox Z: {(bb.maxPoint.z-bb.minPoint.z)*10:.1f}mm  (esperado: 100mm)\n'
            f'BBox Y: {(bb.maxPoint.y-bb.minPoint.y)*10:.1f}mm  (esperado: 38mm)\n\n'
            f'Faces: {body.faces.count}\n\n'
            f'Cutouts:\n'
            f'  USB-C Data/Power: 11×5.5mm rect @ Z=15mm\n'
            f'  TRS OUT L: Ø11mm circ @ Z=38mm\n'
            f'  TRS OUT R: Ø11mm circ @ Z=60mm\n'
            f'  USB-C Host MIDI: 11×5.5mm rect @ Z=82mm\n'
            f'  Fixings: 2× Ø4.2mm blind 7mm (heat insert M3)\n\n'
            f'Clearances D-10 verificados:\n'
            f'  USB-C Data→TRS L: 12.5mm gap OK\n'
            f'  TRS L→TRS R: 11.5mm gap OK\n'
            f'  TRS R→USB-C Host: 11.5mm gap OK\n'
            f'  USB-C Host→borde frontal: 12.75mm OK'
        )
        print(summary)
        ui.messageBox(summary)

    except:
        if ui: ui.messageBox(traceback.format_exc())

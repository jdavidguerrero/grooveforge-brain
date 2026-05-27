# =============================================================================
# GrooveForge Brain — 02_Top_Panel_Aluminum
# Script Fusion 360 (Python API) — v0.6 orientación correcta
#
# SISTEMA DE COORDENADAS (consistente con todos los demás scripts):
#   X: ±90mm desde el centro (ancho 180mm, izquierda negativa)
#   Y: 0..2mm hacia arriba (espesor del panel, Y=0 = cara inferior)
#   Z: 0..100mm (profundidad: Z=0 = cara trasera, Z=100 = cara frontal/usuario)
#
# TOP VIEW de Fusion (mirando desde +Y) muestra la cara del panel correctamente.
#
# CÓMO USAR:
#   Shift+S → Scripts → "+" → Python → pegar → Run
#   El componente '02_Top_Panel_Aluminum' debe estar vacío.
#
# Translación de coordenadas de panel a Fusion:
#   X_panel (0..180mm, desde borde izq) → X_world = (X_panel - 90) / 10  [cm]
#   Y_panel (0..100mm, desde borde trasero) → Z_world = Y_panel / 10      [cm]
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

        # Coordenadas panel → Fusion (todo en cm)
        def cx(xp): return (xp - 90.0) / 10.0   # X_panel mm → X cm
        def cz(yp): return yp / 10.0             # Y_panel mm → Z cm (POSITIVO)
        def pt(xp, yp, y_w=0.0): return P.create(cx(xp), y_w, cz(yp))

        # Localizar componente
        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '02' in n or 'Top_Panel' in n:
                comp = occ.component
                break
        if comp is None:
            occ2 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
            comp = occ2.component
            comp.name = '02_Top_Panel_Aluminum'

        exts   = comp.features.extrudeFeatures
        combos = comp.features.combineFeatures

        # ==============================================================
        # STEP 1 — CUERPO DEL PANEL  180 × 100 × 2 mm
        # ==============================================================
        # Sketch en xZConstructionPlane (Y=0, el plano horizontal de la base)
        # Footprint 180×100mm → extrude 2mm en +Y
        # → Top view de Fusion muestra la cara correctamente ✓

        sk1 = comp.sketches.add(comp.xZConstructionPlane)
        sk1.name = 'Panel_Footprint'
        ln = sk1.sketchCurves.sketchLines
        # X: -9 a +9cm (180mm), Z: 0 a 10cm (100mm)
        ln.addByTwoPoints(P.create(-9.0, 0, 0.0), P.create( 9.0, 0, 0.0))
        ln.addByTwoPoints(P.create( 9.0, 0, 0.0), P.create( 9.0, 0,10.0))
        ln.addByTwoPoints(P.create( 9.0, 0,10.0), P.create(-9.0, 0,10.0))
        ln.addByTwoPoints(P.create(-9.0, 0,10.0), P.create(-9.0, 0, 0.0))

        inp1 = exts.createInput(sk1.profiles.item(0), FO.NewBodyFeatureOperation)
        inp1.setOneSideExtent(DD.create(VI.createByReal(0.2)), ED.PositiveExtentDirection)
        feat1 = exts.add(inp1)
        feat1.bodies.item(0).name = 'Aluminum_Panel'
        body = comp.bRepBodies.item(0)
        print(f'Step 1 — Panel body: health={feat1.healthState}  faces={body.faces.count}')

        # ==============================================================
        # STEP 2 — FILLETS R3mm EN LAS 4 ESQUINAS (aristas verticales)
        # ==============================================================
        # Las aristas Y-paralelas en las 4 esquinas del plan (XZ)
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
            fi.addConstantRadiusEdgeSet(fillet_edges, VI.createByReal(0.3), True)
            comp.features.filletFeatures.add(fi)

        # ==============================================================
        # STEP 3 — AGUJEROS CIRCULARES (9 holes)
        # ==============================================================
        # xZConstructionPlane + circles → setAllExtent +Y → health=1 warning OK
        sk2 = comp.sketches.add(comp.xZConstructionPlane)
        sk2.name = 'Round_Cutouts'
        circ = sk2.sketchCurves.sketchCircles

        # (X_panel mm, Y_panel mm, diámetro mm)
        holes = [
            (22,  55,  6.5),   # ENC1 shaft Ø6.5mm
            (158, 55,  6.5),   # ENC2 shaft
            (90,  22,  6.5),   # ENC NAV shaft
            (22,  14,  7.0),   # VOL pot
            (90,  60, 36.0),   # Display window Ø36mm
            (10,   8,  3.2),   # MTG ↖
            (10,  92,  3.2),   # MTG ↙
            (170,  8,  3.2),   # MTG ↗
            (170, 92,  3.2),   # MTG ↘
        ]
        for xp, yp, d in holes:
            circ.addByCenterRadius(P.create(cx(xp), 0, cz(yp)), d/2/10)

        sel_r = OC.create()
        for i in range(sk2.profiles.count):
            sel_r.add(sk2.profiles.item(i))
        inp2 = exts.createInput(sel_r, FO.CutFeatureOperation)
        inp2.setAllExtent(ED.PositiveExtentDirection)
        inp2.participantBodies = [body]
        feat2 = exts.add(inp2)
        print(f'Step 3 — Round holes: health={feat2.healthState}  faces={body.faces.count}')

        # ==============================================================
        # STEP 4 — BOTONES 14×14mm (tool body + combine)
        # ==============================================================
        # Rectangles no funcionan con extrude-cut directo en planos de construcción.
        # Solución verificada: NewBody desde yZConstructionPlane + combineFeatures CUT.

        half = 0.7   # 7mm = 0.7cm (mitad de 14mm)
        buttons = [
            (52,  66, 'B1_ENGINE'),
            (52,  44, 'B2_FX'),
            (128, 66, 'B3_AI'),
            (128, 44, 'B4_PRESET'),
        ]
        btn_tools = []
        for bx, by, bname in buttons:
            x0, z0 = cx(bx), cz(by)
            # Cross-section en YZ: Y -1mm..3mm (cubre 0..2mm panel), Z z0±half
            sk_b = comp.sketches.add(comp.yZConstructionPlane)
            sk_b.name = f'BTN_{bname}'
            ln_b = sk_b.sketchCurves.sketchLines
            for (ya, za), (yb, zb) in [
                ((-0.01, z0-half), (0.03, z0-half)),
                ((0.03, z0-half),  (0.03, z0+half)),
                ((0.03, z0+half),  (-0.01, z0+half)),
                ((-0.01, z0+half), (-0.01, z0-half)),
            ]:
                ln_b.addByTwoPoints(P.create(0, ya, za), P.create(0, yb, zb))
            inp_b = exts.createInput(sk_b.profiles.item(0), FO.NewBodyFeatureOperation)
            inp_b.setOneSideExtent(DD.create(VI.createByReal(1.4)), ED.PositiveExtentDirection)
            inp_b.startExtent = adsk.fusion.OffsetStartDefinition.create(VI.createByReal(x0 - half))
            btn_tools.append(exts.add(inp_b).bodies.item(0))

        btn_coll = OC.create()
        for tb in btn_tools: btn_coll.add(tb)
        ci = combos.createInput(body, btn_coll)
        ci.operation = FO.CutFeatureOperation
        ci.isKeepToolBodies = False
        combos.add(ci)
        print(f'Step 4 — Buttons: faces={body.faces.count}')

        # ==============================================================
        # STEP 5 — LED RING Ø24..Ø32mm en ENC NAV (X=90, Y=22)
        # ==============================================================
        sk_r = comp.sketches.add(comp.xZConstructionPlane)
        sk_r.name = 'Ring_Annulus'
        cr = sk_r.sketchCurves.sketchCircles
        c = P.create(cx(90), 0, cz(22))   # ENC NAV center: X=0, Z=2.2cm
        cr.addByCenterRadius(c, 1.6)       # Ø32mm → r=1.6cm
        cr.addByCenterRadius(c, 1.2)       # Ø24mm → r=1.2cm

        # Seleccionar anillo: área ~3.519 cm²
        annulus = min(
            [sk_r.profiles.item(i) for i in range(sk_r.profiles.count)],
            key=lambda p: abs(p.areaProperties().area - 3.519)
        )
        inp_rn = exts.createInput(annulus, FO.NewBodyFeatureOperation)
        inp_rn.setOneSideExtent(DD.create(VI.createByReal(0.03)), ED.PositiveExtentDirection)
        ring_body = exts.add(inp_rn).bodies.item(0)

        rc = OC.create(); rc.add(ring_body)
        ri = combos.createInput(body, rc)
        ri.operation = FO.CutFeatureOperation
        ri.isKeepToolBodies = False
        combos.add(ri)
        print(f'Step 5 — LED ring: faces={body.faces.count}')

        # ==============================================================
        # RESUMEN
        # ==============================================================
        bb = body.boundingBox
        summary = (
            f'✓ 02_Top_Panel_Aluminum — BUILD COMPLETO\n\n'
            f'BBox: {(bb.maxPoint.x-bb.minPoint.x)*10:.0f} × '
            f'{(bb.maxPoint.z-bb.minPoint.z)*10:.0f} × '
            f'{(bb.maxPoint.y-bb.minPoint.y)*10:.1f} mm\n'
            f'  (esperado: 180 × 100 × 2 mm)\n\n'
            f'Faces: {body.faces.count}\n'
            f'Top view (Fusion) → muestra cara del panel ✓\n\n'
            f'Cutouts: 9 circulares + 4 botones 14×14mm + LED ring Ø24..Ø32mm'
        )
        print(summary)
        ui.messageBox(summary)

    except:
        if ui: ui.messageBox(traceback.format_exc())

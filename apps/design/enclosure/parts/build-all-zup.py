# =============================================================================
# GrooveForge Brain — Build All Parts (Z-UP NATIVO) v3.4
#
# REGLAS APRENDIDAS (empirico verificado en doc Fusion Z-UP):
#
# 1. COORDENADAS EN PLANOS OFFSET:
#    Sketch en xYConstructionPlane offset Z=H  → P.create(x, y, 0)   Z=0, no Z=H
#    Sketch en xZConstructionPlane offset Y=k  → P.create(x, 0, z)   Y=0, no Y=k
#    Sketch en yZConstructionPlane offset X=k  → P.create(-z, y, 0)
#       eje X sketch = -Z mundo,  eje Y sketch = +Y mundo (verificado empírico)
#       P.create(0, y, z) INCORRECTO — coloca perfiles a Z=0 y desplazados en X
#
# 2. LED RING: usar CutFeatureOperation directo en el body (no CombineFeature)
#    para evitar body residual del anulo.
#
# 3. PEDESTALES: sketch en Z=0 (base), extrude hasta Z=H para que el cilindro
#    atraviese el piso del PETG y haga JOIN. Tapa flush con la boca del PETG.
#
# 4. PANEL ANGOSTO: 168mm X[-8.4,+8.4] para no solapar los cheeks en el techo.
#
# 5. CHEEK CUTOUTS — patrón obligatorio para yZConstructionPlane:
#    NewBodyFeatureOperation + CombineFeature.CutFeatureOperation
#    CutFeatureOperation + participantBodies FALLA en sketches de yZConstructionPlane.
#
# CONVENCION Z-UP:
#   X: -90..+90 mm   (ancho, izquierda negativo)
#   Y:   0..100 mm   (profundidad, Y=0 = cara trasera/Pogo, Y=100 = frontal/user)
#   Z:   0..38  mm   (altura, Z=0 = base, Z=38 = boca PETG)
#
# Cheeks v0.8 — 10mm grosor (reducido de 12mm):
#   X_INNER = ±8.4cm  (cara interior del cheek = fondo rebaje PETG)
#   X_OUTER = ±9.4cm  (cara exterior visible)
#   Total instrumento: 188mm × 100mm × 42mm (con cheeks proud 2mm sobre panel)
#   Rebaje PETG sin cambios: 6mm (X_INNER ±8.4 → ±9.0), 4mm protrude visible
#
# PARA EJECUTAR:
#   Fusion 360: Shift+S → Scripts → "+" → Python → pegar → Run
#   El documento debe tener defaultModelingOrientation = Z-UP (1)
# =============================================================================

import adsk.core, adsk.fusion, traceback, math


def run(_ctx):
    app = adsk.core.Application.get()
    try:
        des  = adsk.fusion.Design.cast(app.activeProduct)
        root = des.rootComponent

        P  = adsk.core.Point3D
        OC = adsk.core.ObjectCollection
        VI = adsk.core.ValueInput
        DD = adsk.fusion.DistanceExtentDefinition
        FO = adsk.fusion.FeatureOperations
        ED = adsk.fusion.ExtentDirections

        def dist(v): return DD.create(VI.createByReal(v))
        def val(v):  return VI.createByReal(v)
        def bbox(b):
            bb = b.boundingBox; mn, mx = bb.minPoint, bb.maxPoint
            return (f'X[{mn.x*10:6.1f},{mx.x*10:6.1f}]  '
                    f'Y[{mn.y*10:6.1f},{mx.y*10:6.1f}]  '
                    f'Z[{mn.z*10:6.1f},{mx.z*10:6.1f}]  '
                    f'W{(mx.x-mn.x)*10:5.1f} D{(mx.y-mn.y)*10:5.1f} H{(mx.z-mn.z)*10:.1f}mm')

        # ------------------------------------------------------------------
        # PURGE — borrar todo antes de rebuild limpio
        # ------------------------------------------------------------------
        for occ in list(root.occurrences):
            try: occ.deleteMe()
            except: pass
        for i in range(root.bRepBodies.count - 1, -1, -1):
            try: root.bRepBodies.item(i).deleteMe()
            except: pass
        print(f'Doc purged. occ={root.occurrences.count} bodies={root.bRepBodies.count}')

        # ------------------------------------------------------------------
        # PARAMETROS (cm)
        # ------------------------------------------------------------------
        W, D, H    = 18.0, 10.0, 3.8     # PETG body: 180 x 100 x 38 mm
        H_CHEEK    = H + 0.4              # 42mm — cheeks proud 2mm sobre panel top (Z=40)
        WT, PT     = 0.3, 0.2             # pared PETG 3mm, panel aluminio 2mm
        CT         = 1.0                  # grosor cheek 10mm (v0.8, reducido de 12mm)
        SH, SOD    = 0.5, 0.6             # standoff height 5mm, diameter 6mm
        IMD, IMH   = 0.42, 0.6            # heat insert Ø4.2mm, depth 6mm
        PTD, PTH   = 1.0, 0.2             # pata silicona Ø10mm, depth 2mm
        X_INNER_R, X_OUTER_R =  8.4,  9.4   # 10mm cheek: 6mm rebaje PETG + 4mm protrude
        X_INNER_L, X_OUTER_L = -8.4, -9.4
        JZ = 2.1   # centro altura jacks = 21mm

        # cx/cy: convierte posicion en mm (ref 0..180 horizontal) a cm (ref -9..+9)
        def cx(xp): return (xp - 90.0) / 10.0
        def cy(yp): return yp / 10.0

        # ==================================================================
        # PART 01 — PETG BODY  180 x 100 x 38 mm
        # ==================================================================
        print('\n===== 01_PETG_Body =====')
        occ1 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        c1 = occ1.component; c1.name = '01_PETG_Body'
        exts1    = c1.features.extrudeFeatures
        combos1  = c1.features.combineFeatures
        fillets1 = c1.features.filletFeatures
        cplanes1 = c1.constructionPlanes

        # Step 1 — cuerpo exterior sólido 180×100×38mm
        sk1 = c1.sketches.add(c1.xYConstructionPlane)
        ln1 = sk1.sketchCurves.sketchLines
        ln1.addByTwoPoints(P.create(-9.0, 0.0, 0), P.create( 9.0, 0.0, 0))
        ln1.addByTwoPoints(P.create( 9.0, 0.0, 0), P.create( 9.0,10.0, 0))
        ln1.addByTwoPoints(P.create( 9.0,10.0, 0), P.create(-9.0,10.0, 0))
        ln1.addByTwoPoints(P.create(-9.0,10.0, 0), P.create(-9.0, 0.0, 0))
        inp1 = exts1.createInput(sk1.profiles.item(0), FO.NewBodyFeatureOperation)
        inp1.setOneSideExtent(dist(H), ED.PositiveExtentDirection)
        body1 = exts1.add(inp1).bodies.item(0); body1.name = 'PETG_Body'
        print(f'  Step1: {bbox(body1)}')

        # Step 2 — fillet R2mm aristas verticales (4 esquinas)
        fe = OC.create()
        for e in body1.edges:
            v1, v2 = e.startVertex.geometry, e.endVertex.geometry
            if abs(v1.x-v2.x)<1e-4 and abs(v1.y-v2.y)<1e-4 and abs(v1.z-v2.z)>1e-2:
                fe.add(e)
        if fe.count > 0:
            fi = fillets1.createInput()
            fi.addConstantRadiusEdgeSet(fe, val(0.2), True)
            fillets1.add(fi)

        # Step 3 — vaciado interior (plano Z=WT, coords relativas Z=0)
        Wi = W/2 - WT  # = 8.7cm
        cp_i = cplanes1.createInput()
        cp_i.setByOffset(c1.xYConstructionPlane, val(WT))
        pl_i = cplanes1.add(cp_i)
        sk_i = c1.sketches.add(pl_i)
        li = sk_i.sketchCurves.sketchLines
        li.addByTwoPoints(P.create(-Wi, WT,    0), P.create( Wi, WT,    0))
        li.addByTwoPoints(P.create( Wi, WT,    0), P.create( Wi, D-WT,  0))
        li.addByTwoPoints(P.create( Wi, D-WT,  0), P.create(-Wi, D-WT,  0))
        li.addByTwoPoints(P.create(-Wi, D-WT,  0), P.create(-Wi, WT,    0))
        inpi = exts1.createInput(sk_i.profiles.item(0), FO.NewBodyFeatureOperation)
        inpi.setOneSideExtent(dist(H - WT), ED.PositiveExtentDirection)
        fi_b = exts1.add(inpi).bodies.item(0)
        ic = OC.create(); ic.add(fi_b)
        ci = combos1.createInput(body1, ic)
        ci.operation = FO.CutFeatureOperation; ci.isKeepToolBodies = False
        combos1.add(ci)

        # Step 4 — rebajes laterales 6mm (retiran las paredes laterales completas)
        # El cheek encaja en el espacio liberado: X=±8.4..±9.6
        for side, x_out, x_in in [('L', -9.0, -8.4), ('R', 8.4, 9.0)]:
            sk_r = c1.sketches.add(c1.xYConstructionPlane)
            lr = sk_r.sketchCurves.sketchLines
            lr.addByTwoPoints(P.create(x_out, 0.0, 0), P.create(x_in,  0.0, 0))
            lr.addByTwoPoints(P.create(x_in,  0.0, 0), P.create(x_in, 10.0, 0))
            lr.addByTwoPoints(P.create(x_in, 10.0, 0), P.create(x_out,10.0, 0))
            lr.addByTwoPoints(P.create(x_out,10.0, 0), P.create(x_out, 0.0, 0))
            inpr = exts1.createInput(sk_r.profiles.item(0), FO.NewBodyFeatureOperation)
            inpr.setOneSideExtent(dist(H), ED.PositiveExtentDirection)
            fr_b = exts1.add(inpr).bodies.item(0)
            rc = OC.create(); rc.add(fr_b)
            cir = combos1.createInput(body1, rc)
            cir.operation = FO.CutFeatureOperation; cir.isKeepToolBodies = False
            combos1.add(cir)
        print(f'  Step4 rebajes: {bbox(body1)}')

        # Step 5 — PCB standoffs (6): sketch Z=WT, coord relativa Z=0
        standoff_pos = [(-7.4, 1.3), (0.0, 1.3), (7.4, 1.3),
                        (-7.4, 8.7), (0.0, 8.7), (7.4, 8.7)]
        cp_so = cplanes1.createInput()
        cp_so.setByOffset(c1.xYConstructionPlane, val(WT))
        pl_so = cplanes1.add(cp_so)
        sk_so = c1.sketches.add(pl_so)
        for sx, sy in standoff_pos:
            sk_so.sketchCurves.sketchCircles.addByCenterRadius(P.create(sx, sy, 0), SOD/2)
        sel_so = OC.create()
        for i in range(sk_so.profiles.count): sel_so.add(sk_so.profiles.item(i))
        inp_so = exts1.createInput(sel_so, FO.NewBodyFeatureOperation)
        inp_so.setOneSideExtent(dist(SH), ED.PositiveExtentDirection)
        f_so = exts1.add(inp_so)
        soc = OC.create()
        for i in range(f_so.bodies.count): soc.add(f_so.bodies.item(i))
        ci_so = combos1.createInput(body1, soc)
        ci_so.operation = FO.JoinFeatureOperation; ci_so.isKeepToolBodies = False
        combos1.add(ci_so)
        # Heat inserts en standoffs (Z=WT+SH, extruye -Z)
        cp_soH = cplanes1.createInput()
        cp_soH.setByOffset(c1.xYConstructionPlane, val(WT + SH))
        pl_soH = cplanes1.add(cp_soH)
        sk_soH = c1.sketches.add(pl_soH)
        for sx, sy in standoff_pos:
            sk_soH.sketchCurves.sketchCircles.addByCenterRadius(P.create(sx, sy, 0), IMD/2)
        sel_soH = OC.create()
        for i in range(sk_soH.profiles.count): sel_soH.add(sk_soH.profiles.item(i))
        inp_soH = exts1.createInput(sel_soH, FO.CutFeatureOperation)
        inp_soH.setOneSideExtent(dist(IMH), ED.NegativeExtentDirection)
        inp_soH.participantBodies = [body1]
        exts1.add(inp_soH)
        print(f'  Step5 standoffs: faces={body1.faces.count}')

        # Step 6 — Pedestales para panel (4): sketch en Z=0 para JOIN con piso
        # Los cilindros atraviesan el piso (Z=0..WT) y suben hasta la boca (Z=H)
        # El panel descansa sobre sus topes (Z=H) y los tornillos entran por heat inserts
        ped_pos = [(-8.0, 0.8), (8.0, 0.8), (-8.0, 9.2), (8.0, 9.2)]
        sk_ped = c1.sketches.add(c1.xYConstructionPlane)  # sketch en Z=0 — crucial para JOIN
        for px, py in ped_pos:
            sk_ped.sketchCurves.sketchCircles.addByCenterRadius(P.create(px, py, 0), SOD/2)
        sel_ped = OC.create()
        for i in range(sk_ped.profiles.count): sel_ped.add(sk_ped.profiles.item(i))
        inp_ped = exts1.createInput(sel_ped, FO.NewBodyFeatureOperation)
        inp_ped.setOneSideExtent(dist(H), ED.PositiveExtentDirection)  # Z=0 → Z=H, flush con boca
        f_ped = exts1.add(inp_ped)
        pedc = OC.create()
        for i in range(f_ped.bodies.count): pedc.add(f_ped.bodies.item(i))
        ci_ped = combos1.createInput(body1, pedc)
        ci_ped.operation = FO.JoinFeatureOperation; ci_ped.isKeepToolBodies = False
        combos1.add(ci_ped)
        # Heat inserts en tapa de pedestales (Z=H, extruye -Z para IMH)
        cp_pedH = cplanes1.createInput()
        cp_pedH.setByOffset(c1.xYConstructionPlane, val(H))
        pl_pedH = cplanes1.add(cp_pedH)
        sk_pedH = c1.sketches.add(pl_pedH)
        for px, py in ped_pos:
            sk_pedH.sketchCurves.sketchCircles.addByCenterRadius(P.create(px, py, 0), IMD/2)
        sel_pedH = OC.create()
        for i in range(sk_pedH.profiles.count): sel_pedH.add(sk_pedH.profiles.item(i))
        inp_pedH = exts1.createInput(sel_pedH, FO.CutFeatureOperation)
        inp_pedH.setOneSideExtent(dist(IMH), ED.NegativeExtentDirection)
        inp_pedH.participantBodies = [body1]
        exts1.add(inp_pedH)
        print(f'  Step6 pedestales: bodies_in={f_ped.bodies.count} faces={body1.faces.count}')

        # Step 7 — Pogo slot (cara trasera Y=0)
        cp_pg = cplanes1.createInput()
        cp_pg.setByOffset(c1.xZConstructionPlane, val(-0.05))
        pl_pg = cplanes1.add(cp_pg)
        sk_pg = c1.sketches.add(pl_pg)
        pw, ph, z_ctr = 1.43/2, 0.83/2, 1.5
        lpg = sk_pg.sketchCurves.sketchLines
        lpg.addByTwoPoints(P.create(-pw, 0, z_ctr-ph), P.create( pw, 0, z_ctr-ph))
        lpg.addByTwoPoints(P.create( pw, 0, z_ctr-ph), P.create( pw, 0, z_ctr+ph))
        lpg.addByTwoPoints(P.create( pw, 0, z_ctr+ph), P.create(-pw, 0, z_ctr+ph))
        lpg.addByTwoPoints(P.create(-pw, 0, z_ctr+ph), P.create(-pw, 0, z_ctr-ph))
        if sk_pg.profiles.count > 0:
            inp_pg = exts1.createInput(sk_pg.profiles.item(0), FO.NewBodyFeatureOperation)
            inp_pg.setOneSideExtent(dist(WT + 0.1), ED.PositiveExtentDirection)
            f_pg = exts1.add(inp_pg)
            pgc = OC.create(); pgc.add(f_pg.bodies.item(0))
            ci_pg = combos1.createInput(body1, pgc)
            ci_pg.operation = FO.CutFeatureOperation; ci_pg.isKeepToolBodies = False
            combos1.add(ci_pg)

        # Step 8 — Patas de silicona (4 recesos en base Z=0)
        pata_pos = [(-8.0, 0.5), (8.0, 0.5), (-8.0, 9.5), (8.0, 9.5)]
        sk_pa = c1.sketches.add(c1.xYConstructionPlane)
        for px, py in pata_pos:
            sk_pa.sketchCurves.sketchCircles.addByCenterRadius(P.create(px, py, 0), PTD/2 - 0.05)
        sel_pa = OC.create()
        for i in range(sk_pa.profiles.count): sel_pa.add(sk_pa.profiles.item(i))
        inp_pa = exts1.createInput(sel_pa, FO.CutFeatureOperation)
        inp_pa.setOneSideExtent(dist(PTH), ED.PositiveExtentDirection)
        inp_pa.participantBodies = [body1]
        exts1.add(inp_pa)
        print(f'  Step8 patas: faces={body1.faces.count}')
        print(f'  01_PETG_Body FINAL: {bbox(body1)}')

        # ==================================================================
        # PART 02 — TOP PANEL ALUMINUM  168 x 100 x 2 mm
        #   Panel angosto X[-8.4,+8.4] = 168mm para no solapar los cheeks
        #   que van hasta X=±9.6. El panel se asienta sobre los pedestales PETG.
        # ==================================================================
        print('\n===== 02_Top_Panel_Aluminum =====')
        occ2 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        c2 = occ2.component; c2.name = '02_Top_Panel_Aluminum'
        exts2    = c2.features.extrudeFeatures
        fillets2 = c2.features.filletFeatures
        cplanes2 = c2.constructionPlanes

        cp_p = cplanes2.createInput()
        cp_p.setByOffset(c2.xYConstructionPlane, val(H))
        pl_p = cplanes2.add(cp_p)

        # Step 1 — footprint 168×100mm (coords relativas Z=0)
        sk2 = c2.sketches.add(pl_p)
        l2 = sk2.sketchCurves.sketchLines
        l2.addByTwoPoints(P.create(-8.4, 0.0, 0), P.create( 8.4, 0.0, 0))
        l2.addByTwoPoints(P.create( 8.4, 0.0, 0), P.create( 8.4,10.0, 0))
        l2.addByTwoPoints(P.create( 8.4,10.0, 0), P.create(-8.4,10.0, 0))
        l2.addByTwoPoints(P.create(-8.4,10.0, 0), P.create(-8.4, 0.0, 0))
        inp2 = exts2.createInput(sk2.profiles.item(0), FO.NewBodyFeatureOperation)
        inp2.setOneSideExtent(dist(PT), ED.PositiveExtentDirection)
        body2 = exts2.add(inp2).bodies.item(0); body2.name = 'Aluminum_Panel'
        print(f'  Step1: {bbox(body2)}')

        # Fillet R3mm aristas verticales
        fe2 = OC.create()
        for e in body2.edges:
            v1, v2 = e.startVertex.geometry, e.endVertex.geometry
            if abs(v1.x-v2.x)<1e-4 and abs(v1.y-v2.y)<1e-4 and abs(v1.z-v2.z)>1e-3:
                fe2.add(e)
        if fe2.count > 0:
            fi2 = fillets2.createInput()
            fi2.addConstantRadiusEdgeSet(fe2, val(0.3), True)
            fillets2.add(fi2)

        # Step 2 — holes circulares: encoder screws, LED mount, pogo, display, 4× M3 corners
        # Posiciones en mm desde borde izquierdo (0..180) y profundidad (0..100)
        # cx() y cy() convierten a cm desde centro
        sk2h = c2.sketches.add(pl_p)
        holes = [
            (22,  55, 6.5),   # encoder mount screw izquierdo
            (158, 55, 6.5),   # encoder mount screw derecho
            # (90, 22) → encoder shaft + LED ring en Step 4 (no Ø6.5 aquí)
            (22,  14, 7.0),   # pogo connector mount
            (90,  60, 36.0),  # display Ø36mm
            (10,   8, 3.2),   # M3 esquina FI (trasera-izq)
            (10,  92, 3.2),   # M3 esquina FD (frontal-izq)
            (170,  8, 3.2),   # M3 esquina TI (trasera-der)
            (170, 92, 3.2),   # M3 esquina TD (frontal-der)
        ]
        for xp, yp, d in holes:
            sk2h.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(cx(xp), cy(yp), 0), d/2/10)
        sel2h = OC.create()
        for i in range(sk2h.profiles.count): sel2h.add(sk2h.profiles.item(i))
        inp2h = exts2.createInput(sel2h, FO.CutFeatureOperation)
        inp2h.setOneSideExtent(dist(PT + 0.05), ED.PositiveExtentDirection)
        inp2h.participantBodies = [body2]
        exts2.add(inp2h)
        print(f'  Step2 round holes: faces={body2.faces.count}')

        # Step 3 — button cutouts 14×14mm (4 botones en 2 columnas v0.5)
        # Columna izq X=52mm, Columna der X=128mm, filas Y=66mm y Y=44mm
        sk2b = c2.sketches.add(pl_p)
        half = 0.7  # 7mm = mitad de 14mm
        for bx, by in [(52,66),(52,44),(128,66),(128,44)]:
            x0, y0 = cx(bx), cy(by)
            lb = sk2b.sketchCurves.sketchLines
            lb.addByTwoPoints(P.create(x0-half,y0-half,0),P.create(x0+half,y0-half,0))
            lb.addByTwoPoints(P.create(x0+half,y0-half,0),P.create(x0+half,y0+half,0))
            lb.addByTwoPoints(P.create(x0+half,y0+half,0),P.create(x0-half,y0+half,0))
            lb.addByTwoPoints(P.create(x0-half,y0+half,0),P.create(x0-half,y0-half,0))
        sel2b = OC.create()
        for i in range(sk2b.profiles.count): sel2b.add(sk2b.profiles.item(i))
        inp2b = exts2.createInput(sel2b, FO.CutFeatureOperation)
        inp2b.setOneSideExtent(dist(PT + 0.05), ED.PositiveExtentDirection)
        inp2b.participantBodies = [body2]
        exts2.add(inp2b)
        print(f'  Step3 button cutouts: faces={body2.faces.count}')

        # Step 4 — Nav encoder: shaft Ø7mm + LED indicator ring (12 × Ø2.5mm @ R=12mm)
        # El encoder nav tiene solo el orificio del eje; los LEDs pasan por 12 pequeños
        # orificios dispuestos en anillo — sin ventana abierta Ø32mm.
        EX, EY = cx(90), cy(22)   # centro encoder: X=0.0cm, Y=2.2cm
        sk2r = c2.sketches.add(pl_p)

        # Orificio del eje Ø7mm (solo la palanca/knob pasa por aquí)
        sk2r.sketchCurves.sketchCircles.addByCenterRadius(
            P.create(EX, EY, 0), 0.35)   # r=3.5mm

        # Anillo de LEDs: 12 × Ø2.5mm a R=12mm del centro
        R_LED = 1.6   # 16mm en cm (v3.4 — anillo ampliado para mejor visibilidad)
        for k in range(12):
            ang = math.radians(k * 30)
            sk2r.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(EX + R_LED * math.cos(ang),
                         EY + R_LED * math.sin(ang), 0),
                0.125)   # r=1.25mm → Ø2.5mm

        sel2r = OC.create()
        for i in range(sk2r.profiles.count): sel2r.add(sk2r.profiles.item(i))
        # 13 profiles: 1 eje + 12 LED  — todos son orificios, no incluye el fondo del panel
        inp2r = exts2.createInput(sel2r, FO.CutFeatureOperation)
        inp2r.setOneSideExtent(dist(PT + 0.05), ED.PositiveExtentDirection)
        inp2r.participantBodies = [body2]
        exts2.add(inp2r)
        print(f'  Step4 encoder+LED ring: {sk2r.profiles.count} profiles → faces={body2.faces.count}')
        print(f'  02_Top_Panel FINAL: {bbox(body2)}  bodies_in_comp={c2.bRepBodies.count}')

        # ==================================================================
        # PART 03 — CHEEK LEFT GUADUA  12 x 100 x 42 mm
        #   H_CHEEK = 42mm — 2mm proud sobre panel top (Z=40mm)
        #   Ports cortados en cara exterior (cara de guadua visible)
        # ==================================================================
        print('\n===== 03_Cheek_Left_Guadua =====')
        occ3 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        c3 = occ3.component; c3.name = '03_Cheek_Left_Guadua'
        exts3   = c3.features.extrudeFeatures
        combos3 = c3.features.combineFeatures
        cplanes3 = c3.constructionPlanes

        # Cuerpo principal
        sk3 = c3.sketches.add(c3.xYConstructionPlane)
        l3 = sk3.sketchCurves.sketchLines
        l3.addByTwoPoints(P.create(X_OUTER_L, 0.0, 0), P.create(X_INNER_L, 0.0, 0))
        l3.addByTwoPoints(P.create(X_INNER_L, 0.0, 0), P.create(X_INNER_L,10.0, 0))
        l3.addByTwoPoints(P.create(X_INNER_L,10.0, 0), P.create(X_OUTER_L,10.0, 0))
        l3.addByTwoPoints(P.create(X_OUTER_L,10.0, 0), P.create(X_OUTER_L, 0.0, 0))
        inp3 = exts3.createInput(sk3.profiles.item(0), FO.NewBodyFeatureOperation)
        inp3.setOneSideExtent(dist(H_CHEEK), ED.PositiveExtentDirection)
        body3 = exts3.add(inp3).bodies.item(0); body3.name = 'Cheek_Left'
        print(f'  Step1: {bbox(body3)}')

        # Fillet R2mm aristas verticales
        fe3 = OC.create()
        for e in body3.edges:
            v1, v2 = e.startVertex.geometry, e.endVertex.geometry
            if abs(v1.x-v2.x)<1e-4 and abs(v1.y-v2.y)<1e-4 and abs(v1.z-v2.z)>1e-3:
                fe3.add(e)
        if fe3.count > 0:
            fi3 = c3.features.filletFeatures.createInput()
            fi3.addConstantRadiusEdgeSet(fe3, val(0.2), True)
            c3.features.filletFeatures.add(fi3)

        # Cutouts en cara exterior X=X_OUTER_L=-9.6cm (cara visible de la guadua)
        #   TRS IN L: Ø11mm @ Y=30mm
        #   TRS IN R: Ø11mm @ Y=55mm
        #   AUX 3.5mm: Ø7.5mm @ Y=78mm
        #   Centro en Z=JZ=21mm (mitad de la altura de conexion)
        cp3o = cplanes3.createInput()
        cp3o.setByOffset(c3.yZConstructionPlane, val(X_OUTER_L))
        pl3o = cplanes3.add(cp3o)
        sk3c = c3.sketches.add(pl3o)
        # sketch_x = -Z_mundo, sketch_y = +Y_mundo → P.create(-JZ, y_cm, 0)
        for y_cm, r_cm in [(3.0, 0.55), (5.5, 0.55), (7.8, 0.375)]:
            sk3c.sketchCurves.sketchCircles.addByCenterRadius(P.create(-JZ, y_cm, 0), r_cm)
        sel3c = OC.create()
        for i in range(sk3c.profiles.count): sel3c.add(sk3c.profiles.item(i))
        print(f'  cutout profiles: {sk3c.profiles.count}  (esperado 3)')
        if sk3c.profiles.count > 0:
            # NewBodyFeatureOperation + CombineFeature — patrón obligatorio para yZConstructionPlane
            # Cara exterior X=-9.6: PositiveExtentDirection = +X = interior del cheek izquierdo
            inp3c = exts3.createInput(sel3c, FO.NewBodyFeatureOperation)
            inp3c.setOneSideExtent(dist(CT + 0.1), ED.PositiveExtentDirection)
            f3c = exts3.add(inp3c)
            cc3 = OC.create()
            for i in range(f3c.bodies.count): cc3.add(f3c.bodies.item(i))
            ci3c = combos3.createInput(body3, cc3)
            ci3c.operation = FO.CutFeatureOperation; ci3c.isKeepToolBodies = False
            combos3.add(ci3c)

        # Heat inserts cara interior X=X_INNER_L=-8.4cm (2× M3 para fijación al PETG)
        cp3i = cplanes3.createInput()
        cp3i.setByOffset(c3.yZConstructionPlane, val(X_INNER_L))
        pl3i = cplanes3.add(cp3i)
        sk3f = c3.sketches.add(pl3i)
        # sketch_x = -Z_mundo → P.create(-H/2, y_cm, 0) coloca en Z_mundo=H/2=19mm
        for y_cm in [2.5, 7.5]:
            sk3f.sketchCurves.sketchCircles.addByCenterRadius(P.create(-H/2, y_cm, 0), 0.21)
        sel3f = OC.create()
        for i in range(sk3f.profiles.count): sel3f.add(sk3f.profiles.item(i))
        if sel3f.count > 0:
            # NewBodyFeatureOperation + CombineFeature — patrón obligatorio para yZConstructionPlane
            # Cara interior X=-8.4: NegativeExtentDirection = -X = adentro del cheek izquierdo
            inp3f = exts3.createInput(sel3f, FO.NewBodyFeatureOperation)
            inp3f.setOneSideExtent(dist(0.7), ED.NegativeExtentDirection)
            f3f = exts3.add(inp3f)
            cf3 = OC.create()
            for i in range(f3f.bodies.count): cf3.add(f3f.bodies.item(i))
            ci3f = combos3.createInput(body3, cf3)
            ci3f.operation = FO.CutFeatureOperation; ci3f.isKeepToolBodies = False
            combos3.add(ci3f)
        print(f'  03_Cheek_Left FINAL: {bbox(body3)}')

        # ==================================================================
        # PART 04 — CHEEK RIGHT GUADUA  12 x 100 x 42 mm
        #   Ports: USB-C Data (Y=15mm), TRS OUT L (Y=38mm),
        #          TRS OUT R (Y=60mm), USB-C Host (Y=82mm)
        # ==================================================================
        print('\n===== 04_Cheek_Right_Guadua =====')
        occ4 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        c4 = occ4.component; c4.name = '04_Cheek_Right_Guadua'
        exts4    = c4.features.extrudeFeatures
        combos4  = c4.features.combineFeatures
        cplanes4 = c4.constructionPlanes

        sk4 = c4.sketches.add(c4.xYConstructionPlane)
        l4 = sk4.sketchCurves.sketchLines
        l4.addByTwoPoints(P.create(X_INNER_R, 0.0, 0), P.create(X_OUTER_R, 0.0, 0))
        l4.addByTwoPoints(P.create(X_OUTER_R, 0.0, 0), P.create(X_OUTER_R,10.0, 0))
        l4.addByTwoPoints(P.create(X_OUTER_R,10.0, 0), P.create(X_INNER_R,10.0, 0))
        l4.addByTwoPoints(P.create(X_INNER_R,10.0, 0), P.create(X_INNER_R, 0.0, 0))
        inp4 = exts4.createInput(sk4.profiles.item(0), FO.NewBodyFeatureOperation)
        inp4.setOneSideExtent(dist(H_CHEEK), ED.PositiveExtentDirection)
        body4 = exts4.add(inp4).bodies.item(0); body4.name = 'Cheek_Right'

        fe4 = OC.create()
        for e in body4.edges:
            v1, v2 = e.startVertex.geometry, e.endVertex.geometry
            if abs(v1.x-v2.x)<1e-4 and abs(v1.y-v2.y)<1e-4 and abs(v1.z-v2.z)>1e-3:
                fe4.add(e)
        if fe4.count > 0:
            fi4 = c4.features.filletFeatures.createInput()
            fi4.addConstantRadiusEdgeSet(fe4, val(0.2), True)
            c4.features.filletFeatures.add(fi4)

        # Cutouts cara exterior X=X_OUTER_R=+9.6cm
        cp4o = cplanes4.createInput()
        cp4o.setByOffset(c4.yZConstructionPlane, val(X_OUTER_R))
        pl4o = cplanes4.add(cp4o)
        sk4c = c4.sketches.add(pl4o)
        # sketch_x = -Z_mundo, sketch_y = +Y_mundo → P.create(-z, y, 0)
        # TRS circulares
        for y_cm in [3.8, 6.0]:
            sk4c.sketchCurves.sketchCircles.addByCenterRadius(P.create(-JZ, y_cm, 0), 0.55)
        # USB-C rectangulares 11×5.5mm
        # UW2: mitad en Y (profundidad 11mm), UH2: mitad en Z (altura 5.5mm)
        UW2, UH2 = 1.1/2, 0.55/2
        for y_cm in [1.5, 8.2]:
            y0, y1 = y_cm - UW2, y_cm + UW2
            z0, z1 = JZ - UH2, JZ + UH2
            l4c = sk4c.sketchCurves.sketchLines
            l4c.addByTwoPoints(P.create(-z0, y0, 0), P.create(-z0, y1, 0))
            l4c.addByTwoPoints(P.create(-z0, y1, 0), P.create(-z1, y1, 0))
            l4c.addByTwoPoints(P.create(-z1, y1, 0), P.create(-z1, y0, 0))
            l4c.addByTwoPoints(P.create(-z1, y0, 0), P.create(-z0, y0, 0))
        sel4c = OC.create()
        for i in range(sk4c.profiles.count): sel4c.add(sk4c.profiles.item(i))
        print(f'  cutout profiles: {sk4c.profiles.count}  (esperado 4: 2 circ + 2 rect)')
        if sk4c.profiles.count > 0:
            # NewBodyFeatureOperation + CombineFeature — patrón obligatorio para yZConstructionPlane
            # Cara exterior X=+9.6: NegativeExtentDirection = -X = interior del cheek derecho
            inp4c = exts4.createInput(sel4c, FO.NewBodyFeatureOperation)
            inp4c.setOneSideExtent(dist(CT + 0.1), ED.NegativeExtentDirection)
            f4c = exts4.add(inp4c)
            cc4 = OC.create()
            for i in range(f4c.bodies.count): cc4.add(f4c.bodies.item(i))
            ci4c = combos4.createInput(body4, cc4)
            ci4c.operation = FO.CutFeatureOperation; ci4c.isKeepToolBodies = False
            combos4.add(ci4c)

        # Heat inserts cara interior X=+8.4cm
        cp4i = cplanes4.createInput()
        cp4i.setByOffset(c4.yZConstructionPlane, val(X_INNER_R))
        pl4i = cplanes4.add(cp4i)
        sk4f = c4.sketches.add(pl4i)
        # sketch_x = -Z_mundo → P.create(-H/2, y_cm, 0) coloca en Z_mundo=H/2=19mm
        for y_cm in [2.5, 7.5]:
            sk4f.sketchCurves.sketchCircles.addByCenterRadius(P.create(-H/2, y_cm, 0), 0.21)
        sel4f = OC.create()
        for i in range(sk4f.profiles.count): sel4f.add(sk4f.profiles.item(i))
        if sel4f.count > 0:
            # NewBodyFeatureOperation + CombineFeature — patrón obligatorio para yZConstructionPlane
            # Cara interior X=+8.4: PositiveExtentDirection = +X = adentro del cheek derecho
            inp4f = exts4.createInput(sel4f, FO.NewBodyFeatureOperation)
            inp4f.setOneSideExtent(dist(0.7), ED.PositiveExtentDirection)
            f4f = exts4.add(inp4f)
            cf4 = OC.create()
            for i in range(f4f.bodies.count): cf4.add(f4f.bodies.item(i))
            ci4f = combos4.createInput(body4, cf4)
            ci4f.operation = FO.CutFeatureOperation; ci4f.isKeepToolBodies = False
            combos4.add(ci4f)
        print(f'  04_Cheek_Right FINAL: {bbox(body4)}')

        # ==================================================================
        # PART 05 — DISPLAY BRACKET  (cuelga de Z=H hacia Z=H-1.8)
        #   Brida cuadrada (45×45mm) con collar + tubo para Waveshare 1.28"
        #   PCB Ø41mm | glass OD Ø38.51mm | VA Ø35.67mm | cutout panel Ø36mm
        # ==================================================================
        print('\n===== 05_Display_Bracket =====')
        occ5 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        c5 = occ5.component; c5.name = '05_Display_Bracket'
        exts5    = c5.features.extrudeFeatures
        cplanes5 = c5.constructionPlanes

        BX, BY = 0.0, 6.0                 # centro del display (X=0, Y=60mm desde back)
        FW     = 4.5/2                     # medio lado de la brida cuadrada
        TUBE_OD, TUBE_ID, COL_ID = 4.5, 4.2, 3.65

        # Step 1 — flange anular (cuadrado 45mm con agujero Ø45mm) en Z=H
        cp5a = cplanes5.createInput()
        cp5a.setByOffset(c5.xYConstructionPlane, val(H))
        pl5a = cplanes5.add(cp5a)
        sk5a = c5.sketches.add(pl5a)
        l5a = sk5a.sketchCurves.sketchLines
        l5a.addByTwoPoints(P.create(BX-FW,BY-FW,0),P.create(BX+FW,BY-FW,0))
        l5a.addByTwoPoints(P.create(BX+FW,BY-FW,0),P.create(BX+FW,BY+FW,0))
        l5a.addByTwoPoints(P.create(BX+FW,BY+FW,0),P.create(BX-FW,BY+FW,0))
        l5a.addByTwoPoints(P.create(BX-FW,BY+FW,0),P.create(BX-FW,BY-FW,0))
        sk5a.sketchCurves.sketchCircles.addByCenterRadius(P.create(BX, BY, 0), TUBE_OD/2)
        profs5 = [sk5a.profiles.item(i) for i in range(sk5a.profiles.count)]
        profs5.sort(key=lambda p: p.areaProperties().area)
        inp5a = exts5.createInput(profs5[0], FO.NewBodyFeatureOperation)
        inp5a.setOneSideExtent(dist(0.2), ED.NegativeExtentDirection)
        body5 = exts5.add(inp5a).bodies.item(0); body5.name = 'Display_Bracket'

        # Step 2 — collar intermedio (anulo Ø45/Ø36.5mm, 1mm)
        cp5b = cplanes5.createInput()
        cp5b.setByOffset(c5.xYConstructionPlane, val(H - 0.2))
        pl5b = cplanes5.add(cp5b)
        sk5b = c5.sketches.add(pl5b)
        sk5b.sketchCurves.sketchCircles.addByCenterRadius(P.create(BX,BY,0), TUBE_OD/2)
        sk5b.sketchCurves.sketchCircles.addByCenterRadius(P.create(BX,BY,0), COL_ID/2)
        prof5b = [sk5b.profiles.item(i) for i in range(sk5b.profiles.count)]
        prof5b.sort(key=lambda p: p.areaProperties().area)
        inp5b = exts5.createInput(prof5b[0], FO.JoinFeatureOperation)
        inp5b.setOneSideExtent(dist(0.1), ED.NegativeExtentDirection)
        inp5b.participantBodies = [body5]
        exts5.add(inp5b)

        # Step 3 — tubo principal (Ø45/Ø42mm, 15mm hacia abajo)
        cp5c = cplanes5.createInput()
        cp5c.setByOffset(c5.xYConstructionPlane, val(H - 0.3))
        pl5c = cplanes5.add(cp5c)
        sk5c = c5.sketches.add(pl5c)
        sk5c.sketchCurves.sketchCircles.addByCenterRadius(P.create(BX,BY,0), TUBE_OD/2)
        sk5c.sketchCurves.sketchCircles.addByCenterRadius(P.create(BX,BY,0), TUBE_ID/2)
        prof5c = [sk5c.profiles.item(i) for i in range(sk5c.profiles.count)]
        prof5c.sort(key=lambda p: p.areaProperties().area)
        inp5c = exts5.createInput(prof5c[0], FO.JoinFeatureOperation)
        inp5c.setOneSideExtent(dist(1.5), ED.NegativeExtentDirection)
        inp5c.participantBodies = [body5]
        exts5.add(inp5c)
        print(f'  05_Display_Bracket FINAL: {bbox(body5)}')

        # ==================================================================
        # PART 06 — CONTROLS VISUAL  (render reference — no funcional)
        #   Knobs estilo Moog: frustum (loft base→top) + pointer nub en cara
        #   frontal (+Y). Botones retroiluminados, pantalla, cuerpos conectores.
        #
        #   Moog knob ratios (verificados contra Minimoog/Grandmother):
        #     Encoder L/R : base Ø18mm → top Ø13mm, h=22mm
        #     Nav encoder : base Ø15mm → top Ø11mm, h=18mm  (reducido v3.4)
        #   LED ring: R=16mm (ampliado de 12mm, coincide con panel Part 02)
        # ==================================================================
        print('\n===== 06_Controls_Visual =====')
        occ6 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        c6 = occ6.component; c6.name = '06_Controls_Visual'
        exts6    = c6.features.extrudeFeatures
        lofts6   = c6.features.loftFeatures
        combos6  = c6.features.combineFeatures
        cplanes6 = c6.constructionPlanes

        Z_BASE = H + PT   # 4.0cm — base de los knobs sobre el panel

        def make_knob(kx, ky, base_r, top_r, kh, kname):
            """Knob estilo Moog: frustum (loft) + pointer nub en cara frontal (+Y)."""
            # Bottom circle at Z_BASE
            cp_b = cplanes6.createInput()
            cp_b.setByOffset(c6.xYConstructionPlane, val(Z_BASE))
            pl_b = cplanes6.add(cp_b)
            sk_b = c6.sketches.add(pl_b)
            sk_b.sketchCurves.sketchCircles.addByCenterRadius(P.create(kx, ky, 0), base_r)
            # Top circle at Z_BASE + kh
            cp_t = cplanes6.createInput()
            cp_t.setByOffset(c6.xYConstructionPlane, val(Z_BASE + kh))
            pl_t = cplanes6.add(cp_t)
            sk_t = c6.sketches.add(pl_t)
            sk_t.sketchCurves.sketchCircles.addByCenterRadius(P.create(kx, ky, 0), top_r)
            # Loft: frustum cónico
            inp_l = lofts6.createInput(FO.NewBodyFeatureOperation)
            inp_l.loftSections.add(sk_b.profiles.item(0))
            inp_l.loftSections.add(sk_t.profiles.item(0))
            knob = lofts6.add(inp_l).bodies.item(0)
            knob.name = kname
            # Pointer nub: Ø3mm × 2mm, en cara frontal (max +Y desde centro del knob)
            cp_n = cplanes6.createInput()
            cp_n.setByOffset(c6.xYConstructionPlane, val(Z_BASE + kh))
            pl_n = cplanes6.add(cp_n)
            sk_n = c6.sketches.add(pl_n)
            sk_n.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(kx, ky + top_r * 0.55, 0), 0.15)  # r=1.5mm toward user
            inp_n = exts6.createInput(sk_n.profiles.item(0), FO.NewBodyFeatureOperation)
            inp_n.setOneSideExtent(dist(0.2), ED.PositiveExtentDirection)
            f_n = exts6.add(inp_n)
            nc = OC.create()
            for i in range(f_n.bodies.count): nc.add(f_n.bodies.item(i))
            ci_n = combos6.createInput(knob, nc)
            ci_n.operation = FO.JoinFeatureOperation; ci_n.isKeepToolBodies = False
            combos6.add(ci_n)
            return knob

        # Encoders L/R — estilo Moog clásico: base Ø18mm, top Ø13mm, h=22mm
        make_knob(cx(22),  cy(55), 0.90, 0.65, 2.2, 'Knob_EncL')
        make_knob(cx(158), cy(55), 0.90, 0.65, 2.2, 'Knob_EncR')
        # Nav encoder — reducido v3.4: base Ø15mm, top Ø11mm, h=18mm
        make_knob(cx(90),  cy(22), 0.75, 0.55, 1.8, 'Knob_Nav')
        print('  Knobs Moog OK (L/R: Ø18→13mm h22 | Nav: Ø15→11mm h18)')

        # -- Button caps 13×13mm × 2.5mm (retroiluminados — translucido blanco)
        for bx, by in [(52,66),(52,44),(128,66),(128,44)]:
            x0, y0 = cx(bx), cy(by)
            cp_bc = cplanes6.createInput()
            cp_bc.setByOffset(c6.xYConstructionPlane, val(Z_BASE))
            pl_bc = cplanes6.add(cp_bc)
            sk_bc = c6.sketches.add(pl_bc)
            hb = 0.65  # 6.5mm = mitad de 13mm
            lb = sk_bc.sketchCurves.sketchLines
            lb.addByTwoPoints(P.create(x0-hb,y0-hb,0),P.create(x0+hb,y0-hb,0))
            lb.addByTwoPoints(P.create(x0+hb,y0-hb,0),P.create(x0+hb,y0+hb,0))
            lb.addByTwoPoints(P.create(x0+hb,y0+hb,0),P.create(x0-hb,y0+hb,0))
            lb.addByTwoPoints(P.create(x0-hb,y0+hb,0),P.create(x0-hb,y0-hb,0))
            if sk_bc.profiles.count > 0:
                inp_bc = exts6.createInput(sk_bc.profiles.item(0), FO.NewBodyFeatureOperation)
                inp_bc.setOneSideExtent(dist(0.25), ED.PositiveExtentDirection)
                b_bc = exts6.add(inp_bc).bodies.item(0)
                b_bc.name = f'Btn_{bx}_{by}'
        print('  Button caps OK')

        # -- Display screen face Ø35mm × 2mm (verde — brand color GrooveForge)
        cp_ds = cplanes6.createInput()
        cp_ds.setByOffset(c6.xYConstructionPlane, val(H + 0.05))
        pl_ds = cplanes6.add(cp_ds)
        sk_ds = c6.sketches.add(pl_ds)
        sk_ds.sketchCurves.sketchCircles.addByCenterRadius(P.create(0.0, 6.0, 0), 1.75)
        if sk_ds.profiles.count > 0:
            inp_ds = exts6.createInput(sk_ds.profiles.item(0), FO.NewBodyFeatureOperation)
            inp_ds.setOneSideExtent(dist(0.2), ED.PositiveExtentDirection)
            b_ds = exts6.add(inp_ds).bodies.item(0)
            b_ds.name = 'Display_Screen'
        print('  Display screen OK')

        # -- Conectores cheek izquierdo (cara X=X_OUTER_L, protruyen 4mm hacia afuera)
        #    TRS IN L (Y=30mm), TRS IN R (Y=55mm), AUX 3.5mm (Y=78mm)
        cp_lc = cplanes6.createInput()
        cp_lc.setByOffset(c6.yZConstructionPlane, val(X_OUTER_L - 0.4))
        pl_lc = cplanes6.add(cp_lc)
        sk_lc = c6.sketches.add(pl_lc)
        for y_cm, r_cm in [(3.0, 0.47), (5.5, 0.47), (7.8, 0.28)]:
            sk_lc.sketchCurves.sketchCircles.addByCenterRadius(P.create(-JZ, y_cm, 0), r_cm)
        sel_lc = OC.create()
        for i in range(sk_lc.profiles.count): sel_lc.add(sk_lc.profiles.item(i))
        if sel_lc.count > 0:
            inp_lc = exts6.createInput(sel_lc, FO.NewBodyFeatureOperation)
            inp_lc.setOneSideExtent(dist(0.4), ED.NegativeExtentDirection)
            f_lc = exts6.add(inp_lc)
            for i in range(f_lc.bodies.count):
                f_lc.bodies.item(i).name = f'Conn_L_{i}'
        print('  Conectores izq OK')

        # -- Conectores cheek derecho (cara X=X_OUTER_R, protruyen 4mm hacia afuera)
        #    USB-C Data (Y=15mm), TRS OUT L (Y=38mm), TRS OUT R (Y=60mm), USB-C Host (Y=82mm)
        cp_rc = cplanes6.createInput()
        cp_rc.setByOffset(c6.yZConstructionPlane, val(X_OUTER_R + 0.4))
        pl_rc = cplanes6.add(cp_rc)
        sk_rc = c6.sketches.add(pl_rc)
        for y_cm in [3.8, 6.0]:   # TRS circles
            sk_rc.sketchCurves.sketchCircles.addByCenterRadius(P.create(-JZ, y_cm, 0), 0.47)
        UW2r, UH2r = 0.45, 0.20   # USB-C bodies 9×4mm
        for y_cm in [1.5, 8.2]:
            y0r, y1r = y_cm - UW2r, y_cm + UW2r
            z0r, z1r = JZ - UH2r, JZ + UH2r
            lrc = sk_rc.sketchCurves.sketchLines
            lrc.addByTwoPoints(P.create(-z0r, y0r, 0), P.create(-z0r, y1r, 0))
            lrc.addByTwoPoints(P.create(-z0r, y1r, 0), P.create(-z1r, y1r, 0))
            lrc.addByTwoPoints(P.create(-z1r, y1r, 0), P.create(-z1r, y0r, 0))
            lrc.addByTwoPoints(P.create(-z1r, y0r, 0), P.create(-z0r, y0r, 0))
        sel_rc = OC.create()
        for i in range(sk_rc.profiles.count): sel_rc.add(sk_rc.profiles.item(i))
        if sel_rc.count > 0:
            inp_rc = exts6.createInput(sel_rc, FO.NewBodyFeatureOperation)
            inp_rc.setOneSideExtent(dist(0.4), ED.PositiveExtentDirection)
            f_rc = exts6.add(inp_rc)
            for i in range(f_rc.bodies.count):
                f_rc.bodies.item(i).name = f'Conn_R_{i}'
        print('  Conectores der OK')
        print('  06_Controls_Visual COMPLETO')

        # ==================================================================
        # COLORES POR MATERIAL (00-master-strategy.md §8.2)
        #   PETG body / display bracket / knobs → dark anthracite → Matte Black
        #   Aluminum panel → anodized grey → Aluminum Anodized Rough
        #   Guadua cheeks  → bamboo natural → Bamboo Light Semigloss
        #   Buttons        → retroiluminado → Translucent Glossy White
        #   Connectors     → brushed metal  → Aluminum Satin
        #   Display screen → brand green    → Paint Enamel Glossy Green
        # ==================================================================
        try:
            app_lib = app.materialLibraries.itemByName('Fusion Appearance Library')
            if app_lib:
                def get_app(name):
                    des_nm = f'GF_{name.replace(" ","_").replace("-","_")}'
                    existing = des.appearances.itemByName(des_nm)
                    if existing: return existing
                    src = app_lib.appearances.itemByName(name)
                    return des.appearances.addByCopy(src, des_nm) if src else None

                body_colors = {
                    'PETG_Body':       'Plastic - Matte (Black)',
                    'Aluminum_Panel':  'Aluminum - Anodized Rough (Grey)',
                    'Cheek_Left':      'Bamboo Light - Semigloss',
                    'Cheek_Right':     'Bamboo Light - Semigloss',
                    'Display_Bracket': 'Plastic - Matte (Black)',
                    'Display_Screen':  'Paint - Enamel Glossy (Green)',
                }
                for occ in root.occurrences:
                    cmp = occ.component
                    for j in range(cmp.bRepBodies.count):
                        b = cmp.bRepBodies.item(j)
                        nm = b.name
                        app_name = body_colors.get(nm)
                        if app_name is None:
                            if nm.startswith('Knob_'):    app_name = 'Plastic - Matte (Black)'
                            elif nm.startswith('Btn_'):   app_name = 'Plastic - Translucent Glossy (White)'
                            elif nm.startswith('Conn_'):  app_name = 'Aluminum - Satin'
                        if app_name:
                            a = get_app(app_name)
                            if a: b.appearance = a
                print('  Colores aplicados OK')
        except Exception as e_col:
            print(f'  Colores (warning): {e_col}')

        # ==================================================================
        # RESUMEN FINAL
        # ==================================================================
        print('\n========== BUILD COMPLETO ==========')
        for occ in root.occurrences:
            cmp = occ.component
            for j in range(cmp.bRepBodies.count):
                b = cmp.bRepBodies.item(j)
                bb = b.boundingBox; mn, mx = bb.minPoint, bb.maxPoint
                print(f'  {cmp.name}.{b.name}: '
                      f'X[{mn.x*10:.0f},{mx.x*10:.0f}] '
                      f'Y[{mn.y*10:.0f},{mx.y*10:.0f}] '
                      f'Z[{mn.z*10:.0f},{mx.z*10:.0f}] '
                      f'W{(mx.x-mn.x)*10:.0f} H{(mx.z-mn.z)*10:.0f}mm '
                      f'faces={b.faces.count}')

    except Exception:
        print(traceback.format_exc())

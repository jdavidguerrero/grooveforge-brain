# =============================================================================
# GrooveForge Brain — 01_PETG_Body (v0.6 — Caja rectangular para stack modular)
# Script Fusion 360 (Python API) — v0.8
#
# CAMBIOS vs v0.7:
#   - Revertido bug fix #3 de v0.7: los cutouts de jacks/USB SÍ son necesarios
#     en el PETG body. El rebaje de 6mm elimina la pared exterior, pero la pared
#     interna (WALL_T=3mm) en X=±8.4cm permanece y debe tener los cutouts para
#     que los conectores pasen al interior. Agregado Step 9.
#
# BUGS CORREGIDOS vs v0.6 (mantenidos desde v0.7):
#   1. Pedestal bodies: recolectar TODOS los 4 bodies antes del Join.
#   2. Rebajes laterales: NewBodyFeatureOperation + combineFeatures CUT.
#   4. Rubber feet depth: PTH=2mm (ciego, 1mm de base).
#
# CÓMO USAR:
#   Shift+S en Fusion → Scripts → "+" → Python → pegar → Run
#   Componente '01_PETG_Body' debe estar VACÍO. Si ya tiene contenido, borrar primero.
#
# SISTEMA DE COORDENADAS:
#   X: ±9.0cm (180mm ancho, 0=centro)
#   Y: 0..3.8cm (38mm altura, Y=0=base exterior)
#   Z: 0..10.0cm (100mm profundidad, Z=0=cara trasera Pogo, Z=10=cara frontal)
#
# RESULTADO:
#   Cuerpo PETG 180 × 100 × 38mm
#   ├── Shell interior (paredes 3mm, base 3mm, top ABIERTO)
#   ├── Rebajes laterales 6mm para cheeks (NewBody+Combine)
#   ├── 6 standoffs PCB M3 (5mm alto, heat insert 4.2mm)
#   ├── 4 pedestales heat insert M3 para top panel aluminio (TODOS unidos)
#   ├── Cutout pared trasera: slot Pogo 6-pin (14.3 × 8.3mm)
#   ├── 4 cavidades patas silicona en base (2mm prof, ciego)
#   ├── Cutouts pared interna izq: TRS INL/INR Ø11mm + AUX 3.5mm Ø7.5mm
#   └── Cutouts pared interna der: USB-C Data/Host 10.8×5.3mm + TRS OUTL/R Ø11mm
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

        def pt(x, y, z): return P.create(x, y, z)
        def dist(v):     return DD.create(VI.createByReal(v))
        def val(v):      return VI.createByReal(v)

        # ------------------------------------------------------------------
        # PARÁMETROS (cm — unidad interna de Fusion)
        # ------------------------------------------------------------------
        W   = 18.0   # BODY_W   = 180mm
        D   = 10.0   # BODY_D   = 100mm
        H   = 3.8    # BODY_H   = 38mm  (v0.6 rectangular)
        WT  = 0.3    # WALL_T   = 3mm
        JZ  = 2.1    # JACK_Z   = 21mm (altura Y de jacks — referencia, no usado aquí)
        SH  = 0.5    # STANDOFF_H  = 5mm
        SOD = 0.6    # STANDOFF_OD = 6mm → radio 3mm
        IMD = 0.42   # INSERT_M3_OD = 4.2mm → radio 2.1mm
        IMH = 0.6    # INSERT_M3_H  = 6mm (profundidad agujero heat insert)
        RD  = 0.6    # CHEEK_REBAJE_D = 6mm
        PTD = 1.0    # PATA_D = 10mm → radio 5mm (cavity Ø9.9mm)
        PTH = 0.2    # PATA_H = 2mm (ciego — corregido de 3mm que era pasante)

        # ------------------------------------------------------------------
        # LOCALIZAR O CREAR COMPONENTE
        # ------------------------------------------------------------------
        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '01' in n or 'PETG' in n or 'petg' in n:
                comp = occ.component
                break
        if comp is None:
            occ2 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
            comp = occ2.component
            comp.name = '01_PETG_Body'

        exts    = comp.features.extrudeFeatures
        fillets = comp.features.filletFeatures
        combos  = comp.features.combineFeatures
        cplanes = comp.constructionPlanes

        # ==================================================================
        # STEP 1 — CUERPO EXTERIOR SÓLIDO (180 × 100 × 38mm)
        # ==================================================================
        sk1 = comp.sketches.add(comp.xZConstructionPlane)
        sk1.name = 'Base_Footprint'
        ln = sk1.sketchCurves.sketchLines
        ln.addByTwoPoints(pt(-9.0, 0, 0.0), pt( 9.0, 0, 0.0))
        ln.addByTwoPoints(pt( 9.0, 0, 0.0), pt( 9.0, 0,10.0))
        ln.addByTwoPoints(pt( 9.0, 0,10.0), pt(-9.0, 0,10.0))
        ln.addByTwoPoints(pt(-9.0, 0,10.0), pt(-9.0, 0, 0.0))

        inp1 = exts.createInput(sk1.profiles.item(0), FO.NewBodyFeatureOperation)
        inp1.setOneSideExtent(dist(H), ED.PositiveExtentDirection)
        feat1 = exts.add(inp1)
        feat1.bodies.item(0).name = 'PETG_Body'
        body = comp.bRepBodies.item(0)
        print(f'Step 1 — Exterior sólido: health={feat1.healthState}  faces={body.faces.count}')

        # ==================================================================
        # STEP 2 — FILLETS R2mm EN ARISTAS VERTICALES
        # ==================================================================
        fillet_edges = OC.create()
        for edge in body.edges:
            v1 = edge.startVertex.geometry
            v2 = edge.endVertex.geometry
            if (abs(v1.x - v2.x) < 1e-4 and
                abs(v1.z - v2.z) < 1e-4 and
                abs(v1.y - v2.y) > 0.01):
                fillet_edges.add(edge)
        print(f'Step 2 — Fillet edges: {fillet_edges.count}  (esperado: 4)')
        if fillet_edges.count > 0:
            fi = fillets.createInput()
            fi.addConstantRadiusEdgeSet(fillet_edges, val(0.2), True)
            fillets.add(fi)

        # ==================================================================
        # STEP 3 — VACIADO INTERIOR (Shell)
        # ==================================================================
        # Remover la cara SUPERIOR (Y=H) — la que tiene normal en +Y y mayor área.
        # El top panel de aluminio cubre esta apertura.

        top_face = None
        max_a = -1.0
        for face in body.faces:
            ret = face.evaluator.getNormalAtPoint(face.pointOnFace)
            if ret[0] and ret[1].y > 0.95:   # normal principalmente +Y
                bb = face.boundingBox
                a = (bb.maxPoint.x - bb.minPoint.x) * (bb.maxPoint.z - bb.minPoint.z)
                if a > max_a:
                    max_a = a
                    top_face = face

        if top_face is None:
            ui.messageBox('ERROR Step 3: No se encontró la cara superior para el Shell')
            return

        shell_faces = OC.create()
        shell_faces.add(top_face)
        shell_inp = comp.features.shellFeatures.createInput(shell_faces, False)
        shell_inp.insideThickness = val(WT)
        feat_shell = comp.features.shellFeatures.add(shell_inp)
        print(f'Step 3 — Shell: health={feat_shell.healthState}  faces={body.faces.count}')
        if feat_shell.healthState not in (0, 1):
            ui.messageBox(f'ADVERTENCIA Step 3: Shell health={feat_shell.healthState}. Verificar en Fusion.')

        # ==================================================================
        # STEP 4 — REBAJES LATERALES PARA CHEEKS (6mm cada lado)
        # ==================================================================
        # BUG FIX: CutFeatureOperation con rectángulos desde planos de construcción
        # puede fallar. Usar NewBodyFeatureOperation + combineFeatures CUT.
        #
        # Rebaje izquierdo: corta desde X=-9.0cm en +X por 0.6cm (6mm)
        # Rebaje derecho:   corta desde X=+9.0cm en -X por 0.6cm (6mm)
        # El rebaje elimina el área full-height (Y=0..H, Z=0..D) × 6mm en X.
        # El cheek guadua (12mm) asienta en este canal para alineación y adhesión.

        # --- Rebaje izquierdo ---
        cp_L = cplanes.createInput()
        cp_L.setByOffset(comp.yZConstructionPlane, val(-9.0))  # X=-9cm (cara ext izq)
        plane_L = cplanes.add(cp_L)
        plane_L.name = 'Plane_RebL'

        sk_RL = comp.sketches.add(plane_L)
        sk_RL.name = 'Rebaje_Left'
        ln_RL = sk_RL.sketchCurves.sketchLines
        ln_RL.addByTwoPoints(pt(-9.0, 0.0, 0.0), pt(-9.0, H,   0.0))
        ln_RL.addByTwoPoints(pt(-9.0, H,   0.0), pt(-9.0, H,   D  ))
        ln_RL.addByTwoPoints(pt(-9.0, H,   D  ), pt(-9.0, 0.0, D  ))
        ln_RL.addByTwoPoints(pt(-9.0, 0.0, D  ), pt(-9.0, 0.0, 0.0))

        inp_RL = exts.createInput(sk_RL.profiles.item(0), FO.NewBodyFeatureOperation)
        inp_RL.setOneSideExtent(dist(RD), ED.PositiveExtentDirection)  # +X = hacia interior
        feat_RL = exts.add(inp_RL)
        rl_body = feat_RL.bodies.item(0)

        rl_coll = OC.create(); rl_coll.add(rl_body)
        ci_RL = combos.createInput(body, rl_coll)
        ci_RL.operation = FO.CutFeatureOperation
        ci_RL.isKeepToolBodies = False
        combos.add(ci_RL)
        print(f'Step 4L — Rebaje izquierdo: health={feat_RL.healthState}  faces={body.faces.count}')

        # --- Rebaje derecho (simétrico) ---
        cp_R = cplanes.createInput()
        cp_R.setByOffset(comp.yZConstructionPlane, val(9.0))   # X=+9cm (cara ext der)
        plane_R = cplanes.add(cp_R)
        plane_R.name = 'Plane_RebR'

        sk_RR = comp.sketches.add(plane_R)
        sk_RR.name = 'Rebaje_Right'
        ln_RR = sk_RR.sketchCurves.sketchLines
        ln_RR.addByTwoPoints(pt(9.0, 0.0, 0.0), pt(9.0, H,   0.0))
        ln_RR.addByTwoPoints(pt(9.0, H,   0.0), pt(9.0, H,   D  ))
        ln_RR.addByTwoPoints(pt(9.0, H,   D  ), pt(9.0, 0.0, D  ))
        ln_RR.addByTwoPoints(pt(9.0, 0.0, D  ), pt(9.0, 0.0, 0.0))

        inp_RR = exts.createInput(sk_RR.profiles.item(0), FO.NewBodyFeatureOperation)
        inp_RR.setOneSideExtent(dist(RD), ED.NegativeExtentDirection)  # -X = hacia interior
        feat_RR = exts.add(inp_RR)
        rr_body = feat_RR.bodies.item(0)

        rr_coll = OC.create(); rr_coll.add(rr_body)
        ci_RR = combos.createInput(body, rr_coll)
        ci_RR.operation = FO.CutFeatureOperation
        ci_RR.isKeepToolBodies = False
        combos.add(ci_RR)
        print(f'Step 4R — Rebaje derecho: health={feat_RR.healthState}  faces={body.faces.count}')

        # ==================================================================
        # STEP 5 — STANDOFFS PCB (6 unidades, heat insert M3)
        # ==================================================================
        # Sobre el suelo interior (Y = WALL_T = 3mm).
        # Posiciones XZ en dos filas: Z=13mm (trasero) y Z=87mm (frontal).

        standoff_positions = [
            (-7.4, 1.3), (0.0, 1.3), (7.4, 1.3),
            (-7.4, 8.7), (0.0, 8.7), (7.4, 8.7),
        ]
        floor_y = WT   # Y=0.3cm = cara interior de la base

        standoff_tools = []
        for idx, (sx, sz) in enumerate(standoff_positions):
            cp_so = cplanes.createInput()
            cp_so.setByOffset(comp.xZConstructionPlane, val(floor_y))
            plane_so = cplanes.add(cp_so)

            sk_so = comp.sketches.add(plane_so)
            sk_so.name = f'SO_{idx}'
            sk_so.sketchCurves.sketchCircles.addByCenterRadius(
                pt(sx, floor_y, sz), SOD/2)

            inp_so = exts.createInput(sk_so.profiles.item(0), FO.NewBodyFeatureOperation)
            inp_so.setOneSideExtent(dist(SH), ED.PositiveExtentDirection)  # 5mm +Y
            feat_so = exts.add(inp_so)
            standoff_tools.append(feat_so.bodies.item(0))

        so_coll = OC.create()
        for sb in standoff_tools: so_coll.add(sb)
        join_so = combos.createInput(body, so_coll)
        join_so.operation = FO.JoinFeatureOperation
        join_so.isKeepToolBodies = False
        combos.add(join_so)
        print(f'Step 5 — Standoffs joined: faces={body.faces.count}')

        # Heat insert holes en standoffs (Ø4.2mm × 6mm, desde tapa del standoff hacia abajo)
        so_top_y = floor_y + SH   # Y=0.8cm (tapa del standoff)
        cp_soH = cplanes.createInput()
        cp_soH.setByOffset(comp.xZConstructionPlane, val(so_top_y))
        plane_soH = cplanes.add(cp_soH)

        sk_soH = comp.sketches.add(plane_soH)
        sk_soH.name = 'SO_Holes'
        for sx, sz in standoff_positions:
            sk_soH.sketchCurves.sketchCircles.addByCenterRadius(
                pt(sx, so_top_y, sz), IMD/2)

        sel_soH = OC.create()
        for i in range(sk_soH.profiles.count):
            sel_soH.add(sk_soH.profiles.item(i))
        inp_soH = exts.createInput(sel_soH, FO.CutFeatureOperation)
        inp_soH.setOneSideExtent(dist(IMH), ED.NegativeExtentDirection)  # 6mm -Y hacia base
        inp_soH.participantBodies = [body]
        feat_soH = exts.add(inp_soH)
        print(f'Step 5b — Heat insert holes standoffs: health={feat_soH.healthState}')

        # ==================================================================
        # STEP 6 — PEDESTALES HEAT INSERT PARA TOP PANEL (4 esquinas)
        # ==================================================================
        # BUG FIX: Con 4 perfiles en un sketch + NewBodyFeatureOperation,
        # Fusion crea 4 bodies separados. Hay que colectar TODOS los bodies
        # del feat_ped.bodies, no solo item(0).
        #
        # Los pedestales son 4 cilindros Ø6mm × 8mm, en las esquinas internas,
        # que suben hasta Y=H (la apertura del top).
        # Heat inserts M3 perforados desde Y=H hacia abajo 6mm.

        panel_insert_pos = [
            (-8.0, 0.8),   # trasera-izquierda
            ( 8.0, 0.8),   # trasera-derecha
            (-8.0, 9.2),   # frontal-izquierda
            ( 8.0, 9.2),   # frontal-derecha
        ]
        pedestal_y_base = H - 0.8   # 8mm de pedestal, desde Y=3.0 hasta Y=H=3.8cm

        cp_ped = cplanes.createInput()
        cp_ped.setByOffset(comp.xZConstructionPlane, val(pedestal_y_base))
        plane_ped = cplanes.add(cp_ped)
        plane_ped.name = 'Plane_Ped'

        sk_ped = comp.sketches.add(plane_ped)
        sk_ped.name = 'Panel_Inserts'
        for px, pz in panel_insert_pos:
            sk_ped.sketchCurves.sketchCircles.addByCenterRadius(
                pt(px, pedestal_y_base, pz), SOD/2)

        sel_ped = OC.create()
        for i in range(sk_ped.profiles.count):
            sel_ped.add(sk_ped.profiles.item(i))
        inp_ped = exts.createInput(sel_ped, FO.NewBodyFeatureOperation)
        inp_ped.setOneSideExtent(dist(0.8), ED.PositiveExtentDirection)  # 8mm +Y
        feat_ped = exts.add(inp_ped)

        # ← CRITICAL FIX: recolectar TODOS los bodies (4), no solo item(0)
        ped_coll = OC.create()
        for i in range(feat_ped.bodies.count):
            ped_coll.add(feat_ped.bodies.item(i))
        print(f'Step 6 — Pedestal bodies creados: {feat_ped.bodies.count}  (esperado: 4)')

        join_ped = combos.createInput(body, ped_coll)
        join_ped.operation = FO.JoinFeatureOperation
        join_ped.isKeepToolBodies = False
        combos.add(join_ped)

        # Heat insert holes en pedestales (Ø4.2mm × 6mm, desde Y=H hacia abajo)
        cp_pedH = cplanes.createInput()
        cp_pedH.setByOffset(comp.xZConstructionPlane, val(H))
        plane_pedH = cplanes.add(cp_pedH)

        sk_pedH = comp.sketches.add(plane_pedH)
        sk_pedH.name = 'Panel_Insert_Holes'
        for px, pz in panel_insert_pos:
            sk_pedH.sketchCurves.sketchCircles.addByCenterRadius(
                pt(px, H, pz), IMD/2)

        sel_pedH = OC.create()
        for i in range(sk_pedH.profiles.count):
            sel_pedH.add(sk_pedH.profiles.item(i))
        inp_pedH = exts.createInput(sel_pedH, FO.CutFeatureOperation)
        inp_pedH.setOneSideExtent(dist(IMH), ED.NegativeExtentDirection)  # 6mm -Y
        inp_pedH.participantBodies = [body]
        feat_pedH = exts.add(inp_pedH)
        print(f'Step 6b — Panel insert holes: health={feat_pedH.healthState}  faces={body.faces.count}')

        # ==================================================================
        # STEP 7 — SLOT POGO EN CARA TRASERA (Z=0)
        # ==================================================================
        # Pogo 6-pin Wurth WR-WST: slot 14.3×8.3mm centrado en X=0, Y=15mm.
        # Tool body extruido desde Z=−0.05cm en +Z por WALL_T+0.1cm (pasante).

        cp_pogo = cplanes.createInput()
        cp_pogo.setByOffset(comp.xYConstructionPlane, val(-0.05))  # ligeramente exterior Z=0
        plane_pogo = cplanes.add(cp_pogo)
        plane_pogo.name = 'Plane_Pogo'

        sk_pogo = comp.sketches.add(plane_pogo)
        sk_pogo.name = 'Pogo_Slot'
        pw, ph = 1.43/2, 0.83/2   # 14.3mm/2, 8.3mm/2
        py_pogo = 1.5              # 15mm desde base
        z_ref  = -0.05             # Z del plano (ligeramente exterior)
        ln_pg = sk_pogo.sketchCurves.sketchLines
        ln_pg.addByTwoPoints(pt(-pw, py_pogo - ph, z_ref), pt( pw, py_pogo - ph, z_ref))
        ln_pg.addByTwoPoints(pt( pw, py_pogo - ph, z_ref), pt( pw, py_pogo + ph, z_ref))
        ln_pg.addByTwoPoints(pt( pw, py_pogo + ph, z_ref), pt(-pw, py_pogo + ph, z_ref))
        ln_pg.addByTwoPoints(pt(-pw, py_pogo + ph, z_ref), pt(-pw, py_pogo - ph, z_ref))

        if sk_pogo.profiles.count > 0:
            inp_pg = exts.createInput(sk_pogo.profiles.item(0), FO.NewBodyFeatureOperation)
            inp_pg.setOneSideExtent(dist(WT + 0.1), ED.PositiveExtentDirection)  # +Z pasante
            feat_pg = exts.add(inp_pg)
            pg_coll = OC.create(); pg_coll.add(feat_pg.bodies.item(0))
            pg_comb = combos.createInput(body, pg_coll)
            pg_comb.operation = FO.CutFeatureOperation
            pg_comb.isKeepToolBodies = False
            combos.add(pg_comb)
            print(f'Step 7 — Pogo slot: health={feat_pg.healthState}  faces={body.faces.count}')
        else:
            print('Step 7 — Pogo slot: NO PROFILE — verificar plano trasero en Fusion')

        # ==================================================================
        # STEP 9 — CUTOUTS LATERALES PARA JACKS Y USB-C
        # ==================================================================
        # Los jacks/USB deben atravesar la pared interna del PETG (WALL_T=3mm)
        # que queda en X=±(W/2 - RD) = ±(9.0-0.6) = ±8.4cm tras el rebaje.
        # Sin estos cutouts los conectores no pueden pasar al interior.
        # Técnica: NewBody cilindro/paralelepípedo + Combine CUT (misma que rebajes).
        #
        # PARED INTERNA IZQUIERDA: X=-8.4cm (interior del rebaje izq)
        # Cutouts pasan en -X a través de WALL_T=0.3cm
        #
        # PARED INTERNA DERECHA: X=+8.4cm (interior del rebaje der)
        # Cutouts pasan en +X a través de WALL_T=0.3cm

        JY  = 2.1    # JACK_Z = 21mm → Y=2.1cm (centro vertical de todos los conectores)
        R_J = 0.55   # Ø11.0mm → r=0.55cm (Ø10.5mm + tolerancia FDM 0.3mm = Ø10.8mm → usar 11mm)
        R_A = 0.375  # Ø7.5mm  → r=0.375cm (mini jack 3.5mm con tolerancia)
        UW  = 1.08/2 # Ø10.8mm ancho USB-C con tolerancia FDM: (10.5+0.3)/2
        UH  = 0.53/2 # alto USB-C con tolerancia: (5.0+0.3)/2
        CUT_D = WT + 0.1  # profundidad de corte = WALL_T + 1mm margen = 0.4cm

        # --- LADO IZQUIERDO: TRS IN L, TRS IN R, AUX 3.5mm ---
        X_WALL_L = -(W/2 - RD)  # -8.4cm = cara interior del rebaje izquierdo
        left_cuts = []

        for z_cm, r_cm, cname in [
            (3.0, R_J, 'TRS_INL'),
            (5.5, R_J, 'TRS_INR'),
            (7.8, R_A, 'AUX_35'),
        ]:
            cp_lc = cplanes.createInput()
            cp_lc.setByOffset(comp.yZConstructionPlane, val(X_WALL_L))
            plane_lc = cplanes.add(cp_lc)
            sk_lc = comp.sketches.add(plane_lc)
            sk_lc.name = f'Cut_L_{cname}'
            sk_lc.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(X_WALL_L, JY, z_cm), r_cm)
            inp_lc = exts.createInput(sk_lc.profiles.item(0), FO.NewBodyFeatureOperation)
            inp_lc.setOneSideExtent(dist(CUT_D), ED.NegativeExtentDirection)  # -X = hacia exterior
            left_cuts.append(exts.add(inp_lc).bodies.item(0))

        lc_coll = OC.create()
        for b in left_cuts: lc_coll.add(b)
        ci_lc = combos.createInput(body, lc_coll)
        ci_lc.operation = FO.CutFeatureOperation
        ci_lc.isKeepToolBodies = False
        combos.add(ci_lc)
        print(f'Step 9L — Jack cutouts izq: faces={body.faces.count}')

        # --- LADO DERECHO: USB-C Data, TRS OUT L, TRS OUT R, USB-C Host ---
        X_WALL_R = W/2 - RD   # +8.4cm = cara interior del rebaje derecho
        right_cuts = []

        for z_cm, shape, cname in [
            (1.5, 'usbc', 'USBC_DATA'),
            (3.8, 'trs',  'TRS_OUTL'),
            (6.0, 'trs',  'TRS_OUTR'),
            (8.2, 'usbc', 'USBC_HOST'),
        ]:
            cp_rc = cplanes.createInput()
            cp_rc.setByOffset(comp.yZConstructionPlane, val(X_WALL_R))
            plane_rc = cplanes.add(cp_rc)
            sk_rc = comp.sketches.add(plane_rc)
            sk_rc.name = f'Cut_R_{cname}'

            if shape == 'trs':
                sk_rc.sketchCurves.sketchCircles.addByCenterRadius(
                    P.create(X_WALL_R, JY, z_cm), R_J)
                inp_rc = exts.createInput(sk_rc.profiles.item(0), FO.NewBodyFeatureOperation)
                inp_rc.setOneSideExtent(dist(CUT_D), ED.PositiveExtentDirection)  # +X = hacia exterior
            else:  # usbc — rectangulo via yZConstructionPlane + OffsetStart
                ln_rc = sk_rc.sketchCurves.sketchLines
                y0, y1 = JY - UH, JY + UH
                z0, z1 = z_cm - UW, z_cm + UW
                for (ya, za), (yb, zb) in [
                    ((y0,z0),(y1,z0)), ((y1,z0),(y1,z1)),
                    ((y1,z1),(y0,z1)), ((y0,z1),(y0,z0)),
                ]:
                    ln_rc.addByTwoPoints(P.create(X_WALL_R, ya, za),
                                         P.create(X_WALL_R, yb, zb))
                inp_rc = exts.createInput(sk_rc.profiles.item(0), FO.NewBodyFeatureOperation)
                inp_rc.setOneSideExtent(dist(CUT_D), ED.PositiveExtentDirection)

            right_cuts.append(exts.add(inp_rc).bodies.item(0))

        rc_coll = OC.create()
        for b in right_cuts: rc_coll.add(b)
        ci_rc = combos.createInput(body, rc_coll)
        ci_rc.operation = FO.CutFeatureOperation
        ci_rc.isKeepToolBodies = False
        combos.add(ci_rc)
        print(f'Step 9R — Port cutouts der: faces={body.faces.count}')

        # ==================================================================
        # STEP 8 — CAVIDADES PATAS SILICONA (4 esquinas, base exterior)
        # ==================================================================
        # Ø9.9mm press-fit (pata Ø10mm), profundidad 2mm (ciego, queda 1mm de base).
        # BUG FIX: PTH=3mm = WALL_T = agujero pasante. Corregido a PTH=2mm.

        pata_pos = [
            (-8.0, 1.0), (8.0, 1.0),
            (-8.0, 9.0), (8.0, 9.0),
        ]

        sk_pata = comp.sketches.add(comp.xZConstructionPlane)  # Y=0 = base exterior
        sk_pata.name = 'Rubber_Feet'
        for ppx, ppz in pata_pos:
            sk_pata.sketchCurves.sketchCircles.addByCenterRadius(
                pt(ppx, 0.0, ppz), PTD/2 - 0.05)  # Ø9.9mm

        sel_pata = OC.create()
        for i in range(sk_pata.profiles.count):
            sel_pata.add(sk_pata.profiles.item(i))
        inp_pata = exts.createInput(sel_pata, FO.CutFeatureOperation)
        inp_pata.setOneSideExtent(dist(PTH), ED.PositiveExtentDirection)  # 2mm hacia +Y (interior)
        inp_pata.participantBodies = [body]
        feat_pata = exts.add(inp_pata)
        print(f'Step 8 — Patas silicona: health={feat_pata.healthState}  faces={body.faces.count}')

        # ==================================================================
        # RESUMEN
        # ==================================================================
        bb = body.boundingBox
        W_r = (bb.maxPoint.x - bb.minPoint.x) * 10
        H_r = (bb.maxPoint.y - bb.minPoint.y) * 10
        D_r = (bb.maxPoint.z - bb.minPoint.z) * 10

        summary = (
            f'✓ 01_PETG_Body v0.8 — BUILD COMPLETO\n\n'
            f'BBox: {W_r:.0f} × {D_r:.0f} × {H_r:.1f} mm\n'
            f'  (esperado: 180 × 100 × 38 mm)\n\n'
            f'Faces: {body.faces.count}\n\n'
            f'Features aplicadas:\n'
            f'  1. Cuerpo exterior 180×100×38mm\n'
            f'  2. Fillets R2mm en 4 aristas verticales\n'
            f'  3. Shell interior 3mm (top ABIERTO)\n'
            f'  4. Rebajes cheeks 6mm izq+der (NewBody+Combine)\n'
            f'  5. 6 standoffs PCB M3 (Ø6mm × 5mm) + heat inserts\n'
            f'  6. 4 pedestales top panel (TODOS unidos) + heat inserts\n'
            f'  7. Slot pogo trasero 14.3×8.3mm @ Y=15mm\n'
            f'  8. 4 cavidades patas silicona Ø9.9mm × 2mm\n'
            f'  9. Cutouts pared izq: TRS INL/INR Ø11mm + AUX Ø7.5mm\n'
            f'     Cutouts pared der: USB-C Data/Host 10.8×5.3mm + TRS OUTL/R Ø11mm\n\n'
            f'STACK: top y bottom planos → apilable sobre slaves/mixer ✓\n'
            f'POGO: cara trasera Z=0, centro Y=15mm ✓'
        )
        print(summary)
        ui.messageBox(summary)

    except Exception:
        if ui:
            ui.messageBox(traceback.format_exc())
        else:
            print(traceback.format_exc())

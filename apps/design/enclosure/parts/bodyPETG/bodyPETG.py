# =============================================================================
# GrooveForge Brain — 01_PETG_Body (v0.6 — Caja rectangular para stack modular)
# Script Fusion 360 (Python API) — v0.17
#
# DIAGNÓSTICO RAÍZ v0.15 → v0.16
# ===========================================================================
# El bug de pedestales flotantes tenía dos causas independientes:
#
# CAUSA 1 — ped_base_y incorrecto:
#   v0.15: ped_base_y = H - 0.8 = 3.0cm  (8mm antes del techo)
#   El body tiene el techo ABIERTO (el shell lo removió). En Y=3.0cm los
#   pedestales están en el vacío interior, sin cara sólida adyacente para
#   hacer Join. Fusion crea el cilindro como body separado → flota.
#   CORRECCIÓN: pedestales nacen desde el PISO interior (Y=WT=0.3cm) y
#   crecen hasta Y=H (la boca). Anclados al piso → Join exitoso.
#
# CAUSA 2 — altura del pedestal (0.8cm) era solo 8mm:
#   El spec SSoT (01-petg-body.md §3 Op.6 + 00-master-parameters.md §3)
#   define pedestales con INSERT_M3_H=6mm de heat insert, no 8mm de altura.
#   La altura útil del pedestal es H-WT = 35mm para que el heat insert
#   quede a ras de la boca. Esto también asegura que el cilindro intersecta
#   la pared lateral sólida (borde del cilindro en X=±8.3cm alcanza la pared
#   interior en X=±8.1–8.4cm) → Join garantizado.
#
# CAUSA 3 — Standoffs: floor_y correcto (Y=WT), pero sin bounding-box prints
#   que permitieran diagnosticar posición en consola. Añadido en v0.16.
#
# REGLAS INMUTABLES (documentadas en versiones anteriores, mantenidas):
#   - Sketches en plano XZ-offset: pt(x, 0, z)  [Y=0 local, nunca Y_mundo]
#   - Sketches en plano YZ-offset: P.create(X_offset, Y_mundo, Z_mundo)
#     [Fusion proyecta X automáticamente, usa Y y Z como coordenadas locales]
#   - Sketches en plano XY-offset: P.create(X_mundo, Y_mundo, Z_offset)
#   - NUNCA usar OffsetStartDefinition (comportamiento invertido documentado)
#   - NUNCA usar shellFeatures (falla con fillets, comportamiento impredecible)
#   - Todos los sólidos-tool: NewBodyFeatureOperation + combineFeatures CUT/JOIN
#
# SISTEMA DE COORDENADAS (Fusion 360 Y-UP, cm internos):
#   X: –9.0 a +9.0 cm  (180mm ancho, 0 = centro)
#   Y:  0.0 a +3.8 cm  (38mm altura, Y=0 = base exterior plana)
#   Z:  0.0 a +10.0 cm (100mm profundidad, Z=0 = cara trasera pogo,
#                        Z=10 = cara frontal)
#
# NOVEDADES v0.16:
#   - Step 6: ped_base_y = WT (piso interior), altura = H-WT (hasta la boca)
#   - Bounding-box print después de cada feature para diagnóstico en consola
#   - Comentarios de cada print con la bbox esperada
#   - Verificación explícita de feat.bodies.count antes de combinar
#   - Mensajes de error descriptivos si un profile no se genera
#
# CÓMO USAR:
#   Shift+S en Fusion → Scripts → "+" → Python → pegar → Run
#   Componente '01_PETG_Body' debe estar VACÍO (el script borra el anterior).
#
# RESULTADO ESPERADO:
#   Cuerpo PETG 180 × 100 × 38mm
#   ├── Shell interior (paredes 3mm, base 3mm, top ABIERTO)
#   ├── Rebajes laterales 6mm para cheeks (NewBody+CombineCUT)
#   ├── 6 standoffs PCB M3 (Ø6mm × 5mm, floor Y=3mm, top Y=8mm) + heat inserts
#   ├── 4 pedestales top panel (Ø6mm, floor Y=3mm, top Y=38mm) + heat inserts
#   ├── Slot Pogo trasero 14.3×8.3mm @ centro X=0, Y=15mm
#   ├── 4 cavidades patas silicona Ø9.9mm × 2mm en base
#   ├── Cutouts pared izq: TRS INL/INR Ø11mm + AUX 3.5mm Ø7.5mm
#   └── Cutouts pared der: USB-C Data/Host rect 10.8×5.3mm + TRS OUTL/R Ø11mm
# =============================================================================

import adsk.core
import adsk.fusion
import traceback


def run(context):
    ui = None
    try:
        app  = adsk.core.Application.get()
        ui   = app.userInterface
        des  = adsk.fusion.Design.cast(app.activeProduct)
        root = des.rootComponent

        P  = adsk.core.Point3D
        OC = adsk.core.ObjectCollection
        VI = adsk.core.ValueInput
        DD = adsk.fusion.DistanceExtentDefinition
        FO = adsk.fusion.FeatureOperations
        ED = adsk.fusion.ExtentDirections

        def pt(x, y, z):  return P.create(x, y, z)
        def dist(v):       return DD.create(VI.createByReal(v))
        def val(v):        return VI.createByReal(v)

        def bbox_str(b):
            """Devuelve string con bounding box en mm para diagnóstico."""
            mn = b.boundingBox.minPoint
            mx = b.boundingBox.maxPoint
            w  = (mx.x - mn.x) * 10
            h  = (mx.y - mn.y) * 10
            d  = (mx.z - mn.z) * 10
            return (f'BBox: {w:.1f}W × {d:.1f}D × {h:.1f}H mm  '
                    f'[X {mn.x*10:.1f}..{mx.x*10:.1f}  '
                    f'Y {mn.y*10:.1f}..{mx.y*10:.1f}  '
                    f'Z {mn.z*10:.1f}..{mx.z*10:.1f}]')

        # ------------------------------------------------------------------
        # PARÁMETROS (cm — unidad interna de Fusion)
        # Fuente SSoT: apps/design/enclosure/parts/00-master-parameters.md
        # ------------------------------------------------------------------
        W    = 18.0   # BODY_W      = 180mm
        D    = 10.0   # BODY_D      = 100mm
        H    = 3.8    # BODY_H      = 38mm  (v0.6 rectangular)
        WT   = 0.3    # WALL_T      = 3mm
        SH   = 0.5    # STANDOFF_H  = 5mm
        SOD  = 0.6    # STANDOFF_OD = 6mm   → radio = 3mm
        IMD  = 0.42   # INSERT_M3_OD= 4.2mm → radio = 2.1mm
        IMH  = 0.6    # INSERT_M3_H = 6mm
        RD   = 0.6    # CHEEK_REBAJE_D = 6mm
        PTD  = 1.0    # PATA_D      = 10mm  → radio = 5mm
        PTH  = 0.2    # PATA_H      = 2mm   (ciego — 1mm de base queda)

        # Derivados
        Wi   = W/2 - WT              # 8.7cm — semiancho interior
        X_WL = -(W/2 - RD)          # -8.4cm — pared interna izquierda (tras rebaje)
        X_WR =  (W/2 - RD)          #  8.4cm — pared interna derecha (tras rebaje)

        # ------------------------------------------------------------------
        # LOCALIZAR O RECREAR COMPONENTE
        # ------------------------------------------------------------------
        for occ in root.occurrences:
            n = occ.component.name
            if '01' in n or 'PETG' in n or 'petg' in n:
                occ.deleteMe()
                break

        occ2 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        comp = occ2.component
        comp.name = '01_PETG_Body'

        exts    = comp.features.extrudeFeatures
        fillets = comp.features.filletFeatures
        combos  = comp.features.combineFeatures
        cplanes = comp.constructionPlanes

        # ==================================================================
        # STEP 1 — CUERPO EXTERIOR SÓLIDO (180 × 100 × 38mm)
        # Sketch en xZConstructionPlane (Y=0, plano horizontal base).
        # Rectangulo 180×100mm centrado en X=0, Z=0..10cm.
        # Extrude +Y por H=3.8cm.
        # Esperado: BBox 180W × 100D × 38H mm, X –90..90, Y 0..38, Z 0..100
        # ==================================================================
        sk1 = comp.sketches.add(comp.xZConstructionPlane)
        sk1.name = 'Base_Footprint'
        ln = sk1.sketchCurves.sketchLines
        ln.addByTwoPoints(pt(-9.0, 0, 0.0), pt( 9.0, 0, 0.0))
        ln.addByTwoPoints(pt( 9.0, 0, 0.0), pt( 9.0, 0,10.0))
        ln.addByTwoPoints(pt( 9.0, 0,10.0), pt(-9.0, 0,10.0))
        ln.addByTwoPoints(pt(-9.0, 0,10.0), pt(-9.0, 0, 0.0))

        inp1  = exts.createInput(sk1.profiles.item(0), FO.NewBodyFeatureOperation)
        inp1.setOneSideExtent(dist(H), ED.PositiveExtentDirection)
        feat1 = exts.add(inp1)
        feat1.bodies.item(0).name = 'PETG_Body'
        body  = comp.bRepBodies.item(0)
        print(f'Step 1 — Exterior sólido: health={feat1.healthState}  faces={body.faces.count}')
        print(f'  {bbox_str(body)}')
        print(f'  Esperado: 180W × 100D × 38H mm, X –90..90, Y 0..38, Z 0..100')

        # ==================================================================
        # STEP 2 — FILLETS R2mm EN 4 ARISTAS VERTICALES
        # Las 4 aristas verticales son las que tienen X,Z constantes y Y variable.
        # Esperado: 4 aristas seleccionadas.
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
        # STEP 3 — VACIADO INTERIOR (sin Shell API)
        # Bloque interior: X=±Wi=±8.7cm, Z=WT..D-WT=0.3..9.7cm,
        #                  Y=WT..H=0.3..3.8cm (top ABIERTO).
        # Plano explícito en Y=WT. Extrude +Y por (H-WT).
        # Combine CUT contra body principal.
        # Después del cut: BBox exterior mantiene 180×100×38, pero con cavidad interna.
        # ==================================================================
        cp_int = cplanes.createInput()
        cp_int.setByOffset(comp.xZConstructionPlane, val(WT))
        plane_int = cplanes.add(cp_int)
        plane_int.name = 'Plane_Interior'

        sk3 = comp.sketches.add(plane_int)
        sk3.name = 'Interior_Footprint'
        ln3 = sk3.sketchCurves.sketchLines
        ln3.addByTwoPoints(pt(-Wi, 0, WT   ), pt( Wi, 0, WT   ))
        ln3.addByTwoPoints(pt( Wi, 0, WT   ), pt( Wi, 0, D-WT ))
        ln3.addByTwoPoints(pt( Wi, 0, D-WT ), pt(-Wi, 0, D-WT ))
        ln3.addByTwoPoints(pt(-Wi, 0, D-WT ), pt(-Wi, 0, WT   ))

        inp3  = exts.createInput(sk3.profiles.item(0), FO.NewBodyFeatureOperation)
        inp3.setOneSideExtent(dist(H - WT), ED.PositiveExtentDirection)
        feat3 = exts.add(inp3)
        int_body = feat3.bodies.item(0)

        int_coll = OC.create(); int_coll.add(int_body)
        ci3 = combos.createInput(body, int_coll)
        ci3.operation = FO.CutFeatureOperation
        ci3.isKeepToolBodies = False
        combos.add(ci3)
        print(f'Step 3 — Vaciado interior: health={feat3.healthState}  faces={body.faces.count}')
        print(f'  {bbox_str(body)}')
        print(f'  Esperado: BBox exterior 180×100×38mm (interior vaciado no cambia bbox)')

        # ==================================================================
        # STEP 4 — REBAJES LATERALES PARA CHEEKS (6mm cada lado)
        # RD=6mm de profundidad en X. Corren toda la altura (Y=0..H) y
        # toda la profundidad (Z=0..D).
        # Izquierdo: tool body en X=–9.0..–8.4, extrude +X (hacia interior).
        # Derecho:   tool body en X=+8.4..+9.0, extrude –X (simetrico).
        # Esperado tras rebaje izq: pared izq exterior pasa de X=–9.0 a X=–8.4
        #   (el escalon es visible, el exterior del PETG se adelgaza 6mm por lado).
        # ==================================================================

        # --- Rebaje izquierdo ---
        cp_L = cplanes.createInput()
        cp_L.setByOffset(comp.yZConstructionPlane, val(-9.0))
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
        inp_RL.setOneSideExtent(dist(RD), ED.PositiveExtentDirection)
        feat_RL = exts.add(inp_RL)
        rl_coll = OC.create(); rl_coll.add(feat_RL.bodies.item(0))
        ci_RL = combos.createInput(body, rl_coll)
        ci_RL.operation = FO.CutFeatureOperation
        ci_RL.isKeepToolBodies = False
        combos.add(ci_RL)
        print(f'Step 4L — Rebaje izquierdo: health={feat_RL.healthState}  faces={body.faces.count}')
        print(f'  {bbox_str(body)}')
        print(f'  Esperado: BBox.minX = –84mm (era –90mm; rebaje de 6mm)')

        # --- Rebaje derecho (simétrico) ---
        cp_R = cplanes.createInput()
        cp_R.setByOffset(comp.yZConstructionPlane, val(9.0))
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
        inp_RR.setOneSideExtent(dist(RD), ED.NegativeExtentDirection)
        feat_RR = exts.add(inp_RR)
        rr_coll = OC.create(); rr_coll.add(feat_RR.bodies.item(0))
        ci_RR = combos.createInput(body, rr_coll)
        ci_RR.operation = FO.CutFeatureOperation
        ci_RR.isKeepToolBodies = False
        combos.add(ci_RR)
        print(f'Step 4R — Rebaje derecho:  health={feat_RR.healthState}  faces={body.faces.count}')
        print(f'  {bbox_str(body)}')
        print(f'  Esperado: BBox.maxX = +84mm (era +90mm; rebaje de 6mm)')

        # ==================================================================
        # STEP 5 — STANDOFFS PCB (6 unidades, heat insert M3)
        # Parámetros (SSoT 00-master-parameters.md §3, 01-petg-body.md §3 Op.7):
        #   Fila trasera: Z=PCB_CLEAR+10mm = 3+10 = 13mm = 1.3cm
        #   Fila frontal: Z=BODY_D–PCB_CLEAR–10mm = 100–3–10 = 87mm = 8.7cm
        #   X: ±(PCB_W/2 – 10mm) = ±(84 – 10) = ±74mm = ±7.4cm
        #   X=0: standoff central en ambas filas
        # Plano en Y=WT=0.3cm (piso interior). Extrude +Y por SH=0.5cm.
        # BBox standoffs: X=±7.4±0.3cm, Y=0.3..0.8cm, Z=(1.3±0.3) y (8.7±0.3)cm
        # Luego holes desde Y=WT+SH=0.8cm, NegativeExtentDirection por IMH=0.6cm.
        # ==================================================================
        standoff_positions = [
            (-7.4, 1.3), (0.0, 1.3), (7.4, 1.3),   # fila trasera
            (-7.4, 8.7), (0.0, 8.7), (7.4, 8.7),   # fila frontal
        ]
        floor_y = WT   # Y=0.3cm = 3mm interior del piso

        cp_so = cplanes.createInput()
        cp_so.setByOffset(comp.xZConstructionPlane, val(floor_y))
        plane_so = cplanes.add(cp_so)
        plane_so.name = 'Plane_SO'

        sk_so = comp.sketches.add(plane_so)
        sk_so.name = 'Standoffs'
        for sx, sz in standoff_positions:
            # Coordenadas locales del plano XZ-offset: Y=0 siempre
            sk_so.sketchCurves.sketchCircles.addByCenterRadius(pt(sx, 0, sz), SOD/2)

        sel_so = OC.create()
        for i in range(sk_so.profiles.count):
            sel_so.add(sk_so.profiles.item(i))

        if sk_so.profiles.count != 6:
            print(f'  ADVERTENCIA Step 5: profiles={sk_so.profiles.count} (esperado 6)')

        inp_so = exts.createInput(sel_so, FO.NewBodyFeatureOperation)
        inp_so.setOneSideExtent(dist(SH), ED.PositiveExtentDirection)
        feat_so = exts.add(inp_so)
        print(f'Step 5 — Standoff bodies generados: {feat_so.bodies.count}  (esperado 6)')

        so_coll = OC.create()
        for i in range(feat_so.bodies.count):
            so_coll.add(feat_so.bodies.item(i))
        ci_so = combos.createInput(body, so_coll)
        ci_so.operation = FO.JoinFeatureOperation
        ci_so.isKeepToolBodies = False
        combos.add(ci_so)
        print(f'Step 5 — Standoffs joined: faces={body.faces.count}')
        print(f'  {bbox_str(body)}')
        print(f'  Esperado: standoffs en Y=3..8mm — BBox Y debería seguir siendo 0..38mm')

        # Heat insert holes en tapa de cada standoff
        so_top = floor_y + SH   # Y = 0.3 + 0.5 = 0.8cm = 8mm
        cp_soH = cplanes.createInput()
        cp_soH.setByOffset(comp.xZConstructionPlane, val(so_top))
        plane_soH = cplanes.add(cp_soH)
        plane_soH.name = 'Plane_SO_Holes'

        sk_soH = comp.sketches.add(plane_soH)
        sk_soH.name = 'SO_Holes'
        for sx, sz in standoff_positions:
            sk_soH.sketchCurves.sketchCircles.addByCenterRadius(pt(sx, 0, sz), IMD/2)

        sel_soH = OC.create()
        for i in range(sk_soH.profiles.count):
            sel_soH.add(sk_soH.profiles.item(i))
        inp_soH = exts.createInput(sel_soH, FO.CutFeatureOperation)
        inp_soH.setOneSideExtent(dist(IMH), ED.NegativeExtentDirection)
        inp_soH.participantBodies = [body]
        feat_soH = exts.add(inp_soH)
        print(f'Step 5b — Heat insert holes standoffs: health={feat_soH.healthState}')

        # ==================================================================
        # STEP 6 — PEDESTALES HEAT INSERT PARA TOP PANEL (4 esquinas)
        #
        # Spec SSoT (01-petg-body.md §3 Op.6):
        #   Pines que SOBRESALEN por encima de Y=H (la boca abierta).
        #   El top panel aluminio se apoya sobre ellos. Tornillos M3 pasan
        #   por el aluminio y atornillan en los heat inserts de los pines.
        #   Altura del pin = INSERT_M3_H = 6mm.
        #   La base del pin está en Y=H (borde superior de las paredes
        #   laterales) — que ES una cara sólida del body → Join garantizado.
        #
        # Posiciones (SSoT 00-master-parameters.md §3):
        #   X = ±(90–10) = ±80mm = ±8.0cm
        #   Z trasero = 8mm = 0.8cm, Z frontal = 92mm = 9.2cm
        #
        # BBox esperada: Y=38..44mm (6mm sobre la boca)
        # ==================================================================
        panel_insert_pos = [
            (-8.0, 0.8),   # trasera-izquierda
            ( 8.0, 0.8),   # trasera-derecha
            (-8.0, 9.2),   # frontal-izquierda
            ( 8.0, 9.2),   # frontal-derecha
        ]

        # Plano en Y=H (borde superior de las paredes = cara sólida real)
        cp_ped = cplanes.createInput()
        cp_ped.setByOffset(comp.xZConstructionPlane, val(H))
        plane_ped = cplanes.add(cp_ped)
        plane_ped.name = 'Plane_Ped'

        sk_ped = comp.sketches.add(plane_ped)
        sk_ped.name = 'Panel_Inserts'
        for px, pz in panel_insert_pos:
            sk_ped.sketchCurves.sketchCircles.addByCenterRadius(pt(px, 0, pz), SOD/2)

        sel_ped = OC.create()
        for i in range(sk_ped.profiles.count):
            sel_ped.add(sk_ped.profiles.item(i))

        if sk_ped.profiles.count != 4:
            print(f'  ADVERTENCIA Step 6: profiles={sk_ped.profiles.count} (esperado 4)')

        inp_ped = exts.createInput(sel_ped, FO.NewBodyFeatureOperation)
        inp_ped.setOneSideExtent(dist(IMH), ED.PositiveExtentDirection)  # +Y: pines sobre la boca
        feat_ped = exts.add(inp_ped)
        print(f'Step 6 — Pedestal bodies generados: {feat_ped.bodies.count}  (esperado 4)')

        ped_coll = OC.create()
        for i in range(feat_ped.bodies.count):
            ped_coll.add(feat_ped.bodies.item(i))
        ci_ped = combos.createInput(body, ped_coll)
        ci_ped.operation = FO.JoinFeatureOperation
        ci_ped.isKeepToolBodies = False
        combos.add(ci_ped)
        print(f'Step 6 — Pedestales joined: faces={body.faces.count}')
        print(f'  {bbox_str(body)}')
        print(f'  Esperado: BBox.maxY = 44mm (body 38mm + pines 6mm sobresliendo)')

        # Heat insert holes desde la tapa de cada pin hacia -Y
        cp_pedH = cplanes.createInput()
        cp_pedH.setByOffset(comp.xZConstructionPlane, val(H + IMH))
        plane_pedH = cplanes.add(cp_pedH)
        plane_pedH.name = 'Plane_Ped_Holes'

        sk_pedH = comp.sketches.add(plane_pedH)
        sk_pedH.name = 'Panel_Insert_Holes'
        for px, pz in panel_insert_pos:
            sk_pedH.sketchCurves.sketchCircles.addByCenterRadius(pt(px, 0, pz), IMD/2)

        sel_pedH = OC.create()
        for i in range(sk_pedH.profiles.count):
            sel_pedH.add(sk_pedH.profiles.item(i))
        inp_pedH = exts.createInput(sel_pedH, FO.CutFeatureOperation)
        inp_pedH.setOneSideExtent(dist(IMH), ED.NegativeExtentDirection)
        inp_pedH.participantBodies = [body]
        feat_pedH = exts.add(inp_pedH)
        print(f'Step 6b — Panel insert holes: health={feat_pedH.healthState}  faces={body.faces.count}')
        print(f'  Esperado: 4 agujeros Ø4.2mm en pines, desde tapa (Y=44mm) hacia -Y 6mm')

        # ==================================================================
        # STEP 7 — SLOT POGO EN CARA TRASERA (Z=0)
        # Pogo 6-pin Wurth WR-WST: slot 14.3×8.3mm
        # Centro: X=0, Y=15mm (POGO_Y_CENTER)
        # El slot atraviesa WALL_T=3mm de la pared trasera.
        # Plano en Z=–0.05cm (ligeramente exterior a Z=0) para garantizar
        # que el sólido-tool se solapa con la cara trasera.
        # Coordenadas del sketch (xYConstructionPlane offset en Z=–0.05):
        #   X en eje X mundo, Y en eje Y mundo, Z del plano en todos los puntos.
        # Esperado: slot en X=–7.15..+7.15mm, Y=11.35..18.65mm, Z=0..3mm
        # ==================================================================
        cp_pogo = cplanes.createInput()
        cp_pogo.setByOffset(comp.xYConstructionPlane, val(-0.05))
        plane_pogo = cplanes.add(cp_pogo)
        plane_pogo.name = 'Plane_Pogo'

        sk_pogo = comp.sketches.add(plane_pogo)
        sk_pogo.name = 'Pogo_Slot'
        pw     = 1.43/2   # semiancho slot = 7.15mm
        ph     = 0.83/2   # semialto slot  = 4.15mm
        py_pg  = 1.5      # Y centro pogo  = 15mm = 1.5cm
        z_pg   = -0.05    # Z del plano (coordenada mundo, usada en los puntos)
        ln_pg  = sk_pogo.sketchCurves.sketchLines
        ln_pg.addByTwoPoints(pt(-pw, py_pg - ph, z_pg), pt( pw, py_pg - ph, z_pg))
        ln_pg.addByTwoPoints(pt( pw, py_pg - ph, z_pg), pt( pw, py_pg + ph, z_pg))
        ln_pg.addByTwoPoints(pt( pw, py_pg + ph, z_pg), pt(-pw, py_pg + ph, z_pg))
        ln_pg.addByTwoPoints(pt(-pw, py_pg + ph, z_pg), pt(-pw, py_pg - ph, z_pg))

        if sk_pogo.profiles.count > 0:
            inp_pg = exts.createInput(sk_pogo.profiles.item(0), FO.NewBodyFeatureOperation)
            inp_pg.setOneSideExtent(dist(WT + 0.1), ED.PositiveExtentDirection)
            feat_pg = exts.add(inp_pg)
            pg_coll = OC.create(); pg_coll.add(feat_pg.bodies.item(0))
            ci_pg = combos.createInput(body, pg_coll)
            ci_pg.operation = FO.CutFeatureOperation
            ci_pg.isKeepToolBodies = False
            combos.add(ci_pg)
            print(f'Step 7 — Pogo slot: health={feat_pg.healthState}  faces={body.faces.count}')
            print(f'  {bbox_str(body)}')
            print(f'  Esperado: slot X=±7.15mm, Y=11.35..18.65mm, corta pared trasera Z=0..3mm')
        else:
            print('Step 7 — FALLO: no se generó perfil para pogo slot. Verificar plano Z=–0.05cm.')

        # ==================================================================
        # STEP 8 — CAVIDADES PATAS SILICONA (4 esquinas, base exterior)
        # Ø9.9mm (press-fit para patas Ø10mm), profundidad 2mm (ciego).
        # Sketch en xZConstructionPlane (Y=0 = base exterior).
        # Cut hacia +Y (hacia el interior de la base sólida de 3mm).
        # Posiciones: X=±80mm, Z=10mm y Z=90mm (10mm desde cada borde).
        # Esperado: 4 cilindros ciegos Y=0..2mm en esquinas base.
        # ==================================================================
        # SSoT 01-petg-body.md §4: posiciones (±80mm, ±45mm)
        # ±45mm en Z = Z=5mm (trasera) y Z=95mm (frontal) desde Z=0
        pata_pos = [
            (-8.0, 0.5),  # izq-trasera  (X=–80mm, Z=5mm desde fondo)
            ( 8.0, 0.5),  # der-trasera  (X=+80mm, Z=5mm desde fondo)
            (-8.0, 9.5),  # izq-frontal  (X=–80mm, Z=95mm desde fondo)
            ( 8.0, 9.5),  # der-frontal  (X=+80mm, Z=95mm desde fondo)
        ]
        sk_pata = comp.sketches.add(comp.xZConstructionPlane)
        sk_pata.name = 'Rubber_Feet'
        for ppx, ppz in pata_pos:
            sk_pata.sketchCurves.sketchCircles.addByCenterRadius(
                pt(ppx, 0.0, ppz), PTD/2 - 0.05)   # Ø9.9mm

        sel_pata = OC.create()
        for i in range(sk_pata.profiles.count):
            sel_pata.add(sk_pata.profiles.item(i))
        inp_pata = exts.createInput(sel_pata, FO.CutFeatureOperation)
        inp_pata.setOneSideExtent(dist(PTH), ED.PositiveExtentDirection)
        inp_pata.participantBodies = [body]
        feat_pata = exts.add(inp_pata)
        print(f'Step 8 — Patas silicona: health={feat_pata.healthState}  faces={body.faces.count}')
        print(f'  Esperado: 4 cavidades Ø9.9mm × 2mm en base, Y=0..2mm')

        # ==================================================================
        # STEP 9 — CUTOUTS LATERALES PARA JACKS Y USB-C
        # La pared interna del PETG tras el rebaje está en X=±(W/2–RD)=±8.4cm.
        # Los conectores deben atravesar esa pared (WALL_T=3mm).
        #
        # IZQUIERDO (jacks IN):
        #   TRS IN L:  Ø11.0mm @ Z=30mm=3.0cm, Y=21mm=2.1cm
        #   TRS IN R:  Ø11.0mm @ Z=55mm=5.5cm, Y=21mm=2.1cm
        #   AUX 3.5mm: Ø7.5mm  @ Z=78mm=7.8cm, Y=21mm=2.1cm
        #   Plano en X=X_WL=–8.4cm, extrude –X (hacia exterior) por CUT_D.
        #
        # DERECHO (jacks OUT + USB-C):
        #   TRS OUT L:  Ø11.0mm @ Z=38mm=3.8cm, Y=21mm=2.1cm
        #   TRS OUT R:  Ø11.0mm @ Z=60mm=6.0cm, Y=21mm=2.1cm
        #   USB-C Data: rect 10.8×5.3mm @ Z=15mm=1.5cm, Y=21mm=2.1cm
        #   USB-C Host: rect 10.8×5.3mm @ Z=82mm=8.2cm, Y=21mm=2.1cm
        #   Plano en X=X_WR=+8.4cm, extrude +X (hacia exterior) por CUT_D.
        #
        # Todos los centros verticales en JY=2.1cm (JACK_Z=21mm desde base).
        # ==================================================================
        JY    = 2.1        # Y centro conectores = 21mm
        R_J   = 0.55       # radio TRS Ø11mm (10.5 nominal + 0.3 FDM + 0.2 extra)
        R_A   = 0.375      # radio AUX 3.5mm Ø7.5mm
        UW    = 1.08/2     # semiancho USB-C rect = 5.4mm  (10.8/2)
        UH    = 0.53/2     # semialto  USB-C rect = 2.65mm (5.3/2)
        CUT_D = WT + 0.1   # profundidad corte = 3+1mm = 4mm (pasante con margen)

        # --- LADO IZQUIERDO ---
        cp_lc = cplanes.createInput()
        cp_lc.setByOffset(comp.yZConstructionPlane, val(X_WL))
        plane_lc = cplanes.add(cp_lc)
        plane_lc.name = 'Plane_CutL'

        sk_lc = comp.sketches.add(plane_lc)
        sk_lc.name = 'Cutouts_Left'
        for z_cm, r_cm in [(3.0, R_J), (5.5, R_J), (7.8, R_A)]:
            sk_lc.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(X_WL, JY, z_cm), r_cm)

        sel_lc = OC.create()
        for i in range(sk_lc.profiles.count):
            sel_lc.add(sk_lc.profiles.item(i))
        inp_lc = exts.createInput(sel_lc, FO.NewBodyFeatureOperation)
        inp_lc.setOneSideExtent(dist(CUT_D), ED.NegativeExtentDirection)
        feat_lc = exts.add(inp_lc)
        lc_coll = OC.create()
        for i in range(feat_lc.bodies.count):
            lc_coll.add(feat_lc.bodies.item(i))
        ci_lc = combos.createInput(body, lc_coll)
        ci_lc.operation = FO.CutFeatureOperation
        ci_lc.isKeepToolBodies = False
        combos.add(ci_lc)
        print(f'Step 9L — Cutouts izq ({feat_lc.bodies.count} bodies, esperado 3): faces={body.faces.count}')
        print(f'  Esperado: TRS INL/INR Ø11mm @ Z=30,55mm Y=21mm + AUX Ø7.5mm @ Z=78mm Y=21mm')

        # --- LADO DERECHO ---
        cp_rc = cplanes.createInput()
        cp_rc.setByOffset(comp.yZConstructionPlane, val(X_WR))
        plane_rc = cplanes.add(cp_rc)
        plane_rc.name = 'Plane_CutR'

        sk_rc = comp.sketches.add(plane_rc)
        sk_rc.name = 'Cutouts_Right'
        # Círculos TRS OUT L y OUT R
        for z_cm in [3.8, 6.0]:
            sk_rc.sketchCurves.sketchCircles.addByCenterRadius(
                P.create(X_WR, JY, z_cm), R_J)
        # Rectángulos USB-C (Data en Z=1.5cm, Host en Z=8.2cm)
        for z_cm in [1.5, 8.2]:
            y0 = JY - UH;  y1 = JY + UH
            z0 = z_cm - UW; z1 = z_cm + UW
            ln_rc = sk_rc.sketchCurves.sketchLines
            ln_rc.addByTwoPoints(P.create(X_WR, y0, z0), P.create(X_WR, y1, z0))
            ln_rc.addByTwoPoints(P.create(X_WR, y1, z0), P.create(X_WR, y1, z1))
            ln_rc.addByTwoPoints(P.create(X_WR, y1, z1), P.create(X_WR, y0, z1))
            ln_rc.addByTwoPoints(P.create(X_WR, y0, z1), P.create(X_WR, y0, z0))

        sel_rc = OC.create()
        for i in range(sk_rc.profiles.count):
            sel_rc.add(sk_rc.profiles.item(i))
        inp_rc = exts.createInput(sel_rc, FO.NewBodyFeatureOperation)
        inp_rc.setOneSideExtent(dist(CUT_D), ED.PositiveExtentDirection)
        feat_rc = exts.add(inp_rc)
        rc_coll = OC.create()
        for i in range(feat_rc.bodies.count):
            rc_coll.add(feat_rc.bodies.item(i))
        ci_rc = combos.createInput(body, rc_coll)
        ci_rc.operation = FO.CutFeatureOperation
        ci_rc.isKeepToolBodies = False
        combos.add(ci_rc)
        print(f'Step 9R — Cutouts der ({feat_rc.bodies.count} bodies, esperado 4): faces={body.faces.count}')
        print(f'  Esperado: USB-C Data @ Z=15mm + TRS OUTL/R @ Z=38,60mm + USB-C Host @ Z=82mm, todos Y=21mm')

        # ==================================================================
        # RESUMEN FINAL CON BOUNDING BOX
        # ==================================================================
        bb   = body.boundingBox
        W_r  = (bb.maxPoint.x - bb.minPoint.x) * 10
        H_r  = (bb.maxPoint.y - bb.minPoint.y) * 10
        D_r  = (bb.maxPoint.z - bb.minPoint.z) * 10
        Ymin = bb.minPoint.y * 10
        Ymax = bb.maxPoint.y * 10

        summary = (
            f'01_PETG_Body v0.17 — BUILD COMPLETO\n\n'
            f'BBox final: {W_r:.0f}W x {D_r:.0f}D x {H_r:.1f}H mm\n'
            f'  (esperado: 180 x 100 x 38 mm)\n'
            f'Y range: {Ymin:.1f}..{Ymax:.1f} mm\n'
            f'  (esperado: 0..38 mm — si maxY > 38 los pedestales flotan!)\n\n'
            f'Faces totales: {body.faces.count}\n\n'
            f'Features:\n'
            f'  1. Exterior 180x100x38mm\n'
            f'  2. Fillets R2mm x4 aristas verticales\n'
            f'  3. Shell interior 3mm (top ABIERTO)\n'
            f'  4. Rebajes cheeks 6mm izq+der\n'
            f'  5. 6 standoffs PCB M3 (Y=3..8mm) + heat inserts\n'
            f'  6. 4 pedestales panel (Y=3..38mm, FIX v0.16) + heat inserts\n'
            f'  7. Slot pogo trasero 14.3x8.3mm @ Y=15mm\n'
            f'  8. 4 cavidades patas Ø9.9mm x 2mm\n'
            f'  9. Cutouts pared izq: TRS x2 Ø11mm + AUX Ø7.5mm\n'
            f'     Cutouts pared der: USB-C x2 rect + TRS x2 Ø11mm\n\n'
            f'STACK: top y bottom planos => apilable sobre slaves\n'
            f'POGO: cara trasera Z=0, centro Y=15mm\n'
            f'PEDESTALES: anclados al piso interior Y=3mm (FIX v0.16)\n'
            f'            crecen hasta boca Y=38mm => Join garantizado'
        )
        print('\n' + summary)
        ui.messageBox(summary)

    except Exception:
        if ui:
            ui.messageBox(traceback.format_exc())
        else:
            print(traceback.format_exc())

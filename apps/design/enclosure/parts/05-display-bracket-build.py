# =============================================================================
# GrooveForge Brain — 05_Display_Bracket (Soporte ESP32-S3 GC9A01 bajo panel)
# Script Fusion 360 (Python API) — v0.2
#
# CORRECCION vs v0.1:
#   - PCB Waveshare es CIRCULAR Ø41mm (no cuadrada 25.5×25.5mm — error del spec v0.1)
#   - El modulo va DEBAJO del top panel, no encima
#   - El vidrio (Ø38.51mm) asoma por el cutout (Ø36mm); labio 1.25mm/lado retiene
#   - El bracket es un tubo cilindrico que abraza la PCB desde abajo
#   - BRACKET_H = 15mm es la profundidad desde cara inferior del aluminio hacia abajo
#
# SISTEMA DE COORDENADAS (bracket standalone, se modela en posicion de montaje):
#   Y=0  → cara superior del top panel (nivel de referencia)
#   Y=-2mm → cara inferior del aluminio (donde asienta el collarín del bracket)
#   Y=-2mm a Y=-17mm → zona del bracket (PANEL_T + BRACKET_H = 2 + 15 = 17mm)
#
# ANATOMIA VERTICAL (desde arriba hacia abajo):
#   Y=0        → cara superior aluminio — vidrio Ø38.51mm aflora aqui flush
#   Y=-2mm     → cara inferior aluminio — labio del vidrio apoya aqui
#   Y=-2mm     → tope del collarín / flange de montaje (fijado contra cara inf Al)
#   Y=-3mm     → base del collarín (1mm de espesor)
#   Y=-3mm     → inicio del tubo (abraza PCB Ø41mm)
#   Y=-17mm    → base del tubo (BRACKET_H=15mm + collarín 1mm + flange 2mm - 1mm = 17mm)
#
# POSICION EN ENSAMBLAJE:
#   Centro: X=0cm (X_panel=90mm), Z=6.0cm (Y_panel=60mm — D-08 resuelto)
#   El bracket baja desde la cara inferior del aluminio hacia el interior del PETG body.
#   Clearance necesario en el PCB principal debajo del bracket: BRACKET_H = 15mm
#
# COMO USAR:
#   Shift+S → Scripts → "+" → Python → pegar → Run
#   El componente '05_Display_Bracket' debe estar vacio.
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

        # ------------------------------------------------------------------
        # PARAMETROS (cm — unidad interna de Fusion)
        # Fuente: 00-master-parameters.md §12, datasheet Waveshare verificado
        # ------------------------------------------------------------------
        PCB_D     = 4.1   # ESP32_MODULE_D_PCB = Ø41mm PCB circular
        TUBE_ID   = 4.2   # cavidad interior tubo = PCB_D + 1mm clearance total
        TUBE_OD   = 4.5   # exterior tubo = BRACKET_BASE_W = 45mm
        COLLAR_ID = 3.65  # collarín interior = 36.5mm (< GLASS_OD=38.51mm; vidrio no cae)
        COLLAR_H  = 0.1   # 1mm espesor del collarín de retencion
        FLANGE_W  = 4.5   # flange cuadrado 45mm (coincide con TUBE_OD)
        FLANGE_H  = 0.2   # 2mm flange de montaje contra cara inferior aluminio
        TUBE_H    = 1.5   # 15mm profundidad tubo = BRACKET_H
        SLOT_W    = 0.8   # 8mm ranura cable FPC
        SLOT_H    = 0.4   # 4mm ranura cable FPC

        # Alturas Y acumuladas (Y=0 = cara inferior del aluminio = punto de referencia)
        # El bracket baja en Y negativo
        Y_TOP        = 0.0                        # cara inferior aluminio / tope del flange
        Y_FLANGE_BOT = Y_TOP - FLANGE_H           # -0.2cm
        Y_COLLAR_BOT = Y_FLANGE_BOT - COLLAR_H    # -0.3cm
        Y_TUBE_BOT   = Y_COLLAR_BOT - TUBE_H      # -1.8cm

        # Localizar o crear componente
        comp = None
        for occ in root.occurrences:
            if '05' in occ.component.name or 'Display_Bracket' in occ.component.name:
                comp = occ.component
                break
        if comp is None:
            occ2 = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
            comp = occ2.component
            comp.name = '05_Display_Bracket'

        exts   = comp.features.extrudeFeatures
        combos = comp.features.combineFeatures
        planes = comp.constructionPlanes

        # ==============================================================
        # STEP 1 — FLANGE DE MONTAJE  45 × 45 × 2mm
        # ==============================================================
        # Cuadrado anular: exterior 45×45mm, interior circular Ø45mm (= TUBE_OD).
        # Asienta contra la cara inferior del aluminio. Fijacion: VHB tape o 2× M2 screws.
        # Y: desde Y_TOP hacia Y_FLANGE_BOT (baja 2mm desde cara inf del aluminio).

        pin_fl = planes.createInput()
        pin_fl.setByOffset(comp.xZConstructionPlane, VI.createByReal(Y_TOP))
        flange_plane = planes.add(pin_fl)
        flange_plane.name = 'FlangeTop'

        sk1 = comp.sketches.add(flange_plane)
        sk1.name = 'Bracket_Flange'
        hw = FLANGE_W / 2  # 2.25cm
        ln = sk1.sketchCurves.sketchLines
        ln.addByTwoPoints(P.create(-hw, Y_TOP, -hw), P.create( hw, Y_TOP, -hw))
        ln.addByTwoPoints(P.create( hw, Y_TOP, -hw), P.create( hw, Y_TOP,  hw))
        ln.addByTwoPoints(P.create( hw, Y_TOP,  hw), P.create(-hw, Y_TOP,  hw))
        ln.addByTwoPoints(P.create(-hw, Y_TOP,  hw), P.create(-hw, Y_TOP, -hw))
        sk1.sketchCurves.sketchCircles.addByCenterRadius(P.create(0, Y_TOP, 0), TUBE_OD / 2)

        # Perfil del anular cuadrado-circular (el marco entre el cuadrado y el circulo)
        # Es el perfil que NO es el circulo — seleccionar por area: el cuadrado exterior menos el circulo
        # Area cuadrado: 4.5×4.5=20.25cm², area circulo: π×2.25²≈15.9cm²
        # El perfil del marco tiene area ≈ 4.35cm²
        # El perfil del circulo interior tiene area ≈ 15.9cm²
        # Queremos el marco — area intermedia (no la mas grande ni la mas pequena si hay 3 perfiles)
        profiles = [sk1.profiles.item(i) for i in range(sk1.profiles.count)]
        profiles.sort(key=lambda p: p.areaProperties().area)
        # El perfil mas pequeno es el anular (marco cuadrado - circulo) — o puede ser el circulo
        # Con un cuadrado y un circulo inscrito: Fusion crea 2 perfiles: circulo interior + marco
        # El circulo interior es el mas pequeno (≈15.9cm²? No — el marco es mas pequeno)
        # Marco ≈ 4.35cm², circulo ≈ 15.9cm² → el mas pequeno es el marco
        flange_prof = profiles[0]  # perfil mas pequeno = marco anular

        inp1 = exts.createInput(flange_prof, FO.NewBodyFeatureOperation)
        inp1.setOneSideExtent(
            DD.create(VI.createByReal(abs(FLANGE_H))),
            ED.NegativeExtentDirection  # baja desde Y_TOP
        )
        feat1 = exts.add(inp1)
        feat1.bodies.item(0).name = 'Bracket_Body'
        body = comp.bRepBodies.item(0)
        print(f'Step 1 — Flange: health={feat1.healthState}  faces={body.faces.count}')

        # ==============================================================
        # STEP 2 — COLLARÍN DE RETENCION  Ø45 ext / Ø36.5 int × 1mm
        # ==============================================================
        # El collarín retiene la PCB circular Ø41mm (pasa por Ø45 interior del tubo,
        # queda bloqueada contra el collarín Ø36.5mm interior).
        # El vidrio Ø38.51mm si supera el interior del collarín — el labio de vidrio
        # (Ø38.51) es mayor que Ø36.5 → el vidrio no cae por el collarín. Correcto.
        # PCB Ø41mm > Ø36.5mm → la PCB no cae por el collarín. Correcto.
        # El vidrio (Ø35.67mm viewing area) pasa libremente por el cutout del aluminio Ø36mm.

        pin_col = planes.createInput()
        pin_col.setByOffset(comp.xZConstructionPlane, VI.createByReal(Y_FLANGE_BOT))
        collar_plane = planes.add(pin_col)
        collar_plane.name = 'CollarBase'

        sk2 = comp.sketches.add(collar_plane)
        sk2.name = 'Bracket_Collar'
        circles2 = sk2.sketchCurves.sketchCircles
        circles2.addByCenterRadius(P.create(0, Y_FLANGE_BOT, 0), TUBE_OD / 2)   # exterior Ø45
        circles2.addByCenterRadius(P.create(0, Y_FLANGE_BOT, 0), COLLAR_ID / 2) # interior Ø36.5

        # Perfil anular (el marco entre los dos circulos)
        profiles2 = [sk2.profiles.item(i) for i in range(sk2.profiles.count)]
        profiles2.sort(key=lambda p: p.areaProperties().area)
        collar_prof = profiles2[0]  # anular = mas pequeno de los dos perfiles

        inp2 = exts.createInput(collar_prof, FO.JoinFeatureOperation)
        inp2.setOneSideExtent(
            DD.create(VI.createByReal(abs(COLLAR_H))),
            ED.NegativeExtentDirection
        )
        feat2 = exts.add(inp2)
        print(f'Step 2 — Collarín: health={feat2.healthState}  faces={body.faces.count}')

        # ==============================================================
        # STEP 3 — TUBO PRINCIPAL  Ø45 ext / Ø42 int × 15mm
        # ==============================================================
        # El tubo abraza la PCB circular Ø41mm (clearance 0.5mm/lado).
        # Se extiende 15mm hacia abajo desde la base del collarín.

        pin_tb = planes.createInput()
        pin_tb.setByOffset(comp.xZConstructionPlane, VI.createByReal(Y_COLLAR_BOT))
        tube_plane = planes.add(pin_tb)
        tube_plane.name = 'TubeTop'

        sk3 = comp.sketches.add(tube_plane)
        sk3.name = 'Bracket_Tube'
        circles3 = sk3.sketchCurves.sketchCircles
        circles3.addByCenterRadius(P.create(0, Y_COLLAR_BOT, 0), TUBE_OD / 2)  # exterior Ø45
        circles3.addByCenterRadius(P.create(0, Y_COLLAR_BOT, 0), TUBE_ID / 2)  # interior Ø42

        profiles3 = [sk3.profiles.item(i) for i in range(sk3.profiles.count)]
        profiles3.sort(key=lambda p: p.areaProperties().area)
        tube_prof = profiles3[0]  # anular Ø45/Ø42

        inp3 = exts.createInput(tube_prof, FO.JoinFeatureOperation)
        inp3.setOneSideExtent(
            DD.create(VI.createByReal(abs(TUBE_H))),
            ED.NegativeExtentDirection
        )
        feat3 = exts.add(inp3)
        print(f'Step 3 — Tubo: health={feat3.healthState}  faces={body.faces.count}')

        # ==============================================================
        # STEP 4 — RANURA DE CABLE FPC  8 × 4mm
        # ==============================================================
        # Ranura centrada verticalmente en la pared del tubo, cara trasera (Z = -TUBE_OD/2).
        # Permite rutear el conector FPC del modulo ESP32 hacia la PCB principal.
        # "Trasera" = cara que da hacia Z negativo (cara Pogo del instrumento).

        slot_y_center = Y_COLLAR_BOT - TUBE_H / 2  # mitad vertical del tubo
        slot_y0 = slot_y_center - SLOT_H / 2
        slot_y1 = slot_y_center + SLOT_H / 2
        slot_x0 = -SLOT_W / 2
        slot_x1 =  SLOT_W / 2
        face_z  = -(TUBE_OD / 2)  # cara exterior trasera del tubo

        # Plano en la cara exterior trasera del tubo (Z = -TUBE_OD/2)
        # xYConstructionPlane es Z=0; offset en -Z por TUBE_OD/2
        pin_slot = planes.createInput()
        pin_slot.setByOffset(comp.xYConstructionPlane, VI.createByReal(-TUBE_OD / 2))
        slot_plane = planes.add(pin_slot)
        slot_plane.name = 'CableSlotFace'

        sk4 = comp.sketches.add(slot_plane)
        sk4.name = 'Cable_Slot'
        ln4 = sk4.sketchCurves.sketchLines
        for (xa, ya), (xb, yb) in [
            ((slot_x0, slot_y0), (slot_x1, slot_y0)),
            ((slot_x1, slot_y0), (slot_x1, slot_y1)),
            ((slot_x1, slot_y1), (slot_x0, slot_y1)),
            ((slot_x0, slot_y1), (slot_x0, slot_y0)),
        ]:
            ln4.addByTwoPoints(P.create(xa, ya, face_z), P.create(xb, yb, face_z))

        slot_prof = min(
            [sk4.profiles.item(i) for i in range(sk4.profiles.count)],
            key=lambda p: p.areaProperties().area
        )

        # Tool body para el slot
        wall_t = (TUBE_OD - TUBE_ID) / 2  # espesor de pared del tubo = 0.15cm = 1.5mm
        inp4 = exts.createInput(slot_prof, FO.NewBodyFeatureOperation)
        inp4.setOneSideExtent(
            DD.create(VI.createByReal(wall_t + 0.05)),  # +0.5mm para corte limpio
            ED.PositiveExtentDirection  # entra hacia el interior del tubo (+Z)
        )
        slot_tool = exts.add(inp4).bodies.item(0)

        slot_coll = OC.create()
        slot_coll.add(slot_tool)
        ci_slot = combos.createInput(body, slot_coll)
        ci_slot.operation = FO.CutFeatureOperation
        ci_slot.isKeepToolBodies = False
        combos.add(ci_slot)
        print(f'Step 4 — Cable slot: faces={body.faces.count}')

        # ==============================================================
        # RESUMEN
        # ==============================================================
        bb = body.boundingBox
        h_total = abs(bb.maxPoint.y - bb.minPoint.y) * 10  # mm

        summary = (
            f'✓ 05_Display_Bracket v0.2 — BUILD COMPLETO\n\n'
            f'ARQUITECTURA: modulo debajo del panel (v0.5)\n\n'
            f'BBox X: {(bb.maxPoint.x - bb.minPoint.x)*10:.1f}mm  (esperado: 45mm)\n'
            f'BBox Z: {(bb.maxPoint.z - bb.minPoint.z)*10:.1f}mm  (esperado: 45mm)\n'
            f'BBox Y: {h_total:.1f}mm  (flange 2mm + collarín 1mm + tubo 15mm = 18mm)\n\n'
            f'Geometria:\n'
            f'  Flange: 45×45mm anular × 2mm (VHB contra cara inf. aluminio)\n'
            f'  Collarín retención: Ø45/Ø36.5mm × 1mm (bloquea PCB Ø41mm + vidrio Ø38.51mm)\n'
            f'  Tubo: Ø45/Ø42mm × 15mm (abraza PCB Ø41mm, clearance 0.5mm/lado)\n'
            f'  Ranura FPC: 8×4mm en cara trasera del tubo\n\n'
            f'Verificacion datasheet Waveshare:\n'
            f'  PCB Ø41mm → tubo Ø42mm interior: clearance 0.5mm/lado ✓\n'
            f'  Vidrio Ø38.51mm > collarín Ø36.5mm → vidrio NO cae ✓\n'
            f'  Cutout panel Ø36mm < Ø38.51mm → labio 1.25mm/lado sobre aluminio ✓\n'
            f'  Viewing area Ø35.67mm < cutout Ø36mm → visible sin obstruccion ✓\n\n'
            f'POSICION EN ENSAMBLAJE:\n'
            f'  Centro: X=0cm (X_panel=90mm), Z=6.0cm (Y_panel=60mm)\n'
            f'  El bracket baja desde cara inferior del aluminio (Y=BODY_H=38mm)\n'
            f'  Faces: {body.faces.count}'
        )
        print(summary)
        ui.messageBox(summary)

    except:
        if ui:
            ui.messageBox(traceback.format_exc())

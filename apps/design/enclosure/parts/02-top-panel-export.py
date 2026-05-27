# =============================================================================
# GrooveForge Brain — 02_Top_Panel — EXPORT STEP + STL
#
# Exporta el top panel terminado a:
#   apps/design/3d-models/02-top-panel.step
#   apps/design/3d-models/02-top-panel.stl
#
# Correr DESPUÉS de 02-top-panel-build.py (o cuando el panel esté completo).
# =============================================================================

import adsk.core
import adsk.fusion
import os
import traceback


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui  = app.userInterface
        des = adsk.fusion.Design.cast(app.activeProduct)
        root = des.rootComponent

        # Ruta de destino (ajustar si el repo está en otro lugar)
        repo_base = os.path.expanduser(
            '~/GROOVEFORGE/grooveforge-brain/apps/design/3d-models'
        )
        os.makedirs(repo_base, exist_ok=True)

        step_path = os.path.join(repo_base, '02-top-panel.step')
        stl_path  = os.path.join(repo_base, '02-top-panel.stl')

        # Localizar componente
        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '02' in n or 'Top_Panel' in n:
                comp = occ.component
                break
        if comp is None:
            ui.messageBox('ERROR: No se encontró 02_Top_Panel_Aluminum')
            return

        em = des.exportManager

        # --- STEP ---
        step_opts = em.createSTEPExportOptions(step_path, comp)
        em.execute(step_opts)
        print(f'STEP exportado: {step_path}')

        # --- STL (mesh de fabricación) ---
        stl_opts = em.createSTLExportOptions(comp, stl_path)
        stl_opts.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementMedium
        stl_opts.isBinaryFormat = True
        em.execute(stl_opts)
        print(f'STL  exportado: {stl_path}')

        ui.messageBox(
            f'✓ Export completo\n\n'
            f'STEP: {step_path}\n'
            f'STL:  {stl_path}'
        )

    except Exception:
        if ui:
            ui.messageBox(traceback.format_exc())

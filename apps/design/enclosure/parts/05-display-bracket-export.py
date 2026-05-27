# =============================================================================
# GrooveForge Brain — 05_Display_Bracket — EXPORT STEP + STL
#
# Exporta el bracket terminado a:
#   apps/design/3d-models/05-display-bracket.step
#   apps/design/3d-models/05-display-bracket.stl
#
# Correr DESPUÉS de 05-display-bracket-build.py.
# El STL se usa directamente para impresión 3D FDM en PETG.
# =============================================================================

import adsk.core, adsk.fusion, os, traceback

def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui  = app.userInterface
        des = adsk.fusion.Design.cast(app.activeProduct)
        root = des.rootComponent

        repo_base = os.path.expanduser(
            '~/GROOVEFORGE/grooveforge-brain/apps/design/3d-models'
        )
        os.makedirs(repo_base, exist_ok=True)
        step_path = os.path.join(repo_base, '05-display-bracket.step')
        stl_path  = os.path.join(repo_base, '05-display-bracket.stl')

        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '05' in n or 'Display_Bracket' in n:
                comp = occ.component
                break
        if comp is None:
            ui.messageBox('ERROR: No se encontró 05_Display_Bracket')
            return

        em = des.exportManager

        # STEP
        step_opts = em.createSTEPExportOptions(step_path, comp)
        em.execute(step_opts)

        # STL para impresión FDM
        stl_opts = em.createSTLExportOptions(comp, stl_path)
        stl_opts.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementMedium
        stl_opts.isBinaryFormat = True
        em.execute(stl_opts)

        ui.messageBox(
            f'✓ Export completo\n\n'
            f'STEP: {step_path}\n'
            f'STL:  {stl_path}\n\n'
            f'Orientación para imprimir: flange hacia abajo.\n'
            f'Settings: layer 0.2mm, infill 40%, perímetros 4, PETG dark.'
        )

    except Exception:
        if ui: ui.messageBox(traceback.format_exc())

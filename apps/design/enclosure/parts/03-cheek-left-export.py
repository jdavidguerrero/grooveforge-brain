# =============================================================================
# GrooveForge Brain — 03_Cheek_Left — EXPORT STEP
#
# Exporta el cheek izquierdo terminado a:
#   apps/design/3d-models/03-cheek-left.step
#
# Correr DESPUÉS de 03-cheek-left-build.py.
# Nota: No se exporta STL — la guadua se fabrica con carpintería, no 3D print.
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
        step_path = os.path.join(repo_base, '03-cheek-left.step')

        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '03' in n or 'Cheek_Left' in n:
                comp = occ.component
                break
        if comp is None:
            ui.messageBox('ERROR: No se encontró 03_Cheek_Left_Guadua')
            return

        em = des.exportManager
        step_opts = em.createSTEPExportOptions(step_path, comp)
        em.execute(step_opts)

        ui.messageBox(f'✓ STEP exportado:\n{step_path}')

    except Exception:
        if ui: ui.messageBox(traceback.format_exc())

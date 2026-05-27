# =============================================================================
# GrooveForge Brain — 04_Cheek_Right — EXPORT STEP
#
# Exporta el cheek derecho terminado a:
#   apps/design/3d-models/04-cheek-right.step
#
# Correr DESPUÉS de 04-cheek-right-build.py.
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
        step_path = os.path.join(repo_base, '04-cheek-right.step')

        comp = None
        for occ in root.occurrences:
            n = occ.component.name
            if '04' in n or 'Cheek_Right' in n:
                comp = occ.component
                break
        if comp is None:
            ui.messageBox('ERROR: No se encontró 04_Cheek_Right_Guadua')
            return

        em = des.exportManager
        step_opts = em.createSTEPExportOptions(step_path, comp)
        em.execute(step_opts)

        ui.messageBox(f'✓ STEP exportado:\n{step_path}')

    except Exception:
        if ui: ui.messageBox(traceback.format_exc())

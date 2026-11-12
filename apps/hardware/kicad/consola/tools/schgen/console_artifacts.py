"""Reviewable mechanical handoff and project setup, without creating a PCB layout."""

import csv
import json
from pathlib import Path

from .boards import NEW_BOARDS
from .pcb_k import LED_ORDER
from .passive_boards import FSR_NAMES

CONSOLE = Path(__file__).resolve().parents[2]


def write_csv(path, columns, rows):
    """Write stable UTF-8 CSV with explicit headers."""
    with Path(path).open('w', newline='') as stream:
        writer = csv.DictWriter(stream, columns, lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


def placement_rows(name):
    """Return panel coordinates, preserving the sensor/key names from the approved CSVs."""
    with (CONSOLE / 'panel/panel-coordinates-console-v1.csv').open() as stream:
        panel = {row['ref']: row for row in csv.DictReader(stream)}
    rows = []

    def add(ref, component, label, x, y, rotation, side, notes=''):
        rows.append(dict(reference=ref, component=component, name=label, x_mm=float(x),
                         y_mm=float(y), rotation_deg=rotation, side=side, notes=notes))

    if name.startswith('pcb-k-'):
        indices = range(17) if name.endswith('left') else range(17, 26)
        for i in indices:
            label = LED_ORDER[i]
            p = panel[label]
            x, y = float(p['x_mm']), float(p['y_mm'])
            add(f'SW{i+1}', 'key', label, x, y, 0, 'F', 'huella combinada SW + zocalo')
            add(f'SW{i+1}', 'socket', label, x, y, 0, 'B', 'mismo footprint SW; NO colocar dos veces')
            add(f'D{i+1}', 'led', label, x, y+4.93, 0, 'B', f'LED index {i}; centro de ventana')
    elif name == 'pcb-f':
        for i in range(1, 5):
            p = panel[f'F{i}']
            add(f'RV{i}', 'fader', f'F{i}', p['x_mm'], p['y_mm'], 90, 'F',
                'rotacion KiCad CCW; eje largo paralelo a y del panel')
    elif name == 'pcb-p':
        with (CONSOLE / 'panel/pcb-outlines-console-v5.csv').open() as stream:
            sensors = {r['notes'].removeprefix('FSR '): r for r in csv.DictReader(stream)
                       if r['board'] == 'PCB-P' and r['item'] == 'FSR 402'}
        for i, label in enumerate(FSR_NAMES, 1):
            p = sensors[label]
            add(f'FSR{i}', 'fsr', label, p['x_mm'], p['y_mm'], 0, 'F',
                'datum disco CSV; THT socket P2.54 pads x +/-1.27 y_KiCad +11; cola hacia disco (-y_KiCad), validar acomodo/altura')
    return rows


def write_artifacts(name, directory):
    """Create project and placement handoff only for the new boards; never touch PCB-M."""
    if name not in NEW_BOARDS:
        return
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    project = directory / f'{name}.kicad_pro'
    if not project.exists():
        project.write_text(json.dumps({'meta': {'filename': project.name, 'version': 1}}, indent=2)+'\n')
    # Resolve relative to the output location, also when generating review/test copies.
    import os
    gf = os.path.relpath(CONSOLE / 'lib/gf_consola.pretty', directory)
    legacy = os.path.relpath(CONSOLE / 'boards/lib/groovepush.pretty', directory)
    (directory / 'fp-lib-table').write_text(
        '(fp_lib_table\n  (version 7)\n'
        f'  (lib (name "gf_consola")(type "KiCad")(uri "${{KIPRJMOD}}/{gf}")(options "")(descr "Console"))\n'
        f'  (lib (name "groovepush")(type "KiCad")(uri "${{KIPRJMOD}}/{legacy}")(options "")(descr "Legacy fader"))\n)\n')
    rows = placement_rows(name)
    write_csv(directory / 'placement.csv', rows[0].keys(), rows)
    if name in ('pcb-f', 'pcb-p'):
        with (CONSOLE / 'panel/pcb-outlines-console-v5.csv').open() as stream:
            reader = csv.DictReader(stream)
            mechanical = [r for r in reader if r['board'] == name.upper()]
            write_csv(directory / 'mechanical.csv', reader.fieldnames, mechanical)
    sensor_note = ('''
PCB-P D32: FSR402 de Juan con dos machos prensados P2.54, ver ../../docs/fotos/fsr402-juan.png.
Huella gf_consola:FSR402_Socket_1x02_P2.54mm con landing THT hembra/soldadura, taladro1mm.
Datum del footprint = centro disco del CSV, no centro del socket. Pads x +/-1.27, y_KiCad +11mm
(socket y_panel = disco-11mm); cola orientada hacia disco (-y_KiCad / +y_panel). Mantener centros
de los seis discos. Antes de layout definitivo verificar acomodo/longitud/altura con FSR real;
no forzar pliegues ni soldar sobre la película. El layout .kicad_pcb lo actualiza otra tarea.
''' if name == 'pcb-p' else '')
    (directory / 'README.md').write_text(f'''# {name.upper()} — Console, esquema sin layout

Regenerar/verificar desde la raiz: `python3 apps/hardware/kicad/consola/tools/check_netlist.py --board {name} --regenerate`.

`placement.csv`: mm de panel, origen y desde el frente, +y hacia atras, vista superior.
Rotaciones CCW KiCad; F = superior, B = inferior. En layout: Y_KiCad = constante - y_panel.
Las filas key/socket con igual referencia son un solo footprint mixto; la fila socket es
solo para ensamble. LED a +4.93 mm del centro de tecla en y_panel. Las otras piezas de
PCB-K se colocan abajo (ver zonas/alturas de 28). Aun no se asignan posiciones de soporte.

`docs/` contiene XML real, PDF y ERC. Ver `../../tools/REPORT-placas.md` para limites,
huellas provisionales y verificacion manual. Los archivos `.kicad_pcb`/`.kicad_dru` que ya
existieran en PCB-F pertenecen a GroovePush: no son un layout de Console ni son fabricables
con este esquema. No se ha generado ni actualizado layout en esta entrega.
{sensor_note}
'''.rstrip()+'\n')

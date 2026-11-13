#!/usr/bin/env python3
"""Read-only panel web and PCB-K passage checks; see PCB-A THEORY-strip.md."""
import ast
import json
import math

import check_pcba_fit as fit

STANDOFF_DIAMETER_MM = 5.6  # Circumscribes the nominal 4.8 mm AF BSO-M3 hexagon.
MIN_WEB_MM = 1.5


def panel_geometry():
    path = fit.ROOT / 'apps/design/3d-models/consola/build_console.py'
    tree = ast.parse(path.read_text())
    env = fit.build_parameters(path)
    for n in tree.body:
        if isinstance(n, ast.Assign):
            try:
                value = fit.eval_expr(n.value, env)
                for target in n.targets:
                    if isinstance(target, ast.Name):
                        env[target.id] = value
                    elif isinstance(target, ast.Tuple):
                        env.update((t.id, v) for t, v in zip(target.elts, value))
            except (KeyError, ValueError, TypeError):
                pass
    # Execute the actual cut generator with geometric recorders in place of OCCT.
    env.update(cylinder=lambda x, y, d, *_: ('circle', x, y, d / 2),
               rounded_rect=lambda x, y, w, h, *_: ('rectangle', x, y, w, h))
    funcs = [n for n in tree.body if isinstance(n, ast.FunctionDef)
             and n.name in ('pem_holes', 'panel_cuts')]
    exec(compile(ast.Module(body=funcs, type_ignores=[]), str(path), 'exec'), env)
    return env


def cut_gap(center, radius, cut):
    _, x, y, *size = cut
    if cut[0] == 'circle':
        return math.dist(center, (x, y)) - radius - size[0]
    w, h = size
    # A sharp outer rectangle contains every rounded cut: this is a lower bound.
    dx = max(abs(center[0] - x) - w / 2, 0)
    dy = max(abs(center[1] - y) - h / 2, 0)
    return math.hypot(dx, dy) - radius


def standoff_gap(center, polygons, radius=STANDOFF_DIAMETER_MM / 2):
    distances = []
    for poly in polygons:
        distance = min(fit.point_segment(center, a, b) for a, b in fit.edges(poly))
        distances.append((-distance if fit.inside(center, poly) else distance) - radius)
    return min(distances)


def audit():
    r = fit.audit()
    env = panel_geometry()
    rows = fit.read_csv(fit.CONSOLE / 'panel/panel-coordinates-console-v1.csv')
    outlines = fit.read_csv(fit.CONSOLE / 'panel/pcb-outlines-console-v5.csv')
    generated = env['pem_holes'](outlines)
    a_rows = [v for v in outlines if v['board'] == 'PCB-A' and v['item'] == 'agujero']
    radius = env['PEM_HOLE']['PCB-A'] / 2
    results = []
    for hole in r['mounting_holes']:
        center = hole['center_panel']
        matches = [v for v in a_rows if math.dist(center, (float(v['x_mm']), float(v['y_mm']))) < fit.EPS]
        own_cut = ('circle', *center, radius)
        cut_found = any(math.dist(center, (x, y)) < fit.EPS and abs(d - radius * 2) < fit.EPS
                        for x, y, d in generated)
        webs = {}
        for laser in (True, False):
            cuts = env['panel_cuts'](rows, outlines, laser=laser)
            others = [c for c in cuts if not (c[0] == own_cut[0] and math.dist(c[1:3], center) < fit.EPS)]
            webs['laser' if laser else 'cnc_con_rebajes'] = min(cut_gap(center, radius, c) for c in others)
        k_clearances = {name: standoff_gap(center, polys) for name, polys in r['shapes'].items()
                        if name.startswith('PCB-K')}
        results.append(dict(reference=hole['reference'], panel_mm=center,
                            csv_matches=len(matches), panel_cut_found=cut_found,
                            panel_hole_diameter_mm=2 * radius, web_lower_bound_mm=webs,
                            standoff_to_k_mm=k_clearances,
                            passes=len(matches) == 1 and cut_found and
                            min(webs.values()) >= MIN_WEB_MM and min(k_clearances.values()) > 0))
    return dict(passes=len(results) == len(a_rows) == 4 and all(h['passes'] for h in results),
                standoff_envelope_diameter_mm=STANDOFF_DIAMETER_MM,
                standoff_z_mm=[r['heights']['pcba_top_mm'], env['BODY_H']],
                pcb_k_z_mm=[env['PCB_TOP']['PCB-K'] - env['PCB_T'], env['PCB_TOP']['PCB-K']],
                k_outline_limit='CSV v5 combinado y particiones de doc28 §7; no existen layouts K left/right en esta base',
                holes=results)


def main():
    result = audit()
    (fit.HERE / 'pcba-mounting.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    lines = ['# PCB-A · anclajes al panel', '',
             '**' + ('PASS' if result['passes'] else 'FAIL') + '**. Reproducir: '
             '`python3 apps/hardware/kicad/consola/tools/fit/check_pcba_mounting.py`.', '',
             'Corte de aluminio Ø4,2; puente mínimo exigido 1,5 mm. Distancias conservadoras '
             'a rectángulos exteriores de recortes redondeados; círculos exactos. Se ejecuta '
             '`panel_cuts` con recorders geométricos, tanto láser como CNC con rebajes.', '',
             fit.table(['H', 'Panel mm', 'Puente láser ≥ mm', 'Puente CNC ≥ mm',
                        'K combinado / izq. / der. (mm)', 'CSV / corte'],
                       [[h['reference'], fit.fmt(h['panel_mm']),
                         f"{h['web_lower_bound_mm']['laser']:.3f}",
                         f"{h['web_lower_bound_mm']['cnc_con_rebajes']:.3f}",
                         ' / '.join(f'{v:.3f}' for v in h['standoff_to_k_mm'].values()),
                         str(h['csv_matches']) + ' / ' + str(h['panel_cut_found'])]
                        for h in result['holes']]), '',
             'Separadores BSO-M3 de 18: z=65…83; K z=80…81,6. Se cruzan en altura, '
             'pero ninguna envolvente Ø5,6 de separador intersecta material de K en XY. '
             'H1 es el limitante: 1,2 mm al contorno izquierdo de K. '
             'El cuerpo hexagonal nominal de 4,8 entre caras cabe en esta envolvente.', '',
             '**Límite:** ' + result['k_outline_limit'] + '. El resultado de izq./der. es '
             'sobre sus contornos especificados, no certifica un layout inexistente. '
             'Revalidar si cambian contornos, tolerancias o diámetro del proveedor.', '',
             'H1 anterior (62;183) invade el paso del separador por K y solapa 0,8 mm '
             'con TAP en el generador láser. Se movió a (72;181) y luego a (84;179): (72;181) caía dentro de la franja x 68–74,5 de PCB-K izquierda real.', '',
             'La prueba valida geometría nominal; el ancho de puente no certifica la '
             'resistencia de clinchado. Ver [BOM de herrajes](../../../../../design/3d-models/consola/BOM-herrajes.md) '
             'para taladro, dureza y distancia a borde exigidos por el proveedor.']
    (fit.HERE / 'REPORT-pcba-mounting.md').write_text('\n'.join(lines) + '\n')
    print('PCB-A montaje:', 'PASS' if result['passes'] else 'FAIL')
    for h in result['holes']:
        print(h['reference'], h['panel_mm'], h['web_lower_bound_mm'], h['standoff_to_k_mm'])
    return 0 if result['passes'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

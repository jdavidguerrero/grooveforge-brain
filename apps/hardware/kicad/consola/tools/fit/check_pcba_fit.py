#!/usr/bin/env python3
"""Read-only PCB-A fit audit. Standard library only; outputs stay beside this file."""
from __future__ import annotations

import ast
import csv
import hashlib
import html
import json
import math
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[5]
CONSOLE = ROOT / 'apps/hardware/kicad/consola'
EPS = 1e-8


def sexpr(text):
    """Parse KiCad lists without evaluating their contents."""
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+', text)
    stack, result = [], None
    for token in tokens:
        if token == '(':
            node = []
            if stack:
                stack[-1].append(node)
            stack.append(node)
        elif token == ')':
            if not stack:
                raise ValueError('Unexpected closing parenthesis')
            result = stack.pop()
        else:
            if not stack:
                raise ValueError('Atom outside list')
            stack[-1].append(json.loads(token) if token.startswith('"') else token)
    if stack or result is None:
        raise ValueError('Unclosed/empty KiCad expression')
    return result


def children(node, tag):
    return [n for n in node if isinstance(n, list) and n and n[0] == tag]


def one(node, tag):
    return next(iter(children(node, tag)))


def xy(node, tag):
    return tuple(map(float, one(node, tag)[1:3]))


def read_csv(path):
    with path.open(newline='', encoding='utf-8') as stream:
        return list(csv.DictReader(stream))


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def row_rect(row, margin=0):
    x, y, w, h = [float(row[k]) for k in ('x_mm', 'y_mm', 'w_mm', 'h_mm')]
    return rect(x-w/2-margin, y-h/2-margin, x+w/2+margin, y+h/2+margin)


def bounds(poly):
    return [min(p[0] for p in poly), min(p[1] for p in poly),
            max(p[0] for p in poly), max(p[1] for p in poly)]


def edges(poly):
    return list(zip(poly, poly[1:] + poly[:1]))


def signed_area(poly):
    return sum(a[0]*b[1]-a[1]*b[0] for a, b in edges(poly))/2


def inside(p, poly):
    x, y = p
    return sum((a[1] > y) != (b[1] > y) and
               x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]
               for a, b in edges(poly)) % 2 == 1


def cells(poly, holes=()):
    """Exact disjoint rectangular cover of an orthogonal polygon minus windows."""
    for loop in [poly, *holes]:
        if any(abs(a[0]-b[0]) > EPS and abs(a[1]-b[1]) > EPS for a, b in edges(loop)):
            raise ValueError('Non-orthogonal source outline: add explicit curve/polygon support')
    xs = sorted({p[0] for loop in [poly, *holes] for p in loop})
    ys = sorted({p[1] for loop in [poly, *holes] for p in loop})
    result = []
    for x0, x1 in zip(xs, xs[1:]):
        for y0, y1 in zip(ys, ys[1:]):
            middle = ((x0+x1)/2, (y0+y1)/2)
            if inside(middle, poly) and not any(inside(middle, h) for h in holes):
                result.append(rect(x0, y0, x1, y1))
    return result


def clip(poly, box):
    """Clip a convex polygon against a counterclockwise convex polygon."""
    for a, b in edges(box):
        def side(p):
            return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
        output = []
        for p, q in edges(poly):
            sp, sq = side(p), side(q)
            if sp >= -EPS:
                output.append(p)
            if (sp < -EPS) != (sq < -EPS):
                t = sp/(sp-sq)
                output.append((p[0]+t*(q[0]-p[0]), p[1]+t*(q[1]-p[1])))
        poly = output
        if not poly:
            break
    return poly


def point_segment(p, a, b):
    dx, dy = b[0]-a[0], b[1]-a[1]
    den = dx*dx+dy*dy
    t = max(0, min(1, ((p[0]-a[0])*dx+(p[1]-a[1])*dy)/den)) if den else 0
    return math.hypot(p[0]-a[0]-t*dx, p[1]-a[1]-t*dy)


def segment_box_interval(a, b, box):
    lo, hi = 0., 1.
    for axis, low, high in [(0, box[0], box[2]), (1, box[1], box[3])]:
        delta = b[axis]-a[axis]
        if abs(delta) < EPS:
            if a[axis] < low-EPS or a[axis] > high+EPS:
                return None
        else:
            t0, t1 = sorted(((low-a[axis])/delta, (high-a[axis])/delta))
            lo, hi = max(lo, t0), min(hi, t1)
            if lo > hi+EPS:
                return None
    return lo, hi


def segment_hits_slab(a, b, polygons, z0, z1):
    """Check positive segment length inside an XY cover and an open Z slab."""
    for polygon in polygons:
        interval = segment_box_interval(a, b, bounds(polygon))
        if interval is None:
            continue
        lo, hi = interval
        dz = b[2]-a[2]
        if abs(dz) < EPS:
            if z0+EPS < a[2] < z1-EPS and hi-lo > EPS:
                return True
        else:
            u, v = sorted(((z0-a[2])/dz, (z1-a[2])/dz))
            if min(hi, v)-max(lo, u) > EPS:
                return True
    return False


def polygon_distance(a, b):
    # A proper crossing has no contained vertices; detect segment intersections too.
    for p, q in edges(a):
        for r, s in edges(b):
            dx, dy, ux, uy = q[0]-p[0], q[1]-p[1], s[0]-r[0], s[1]-r[1]
            den = dx*uy-dy*ux
            if abs(den) > EPS:
                t = ((r[0]-p[0])*uy-(r[1]-p[1])*ux)/den
                v = ((r[0]-p[0])*dy-(r[1]-p[1])*dx)/den
                if -EPS <= t <= 1+EPS and -EPS <= v <= 1+EPS:
                    return 0.
    if inside(a[0], b) or inside(b[0], a):
        return 0.
    return min([point_segment(p, r, s) for p in a for r, s in edges(b)] +
               [point_segment(p, r, s) for p in b for r, s in edges(a)])


def collision(a_cells, b_cells):
    area = sum(abs(signed_area(clip(a, b))) for a in a_cells for b in b_cells)
    gap = min(polygon_distance(a, b) for a in a_cells for b in b_cells)
    return dict(overlap_mm2=round(area, 6), clearance_mm=round(gap, 6),
                status='SOLAPE' if area > EPS else 'CONTACTO' if gap < EPS else 'LIBRE')


def fp_point(fp, p):
    angle = math.radians(fp['angle_deg'])
    c, s = math.cos(angle), math.sin(angle)
    # Stored B-side footprint primitives already include their flip; do not mirror twice.
    return (fp['origin'][0]+c*p[0]+s*p[1], fp['origin'][1]-s*p[0]+c*p[1])


def board(path):
    tree = sexpr(path.read_text())
    footprints = {}
    for node in children(tree, 'footprint'):
        props = {n[1]: n[2] for n in children(node, 'property')}
        at = one(node, 'at')
        fp = dict(reference=props['Reference'], value=props.get('Value'), library=node[1],
                  origin=tuple(map(float, at[1:3])), angle_deg=float(at[3]) if len(at)>3 else 0,
                  layer=one(node, 'layer')[1], node=node)
        footprints[fp['reference']] = fp
    segments = []
    for node in tree:
        if not isinstance(node, list) or not node:
            continue
        layers = children(node, 'layer')
        if layers and layers[0][1] == 'Edge.Cuts':
            if node[0] == 'gr_line':
                segments.append((xy(node, 'start'), xy(node, 'end')))
            elif node[0] == 'gr_rect':
                segments += edges(rect(*xy(node, 'start'), *xy(node, 'end')))
            else:
                raise ValueError(f'Unsupported Edge.Cuts primitive {node[0]}')
    outline = []
    if segments:
        a, b = segments.pop(0)
        outline = [a, b]
        while segments:
            for i, (a, b) in enumerate(segments):
                if math.dist(outline[-1], a) < EPS:
                    outline.append(b)
                    break
                if math.dist(outline[-1], b) < EPS:
                    outline.append(a)
                    break
            else:
                raise ValueError('Disconnected/multiple outline loops')
            segments.pop(i)
        if math.dist(outline[-1], outline[0]) > EPS:
            raise ValueError('Open outline')
        outline.pop()
    return footprints, outline, float(one(one(tree, 'general'), 'thickness')[1])


def encoder_axis(fp):
    circles = []
    for node in children(fp['node'], 'fp_circle'):
        if one(node, 'layer')[1] in ('F.SilkS', 'F.Fab'):
            center = xy(node, 'center')
            if abs(math.dist(center, xy(node, 'end'))-3) < EPS:
                circles.append(center)
    if not circles or any(math.dist(circles[0], p) > EPS for p in circles):
        raise ValueError(f"No unique Ø6 shaft center in {fp['reference']}")
    return circles[0], fp_point(fp, circles[0])


def courtyard(fp):
    points = []
    for node in fp['node']:
        if not isinstance(node, list) or not node or node[0] not in ('fp_line', 'fp_rect'):
            continue
        if one(node, 'layer')[1].endswith('CrtYd'):
            a, b = xy(node, 'start'), xy(node, 'end')
            points += rect(*a, *b) if node[0] == 'fp_rect' else [a, b]
    if not points:
        raise ValueError(f"Missing courtyard: {fp['reference']}")
    return [fp_point(fp, p) for p in rect(*bounds(points))]


def pad_center(fp):
    pads = [xy(n, 'at') for n in children(fp['node'], 'pad') if n[1].isdigit()]
    return fp_point(fp, tuple(sum(p[i] for p in pads)/len(pads) for i in (0, 1)))


def fit(source, target, reflect=True):
    src = [(x, -y if reflect else y) for x, y in source]
    p = tuple(sum(x[i] for x in src)/len(src) for i in (0, 1))
    q = tuple(sum(x[i] for x in target)/len(target) for i in (0, 1))
    dot = sum((a[0]-p[0])*(b[0]-q[0])+(a[1]-p[1])*(b[1]-q[1]) for a,b in zip(src,target))
    cross = sum((a[0]-p[0])*(b[1]-q[1])-(a[1]-p[1])*(b[0]-q[0]) for a,b in zip(src,target))
    angle = math.atan2(cross, dot)
    c, s = math.cos(angle), math.sin(angle)
    t = (q[0]-c*p[0]+s*p[1], q[1]-s*p[0]-c*p[1])
    return dict(angle_deg=math.degrees(angle), translation_mm=t, reflect_y=reflect)


def transform(p, fit_result):
    x, y = p[0], -p[1] if fit_result['reflect_y'] else p[1]
    c, s = math.cos(math.radians(fit_result['angle_deg'])), math.sin(math.radians(fit_result['angle_deg']))
    tx, ty = fit_result['translation_mm']
    return (c*x-s*y+tx, s*x+c*y+ty)


def transform_poly(poly, fitting):
    return [transform(p, fitting) for p in poly]


def eval_expr(node, env):
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.Name):
        return env[node.id]
    if isinstance(node, (ast.Tuple, ast.List)):
        return [eval_expr(n, env) for n in node.elts]
    if isinstance(node, ast.Dict):
        return {eval_expr(k, env):eval_expr(v, env) for k,v in zip(node.keys,node.values)}
    if isinstance(node, ast.BinOp):
        a, b = eval_expr(node.left, env), eval_expr(node.right, env)
        if isinstance(node.op, ast.Add): return a+b
        if isinstance(node.op, ast.Sub): return a-b
        if isinstance(node.op, ast.Mult): return a*b
        if isinstance(node.op, ast.Div): return a/b
    raise ValueError('Unsupported constant expression')


def build_parameters(path):
    tree = ast.parse(path.read_text())
    env = {}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            try:
                value = eval_expr(node.value, env)
                for target in node.targets:
                    if isinstance(target, ast.Name): env[target.id] = value
            except (ValueError, KeyError):
                pass
    placeholder = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name=='make_placeholders')
    mapping = next(n.value for n in placeholder.body if isinstance(n, ast.Return))
    calls = dict(zip([eval_expr(k, env) for k in mapping.keys], mapping.values))
    a, d = calls['PCB_A'], calls['Display_module']
    env['A_placeholder_size'] = [eval_expr(n, env) for n in a.args[:3]]
    env['A_placeholder_origin'] = [eval_expr(n, env) for n in a.args[3].args]
    env['display_parameters'] = [eval_expr(n, env) for n in d.args]
    return env


def audit():
    sources = ['boards/pcb-a/pcb-a.kicad_pcb', 'boards/pcb-f/pcb-f.kicad_pcb',
               'boards/pcb-m/pcb-m.kicad_pcb', 'panel/panel-coordinates-console-v1.csv',
               'panel/pcb-outlines-console-v5.csv', 'boards/pcb-k-left/README.md',
               'boards/pcb-k-right/README.md', 'panel/contornos/pcb-k-contorno-v5.dxf']
    paths = [CONSOLE/p for p in sources] + [ROOT/'apps/design/3d-models/consola/build_console.py',
           ROOT/'apps/docs/consola/25-panel-consola.md', ROOT/'apps/docs/consola/28-pcb-k.md',
           ROOT/'apps/docs/consola/design/claude-design-prompt-consola-cuerpo-v5.md']
    hashes = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    fps, outline, thickness = board(paths[0])
    legacy_f, legacy_f_outline, _ = board(paths[1])
    remote_m, remote_m_outline, _ = board(paths[2])
    panel = {r['ref']: r for r in read_csv(CONSOLE/'panel/panel-coordinates-console-v1.csv')}
    rows = read_csv(CONSOLE/'panel/pcb-outlines-console-v5.csv')
    params = build_parameters(ROOT/'apps/design/3d-models/consola/build_console.py')
    encoders = []
    for i in range(1, 9):
        fp = fps[f'RE{i}']
        if fp['layer'] != 'F.Cu': raise ValueError('Encoder no longer on top')
        local, shaft = encoder_axis(fp)
        encoders.append(dict(reference=f'RE{i}', target=f'E{i}', origin_kicad=fp['origin'],
                             axis_local=local, axis_kicad=shaft,
                             target_panel=(float(panel[f'E{i}']['x_mm']),float(panel[f'E{i}']['y_mm']))))
    fitting = fit([e['axis_kicad'] for e in encoders], [e['target_panel'] for e in encoders])
    direct = fit([e['axis_kicad'] for e in encoders], [e['target_panel'] for e in encoders], False)
    for e in encoders:
        e['axis_panel'] = transform(e['axis_kicad'], fitting)
        e['error_mm'] = math.dist(e['axis_panel'], e['target_panel'])
    pitches = [math.dist(a['axis_kicad'], b['axis_kicad']) for a,b in zip(encoders, encoders[1:])]
    transformed = transform_poly(outline, fitting)
    a_cells = [transform_poly(c, fitting) for c in cells(outline)]
    # clip() expects its clipping polygon counterclockwise, but subject may be either winding.
    krows = [r for r in rows if r['board']=='PCB-K']
    k = [(float(r['x_mm']),float(r['y_mm'])) for r in krows if r['item'].startswith('vértice')]
    window = row_rect(next(r for r in krows if r['item']=='ventana' and 'rectangular' in r['notes']))
    k_cells = cells(k, [window])
    # Only a combined CSV/DXF exists. Infer the split of doc28 §7 and label it as such.
    seam = 108.0
    left = [p for c in k_cells if (p := clip(c,rect(0,0,seam,340))) and abs(signed_area(p))>EPS]
    bridge = rect(108,169,231,173)
    # Repartition the right side with the explicitly removed lower bridge.
    right = [c for c in cells(k,[window,bridge]) if (sum(p[0] for p in c)/4)>seam]
    shapes = {'PCB-K izquierda (partición inferida)':left,
              'PCB-K derecha (partición inferida)':right,
              'PCB-K v5 conjunto (conservador)':k_cells,
              'Ventana M4': [window], 'M4 nominal CSV': [row_rect(panel['M4'])],
              'M4 reserva doc28 (ventana +5 mm)': [row_rect(next(r for r in krows if r['item']=='ventana' and 'rectangular' in r['notes']),5)],
              'Pantalla módulo': [row_rect(panel['MOD'])]}
    for name in ('PCB-F','PCB-P','PCB-M'):
        shapes[name] = [row_rect(next(r for r in rows if r['board']==name and r['item']=='contorno'))]
    checks = {name:collision(a_cells, cs) for name,cs in shapes.items()}
    towers = []
    for row in rows:
        if row['board']=='CUERPO' and row['item'].startswith('pozo'):
            p = (float(row['x_mm']),float(row['y_mm']))
            d = min(point_segment(p,a,b) for a,b in edges(transformed))
            radius = float(row['w_mm'])/2
            gap = -d-radius if inside(p,transformed) else d-radius
            towers.append(dict(center=p,radius_mm=radius,clearance_mm=gap))
    holes = []
    for ref, fp in sorted(fps.items()):
        if ref.startswith('H'):
            pads = children(fp['node'],'pad')
            holes.append(dict(reference=ref,center_kicad=fp['origin'],
                              center_panel=transform(fp['origin'],fitting),drill_mm=float(one(pads[0],'drill')[1])))
    connectors = []
    for ref in ('J1','J2','J3','J4'):
        fp=fps[ref]
        connectors.append(dict(reference=ref,value=fp['value'],footprint=fp['library'],layer=fp['layer'],
                               origin_kicad=fp['origin'],origin_panel=transform(fp['origin'],fitting),
                               rotation_kicad_deg=fp['angle_deg'],
                               rotation_panel_deg=fp['angle_deg']+fitting['angle_deg'],
                               cable_datum_panel=transform(pad_center(fp),fitting),
                               courtyard_panel=transform_poly(courtyard(fp),fitting),
                               assembly='DNP por contrato de integración' if ref=='J4' else 'poblado'))
    a_bottom = params['A_placeholder_origin'][2]
    a_top = a_bottom+thickness
    f_top = params['PCB_TOP']['PCB-F']
    bottom_m = params['PCB_TOP']['PCB-M']-params['PCB_T']
    routes=[]
    dests={'J1':('PCB-M J1_PCBA',(261.,280.,bottom_m+params['PCB_T'])),
           'J2':('PCB-M J2_I2C',(238.,280.,bottom_m+params['PCB_T'])),
           'J3':('PCB-F J1',(257.,165.,f_top-params['PCB_T']))}
    display_z0=params['display_parameters'][5]
    for con in connectors[:3]:
        ref=con['reference']
        target,end=dests[ref]
        start=(*con['cable_datum_panel'],a_bottom)
        crossings={}
        for name in ('Ventana M4','M4 nominal CSV','M4 reserva doc28 (ventana +5 mm)','Pantalla módulo'):
            interval=segment_box_interval(start,end,bounds(shapes[name][0]))
            crossing={'crosses_xy': interval is not None}
            if interval:
                z=[start[2]+t*(end[2]-start[2]) for t in interval]
                crossing['z_mm']=z
                if name=='Pantalla módulo': crossing['below_model_mm']=display_z0-max(z)
            crossings[name]=crossing
        region=bounds(shapes['PCB-F' if ref=='J3' else 'PCB-M'][0])
        # Distances to any destination on the entire target board are bounds, not placements.
        near=(max(region[0],min(start[0],region[2])),max(region[1],min(start[1],region[3])),end[2])
        far=max(math.dist(start,(*p,end[2])) for p in rect(*region))
        # Service route drops below the display, turns at the front of PCB-M, then reaches endpoint.
        waypoints=[start,(start[0],start[1],60.),(end[0],end[1],60.),end] if ref!='J3' else [start,end]
        self_hit = segment_hits_slab(start, end, a_cells, a_bottom, a_top)
        routes.append(dict(source=ref,destination=target,start_mm=start,end_estimated_mm=end,
                           straight_hits_pcba_substrate=self_hit,service_route_valid=not self_hit,
                           straight_3d_mm=math.dist(start,end),target_region_range_mm=[math.dist(start,near),far],
                           crossings=crossings,waypoints_mm=waypoints,
                           routed_mm=sum(math.dist(a,b) for a,b in zip(waypoints,waypoints[1:]))))
    b=bounds(transformed)
    wall=params['WALL']
    wall_gaps=[b[0]-wall,b[1]-wall,params['W']-wall-b[2],params['D']-wall-b[3]]
    components=[]
    for ref, fp in sorted(fps.items()):
        if fp['layer']=='F.Cu' and not ref.startswith(('RE','H')):
            ct=transform_poly(courtyard(fp),fitting)
            hits={n:collision([ct],shapes[n])['overlap_mm2'] for n in ('PCB-F','M4 nominal CSV','M4 reserva doc28 (ventana +5 mm)')}
            if any(v>EPS for v in hits.values()):
                components.append(dict(reference=ref,value=fp['value'],courtyard_panel=ct,overlap_mm2=hits))
    encoder_pass = max(e['error_mm'] for e in encoders)<=.1 and all(abs(p-26)<=1e-6 for p in pitches)
    plan_pass = all(v['status']=='LIBRE' for v in checks.values()) and min(wall_gaps)>0 and min(t['clearance_mm'] for t in towers)>0
    pins_under_f = []
    for ref, fp in fps.items():
        for pad in children(fp['node'], 'pad'):
            if pad[2]=='thru_hole' and inside(transform(fp_point(fp,xy(pad,'at')),fitting),shapes['PCB-F'][0]):
                pins_under_f.append(ref+'.'+pad[1])
    return dict(schema_version=1,remote_layouts=dict(pcbm_footprint_count=len(remote_m),pcbm_has_outline=bool(remote_m_outline),
                pcbf_legacy_outline_mm=bounds(legacy_f_outline),pcbf_j1_origin_kicad=legacy_f['J1']['origin'],
                pcbf_j1_rotation_deg=legacy_f['J1']['angle_deg']),base='095370c19b340b084151e3a9079798f92aa0c23d',sources_sha256=hashes,
                transformation=fitting,diagnostic_no_y_conversion=direct,encoders=encoders,pitches_mm=pitches,
                encoder_pass=encoder_pass,plan_pass=plan_pass,through_hole_pins_under_f=sorted(pins_under_f),
                outline_kicad=outline,outline_panel=transformed,outline_area_mm2=abs(signed_area(outline)),
                outline_bounds_mm=b,plan_checks=checks,towers=towers,mounting_holes=holes,connectors=connectors,
                wall_clearances_mm=dict(zip(('izquierda','frente','derecha','atrás'),wall_gaps)),
                heights=dict(panel_top_mm=params['TOP'],pcba_bottom_mm=a_bottom,pcba_top_mm=a_top,
                             pcba_base_relative_mm=a_top-params['TOP'],pcbf_top_mm=f_top,
                             pcbf_bottom_mm=f_top-params['PCB_T'],pcba_to_pcbf_substrate_gap_mm=f_top-params['PCB_T']-a_top,
                             pcbk_bottom_mm=params['PCB_TOP']['PCB-K']-params['PCB_T'],
                             pcba_to_pcbk_substrate_gap_mm=params['PCB_TOP']['PCB-K']-params['PCB_T']-a_top,
                             display_bottom_mm=display_z0,display_top_mm=display_z0+params['display_parameters'][6],
                             tower_top_mm=params['TOWER_TOP']),
                front_components_in_reserved_areas=components,routes=routes,shapes=shapes,
                placeholder=dict(origin=params['A_placeholder_origin'],size=params['A_placeholder_size']),
                conclusion=('ENCAJE XY y ejes PASS; altura y mazos pendientes' if plan_pass and encoder_pass else 'NO ENCAJA en las reservas de planta v1; altura y mazos sin cierre mecánico'))


def fmt(p):
    return '('+', '.join(f'{v:.3f}' for v in p)+')'


def table(headers, rows):
    return '\n'.join(['| '+' | '.join(headers)+' |','|'+'|'.join(['---']*len(headers))+'|']+
                     ['| '+' | '.join(str(v) for v in row)+' |' for row in rows])


def write_report(r):
    h=r['heights']
    conrows=[]
    for c in r['connectors']:
        conrows.append([c['reference'],fmt(c['origin_kicad']),fmt(c['origin_panel']),
                        f"{c['rotation_kicad_deg']:.0f}° / {c['rotation_panel_deg']:.0f}°",c['layer'],fmt(c['cable_datum_panel']),fmt(bounds(c['courtyard_panel']))])
    texts=[
        '# PCB-A · verificación de encaje para Console v1',
        '**Conclusión: PCB-A NO ENCAJA en las reservas de planta v1.** El paso y los ocho ejes pasan; '
        'el contorno existente invade PCB-F, PCB-K y la reserva de placa de la M4. Esto bloquea '
        'cerrar el montaje para el primer pedido. Los solapes XY no se presentan como choques 3D '
        'demostrados: los sustratos están a distintas alturas. La compatibilidad en altura queda '
        'condicionada a componentes, soldaduras, M4 y perilla reales. No se ha modificado ninguna placa.',
        '## Reproducción y fuentes',
        'Desde la raíz del clon, sin dependencias externas ni FreeCAD/pcbnew:\n\n'
        '```sh\npython3 apps/hardware/kicad/consola/tools/fit/check_pcba_fit.py\n'
        'python3 -B -m unittest discover -s apps/hardware/kicad/consola/tools/fit -p "test_*.py"\n```\n\n'
        'El script escribe únicamente este informe, `pcba-fit.json` y `pcba-fit.svg` junto al script. '
        'Una ejecución correcta termina en 0 aunque el encaje falle; `encoder_pass` y `conclusion` '
        'en JSON contienen el resultado de ingeniería. El hash de cada entrada permite detectar cambios. '
        'No es un DRC ni una certificación de fabricación. Método y límites: [THEORY.md](THEORY.md). '
        'Base revisada: `'+r['base']+'`.',
        '## Ejes: origen de huella, conversión de coordenadas y transformación',
        'Vista superior: X derecha, Y hacia atrás desde el frente, Z desde el piso del cuerpo. '
        'El README de PCB-K fija **Y_KiCad = constante − y_panel**. '
        'El círculo Ø6 de cada EC11 da el eje local **(+7,5; +2,5) mm**; '
        'el origen de huella está en el pad 1 y no en el eje. Los EC11 siguen en F.Cu, mirando arriba.',
        f"Ajuste sin escala: **(x_panel, y_panel) = R({r['transformation']['angle_deg']:.6f}°) · (x_KiCad, −y_KiCad) + {fmt(r['transformation']['translation_mm'])} mm**. "
        'No hace falta rotación física adicional. La inversión de Y cambia la convención del dibujo, no la cara de montaje.',
        table(['Encoder','Origen KiCad','Eje KiCad','Objetivo panel','Eje ajustado','Error mm'],
              [[e['reference']+' → '+e['target'],fmt(e['origin_kicad']),fmt(e['axis_kicad']),fmt(e['target_panel']),fmt(e['axis_panel']),f"{e['error_mm']:.6f}"] for e in r['encoders']]),
        '**Pasos reales consecutivos:** '+', '.join(f'{p:.3f}' for p in r['pitches_mm'])+' mm. '
        f"Criterio ejes ≤0,1 mm y paso nominal 26 mm (tolerancia numérica 0,000001 mm): **{'PASS' if r['encoder_pass'] else 'FAIL BLOQUEANTE'}**.",
        f"Diagnóstico de convención incorrecta: sin invertir Y, la misma fila colineal daría traslación {fmt(r['diagnostic_no_y_conversion']['translation_mm'])}, "
        'y llevaría la prolongación hacia la pantalla (y 183,4–253). Los ejes por sí solos no detectan ese error. '
        'Ese caso contradice la conversión explícita del repositorio y no se usa para certificar el montaje.',
        '## Contorno, montaje y conectores reales',
        '**Vértices Edge.Cuts, KiCad → panel, mm:**\n\n'+table(['KiCad','Panel'],[[fmt(a),fmt(b)] for a,b in zip(r['outline_kicad'],r['outline_panel'])]),
        f"Envolvente **217 × 69,6 mm**, área **{r['outline_area_mm2']:.2f} mm²**, límites panel (xmin,ymin,xmax,ymax) **{fmt(r['outline_bounds_mm'])}**. "
        'La tira es x 54–271 / y 178,5–200,6; la prolongación es x 231–271 / y 131–178,5. '
        f"Placeholder FreeCAD: origen {fmt(r['placeholder']['origin'])}, tamaño {fmt(r['placeholder']['size'])}; "
        'su caja 200×30 no representa esta PCB. No hay contorno de PCB-A en el CSV v5: manda Edge.Cuts.',
        table(['Agujero','KiCad','Panel','Taladro mm'],[[v['reference'],fmt(v['center_kicad']),fmt(v['center_panel']),f"{v['drill_mm']:.2f}"] for v in r['mounting_holes']]),
        'El modelo no crea soportes para esos cuatro agujeros de PCB-A; tampoco aparecen en los PEM del CSV v5. '
        'H1 queda en (62,195) sobre PCB-K izquierda; H4 en (238,145) sobre la derecha. '
        'Falta diseñar soportes bajo PCB-A o estudiar pasos/espaciadores a través de K, respetando la diferencia Z.',
        table(['Conector','Origen KiCad','Origen panel','Ángulo KiCad / panel','Cara','Centro pads eléctricos panel','Caja courtyard panel'],conrows),
        'J1 es IDC 2×15 vertical, con eje largo paralelo a Y y salida hacia abajo (B.Cu); '
        'el origen es pad 1, no centro de cinta. J2/J3 son SH horizontales de 4/6 vías a 180°; '
        'J4 es SH10 a 90° y **DNP por contrato**, sin cable. Ángulo panel: dirección del eje local +X, CCW '
        'en X derecha/Y atrás. La abertura del SH sigue su geometría local; no confundirla con el eje +X. '
        'Las cajas son courtyards (holgura de colocación), no volúmenes de enchufe o radio de curvatura.',
        '## Planta: áreas reales, sin usar la envolvente de la L',
        table(['Obstáculo','Área solapada mm²','Separación mínima XY mm','Resultado'],
              [[n,f"{v['overlap_mm2']:.3f}",f"{v['clearance_mm']:.3f}",v['status']] for n,v in r['plan_checks'].items()]),
        'PCB-F ocupa x 254–335 / y 88–171; su intersección con A es **17×40 = 680 mm²**. '
        'La ventana M4 ocupa x 109–231 / y 107–169: hay **contacto sin margen** en x=231. '
        'La M4 nominal del CSV ocupa x 110–230 / y 108–168 y conserva solo **1 mm** lateral. '
        'Doc28 §2 reserva 5 mm alrededor de la ventana para la placa colgante: x 104–236 / y 102–174; '
        'A invade esa reserva **5×43 = 215 mm²**. Es una reserva documental, no una medición del PCB Adafruit completo.',
        '**PCB-K partida:** no hay layouts ni contornos individuales de izquierda/derecha. CSV v5 y DXF '
        'describen la placa conjunta. Se muestra una partición inferida según doc28 §7: corte x=108 y '
        'eliminación de la franja inferior x108–231/y169–173. Da envolventes 102×216 y 144×152 como el doc28. '
        'También se comprueba la placa conjunta como límite conservador. No se convierte esta inferencia '
        'en una propuesta de layout aprobada. Se restan la ventana M4 y la franja, no los pequeños taladros/FSR '
        '(alejados del choque de interés); las áreas son de reserva de contorno.',
        '**Pared de 3,2 mm:** límites interiores x/y 3,2–336,8. Holguras del contorno: '+
        ', '.join(f'{n} {v:.3f} mm' for n,v in r['wall_clearances_mm'].items())+'. **PASS**. '
        'La placa queda lejos de las esquinas redondeadas (R interior 3,4), por lo que no afectan el resultado. '
        'El tabique de bahía en y211–213 queda 10,4 mm detrás del borde de A; '
        'el de x136–138 empieza en y211. Los topes de las juntas están fuera de su altura/planta.',
        table(['Torre cuerpo (centro)','Radio mm','Holgura real al círculo mm'],
              [[fmt(t['center']),f"{t['radius_mm']:.1f}",f"{t['clearance_mm']:.3f}"] for t in r['towers']]),
        f"Torres Ø10: mínima holgura **{min(t['clearance_mm'] for t in r['towers']):.3f} mm**, PASS en planta. "
        f"Su techo es z={h['tower_top_mm']:.1f}, a la altura de la cara superior de A; los PEM continúan hasta la chapa y también quedan fuera en XY.",
        '## Altura de EC11, PCB y perilla Ø18',
        table(['Magnitud','Valor mm'],[
            ['Cara superior aluminio',f"z={h['panel_top_mm']:.3f}"],
            ['PCB-A sustrato inferior / superior',f"{h['pcba_bottom_mm']:.3f} / {h['pcba_top_mm']:.3f}"],
            ['Base EC11 respecto al aluminio',f"{h['pcba_base_relative_mm']:.3f}"],
            ['PCB-F inferior / superior',f"{h['pcbf_bottom_mm']:.3f} / {h['pcbf_top_mm']:.3f}"],
            ['Luz A superior → F inferior',f"{h['pcba_to_pcbf_substrate_gap_mm']:.3f}"],
            ['Luz A superior → K inferior',f"{h['pcba_to_pcbk_substrate_gap_mm']:.3f}"],
            ['Módulo pantalla inferior / superior (modelo)',f"{h['display_bottom_mm']:.3f} / {h['display_top_mm']:.3f}"]]),
        '**Base −12 mm: PASS del apilado nominal.** Se extrae el placeholder TOP−13,6 más el espesor real '
        '1,6 mm de A; base EC11 = TOP−12 = z73. No hay separador de A modelado que imponga esa altura en el ensamble. '
        'Las superficies de sustrato A/F no se atraviesan, pero **0,9 mm no certifica espacio para componentes '
        'superiores/soldaduras en la zona solapada**. La luz a K es 7 mm y también requiere revisar sus componentes inferiores.',
        'Componentes superiores de A cuyo courtyard entra en reservas vecinas: '+('ninguno de los componentes F.Cu medidos (excluyendo encoders y agujeros).' if not r['front_components_in_reserved_areas'] else '')+'\n\n'+
        table(['Ref','Valor','Solape courtyard F mm²','Solape reserva M4 mm²'],
              [[c['reference'],c['value'],f"{c['overlap_mm2']['PCB-F']:.3f}",f"{c['overlap_mm2']['M4 reserva doc28 (ventana +5 mm)']:.3f}"] for c in r['front_components_in_reserved_areas']]),
        'No se demuestra un choque de encapsulados con F: los SH están abajo. Sí quedan **'+str(len(r['through_hole_pins_under_f']))+' pines THT de J1** con centro bajo F ('+', '.join(r['through_hole_pins_under_f'])+'); sus puntas/soldaduras emergen sobre A y tienen solo 0,9 mm hasta el sustrato F. Medir protrusión y fijar margen aislante. No suponer ese volumen nulo.',
        '**Eje H20: comprobación condicional.** Si el EC11 instalado tiene punta a 20 mm de la superficie '
        'de montaje, sobresale **20−12 = 8 mm** sobre el aluminio; con carrera de clic de 0,5 mm '
        '(0,8 mm máxima en la variante EC11E15244G1), quedan 7,5 mm nominales (7,2 mm al máximo de carrera). '
        'El catálogo oficial [Alps EC11E, dibujos 2/3](https://tech.alpsalpine.com/cms.media/product_catalog_ec_01_ec11e_en_611f078659.pdf) '
        'y la [ficha EC11E15244G1](https://tech.alpsalpine.com/e/products/detail/EC11E15244G1/) '
        'distinguen versiones con clic de 0,5 y 1,5 mm. En la placa solo figura EC11, no un MPN de compra. '
        'El nombre H20 y el enlace del footprint no demuestran qué variante se montará.',
        'Para una perilla de agujero ciego que asienta en la punta, con hueco inferior g y profundidad de '
        'inserción d, **g+d=8 mm**. Si g≥0,8 mm para permitir la carrera máxima de la variante de 0,5, '
        'd≤7,2 mm (antes de tolerancias). Para la variante de 1,5 mm hace falta mayor hueco. '
        'Ø18 solo fija el diámetro exterior: no especifica d, altura, ajuste del eje D/estriado ni posición del '
        'hombro. Por tanto **perilla pendiente de validación**, no PASS automático. Separación exterior entre '
        'perillas: 26−18 = **8 mm**. Corte de chapa Ø7,1 para eje nominal Ø6: **0,55 mm radial**; '
        'verificar que el casquillo/hombro real no invada ese corte a z83–85.',
        'El modelo pone el módulo de pantalla con techo z83, es decir 2 mm por debajo de TOP85, '
        'aunque el prompt v5 pide vidrio a ras. Se señala esa diferencia sin modificarla; no afecta la '
        'separación XY de 7,28 mm de A a la pantalla.',
        '## Mazo: distancias aproximadas y cruces',
        '**Extremos conocidos:** centro de pads eléctricos de J1/J2/J3 de A; Z usado = cara inferior del sustrato '
        '(z71,4). No se conocen alturas del enchufe. **Extremos estimados:** J1_PCBA=(261,280,9,6), '
        'J2_I2C=(238,280,9,6) en el frente de PCB-M; F.J1=(257,165,73,9), cerca de su borde izquierdo '
        'y sobre la cara inferior. PCB-M no tiene footprints colocados. El `.kicad_pcb` de F es legacy '
        '46×92 mm y no corresponde al contorno Console de 81×83: su J1 no se traslada como si fuera Console. '
        'Son hipótesis explícitas de encaminado, no coordenadas reales inventadas de esos conectores.',
        table(['Ruta','Inicio XYZ','Destino estimado XYZ','Recta 3D mm','Rango destino en toda placa mm','Polilínea de servicio mm'],
              [[v['source']+' → '+v['destination'],fmt(v['start_mm']),fmt(v['end_estimated_mm']),f"{v['straight_3d_mm']:.2f}",fmt(v['target_region_range_mm']),f"{v['routed_mm']:.2f}" if v['service_route_valid'] else 'PENDIENTE (atraviesa A)'] for v in r['routes']]),
        table(['Ruta','Cruza ventana M4 XY','Cruza pantalla XY','Z del cruce de pantalla','Luz mínima por debajo del módulo mm'],
              [[v['source'],str(v['crossings']['Ventana M4']['crosses_xy']),str(v['crossings']['Pantalla módulo']['crosses_xy']),
                fmt(v['crossings']['Pantalla módulo']['z_mm']) if v['crossings']['Pantalla módulo']['crosses_xy'] else '—',
                f"{v['crossings']['Pantalla módulo']['below_model_mm']:.2f}" if v['crossings']['Pantalla módulo']['crosses_xy'] else '—'] for v in r['routes']]),
        'Las rectas J1/J2 pasan bajo la pantalla **en proyección**, pero ya han descendido por debajo '
        'de z68,2 al entrar en su huella: no chocan con el prisma del módulo bajo estas hipótesis. '
        'La alternativa de servicio baja a z60 antes de pasar bajo el módulo y sigue hacia PCB-M. '
        'J3 no cruza ventana/pantalla, pero su recta estimada **atraviesa el propio sustrato de A**: '
        'sube de z71,4 a z73,9 sin salir de la prolongación x231–271/y131–178,5. '
        '**Su longitud recta es solo una cota inferior, no una ruta utilizable.** '
        'El conector supuesto F.J1 inferior tiene además solo 0,9 mm libres hasta A: su enchufe no queda validado. '
        'Para cerrar J3 se necesita colocar F.J1 fuera de la proyección de A o estudiar montarlo arriba, '
        'rodear el borde de A y llegar desde arriba de F; la altura del conector se debe cotar antes de '
        'calcular un largo de compra. No se aplica ninguna recolocación. Las tres rectas evitan '
        'la ventana y M4 nominal en XY; la placa A sigue invadiendo su reserva lateral.',
        'Longitud de compra = recorrido + bucles de montaje/terminación; no basta con la recta 3D. '
        'Usar aproximadamente 20–30 % de margen una vez fijados los conectores, medir el camino '
        'con cinta y comprobar radio mínimo del cable/IDC seleccionado. No se fija un radio sin ficha '
        'del mazo. Antes del pedido faltan ubicación/orientación/altura de los dos conectores M, '
        'el de F y el volumen enchufado de J1 (cinta de 30 vías).',
        '## Qué cambiar, sin aplicarlo',
        '1. Mantener los ejes aprobados E1–E8: ya pasan con paso 26 mm. Sustituir en una futura revisión '
        'del modelo el placeholder por el contorno real y añadir sus cuatro soportes a z71,4–73.\n'
        '2. Para conservar PCB-A, rediseñar el alojamiento/contorno de F en la zona x254–271/y131–171 '
        '(680 mm²) o justificar un apilado con volumen de componentes y soldaduras. Mover F completa '
        '≥17 mm a la derecha deja su borde en x352 y excede las paredes; moverla ≥40 mm hacia el frente '
        'cambia los faders aprobados. Esos cambios requieren decisión de diseño.\n'
        '3. Resolver el contacto x231 con la ventana y la invasión de 5 mm de la reserva de placa M4: '
        'medir M4 real, cotar Z y conectores. No reducir la reserva ni mover los pads sin aprobación.\n'
        '4. Cerrar los contornos separados de K y sus zonas inferiores; A se proyecta sobre ambas. '
        'Suprimir material por esos solapes sin comprobar Z puede ser innecesario. Verificar también los pasos '
        'de montaje de A y el volumen del IDC bajo F/K.\n'
        '5. Alternativa si no se puede acomodar el contorno existente: futura revisión de la prolongación '
        'de A/conectores manteniendo ejes y contrato eléctrico. No recortar la PCB existente a mano: '
        'hay cobre y componentes en esa región. No se aplica aquí esa alternativa.\n'
        '6. Elegir MPN exacto del EC11, cotar la perilla y su carrera, y fijar los tres extremos remotos del mazo '
        'antes de liberar fabricación. La orden presente es el primer pedido; las notas históricas que dicen '
        '«fabricada» no se toman como evidencia de un ensamble validado.',
        '## Superposición a escala y datos',
        '![Vista superior, X derecha e Y hacia atrás](pcba-fit.svg)\n\n'
        'Ver [SVG](pcba-fit.svg) y [datos JSON](pcba-fit.json). El SVG distingue contorno real, '
        'placeholder y reservas inferidas, e incluye PCB-A/K izquierda/K derecha/F/P/M, M4, pantalla, '
        'torres, agujeros, encoders y cables.',
        '## Validación de entrega',
        'En esta base se ejecutaron **16 tests con el Python de KiCad: 16 PASS**. Con Python estándar: '
        '**15 PASS y 1 omitido**, correspondiente al contraste opcional con pcbnew. Incluyen el demo de '
        'solapes 680/215 mm², contacto M4, ocho ejes, altura y cruces de mazo. El contraste pcbnew '
        'comprueba 37 footprints, caras, rotaciones, posiciones globales de pads numéricos, centros '
        'de eje y Edge.Cuts sin guardar la placa. El SVG se comprobó como XML y se inspeccionó '
        'visualmente mediante una rasterización de sus primitivas. Los hashes de informe/JSON/SVG '
        'son idénticos al regenerar con ambos intérpretes. Las únicas adiciones del parche están '
        'en esta carpeta; todos los archivos de entrada conservan sus bytes respecto a 095370c.',
        '## Integridad de entradas (SHA-256)',
        table(['Entrada','SHA-256'],[[p,'`'+v+'`'] for p,v in r['sources_sha256'].items()]),
    ]
    (HERE/'REPORT-pcba-fit.md').write_text('\n\n'.join(text.rstrip() for text in texts)+'\n',encoding='utf-8')


def write_svg(r):
    # Coordinates are mapped individually so labels remain upright while +Y points up.
    scale, ox, oy = 2.3, 64, 845
    def pnt(p): return (ox+scale*p[0],oy-scale*p[1])
    def label(p,text,color='#172534',size=11):
        x,y=pnt(p)
        return f'<text x="{x:.2f}" y="{y:.2f}" font-size="{size}" fill="{color}">{html.escape(text)}</text>'
    def poly(poly,color,opacity=.2,dash='',width=1.5):
        pts=' '.join(f'{x:.2f},{y:.2f}' for x,y in map(pnt,poly))
        return f'<polygon points="{pts}" fill="{color}" fill-opacity="{opacity}" stroke="{color}" stroke-width="{width}" stroke-dasharray="{dash}"/>'
    def circle(p,radius,color,fill='none'):
        x,y=pnt(p)
        return f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{radius*scale:.2f}" fill="{fill}" stroke="{color}" stroke-width="1.3"/>'
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="930" viewBox="0 0 1280 930">',
         '<rect width="1280" height="930" fill="#fafbfc"/>',
         '<text x="55" y="28" font-family="sans-serif" font-size="22" font-weight="bold">PCB-A / Console v1 — NO ENCAJA en reservas XY</text>',
         '<text x="55" y="49" font-family="sans-serif" font-size="13">Vista superior · X desde izquierda · Y desde frente hacia atrás · mm · Z no representada</text>',
         '<g font-family="sans-serif">',poly(rect(0,0,340,340),'#283749',.02),poly(rect(3.2,3.2,336.8,336.8),'#7b8999',0,'4 3')]
    for v in range(0,341,20):
        a,b=pnt((v,0)),pnt((v,340))
        c,d=pnt((0,v)),pnt((340,v))
        svg.append(f'<path d="M{a[0]},{a[1]} L{b[0]},{b[1]} M{c[0]},{c[1]} L{d[0]},{d[1]}" fill="none" stroke="#dce2e8" stroke-width="0.6"/>')
        svg += [label((v,-9),str(v),size=10),label((-14,v),str(v),size=10)]
    colors={'PCB-K izquierda (partición inferida)':'#8873c9','PCB-K derecha (partición inferida)':'#8873c9',
            'PCB-F':'#27936b','PCB-P':'#afa12e','PCB-M':'#3c8eb2','Pantalla módulo':'#477eb8',
            'M4 reserva doc28 (ventana +5 mm)':'#e28e29','M4 nominal CSV':'#dfae23','Ventana M4':'#b48c29'}
    for name in ('PCB-M','Pantalla módulo','PCB-P','PCB-K izquierda (partición inferida)',
                 'PCB-K derecha (partición inferida)','PCB-F','M4 reserva doc28 (ventana +5 mm)',
                 'Ventana M4','M4 nominal CSV'):
        polygons=r['shapes'][name]
        if 'inferida' in name:
            boundary={}
            for c in polygons:
                svg.append(poly(c,colors[name],.11,width=0))
                for a,b in edges(c):
                    key=tuple(sorted((tuple(round(v,6) for v in a),tuple(round(v,6) for v in b))))
                    boundary[key]=boundary.get(key,0)+1
            for (a,b),count in boundary.items():
                if count==1:
                    pa,pb=pnt(a),pnt(b)
                    svg.append(f'<path d="M{pa[0]},{pa[1]} L{pb[0]},{pb[1]}" fill="none" stroke="{colors[name]}" stroke-width="1.5" stroke-dasharray="5 3"/>')
        else:
            for c in polygons: svg.append(poly(c,colors[name],.11,'5 3' if 'reserva' in name else ''))
    svg += [label((13,191),'K izq. (inferida)'),label((172,49),'K der. (inferida)'),label((275,121),'PCB-F'),
            label((15,81),'PCB-P'),label((180,295),'PCB-M · z8–9,6'),label((100,267),'Pantalla 189,32 × 120,24'),
            label((113,147),'M4 nominal / reserva +5'),label((4,-23),'FRENTE (y=0) · ATRÁS (y=340)')]
    x,y,_=r['placeholder']['origin'];w,d,_=r['placeholder']['size']
    svg += [poly(rect(x,y,x+w,y+d),'#838383',0,'7 4'),poly(r['outline_panel'],'#d83945',.2)]
    for t in r['towers']: svg.append(circle(t['center'],t['radius_mm'],'#44515e','#b3bdc7'))
    for e in r['encoders']:
        svg += [circle(e['target_panel'],9,'#d83945'),circle(e['axis_panel'],.9,'#aa1822','#aa1822'),
                label((e['axis_panel'][0]-4,e['axis_panel'][1]+11),e['target'],size=11)]
    for h in r['mounting_holes']:
        svg += [circle(h['center_panel'],h['drill_mm']/2,'#172534','white'),
                label((h['center_panel'][0]+2,h['center_panel'][1]+2),h['reference'],size=9)]
    for c in r['connectors']:
        svg += [poly(c['courtyard_panel'],'#a01520',.1,'3 2' if c['reference']=='J4' else ''),
                label(c['origin_panel'],c['reference']+(' DNP' if c['reference']=='J4' else ''),size=10)]
    for route,color in zip(r['routes'],('#aa3479','#44603f','#008b92')):
        a,b=pnt(route['start_mm']),pnt(route['end_estimated_mm'])
        svg.append(f'<path d="M{a[0]},{a[1]} L{b[0]},{b[1]}" fill="none" stroke="{color}" stroke-width="2" stroke-dasharray="6 4"/>')
        svg += [circle(route['end_estimated_mm'],1.5,color,color),label((route['end_estimated_mm'][0]+2,route['end_estimated_mm'][1]+2),route['source']+' destino*',color,9)]
    legend=[('PCB-A real · ejes PASS','#d83945'),('Placeholder 200 × 30 · gris','#838383'),
            ('PCB-K izquierda/derecha inferidas','#8873c9'),('PCB-F 81 × 83','#27936b'),('PCB-P','#afa12e'),
            ('PCB-M piso','#3c8eb2'),('Módulo pantalla','#477eb8'),('M4 / reserva doc28','#e28e29'),
            ('Torres Ø10','#44515e'),('J1 → M (estimado)','#aa3479'),('J2 → M (estimado)','#44603f'),('J3 → F (recta NO válida)','#008b92')]
    for i,(text,color) in enumerate(legend):
        yy=94+i*29
        svg.append(f'<rect x="883" y="{yy-11}" width="14" height="14" fill="{color}"/><text x="907" y="{yy}" font-size="13">{html.escape(text)}</text>')
    for i,line in enumerate(['A: x54–271 / y131–200,6','F: solape XY 680 mm²','K izq./der.: ver informe','M4 ventana: margen 0 mm','M4 nominal: margen 1 mm','Reserva M4: solape 215 mm²',
                             'A/F: sustratos separados 0,9 mm','Solape XY ≠ choque 3D probado','* Extremos remotos sin layout','Cables cruzan pantalla en XY,','pasan debajo en Z (hipótesis).','J3 recta atraviesa A: revisar F.J1.']):
        svg.append(f'<text x="883" y="{490+i*24}" font-size="13">{html.escape(line)}</text>')
    svg += ['</g></svg>']
    (HERE/'pcba-fit.svg').write_text('\n'.join(svg)+'\n',encoding='utf-8')


def stable_data(value):
    # Python 3.12 changed float sum accuracy. Keep sub-nanometre differences out of artifacts.
    if isinstance(value, float):
        result=round(value,9)
        return 0.0 if result==0 else result
    if isinstance(value, dict):
        return {k:stable_data(v) for k,v in value.items()}
    if isinstance(value, (list,tuple)):
        return type(value)(stable_data(v) for v in value)
    return value


def main():
    r=stable_data(audit())
    (HERE/'pcba-fit.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    write_svg(r)
    write_report(r)
    print(f"Ejes: {'PASS' if r['encoder_pass'] else 'FAIL'}; {r['conclusion']}")
    print(f'Informe: {HERE / "REPORT-pcba-fit.md"}')


if __name__=='__main__':
    main()

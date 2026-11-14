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
            elif node[0] == 'gr_arc':
                a, m, b = (xy(node, tag) for tag in ('start', 'mid', 'end'))
                ax,ay=a; mx,my=m; bx,by=b
                den=2*(ax*(my-by)+mx*(by-ay)+bx*(ay-my))
                if abs(den)<EPS: raise ValueError('Degenerate arc')
                aa=ax*ax+ay*ay; mm=mx*mx+my*my; bb=bx*bx+by*by
                cx=(aa*(my-by)+mm*(by-ay)+bb*(ay-my))/den
                cy=(aa*(bx-mx)+mm*(ax-bx)+bb*(mx-ax))/den
                angles=[math.atan2(y-cy,x-cx) for x,y in (a,m,b)]
                sweep=(angles[2]-angles[0])%(2*math.pi)
                if (angles[1]-angles[0])%(2*math.pi)>sweep: sweep-=2*math.pi
                radius=math.dist(a,(cx,cy))
                count=max(2,math.ceil(abs(sweep)/math.radians(5)))
                points=[a]+[(cx+radius*math.cos(angles[0]+sweep*i/count),
                              cy+radius*math.sin(angles[0]+sweep*i/count)) for i in range(1,count)]+[b]
                segments += list(zip(points,points[1:]))
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
    # Conservative outer envelope for rounded strip: no false clearance at sampled corners.
    a_cells = [transform_poly(rect(*bounds(outline)), fitting)]
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
        waypoints=([start,(start[0],start[1],60.),(end[0],end[1],60.),end] if ref!='J3' else
                   [start,(276.,start[1],60.),(276.,175.,60.),(257.,165.,60.),end])
        service_hits={
            'PCB-A':any(segment_hits_slab(a,b,a_cells,a_bottom,a_top) for a,b in zip(waypoints,waypoints[1:])),
            'Pantalla módulo':any(segment_hits_slab(a,b,shapes['Pantalla módulo'],display_z0,display_z0+params['display_parameters'][6]) for a,b in zip(waypoints,waypoints[1:])),
            'M4 reserva':any(segment_hits_slab(a,b,shapes['M4 reserva doc28 (ventana +5 mm)'],0.,params['TOP']) for a,b in zip(waypoints,waypoints[1:])),
            'PCB-F':any(segment_hits_slab(a,b,shapes['PCB-F'],f_top-params['PCB_T'],f_top) for a,b in zip(waypoints,waypoints[1:]))}
        self_hit = segment_hits_slab(start, end, a_cells, a_bottom, a_top)
        routes.append(dict(source=ref,destination=target,start_mm=start,end_estimated_mm=end,
                           straight_hits_pcba_substrate=self_hit,service_route_valid=not any(service_hits.values()),service_hits=service_hits,
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
    plan_pass = all(v['status']=='LIBRE' for n,v in checks.items() if not n.startswith('PCB-K')) and checks['Pantalla módulo']['clearance_mm']>=3 and min(wall_gaps)>0 and min(t['clearance_mm'] for t in towers)>0
    cable_pass=all(route['service_route_valid'] for route in routes)
    pins_under_f = []
    for ref, fp in fps.items():
        for pad in children(fp['node'], 'pad'):
            if pad[2]=='thru_hole' and inside(transform(fp_point(fp,xy(pad,'at')),fitting),shapes['PCB-F'][0]):
                pins_under_f.append(ref+'.'+pad[1])
    return dict(schema_version=1,remote_layouts=dict(pcbm_footprint_count=len(remote_m),pcbm_has_outline=bool(remote_m_outline),
                pcbf_legacy_outline_mm=bounds(legacy_f_outline),pcbf_j1_origin_kicad=legacy_f['J1']['origin'],
                pcbf_j1_rotation_deg=legacy_f['J1']['angle_deg']),base='edc67a3',sources_sha256=hashes,
                transformation=fitting,diagnostic_no_y_conversion=direct,encoders=encoders,pitches_mm=pitches,
                encoder_pass=encoder_pass,plan_pass=plan_pass,cable_pass=cable_pass,through_hole_pins_under_f=sorted(pins_under_f),
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
                conclusion=('PASS: tira, ejes y rutas geométricas; montaje físico pendiente' if plan_pass and encoder_pass and cable_pass else 'FAIL: revisar reservas, ejes o rutas'))


def fmt(p):
    return '('+', '.join(f'{v:.3f}' for v in p)+')'


def table(headers, rows):
    return '\n'.join(['| '+' | '.join(headers)+' |','|'+'|'.join(['---']*len(headers))+'|']+
                     ['| '+' | '.join(str(v) for v in row)+' |' for row in rows])


def write_report(r):
    texts=[
        '# PCB-A · fit de la tira Console v1',
        '**'+r['conclusion']+'**',
        'Reproducir: `python3 -B apps/hardware/kicad/consola/tools/fit/check_pcba_fit.py`. '
        'La ejecución devuelve 1 si falla un criterio. Base: edc67a3. '
        'Método previo: [THEORY.md](THEORY.md); decisión: [THEORY-strip.md](../../boards/pcb-a/THEORY-strip.md).',
        '## Ejes y contorno',
        'Conversión fija esperada: (x+44, 371−y), mm. El ajuste sin escala entrega '+fmt(r['transformation']['translation_mm'])+'. '
        'EC11 en F.Cu: eje local (+7,5; +2,5), distinto del origen. Esquinas R1 muestreadas cada ≤5°. '
        'Las colisiones usan la envolvente rectangular exterior conservadora, incluidos los pequeños recortes R1.',
        table(['Encoder','Eje KiCad','Objetivo panel','Eje panel','Error mm'],
              [[e['reference'],fmt(e['axis_kicad']),fmt(e['target_panel']),fmt(e['axis_panel']),f"{e['error_mm']:.6f}"] for e in r['encoders']]),
        'Paso: '+', '.join(f'{v:.3f}' for v in r['pitches_mm'])+' mm. '
        'Límites panel (xmin,ymin,xmax,ymax): **'+fmt(r['outline_bounds_mm'])+'**. '
        f"Área muestreada: {r['outline_area_mm2']:.3f} mm². Espesor 1,6 mm.",
        table(['Reserva','Solape mm²','Separación mm','Estado'],
              [[n,v['overlap_mm2'],v['clearance_mm'],v['status']] for n,v in r['plan_checks'].items()]),
        'PCB-K se conserva como diagnóstico del CSV v5 combinado y partición inferida: no es el layout nuevo de K. '
        'Su proyección XY no integra el criterio de aprobación de esta tarea. Hay 15 mm nominales entre sustratos A/K; '
        'la interferencia de componentes y el paso de separadores requieren el modelo final. '
        'Pantalla debe tener ≥3 mm; F, M4 nominal, ventana y reserva M4 deben quedar LIBRES. '
        'No hay pines THT de A bajo F: '+str(r['through_hole_pins_under_f'])+'.',
        '## Montaje para FreeCAD',
        table(['Agujero','KiCad mm','Panel mm','Taladro mm'],
              [[h['reference'],fmt(h['center_kicad']),fmt(h['center_panel']),h['drill_mm']] for h in r['mounting_holes']]),
        'Cara inferior/superior A: z=63,4/65,0; chapa z=83…85. Separador nominal desde cara inferior de chapa '
        'a cara superior PCB: 18 mm. Agujeros NPTH Ø3,2 para M3; no son un bond de GND. '
        'Paso de separadores por K y puente de aluminio: [chequeo de montaje](REPORT-pcba-mounting.md). '
        'Placeholder antiguo: '+str(r['placeholder'])+'. No representa el nuevo contorno.',
        '## Conectores y ruta del mazo',
        table(['Ref','Origen KiCad','Origen panel','Ángulo KiCad','Cara','Centro pads panel'],
              [[c['reference'],fmt(c['origin_kicad']),fmt(c['origin_panel']),c['rotation_kicad_deg'],c['layer'],fmt(c['cable_datum_panel'])] for c in r['connectors']]),
        'J1 largo paralelo a X, enchufe hacia abajo; cinta hacia PCB-M después de bajar a z60. '
        'J2 abre hacia atrás (+Y panel), baja a z60 antes de pasar por debajo de pantalla. '
        'J3 abre a la derecha (+X panel), rodea el borde derecho de A por x276 y va al J1 supuesto de F. '
        'J4/R1/R2 DNP. Todos los cables 1:1; conservar pin 1 y polarización.',
        table(['Ruta','Polilínea XYZ mm','Largo mm','Interferencias','Viable geométricamente'],
              [[v['source']+' → '+v['destination'],' → '.join(fmt(p) for p in v['waypoints_mm']),f"{v['routed_mm']:.2f}",str(v['service_hits']),v['service_route_valid']] for v in r['routes']]),
        'Los extremos remotos se conservan como hipótesis explícitas del informe original: M=(261/238,280,9,6), '
        'F=(257,165,73,9). Se comprueban todas las secciones contra prismas A, F, pantalla y reserva M4. '
        'El comienzo en el plano inferior de A representa una salida hacia abajo, no el volumen del enchufe. '
        'Las polilíneas son corredores para dar forma al cable; las esquinas deben redondearse con el radio '
        'del mazo elegido. No son largos de compra: añadir bucles y validar conectores remotos, volumen enchufado '
        'y alturas en el montaje. Encoders/perillas y bond ESD a chapa quedan por verificar físicamente.',
        '## Vista y trazabilidad',
        '![Fit](pcba-fit.svg)',
        table(['Entrada','SHA-256'],[[p,'`'+v+'`'] for p,v in r['sources_sha256'].items()])]
    (HERE/'REPORT-pcba-fit.md').write_text('\n\n'.join(texts)+'\n',encoding='utf-8')


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
         '<text x="55" y="28" font-family="sans-serif" font-size="22" font-weight="bold">PCB-A / Console v1 — tira recta · ver resultados JSON</text>',
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
        pts=' '.join(f'{x:.2f},{y:.2f}' for x,y in map(pnt,route['waypoints_mm']))
        svg.append(f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="2" stroke-dasharray="6 4"/>')
        svg += [circle(route['end_estimated_mm'],1.5,color,color),label((route['end_estimated_mm'][0]+2,route['end_estimated_mm'][1]+2),route['source']+' destino*',color,9)]
    legend=[('PCB-A real · ejes PASS','#d83945'),('Placeholder 200 × 30 · gris','#838383'),
            ('PCB-K izquierda/derecha inferidas','#8873c9'),('PCB-F 81 × 83','#27936b'),('PCB-P','#afa12e'),
            ('PCB-M piso','#3c8eb2'),('Módulo pantalla','#477eb8'),('M4 / reserva doc28','#e28e29'),
            ('Torres Ø10','#44515e'),('J1 → M (estimado)','#aa3479'),('J2 → M (estimado)','#44603f'),('J3 → F (corredor)','#008b92')]
    for i,(text,color) in enumerate(legend):
        yy=94+i*29
        svg.append(f'<rect x="883" y="{yy-11}" width="14" height="14" fill="{color}"/><text x="907" y="{yy}" font-size="13">{html.escape(text)}</text>')
    for i,line in enumerate([r['conclusion'][:48], 'Contorno panel: '+fmt(r['outline_bounds_mm']), 'F / M4: ver separaciones en informe', 'Pantalla: margen mínimo 3 mm', 'K: diagnóstico de reserva histórica', '* Extremos remotos estimados', 'Cables pasan bajo pantalla a z60', 'J3 rodea borde derecho por x276']):
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
    return 0 if r['encoder_pass'] and r['plan_pass'] and r['cable_pass'] else 1


if __name__=='__main__':
    raise SystemExit(main())

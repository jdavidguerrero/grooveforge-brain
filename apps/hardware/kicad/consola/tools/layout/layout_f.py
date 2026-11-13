#!/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
"""Rebuild PCB-F only, then independently verify, DRC and export fabrication data.

Run with KiCad's bundled Python. See pcb-f/docs/THEORY-layout.md before changing
routing or geometry. All outputs are confined to pcb-f, apart from REPORT-f.md.
"""
from pathlib import Path
import argparse
import csv
import heapq
import json
import math
import re
import subprocess
import uuid
import xml.etree.ElementTree as ET
import numpy as np
import pcbnew as p

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'boards/pcb-f'
CLI = Path('/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli')
LIB = Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints')
PCB = OUT / 'pcb-f.kicad_pcb'
STEP = 0.2
SLOTS = [(x - 2.5, 97.0, x + 2.5, 162.0) for x in (264.5, 284.5, 304.5, 324.5)]
X0, Y0, X1, Y1 = 254.0, 88.0, 335.0, 171.0
LAYERS = (p.F_Cu, p.B_Cu)


def run(*args):
    result = subprocess.run([str(CLI), *map(str, args)], check=False, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout.strip(), flush=True)
    result.check_returncode()
    return result.stdout


def vec(x, y):
    return p.VECTOR2I(p.FromMM(x - 44), p.FromMM(371 - y))


def panel(v):
    return (p.ToMM(v.x) + 44, 371 - p.ToMM(v.y))


def rect_panel(box):
    a = panel(box.GetOrigin())
    z = panel(box.GetEnd())
    return min(a[0], z[0]), min(a[1], z[1]), max(a[0], z[0]), max(a[1], z[1])


def rows(file):
    with open(file) as f:
        return list(csv.DictReader(f))


def segment(board, a, b, net, layer, width):
    if math.dist(a, b) < 1e-6:
        return
    track = p.PCB_TRACK(board)
    track.SetStart(vec(*a)); track.SetEnd(vec(*b))
    track.SetWidth(p.FromMM(width)); track.SetLayer(layer); track.SetNet(net)
    board.Add(track)


def via(board, point, net):
    v = p.PCB_VIA(board)
    v.SetPosition(vec(*point)); v.SetWidth(p.FromMM(0.6)); v.SetDrill(p.FromMM(0.3))
    v.SetViaType(p.VIATYPE_THROUGH); v.SetLayerPair(*LAYERS); v.SetNet(net)
    board.Add(v)
    return v


def polygon(board, rect, layers, net=None, keepout=False):
    z = p.ZONE(board)
    ls = p.LSET()
    for layer in layers: ls.AddLayer(layer)
    z.SetLayerSet(ls)
    if keepout:
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True)
        z.SetDoNotAllowZoneFills(True)
        # The body and its NPTH locating pins intentionally span this area.
        z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    else:
        z.SetNet(net); z.SetLocalClearance(p.FromMM(0.2))
        z.SetPadConnection(p.ZONE_CONNECTION_FULL)
        z.SetMinThickness(p.FromMM(0.2))
    poly = z.Outline(); poly.NewOutline()
    a,b,c,d = rect
    for point in ((a,b),(c,b),(c,d),(a,d)):
        v = vec(*point); poly.Append(v.x, v.y)
    board.Add(z)


class Router:
    """Small deterministic raster router; KiCad DRC remains the final authority."""
    def __init__(self, board):
        self.board = board
        self.nx = round((X1-X0)/STEP)+1
        self.ny = round((Y1-Y0)/STEP)+1
        self.xx, self.yy = np.meshgrid(X0+np.arange(self.nx)*STEP,
                                       Y0+np.arange(self.ny)*STEP)

    def idx(self, point):
        return round((point[0]-X0)/STEP), round((point[1]-Y0)/STEP)

    def point(self, x, y):
        return X0+x*STEP, Y0+y*STEP

    def masks(self, net, radius):
        margin = radius + 0.23
        xx, yy = self.xx, self.yy
        base = ((xx < X0+radius+0.3) | (xx > X1-radius-0.3) |
                (yy < Y0+radius+0.3) | (yy > Y1-radius-0.3))
        for a,b,c,d in SLOTS:
            base |= (xx >= a-radius-0.03) & (xx <= c+radius+0.03) & (yy >= b-radius-0.03) & (yy <= d+radius+0.03)
        masks = [base.copy(), base.copy()]
        for fp in self.board.GetFootprints():
            for pad in fp.Pads():
                if pad.GetNetCode() == net.GetNetCode() and pad.GetNumber():
                    continue
                a,b,c,d = rect_panel(pad.GetBoundingBox())
                obstacle = (xx > a-margin) & (xx < c+margin) & (yy > b-margin) & (yy < d+margin)
                for layer, mask in zip(LAYERS, masks):
                    if pad.IsOnLayer(layer) or pad.GetAttribute() == p.PAD_ATTRIB_NPTH:
                        mask |= obstacle
        for t in self.board.GetTracks():
            if t.GetNetCode() == net.GetNetCode():
                continue
            a = panel(t.GetStart()); b = panel(t.GetEnd())
            dx,dy = b[0]-a[0], b[1]-a[1]
            den = dx*dx+dy*dy
            u = np.clip(((xx-a[0])*dx+(yy-a[1])*dy)/den, 0, 1) if den else 0
            obs = (xx-a[0]-u*dx)**2 + (yy-a[1]-u*dy)**2 < (p.ToMM(t.GetWidth(p.F_Cu) if isinstance(t,p.PCB_VIA) else t.GetWidth())/2+margin)**2
            for layer, mask in zip(LAYERS, masks):
                if isinstance(t, p.PCB_VIA) or t.GetLayer() == layer:
                    mask |= obs
        return masks

    def route(self, start, end, net, width, start_layer=0, end_layer=0):
        masks = self.masks(net, width/2)
        vm = self.masks(net, 0.3)
        sx,sy = self.idx(start); ex,ey = self.idx(end)
        first=(sx,sy,start_layer); goal=(ex,ey,end_layer)
        assert not masks[start_layer][sy,sx], ('start blocked', start, net.GetNetname())
        assert not masks[end_layer][ey,ex], ('end blocked', end, net.GetNetname())
        def h(x,y,l):
            dx,dy=abs(x-ex),abs(y-ey)
            return max(dx,dy)+(math.sqrt(2)-1)*min(dx,dy)+(0 if l==end_layer else 12)
        queue=[(h(*first),0,first)]; costs={first:0}; prev={}
        moves=((1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1))
        while queue:
            _,g,node=heapq.heappop(queue)
            if g != costs[node]: continue
            if node==goal: break
            x,y,l=node
            candidates=[]
            for dx,dy in moves:
                nx,ny=x+dx,y+dy
                if not (0<=nx<self.nx and 0<=ny<self.ny) or masks[l][ny,nx]: continue
                if dx and dy and (masks[l][y,nx] or masks[l][ny,x]): continue
                candidates.append(((nx,ny,l),math.hypot(dx,dy)))
            if not vm[0][y,x] and not vm[1][y,x]:
                candidates.append(((x,y,1-l),35))
            for nxt,cost in candidates:
                ng=g+cost
                if ng < costs.get(nxt,math.inf):
                    costs[nxt]=ng; prev[nxt]=node
                    heapq.heappush(queue,(ng+h(*nxt),ng,nxt))
        else:
            raise RuntimeError(f'No route {net.GetNetname()}: {start} -> {end}')
        path=[goal]
        while path[-1]!=first: path.append(prev[path[-1]])
        path.reverse()
        segment(self.board,start,self.point(sx,sy),net,LAYERS[start_layer],width)
        anchor=path[0]; direction=None
        for i in range(1,len(path)):
            old,new=path[i-1],path[i]
            if old[2]!=new[2]:
                segment(self.board,self.point(*anchor[:2]),self.point(*old[:2]),net,LAYERS[old[2]],width)
                via(self.board,self.point(*new[:2]),net)
                anchor=new; direction=None
            else:
                d=(new[0]-old[0],new[1]-old[1])
                if direction is not None and direction!=d:
                    segment(self.board,self.point(*anchor[:2]),self.point(*old[:2]),net,LAYERS[old[2]],width)
                    anchor=old
                direction=d
        segment(self.board,self.point(*anchor[:2]),self.point(ex,ey),net,LAYERS[end_layer],width)
        segment(self.board,self.point(ex,ey),end,net,LAYERS[end_layer],width)
        print('routed',net.GetNetname(),start,end,flush=True)


def prepare_library():
    # The board-local override changes only the ESD connection style, never pad geometry.
    source=ROOT/'boards/lib/groovepush.pretty/ALPS_RS6011P_60mm.kicad_mod'
    local=OUT/'groovepush.pretty'; local.mkdir(exist_ok=True)
    lines=source.read_text().splitlines()
    content='\n'.join(line[:-1]+' (zone_connect 2))' if '(pad "MP"' in line else line for line in lines)+'\n'
    (local/source.name).write_text(content)
    table='(fp_lib_table\n  (version 7)\n'
    table+='  (lib (name "groovepush")(type "KiCad")(uri "${KIPRJMOD}/groovepush.pretty")(options "")(descr "RS6011: upstream geometry, solid ESD mounting pads"))\n'
    for name in ('Capacitor_SMD','Connector_JST','MountingHole'):
        table+=f'  (lib (name "{name}")(type "KiCad")(uri "{LIB}/{name}.pretty")(options "")(descr "KiCad 10"))\n'
    (OUT/'fp-lib-table').write_text(table+')\n')


def build():
    prepare_library()
    docs=OUT/'docs'; docs.mkdir(exist_ok=True)
    run('sch','export','netlist','--format','kicadxml','-o',docs/'pcb-f-layout-netlist.xml',OUT/'pcb-f.kicad_sch')
    xml=ET.parse(docs/'pcb-f-layout-netlist.xml').getroot()
    # Also prove the fresh schematic partitions match the supplied verified XML.
    def partitions(root):
        return {n.get('name'): sorted((v.get('ref'),v.get('pin')) for v in n.findall('node')) for n in root.findall('nets/net')}
    assert partitions(xml)==partitions(ET.parse(docs/'pcb-f-netlist.xml').getroot())
    board=p.BOARD(); board.SetCopperLayerCount(2)
    nets={}
    for n in xml.findall('nets/net'):
        net=p.NETINFO_ITEM(board,n.get('name')); board.Add(net); nets[n.get('name')]=net
    assignments={(v.get('ref'),v.get('pin')):nets[n.get('name')] for n in xml.findall('nets/net') for v in n.findall('node')}
    placements={r['reference']:(float(r['x_mm']),float(r['y_mm']),float(r['rotation_deg']),r['side']) for r in rows(OUT/'placement.csv')}
    placements.update({f'C{i}':(257+20*(i-1),100,90,'F') for i in range(1,5)})
    placements.update({'C5':(257,153,90,'F'),'J1':(257.5,160.2,90,'B')})
    fps={}
    for c in xml.findall('components/comp'):
        ref=c.get('ref'); lib,name=c.findtext('footprint').split(':')
        path=OUT/'groovepush.pretty' if lib=='groovepush' else LIB/(lib+'.pretty')
        fp=p.FootprintLoad(str(path),name); assert fp is not None, (path,name)
        fp.SetReference(ref); fp.SetValue(c.findtext('value')); fp.SetFPID(p.LIB_ID(lib,name))
        fp.SetPath(p.KIID_PATH('/'+c.find('sheetpath').get('tstamps').strip('/')+'/'+c.findtext('tstamps')))
        fp.SetSheetname(c.find('sheetpath').get('names'))
        fp.SetSheetfile('sheets/faders-1.kicad_sch')
        board.Add(fp)
        x,y,angle,side=placements[ref]
        fp.SetPosition(vec(x,y))
        if side=='B': fp.Flip(vec(x,y),p.FLIP_DIRECTION_TOP_BOTTOM)
        fp.SetOrientationDegrees(angle)
        fp.Reference().SetVisible(False); fp.Value().SetVisible(False)
        for pad in fp.Pads():
            if (ref,pad.GetNumber()) in assignments: pad.SetNet(assignments[ref,pad.GetNumber()])
            if ref.startswith('RV') and pad.GetNumber()=='MP': pad.SetLocalZoneConnection(p.ZONE_CONNECTION_FULL)
        fps[ref]=fp
    mech=[r for r in rows(ROOT/'panel/pcb-outlines-console-v5.csv') if r['board']=='PCB-F']
    assert [r for r in rows(OUT/'mechanical.csv') if r['item'] in ('contorno','agujero')]==[r for r in mech if r['item'] in ('contorno','agujero')]
    outline=next(r for r in mech if r['item']=='contorno')
    assert tuple(float(outline[k]) for k in ('x_mm','y_mm','w_mm','h_mm'))==(294.5,129.5,81,83)
    corners=((X0,Y0),(X1,Y0),(X1,Y1),(X0,Y1))
    for a,b in zip(corners,corners[1:]+corners[:1]):
        s=p.PCB_SHAPE(); s.SetShape(p.SHAPE_T_SEGMENT); s.SetStart(vec(*a)); s.SetEnd(vec(*b)); s.SetLayer(p.Edge_Cuts); s.SetWidth(p.FromMM(0.05)); board.Add(s)
    for i,r in enumerate([r for r in mech if r['item']=='agujero'],1):
        fp=p.FootprintLoad(str(LIB/'MountingHole.pretty'),'MountingHole_3.2mm_M3')
        fp.SetReference(f'H{i}'); fp.SetValue('M3'); board.Add(fp)
        fp.SetAttributes(fp.GetAttributes() | p.FP_BOARD_ONLY)
        # M3 socket head Ø5.5; a washer Ø6.4 would overhang the prescribed edge.
        for item in fp.GraphicalItems():
            if isinstance(item,p.PCB_SHAPE) and item.GetShape()==p.SHAPE_T_CIRCLE:
                center=item.GetCenter(); item.SetEnd(p.VECTOR2I(center.x+p.FromMM(2.75),center.y))
                item.SetLayer(p.Cmts_User)
        xy=(float(r['x_mm']),float(r['y_mm'])); fp.SetPosition(vec(*xy))
        fp.Reference().SetVisible(False); fp.Value().SetVisible(False)
        placements[f'H{i}']=(*xy,0,'F')
    for rect in SLOTS: polygon(board,rect,LAYERS,keepout=True)
    def pad(ref,num):
        return next(v for v in fps[ref].Pads() if v.GetNumber()==str(num))
    def pp(ref,num): return panel(pad(ref,num).GetPosition())
    router=Router(board)
    # Route the supply first; the wipers may change layers around it.
    net=nets['+3V3']
    router.route(pp('J1',1),pp('C5',1),net,0.4,start_layer=1)
    router.route(pp('C5',1),pp('RV1',1),net,0.4)
    for i in range(1,4): router.route(pp(f'RV{i}',1),pp(f'RV{i+1}',1),net,0.4)
    for i in range(1,5):
        net=pad(f'RV{i}',2).GetNet()
        router.route(pp(f'RV{i}',2),pp(f'C{i}',1),net,0.25)
        router.route(pp(f'C{i}',1),pp('J1',i+1),net,0.25,end_layer=1)
    # SMD ground pads get a short explicit connection to a plated via.
    net=nets['GND']
    for ref in ('C1','C2','C3','C4','C5','J1'):
        num=6 if ref=='J1' else 2
        point=pp(ref,num)
        safe=router.masks(net,0.3)
        candidates=[]
        x,y=router.idx(point)
        for dx in range(-12,13):
            for dy in range(-12,13):
                nx,ny=x+dx,y+dy
                if math.hypot(dx,dy)*STEP < 1.3: continue
                if 0<=nx<router.nx and 0<=ny<router.ny and not safe[0][ny,nx] and not safe[1][ny,nx]:
                    candidates.append((math.hypot(dx,dy),nx,ny))
        _,nx,ny=min(candidates); target=router.point(nx,ny)
        router.route(point,target,net,0.4,start_layer=1 if ref=='J1' else 0,end_layer=1 if ref=='J1' else 0)
        via(board,target,net)
    # Stitch exposed corridors/perimeter, avoiding all other copper and the slots.
    safe=router.masks(net,0.3); count=0
    for x in (255.5,259.5,274.5,294.5,314.5,333.5):
        for y in (89.5,105,120,135,150,169.5):
            ix,iy=router.idx((x,y))
            if not safe[0][iy,ix] and not safe[1][iy,ix]:
                via(board,router.point(ix,iy),net); count+=1
    for layer in LAYERS:
        polygon(board,(X0+0.3,Y0+0.3,X1-0.3,Y1-0.3),[layer],net)
    # Put silk in the 9 mm gaps instead of over the moving faders.
    for i in range(1,5):
        text=p.PCB_TEXT(board); text.SetText(f'F{i}'); text.SetPosition(vec(274.5+20*(i-1) if i<4 else 332,129.5))
        text.SetTextSize(p.VECTOR2I(p.FromMM(1.2),p.FromMM(1.2))); text.SetTextThickness(p.FromMM(.18)); text.SetLayer(p.F_SilkS); board.Add(text)
    def identify(obj,key):
        obj.SetUuid(p.KIID(str(uuid.uuid5(uuid.NAMESPACE_URL,'grooveforge-console-pcb-f/'+key))))
    for fp in sorted(board.GetFootprints(),key=lambda f:f.GetReference()):
        key=fp.GetReference(); identify(fp,key)
        for i,field in enumerate(fp.GetFields()): identify(field,key+'/field/'+str(i))
        for i,item in enumerate(fp.GraphicalItems()): identify(item,key+'/graphic/'+str(i))
        for i,item in enumerate(fp.Pads()): identify(item,key+'/pad/'+str(i))
    for family,objects in [('drawing',board.GetDrawings()),('track',board.GetTracks()),('zone',board.Zones())]:
        for i,item in enumerate(objects): identify(item,family+'/'+str(i))
    p.SaveBoard(str(PCB),board)
    (docs/'layout-targets.json').write_text(json.dumps(placements,indent=2)+'\n')
    print('stitch vias',count,flush=True)


def project_rules():
    path=OUT/'pcb-f.kicad_pro'; pro=json.loads(path.read_text())
    ds=pro['board']['design_settings']; rules=ds['rules']
    rules.update(min_clearance=0.2,min_track_width=0.2,min_via_diameter=0.6,
                 min_through_hole_diameter=0.3,min_copper_edge_clearance=0.3)
    ds['drc_exclusions']=[]
    ds['rule_severities']['unconnected_items']='error'
    default=pro['net_settings']['classes'][0]; default.update(clearance=0.2,track_width=0.25,via_diameter=0.6,via_drill=0.3)
    power=dict(default); power.update(name='Power',priority=0,track_width=0.4)
    pro['net_settings']['classes']=[default,power]
    pro['net_settings']['netclass_patterns']=[{'netclass':'Power','pattern':'+3V3'},{'netclass':'Power','pattern':'GND'}]
    path.write_text(json.dumps(pro,indent=2)+'\n')
    (OUT/'pcb-f.kicad_dru').write_text('''(version 1)
(rule "Signal width" (constraint track_width (min 0.25)))
(rule "Power width"
  (condition "A.NetName == '+3V3' || A.NetName == 'GND'")
  (constraint track_width (min 0.4)))
''')


def verify():
    board=p.LoadBoard(str(PCB)); targets=json.loads((OUT/'docs/layout-targets.json').read_text())
    results=[]
    assert len(list(board.GetFootprints()))==len(targets)==14
    xml=ET.parse(OUT/'docs/pcb-f-layout-netlist.xml').getroot()
    source=ET.parse(OUT/'docs/pcb-f-netlist.xml').getroot()
    def partition(root):
        return {n.get('name'):set((v.get('ref'),v.get('pin')) for v in n.findall('node')) for n in root.findall('nets/net')}
    expected=partition(xml); assert expected==partition(source)
    actual={name:set() for name in expected}
    components={c.get('ref'):c for c in xml.findall('components/comp')}
    fixed={r['reference']:(float(r['x_mm']),float(r['y_mm']),float(r['rotation_deg']),r['side']) for r in rows(OUT/'placement.csv')}
    holes=[r for r in rows(OUT/'mechanical.csv') if r['item']=='agujero']
    for i,r in enumerate(holes,1): fixed[f'H{i}']=(float(r['x_mm']),float(r['y_mm']),0,'F')
    for ref,target in fixed.items(): assert list(target)==targets[ref]
    for fp in board.GetFootprints():
        ref=fp.GetReference()
        if ref in components:
            c=components[ref]
            assert fp.GetValue()==c.findtext('value'),ref
            assert fp.GetFPID().GetLibItemName()==c.findtext('footprint').split(':')[1],ref
        for pad in fp.Pads():
            if pad.GetNetCode() and pad.GetNumber():
                actual[pad.GetNetname()].add((ref,pad.GetNumber()))
    assert actual==expected, (actual,expected)
    for fp in board.GetFootprints():
        ref=fp.GetReference(); x,y,angle,side=targets[ref]; actual=panel(fp.GetPosition())
        err=math.dist(actual,(x,y)); assert err<=0.1,(ref,actual,(x,y))
        assert abs((fp.GetOrientationDegrees()-angle+180)%360-180)<0.001,ref
        assert fp.GetLayer()==(p.F_Cu if side=='F' else p.B_Cu),ref
        a,b,c,d=rect_panel(fp.GetBoundingBox(False,False))
        assert X0<=a<=c<=X1 and Y0<=b<=d<=Y1,(ref,'outside',a,b,c,d)
        if ref.startswith('H'):
            assert all(abs(p.ToMM(pad.GetDrillSize().x)-3.2)<1e-6 for pad in fp.Pads())
        for pad in fp.Pads():
            if ref.startswith('RV') and pad.GetNumber()=='MP':
                assert pad.GetNetname()=='GND' and pad.GetLocalZoneConnection()==p.ZONE_CONNECTION_FULL
        results.append({'reference':ref,'panel_mm':actual,'error_mm':err,'angle':angle,'side':side,'bounds_panel_mm':[a,b,c,d]})
    for t in board.GetTracks():
        r=p.ToMM(t.GetWidth(p.F_Cu) if isinstance(t,p.PCB_VIA) else t.GetWidth())/2
        for pt in (panel(t.GetStart()),panel(t.GetEnd())):
            assert X0+0.3<=pt[0]-r and pt[0]+r<=X1-0.3 and Y0+0.3<=pt[1]-r and pt[1]+r<=Y1-0.3
        if isinstance(t,p.PCB_VIA):
            assert p.ToMM(t.GetWidth(p.F_Cu) if isinstance(t,p.PCB_VIA) else t.GetWidth())>=0.6 and abs(p.ToMM(t.GetDrillValue())-0.3)<1e-6
        else:
            assert p.ToMM(t.GetWidth(p.F_Cu) if isinstance(t,p.PCB_VIA) else t.GetWidth()) >= (0.4 if t.GetNetname() in ('+3V3','GND') else 0.25)
    keepouts=[z for z in board.Zones() if z.GetIsRuleArea()]
    assert len(keepouts)==4
    for z in keepouts:
        assert z.GetDoNotAllowTracks() and z.GetDoNotAllowVias() and z.GetDoNotAllowZoneFills()
        assert all(z.IsOnLayer(layer) for layer in LAYERS)
    ground=[z for z in board.Zones() if not z.GetIsRuleArea()]
    assert len(ground)==2 and all(z.GetNetname()=='GND' for z in ground)
    for layer in LAYERS:
        zones=[z for z in ground if z.IsOnLayer(layer)]
        assert len(zones)==1
        fill=zones[0].GetFilledPolysList(layer)
        assert fill.Area()>0
        a,b,c,d=rect_panel(fill.BBox())
        assert X0+0.299<=a and c<=X1-0.299 and Y0+0.299<=b and d<=Y1-0.299
        for a,b,c,d in SLOTS:
            slot=p.SHAPE_POLY_SET();slot.NewOutline()
            for xy in ((a,b),(c,b),(c,d),(a,d)):
                v=vec(*xy);slot.Append(v.x,v.y)
            intersection=p.SHAPE_POLY_SET(fill);intersection.BooleanIntersection(slot)
            assert intersection.Area()==0,('copper in slot',layer,a,b,c,d)
    assert board.GetConnectivity().GetUnconnectedCount(True)==0
    (OUT/'docs/geometry-check.json').write_text(json.dumps({'transform':'panel=(kicad.x+44,371-kicad.y)','footprints':sorted(results,key=lambda r:r['reference']),'electrical_partitions':'6/6 match fresh and verified schematic XML','ground_fills':'F.Cu and B.Cu; zero intersection with all four slot reserves','tracks_and_vias':len(list(board.GetTracks())),'slot_keepouts':SLOTS,'result':'PASS'},indent=2)+'\n')
    print('Geometry PASS: 14 footprints; panel ±0.1 mm; copper bounds; widths; MP solid GND',flush=True)


def finish():
    docs=OUT/'docs'; fab=OUT/'fab'; fab.mkdir(exist_ok=True)
    board=p.LoadBoard(str(PCB))
    filler=p.ZONE_FILLER(board); assert filler.Fill(board.Zones())
    p.SaveBoard(str(PCB),board)
    run('pcb','drc','--format','json','-o',docs/'pcb-f-drc.json',PCB)
    verify()
    drc=json.loads((docs/'pcb-f-drc.json').read_text())
    errors=[v for v in drc['violations'] if v['severity']=='error']
    print('DRC',len(errors),'errors;',len(drc['violations'])-len(errors),'warnings;',len(drc['unconnected_items']),'unconnected; XML electrical partitions 6/6 match')
    if errors or drc['unconnected_items'] or drc.get('schematic_parity',[]):
        raise RuntimeError('DRC failed; inspect pcb-f-drc.json (no fabrication export)')
    run('pcb','export','gerbers','-l','F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts','-o',str(fab)+'/',PCB)
    run('pcb','export','drill','--excellon-separate-th','--generate-report','--report-path',fab/'pcb-f-drill-report.txt','-o',str(fab)+'/',PCB)
    run('pcb','export','svg','-l','F.Cu,B.Cu,F.Silkscreen,B.Silkscreen,F.Fab,B.Fab,Edge.Cuts','--common-layers','Edge.Cuts','--mode-multi','--page-size-mode','2','--exclude-drawing-sheet','-o',str(docs)+'/',PCB)
    run('pcb','export','svg','-l','F.Fab,F.Silkscreen,Cmts.User,Edge.Cuts','--sketch-pads-on-fab-layers','--mode-single','--black-and-white','--page-size-mode','2','--exclude-drawing-sheet','-o',docs/'pcb-f-assembly-1to1.svg',PCB)
    run('pcb','export','svg','-l','B.Fab,B.Silkscreen,Cmts.User,Edge.Cuts','--sketch-pads-on-fab-layers','--mode-single','--mirror','--black-and-white','--page-size-mode','2','--exclude-drawing-sheet','-o',docs/'pcb-f-bottom-assembly-1to1.svg',PCB)
    run('pcb','export','dxf','-l','Edge.Cuts','--output-units','mm','--mode-single','-o',docs/'pcb-f-outline.dxf',PCB)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only',action='store_true')
    parser.add_argument('--finish-only',action='store_true',help='Refill, DRC, verify and export existing board')
    args=parser.parse_args()
    if args.verify_only: verify()
    else:
        project_rules()
        if not args.finish_only: build()
        finish()

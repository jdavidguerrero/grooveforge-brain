#!/usr/bin/env python3
"""PCB-A strip only. Run using KiCad's bundled Python; see PCB-A README."""
import ast
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import zipfile
import pcbnew as k

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT/'apps/hardware/kicad/consola/boards'
OUT = BASE/'pcb-a'
PCB = OUT/'pcb-a.kicad_pcb'
CLI = '/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'

def helper(name):
    path=BASE/'tools'/f'{name}.py'
    source=path.read_text()
    if name=='grid_route':
        # KiCad 10 exposes SHAPE_ARC as abstract in SWIG. Sample its PCB_SHAPE instead.
        source=source.replace('arc = pcbnew.SHAPE_ARC(d.GetStart(), d.GetArcMid(), d.GetEnd(), 0)\n        chain = arc.ConvertToPolyline(pcbnew.FromMM(0.05))\n        pts = [pcbnew.ToMM(chain.CPoint(i)) for i in range(chain.PointCount())]',
            'cx,cy = pcbnew.ToMM(d.GetCenter())\n        r = pcbnew.ToMM(d.GetRadius())\n        a = math.atan2(sy-cy,sx-cx)\n        sweep = math.radians(d.GetArcAngle().AsDegrees())\n        pts = [(cx+r*math.cos(a+sweep*i/32),cy+r*math.sin(a+sweep*i/32)) for i in range(33)]')
        source=source.replace('RES = 0.1 ', 'RES = 0.05 ')
        source=source.replace('pcbnew.ToMM(t.GetWidth()) / 2','pcbnew.ToMM(t.GetWidth(pcbnew.F_Cu) if t.GetClass()=="PCB_VIA" else t.GetWidth()) / 2')
    if name=='grid_route':
        source=source.replace('mark_rect(blocked[k], bx0, by0, bx1, by1, CLEAR + half)',
            'mark_rect(blocked[k], bx0, by0, bx1, by1, (max(CLEAR,0.26) if p.GetAttribute()==pcbnew.PAD_ATTRIB_NPTH else CLEAR) + half)')
    tree=ast.parse(source)
    # Load existing repo helpers without their unconditional main() invocation.
    tree.body=[n for n in tree.body if not (isinstance(n,ast.Expr) and isinstance(n.value,ast.Call)
                and isinstance(n.value.func,ast.Name) and n.value.func.id=='main')]
    ns={'__file__':str(path),'__name__':name}
    exec(compile(tree,str(path),'exec'),ns)
    return ns

def point(x,y): return k.VECTOR2I(k.FromMM(x),k.FromMM(y))
def move(b,ref,x,y,angle=None):
    f=b.FindFootprintByReference(ref)
    if angle is not None: f.SetOrientationDegrees(angle)
    f.SetPosition(point(x,y))

def init():
    pro=OUT/'pcb-a.kicad_pro'
    cfg=json.loads(pro.read_text())
    rules=cfg['board']['design_settings']['rules']
    rules.update(min_clearance=.2,min_track_width=.2,min_via_diameter=.6,min_through_hole_diameter=.3)
    default=cfg['net_settings']['classes'][0]
    default.update(track_width=.25,clearance=.2,via_diameter=.6,via_drill=.3)
    power=dict(default,name='Power',priority=0,track_width=.4)
    cfg['net_settings']['classes']=[default,power]
    cfg['net_settings']['netclass_patterns']=[{'netclass':'Power','pattern':n} for n in ('/+3V3','/+5V','/GND')]
    pro.write_text(json.dumps(cfg,indent=2)+'\n')
    b=k.LoadBoard(str(PCB))
    for g in list(b.Groups()):
        for item in list(g.GetItems()): g.RemoveItem(item)
        b.Delete(g)
    for t in list(b.GetTracks()): b.Delete(t)
    for z in list(b.Zones()): b.Delete(z)
    for d in list(b.GetDrawings()):
        if d.GetLayer()==k.Edge_Cuts or d.GetClass()=='PCB_TEXT': b.Delete(d)
    # Four straight sides and exact 1 mm quarter arcs.
    lines=[((11,166.4),(226,166.4)),((227,167.4),(227,195)),
           ((226,196),(11,196)),((10,195),(10,167.4))]
    for a,c in lines:
        d=k.PCB_SHAPE(b);d.SetShape(k.SHAPE_T_SEGMENT);d.SetStart(point(*a));d.SetEnd(point(*c))
        d.SetLayer(k.Edge_Cuts);d.SetWidth(k.FromMM(.05));b.Add(d)
    for cx,cy,a0 in [(226,167.4,-90),(226,195,0),(11,195,90),(11,167.4,180)]:
        def p(a): return point(cx+math.cos(math.radians(a)),cy+math.sin(math.radians(a)))
        d=k.PCB_SHAPE(b);d.SetShape(k.SHAPE_T_ARC)
        d.SetArcGeometry(p(a0),p(a0+45),p(a0+90))
        d.SetLayer(k.Edge_Cuts);d.SetWidth(k.FromMM(.05));b.Add(d)
    # J1 electrical pad centre at (190,190.8); B footprint rotated along X.
    move(b,'J1',207.78,192.07,90)
    move(b,'J2',150,170,0)
    move(b,'J3',223,190,270)
    move(b,'J4',175,170,0)
    move(b,'U1',125.75,186.9,0)
    move(b,'C1',119.5,186,90)
    move(b,'C2',156,170,0)
    move(b,'R1',143,169,90)
    move(b,'R2',144.8,169,90)
    # Existing NPTH M3 holes; keep a head/standoff envelope clear on both faces.
    for ref,x,y in [('H1',28,190),('H2',74,179),('H3',100,179),('H4',204,179)]: move(b,ref,x,y)
    for f in b.GetFootprints():
        f.Reference().SetVisible(True); f.Value().SetVisible(False)
        for p in f.Pads():
            if p.GetNetname()=='/GND' and (f.GetReference().startswith('J') or p.GetNumber()=='MP'):
                p.SetLocalZoneConnection(k.ZONE_CONNECTION_FULL)
    for ref in ('R1','R2','J4'): b.FindFootprintByReference(ref).SetDNP(True)
    k.SaveBoard(str(PCB),b)
    print('Strip placed; encoder footprints and nets preserved')

def route():
    r=helper('route_board')
    b=k.LoadBoard(str(PCB))
    for t in b.GetTracks():
        if t.GetNetname()=='/GND' and t.GetClass()=='PCB_TRACK': t.SetWidth(k.FromMM(.4))
    kept=[t.Duplicate() for t in b.GetTracks() if t.GetNetname()=='/GND']
    for t in list(b.GetTracks()):
        if t.GetNetname()!='/GND': b.Delete(t)
    k.SaveBoard(str(PCB),b)
    for z in list(b.Zones()): b.Delete(z)
    # 0.25 mm is a conservative router clearance; project minimum remains 0.2.
    ds=b.GetDesignSettings()
    ds.m_NetSettings.GetDefaultNetclass().SetClearance(k.FromMM(.25))
    for _,nc in ds.m_NetSettings.GetNetclasses().items(): nc.SetClearance(k.FromMM(.25))
    r['add_npth_keepouts'](b);r['add_kept_keepouts'](b,kept)
    work=OUT/'docs'/'routing-final';work.mkdir(exist_ok=True)
    settings=work/'freerouting'/'freerouting.json'
    settings.parent.mkdir(exist_ok=True)
    settings.write_text(json.dumps({'gui':{'enabled':False},'router':{'max_passes':30,'max_threads':1,
        'job_timeout':'00:02:00','optimizer':{'max_passes':1,'max_threads':1}},
        'profile':{'allow_telemetry':False},'usage_and_diagnostic_data':{'disable_analytics':True}}))
    dsn=work/'pcb-a.dsn';ses=work/'pcb-a.ses'
    for old in (ses,work/'pcb-a_best.ses'):
        if old.exists(): old.unlink()
    if not k.ExportSpecctraDSN(b,str(dsn)): raise RuntimeError('DSN export failed')
    r['strip_nets'](str(dsn),['/GND'])
    cmd=['java','-Xmx2g','-Djava.awt.headless=true','-Djava.io.tmpdir='+str(work),'--class-path',os.path.expanduser('~/.kicad-mcp/freerouting.jar'),str(ROOT/'tools/layout/BoundedFreerouting.java'),
         '-de',str(dsn),'-do',str(ses),'--gui.enabled=false','-mp','30','-oit','100',
         '--router.optimizer.max_passes=1','--router.max_passes=30','-mt','1']
    with (work/'freerouting.log').open('w') as log:
        try: subprocess.run(cmd,cwd=work,stdout=log,stderr=subprocess.STDOUT,timeout=480,check=True)
        except subprocess.TimeoutExpired: print('Router timeout; importing best session if available')
    if not ses.exists(): ses=work/'pcb-a_best.ses'
    b=k.LoadBoard(str(PCB))
    if not ses.exists() or not k.ImportSpecctraSES(b,str(ses)): raise RuntimeError('No valid route session')
    for t in list(b.GetTracks()):
        if t.GetNetname()=='/GND': b.Delete(t)
    for t in kept: t.SetParent(b);b.Add(t)
    print('Dangling removed:',r['remove_dangling'](b))
    k.SaveBoard(str(PCB),b)
    print('Route imported:',len(b.GetTracks()))

def escapes():
    sys.argv=['add_gnd_escapes.py','pcb-a'];helper('add_gnd_escapes')['main']()

def pour():
    sys.argv=['add_gnd_pours.py','pcb-a'];helper('add_gnd_pours')['main']()

def verify():
    base=subprocess.check_output(['git','show','edc67a3:apps/hardware/kicad/consola/boards/pcb-a/pcb-a.kicad_pcb'],cwd=ROOT,text=True)
    # Use the independent S-expression parser already supplied by the fit audit.
    sys.path.insert(0,str(BASE.parent/'tools/fit'))
    import check_pcba_fit as fit
    def signatures(text):
        result={}
        for f in fit.children(fit.sexpr(text),'footprint'):
            ref={p[1]:p[2] for p in fit.children(f,'property')}['Reference']
            result[ref]=sorted((p[1],fit.one(p,'net')[-1] if fit.children(p,'net') else '') for p in fit.children(f,'pad'))
        return result
    assert signatures(base)==signatures(PCB.read_text()),'Pad/net contract changed'
    sch=OUT/'pcb-a.kicad_sch'
    before=subprocess.check_output(['git','show','edc67a3:'+str(sch.relative_to(ROOT))],cwd=ROOT)
    assert before==sch.read_bytes(),'Schematic changed'
    b=k.LoadBoard(str(PCB))
    assert all(f.IsDNP() for f in b.GetFootprints() if f.GetReference() in ('R1','R2','J4'))
    assert b.GetCopperLayerCount()==2
    assert abs(k.ToMM(b.GetDesignSettings().GetBoardThickness())-1.6)<1e-6
    for f in b.GetFootprints():
        for g in f.GraphicalItems():
            if g.GetLayer() in (k.F_CrtYd,k.B_CrtYd):
                bb=g.GetBoundingBox()
                assert k.FromMM(10)<=bb.GetLeft() and bb.GetRight()<=k.FromMM(227),f.GetReference()
                assert k.FromMM(166.4)<=bb.GetTop() and bb.GetBottom()<=k.FromMM(196),f.GetReference()
    for t in b.GetTracks():
        assert k.ToMM((t.GetWidth(k.F_Cu) if t.GetClass()=='PCB_VIA' else t.GetWidth()))>= (.4-1e-6 if t.GetNetname() in ('/+3V3','/+5V','/GND') and t.GetClass()!='PCB_VIA' else .25-1e-6)
        if t.GetClass()=='PCB_VIA': assert abs(k.ToMM(t.GetWidth(k.F_Cu))-.6)<1e-6 and abs(k.ToMM(t.GetDrill())-.3)<1e-6
    for f in b.GetFootprints():
        if f.GetReference().startswith('RE'):
            for p in f.Pads():
                if p.GetNumber()=='MP': assert p.GetNetname()=='/GND' and p.GetLocalZoneConnection()==k.ZONE_CONNECTION_FULL
    report=dict(base='edc67a3',schematic_sha256=hashlib.sha256(before).hexdigest(),pad_net_contract='identical',
                board_sha256=hashlib.sha256(PCB.read_bytes()).hexdigest(),layers=b.GetCopperLayerCount(),thickness_mm=1.6,footprints=len(b.GetFootprints()),tracks_and_vias=len(b.GetTracks()),vias=sum(t.GetClass()=='PCB_VIA' for t in b.GetTracks()),
                trace_widths_mm=sorted({k.ToMM(t.GetWidth()) for t in b.GetTracks() if t.GetClass()!='PCB_VIA'}),
                gnd_zones=sum(z.GetNetname()=='/GND' for z in b.Zones()),dnp=['R1','R2','J4'],encoder_mp='GND solid')
    (OUT/'docs/integrity.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

def drc():
    cmd=[CLI,'pcb','drc','--all-track-errors','--schematic-parity','--format','json','--exit-code-violations',
         '-o',str(OUT/'docs/drc-final.json'),str(PCB)]
    result=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    (OUT/'docs/drc-final.log').write_text(result.stdout)
    print(result.stdout)
    result.check_returncode()

def export():
    drc()
    verify()
    fab=OUT/'fab';fab.mkdir(exist_ok=True)
    layers='F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts'
    for args in [('gerbers','--layers',layers,'--subtract-soldermask'),
                 ('drill','--format','excellon','--drill-origin','absolute','--excellon-units','mm','--excellon-separate-th')]:
        subprocess.run([CLI,'pcb','export',*args,'-o',str(fab)+'/',str(PCB)],check=True)
    views=[(layer.replace('.','_'),layer+',Edge.Cuts') for layer in layers.split(',') if layer!='Edge.Cuts']
    views += [('outline','Edge.Cuts'),('mounting','Cmts.User,Edge.Cuts')]
    for name,ls in views+[('top','F.Cu,F.Mask,F.Silkscreen,Edge.Cuts'),('bottom','B.Cu,B.Mask,B.Silkscreen,Edge.Cuts'),
                    ('assembly-top','F.Fab,F.Silkscreen,Edge.Cuts'),('assembly-bottom','B.Fab,B.Silkscreen,Edge.Cuts')]:
        subprocess.run([CLI,'pcb','export','svg','--layers',ls,'--page-size-mode','2','--mode-single',
                        '-o',str(OUT/'docs'/f'pcb-a-{name}.svg'),str(PCB)],check=True)
    package()

def audit_copper():
    b=k.LoadBoard(str(PCB))
    # Standalone SWIG has no PGM_BASE: library parity dereferences it unconditionally.
    # Retain the real board netclasses; detach only for this diagnostic, never save it.
    ns=b.GetDesignSettings().m_NetSettings
    b.SetProject(None)
    b.GetDesignSettings().m_NetSettings=ns
    assert k.WriteDRCReport(b,str(OUT/'docs/native-drc.rpt'),k.EDA_UNITS_MM,True)

def finish_silk():
    b=k.LoadBoard(str(PCB))
    j=b.FindFootprintByReference('J1')
    for g in j.GraphicalItems():
        if g.GetLayer()==k.B_SilkS and g.GetBoundingBox().GetBottom()>k.FromMM(196): g.SetLayer(k.Dwgs_User)
        if g.GetLayer()==k.Dwgs_User and g.GetBoundingBox().GetTop()<k.FromMM(186.3):
            g.SetLayer(k.B_SilkS)
    for d in list(b.GetDrawings()):
        if d.GetLayer()==k.B_SilkS: b.Delete(d)
    # Custom silk mark inside the strip; the connector's copper, drill and body stay stock.
    compact=any(g.GetLayer()==k.B_SilkS and abs(k.ToMM(g.GetStart().x)-207.48)<.001 and abs(k.ToMM(g.GetStart().y)-195.05)<.001 for g in j.GraphicalItems() if g.GetClass()=='PCB_SHAPE')
    for a,c in ([] if compact else [((207.48,195.05),(207.78,194.45)),((207.78,194.45),(208.08,195.05)),((208.08,195.05),(207.48,195.05))]):
        d=k.PCB_SHAPE(j);d.SetShape(k.SHAPE_T_SEGMENT);d.SetStart(point(*a));d.SetEnd(point(*c))
        d.SetLayer(k.B_SilkS);d.SetWidth(k.FromMM(.12));j.Add(d)
    for ref,x,y,angle in [('C1',117.5,186,90),('R2',144.8,171.3,0)]:
        f=b.FindFootprintByReference(ref);f.Reference().SetPosition(point(x,y));f.Reference().SetTextAngle(k.EDA_ANGLE(angle,k.DEGREES_T))
    f=b.FindFootprintByReference('R1');f.Reference().SetPosition(point(141.6,169));f.Reference().SetTextAngle(k.EDA_ANGLE(90,k.DEGREES_T))
    # A project-local Connector_IDC alias makes the intentional silk adaptation reviewable
    # and allows library-parity DRC without muting that check or changing the schematic.
    library=OUT/'Connector_IDC.pretty';library.mkdir(exist_ok=True)
    fp=k.FOOTPRINT(j);fp.SetParent(b)
    fp.SetPosition(point(0,0));fp.SetOrientationDegrees(0)
    if fp.IsFlipped(): fp.Flip(point(0,0),False)
    fp.SetParent(None)
    fp.Reference().SetText('REF**')
    for pad in fp.Pads(): pad.SetNetCode(0)
    k.PCB_IO_MGR.FindPlugin(k.PCB_IO_MGR.KICAD_SEXP).FootprintSave(str(library),fp)
    assert (library/(str(fp.GetFPID().GetLibItemName())+'.kicad_mod')).exists()
    table=OUT/'fp-lib-table'
    text=table.read_text()
    if '(name "Connector_IDC")' not in text:
        text=text.rstrip()[:-1]+'  (lib (name "Connector_IDC")(type "KiCad")(uri "${KIPRJMOD}/Connector_IDC.pretty")(options "")(descr "Stock IDC copper and body; compact pin-1 silk for PCB-A strip"))\n)\n'
        table.write_text(text)
    for d in list(b.GetDrawings()):
        if d.GetClass()=='PCB_TEXT' and d.GetText().startswith(('H1: panel','H2: panel','H3: panel','H4: panel')): b.Delete(d)
    for ref in ['H1','H2','H3','H4']:
        f=b.FindFootprintByReference(ref);x,y=k.ToMM(f.GetPosition())
        d=k.PCB_TEXT(b);d.SetText(f'{ref}: panel ({x+44:g}, {371-y:g}) mm; NPTH D3.2')
        d.SetPosition(point(x,y+4));d.SetLayer(k.Cmts_User);d.SetTextSize(point(.8,.8));d.SetTextThickness(k.FromMM(.12));b.Add(d)
    k.SaveBoard(str(PCB),b)

def finish_missing():
    for _ in range(100):
        b=k.LoadBoard(str(PCB));b.BuildConnectivity();conn=b.GetConnectivity()
        pairs=[]
        for n in b.GetNetInfo().NetsByName().values():
            net=n.GetNetname()
            if not net or net=='/GND': continue
            pads=[p for f in b.GetFootprints() for p in f.Pads() if p.GetNetname()==net]
            seen=set();groups=[]
            for p in pads:
                if p.m_Uuid.AsString() in seen: continue
                todo=[p];items=set();gp=[]
                while todo:
                    t=todo.pop();u=t.m_Uuid.AsString()
                    if u in items: continue
                    items.add(u)
                    if t.GetClass()=='PAD': gp.append(t);seen.add(u)
                    todo.extend(i for i in conn.GetConnectedItems(t) if i.GetClass()!='ZONE')
                groups.append(gp)
            for i,g in enumerate(groups):
                for h in groups[i+1:]:
                    for p in g:
                        for q in h:
                            a=k.ToMM(p.GetPosition());c=k.ToMM(q.GetPosition())
                            pa=p.GetParentFootprint().GetReference()+'.'+p.GetNumber()
                            pb=q.GetParentFootprint().GetReference()+'.'+q.GetNumber()
                            pairs.append((math.dist(a,c),net,pa,pb))
        if not pairs: print('No missing non-GND branches');return
        good=False
        for _,net,a,c in sorted(pairs):
            width='0.4' if net in ('/+3V3','/+5V') else '0.25'
            sys.argv=['grid_route.py','pcb-a',a,c,width,'0.21']
            try: helper('grid_route')['main']();good=True;break
            except SystemExit as e: print(e)
        if not good: raise RuntimeError('Missing branches have no grid path')
    raise RuntimeError('Unclosed routing after 100 passes')

def tidy():
    b=k.LoadBoard(str(PCB))
    old=[]
    for t in b.GetTracks():
        if t.GetClass()=='PCB_TRACK' and t.GetNetname()=='/+5V' and t.GetLayer()==k.F_Cu:
            a,c=k.ToMM(t.GetStart()),k.ToMM(t.GetEnd())
            if abs(a[1]-181.025)<1e-6 and abs(c[1]-181.025)<1e-6 and min(a[0],c[0])<204<max(a[0],c[0]):
                old=[k.VECTOR2I(t.GetStart()),k.VECTOR2I(t.GetEnd())]
                break
    for pos in old:
        target=k.VECTOR2I(pos.x,pos.y+k.FromMM(.1))
        for t in b.GetTracks():
            if t.GetClass()=='PCB_TRACK' and t.GetNetname()=='/+5V':
                if t.GetStart()==pos:t.SetStart(target)
                if t.GetEnd()==pos:t.SetEnd(target)
    loose=[(174.091,179.710),(179.3215,185.8354),(177.3609,173.4761),(180.7581,186.6965)]
    for t in list(b.GetTracks()):
        if t.GetClass()=='PCB_VIA' and t.GetNetname()!='/GND' and any(math.dist(k.ToMM(t.GetPosition()),p)<.001 for p in loose):b.Delete(t)
    print('Dangling removed:',helper('route_board')['remove_dangling'](b))
    k.SaveBoard(str(PCB),b)

def package():
    fab=OUT/'fab'
    files=sorted(p for p in fab.iterdir() if p.suffix in ('.gtl','.gbl','.gtp','.gbp','.gto','.gbo','.gts','.gbs','.gm1','.drl','.gbrjob'))
    assert len(files)==12,'Expected nine Gerbers, PTH/NPTH and job metadata'
    with zipfile.ZipFile(fab/'pcb-a-gerbers.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for p in files: archive.write(p,p.name)
        assert archive.testzip() is None
    manifest={'board_sha256':hashlib.sha256(PCB.read_bytes()).hexdigest(),
              'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(fab.iterdir()) if p.name!='manifest.json'}}
    (fab/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Package:',len(files),'files')

if __name__=='__main__':
    globals()[sys.argv[1]]()

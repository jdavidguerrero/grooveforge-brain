#!/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
"""PCB-P: recrear, verificar y exportar; leer docs/THEORY-layout.md primero."""
from pathlib import Path
import argparse
import csv
import json
import math
import uuid
import zipfile
import xml.etree.ElementTree as ET
import pcbnew as p
import layout_f as base

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'boards/pcb-p'
DOCS = OUT/'docs'
PCB = OUT/'pcb-p.kicad_pcb'
LIB = base.LIB
CLI = base.CLI
X0,Y0,X1,Y1 = 9.,69.,232.,100.
R, T, H, FILM, INSERT, LENGTH = 2.5,1.6,8.5,.46,3.,37.4
VERTICAL = T+H+FILM/2
BOTTOM_RUN = LENGTH-1-3*math.pi*R/2-VERTICAL-R-INSERT
DISTANCE = 9.15+1+BOTTOM_RUN+3*R
base.X0,base.Y0,base.X1,base.Y1 = X0,Y0,X1,Y1
base.STEP = .2
base.SLOTS = []
vec, panel, rect_panel = base.vec, base.panel, base.rect_panel
run = base.run


def rows(path):
    with path.open() as f: return list(csv.DictReader(f))


def sensors():
    return [r for r in rows(ROOT/'panel/pcb-outlines-console-v5.csv') if r['board']=='PCB-P' and r['item']=='FSR 402']


def mechanics():
    return [r for r in rows(ROOT/'panel/pcb-outlines-console-v5.csv') if r['board']=='PCB-P']


def direction(label):
    a = 205 if label=='V4' else 25
    return a,math.cos(math.radians(a)),math.sin(math.radians(a))


class Router(base.Router):
    """Mismo A* raster de PCB-F, con reserva M2.5 de radio 2,5 mm."""
    def masks(self, net, radius):
        masks=super().masks(net,radius)
        for r in mechanics():
            if r['item']=='agujero':
                disk=(self.xx-float(r['x_mm']))**2+(self.yy-float(r['y_mm']))**2 < (2.5+radius+.2)**2
                for mask in masks: mask |= disk
        for fp in self.board.GetFootprints():
            for pad in fp.Pads():
                if pad.GetAttribute()==p.PAD_ATTRIB_NPTH:
                    a,b,c,d=rect_panel(pad.GetBoundingBox())
                    obstacle=(self.xx>a-radius-.35)&(self.xx<c+radius+.35)&(self.yy>b-radius-.35)&(self.yy<d+radius+.35)
                    for mask in masks: mask |= obstacle
        if radius >= .29:

            # A via must not share a PTH drill, even when the two carry the same net.
            for fp in self.board.GetFootprints():
                for pad in fp.Pads():
                    drill=p.ToMM(pad.GetDrillSize().x)
                    if pad.GetAttribute()==p.PAD_ATTRIB_PTH and drill:
                        x,y=panel(pad.GetPosition())
                        disk=(self.xx-x)**2+(self.yy-y)**2 < (drill/2+radius+.3)**2
                        for mask in masks: mask |= disk
        return masks


def text(board, value, x, y, layer=p.F_SilkS, size=1.):
    size=max(1.,size)
    item=p.PCB_TEXT(board);item.SetText(value);item.SetPosition(vec(x,y));item.SetLayer(layer)
    item.SetTextSize(p.VECTOR2I(p.FromMM(size),p.FromMM(size)));item.SetTextThickness(p.FromMM(.15))
    if layer==p.B_SilkS: item.SetMirrored(True)
    board.Add(item)


def silkscreen_minimums(board):
    """Conservar geometría y exigir trazos de serigrafía de al menos 0,15 mm."""
    for fp in board.GetFootprints():
        for item in fp.GraphicalItems():
            if item.GetLayer() in (p.F_SilkS,p.B_SilkS) and isinstance(item,p.PCB_SHAPE):
                item.SetWidth(max(item.GetWidth(),p.FromMM(.15)))


def sync_fields(fp, component):
    """Mantener los campos de compra y descripcion iguales al XML del esquema."""
    for item in component.findall('fields/field'):
        name=item.get('name')
        if name in ('Reference','Value','Footprint'): continue
        field=fp.GetField(name)
        if field is None:
            field=p.PCB_FIELD(fp,p.FIELD_T_USER,name);fp.Add(field)
        field.SetText(item.text or '');field.SetVisible(False)


def prepare_library():
    local=OUT/'gf_consola.pretty'; local.mkdir(exist_ok=True)
    source=LIB/'Connector_PinSocket_2.54mm.pretty/PinSocket_1x02_P2.54mm_Vertical.kicad_mod'
    content=source.read_text().replace('(footprint "PinSocket_1x02_P2.54mm_Vertical"', '(footprint "FSR402_DirectSolder_1x02_P2.54mm"', 1)
    content=content.replace('Through hole straight socket strip, 1x02, 2.54mm pitch, single row (from Kicad 4.0.7), script generated', 'Socket inferior HCTL C2897365; geometria PinSocket 1x02 P2.54 estandar. Identificador historico D43 retenido para paridad; NO soldadura directa. Datum = pin 1 del socket, no disco.')
    (local/'FSR402_DirectSolder_1x02_P2.54mm.kicad_mod').write_text(content)


def rules():
    template=json.loads((ROOT/'boards/pcb-f/pcb-f.kicad_pro').read_text())
    template['meta']['filename']='pcb-p.kicad_pro'
    ds=template['board']['design_settings'];ds['drc_exclusions']=[]
    ds['rules'].update(min_clearance=.2,min_track_width=.25,min_copper_edge_clearance=.3)
    ds['rule_severities']['unconnected_items']='error'
    template['net_settings']['classes']=template['net_settings']['classes'][:1]
    template['net_settings']['netclass_patterns']=[]
    (OUT/'pcb-p.kicad_pro').write_text(json.dumps(template,indent=2)+'\n')
    (OUT/'pcb-p.kicad_dru').write_text('''(version 1)
(rule "Pistas para ensamble manual" (constraint track_width (min 0.25mm)))
''')
    table='(fp_lib_table (version 7)\n  (lib (name "gf_consola")(type "KiCad")(uri "${KIPRJMOD}/gf_consola.pretty")(options "")(descr "PCB-P: socket inferior; identificador historico del esquema"))\n'
    for lib in ('Connector_PinSocket_2.54mm','Connector_JST','MountingHole'):
        table+=f'(lib (name "{lib}")(type "KiCad")(uri "{LIB}/{lib}.pretty")(options "")(descr "KiCad 10"))\n'
    (OUT/'fp-lib-table').write_text(table+')\n')


def build():
    DOCS.mkdir(exist_ok=True)
    prepare_library()
    run('sch','export','netlist','--format','kicadxml','-o',DOCS/'pcb-p-layout-netlist.xml',OUT/'pcb-p.kicad_sch')
    xml=ET.parse(DOCS/'pcb-p-layout-netlist.xml').getroot()
    board=p.BOARD();board.SetCopperLayerCount(2);board.GetDesignSettings().SetBoardThickness(p.FromMM(T))
    nets={}
    for n in xml.findall('nets/net'):
        net=p.NETINFO_ITEM(board,n.get('name'));board.Add(net);nets[n.get('name')]=net
    assignments={(v.get('ref'),v.get('pin')):nets[n.get('name')] for n in xml.findall('nets/net') for v in n.findall('node')}
    targets={}
    for r in sensors():
        label=r['notes'].removeprefix('FSR ');i=('V1','V2','V3','V4','X1','X2').index(label)+1
        cx,cy=float(r['x_mm']),float(r['y_mm']);angle,dx,dy=direction(label)
        targets[f'FSR{i}']=(cx+DISTANCE*dx+1.27*dy,cy+DISTANCE*dy-1.27*dx,angle,'B')
        targets[f'J{i}']=(cx-1,72.5,0,'B')
    fps={}
    for c in xml.findall('components/comp'):
        ref=c.get('ref')
        lib,name=c.findtext('footprint').split(':')
        fp=p.FootprintLoad(str(OUT/'gf_consola.pretty' if lib=='gf_consola' else LIB/(lib+'.pretty')),name);assert fp
        fp.SetReference(ref);fp.SetValue(c.findtext('value'));fp.SetFPID(p.LIB_ID(lib,name));sync_fields(fp,c)
        fp.SetPath(p.KIID_PATH('/'+c.find('sheetpath').get('tstamps').strip('/')+'/'+c.findtext('tstamps')))
        fp.SetSheetname(c.find('sheetpath').get('names'));fp.SetSheetfile('sheets/'+c.find('sheetpath').get('names').strip('/')+'.kicad_sch')
        board.Add(fp)
        x,y,angle,side=targets[ref];fp.SetPosition(vec(x,y))
        fp.Flip(vec(x,y),p.FLIP_DIRECTION_TOP_BOTTOM);fp.SetOrientationDegrees(angle)
        fp.Reference().SetVisible(False);fp.Value().SetVisible(False)
        for pad in fp.Pads():
            if (ref,pad.GetNumber()) in assignments: pad.SetNet(assignments[ref,pad.GetNumber()])
        fps[ref]=fp
    corners=((X0,Y0),(X1,Y0),(X1,Y1),(X0,Y1))
    for a,b in zip(corners,corners[1:]+corners[:1]):
        s=p.PCB_SHAPE();s.SetShape(p.SHAPE_T_SEGMENT);s.SetStart(vec(*a));s.SetEnd(vec(*b));s.SetLayer(p.Edge_Cuts);s.SetWidth(p.FromMM(.05));board.Add(s)
    assert [r for r in rows(OUT/'mechanical.csv') if r['item'] in ('contorno','agujero')]==[r for r in mechanics() if r['item'] in ('contorno','agujero')]
    for i,r in enumerate([r for r in mechanics() if r['item']=='agujero'],1):
        fp=p.FootprintLoad(str(LIB/'MountingHole.pretty'),'MountingHole_2.7mm_M2.5')
        fp.SetReference(f'H{i}');fp.SetValue('M2.5 NPTH');board.Add(fp);fp.SetAttributes(fp.GetAttributes()|p.FP_BOARD_ONLY)
        fp.SetPosition(vec(float(r['x_mm']),float(r['y_mm'])))
        fp.SetFPID(p.LIB_ID('MountingHole','MountingHole_2.7mm_M2.5'))
        fp.Reference().SetVisible(False);fp.Value().SetVisible(False)
        targets[fp.GetReference()]=(float(r['x_mm']),float(r['y_mm']),0,'F')
    for i,r in enumerate(sensors(),1):
        label=r['notes'].removeprefix('FSR ');angle,dx,dy=direction(label)
        cx,cy=float(r['x_mm']),float(r['y_mm'])
        s=p.PCB_SHAPE();s.SetShape(p.SHAPE_T_CIRCLE);s.SetCenter(vec(cx,cy));s.SetEnd(vec(cx+9.15,cy));s.SetLayer(p.Dwgs_User);s.SetWidth(p.FromMM(.1));board.Add(s)
        # A drilled oblong is explicitly NPTH. KiCad exports it as G85 in the NPTH file.
        fp=p.FOOTPRINT(board);board.Add(fp);fp.SetReference(f'SLOT{i}');fp.SetValue('COLA 3x8.8 NPTH')
        fp.SetAttributes(p.FP_BOARD_ONLY|p.FP_EXCLUDE_FROM_BOM|p.FP_EXCLUDE_FROM_POS_FILES)
        fp.SetPosition(vec(cx+12.3*dx,cy+12.3*dy));fp.SetOrientationDegrees(angle)
        fp.Reference().SetVisible(False);fp.Value().SetVisible(False)
        pad=p.PAD(fp);pad.SetAttribute(p.PAD_ATTRIB_NPTH);pad.SetShape(p.PAD_SHAPE_OVAL)
        pad.SetSize(p.VECTOR2I(p.FromMM(3),p.FromMM(8.8)));pad.SetDrillShape(p.PAD_DRILL_SHAPE_OBLONG)
        pad.SetDrillSize(pad.GetSize());layers=p.LSET(p.LSET.AllCuMask());layers.AddLayer(p.F_Mask);layers.AddLayer(p.B_Mask);pad.SetLayerSet(layers)
        fp.Add(pad);pad.SetPosition(fp.GetPosition());pad.SetOrientationDegrees(angle)
        targets[fp.GetReference()]=(cx+12.3*dx,cy+12.3*dy,angle,'F')
        text(board,label,cx,98.8,p.F_SilkS,.85)
        text(board,f'{label} / J{list(("V1","V2","V3","V4","X1","X2")).index(label)+1}',cx,97.8,p.B_SilkS,.85)
    router=Router(board)
    for i in range(1,7):
        for n in (1,2):
            a=next(pad for pad in fps[f'FSR{i}'].Pads() if pad.GetNumber()==str(n))
            b=next(pad for pad in fps[f'J{i}'].Pads() if pad.GetNumber()==str(n))
            router.route(panel(a.GetPosition()),panel(b.GetPosition()),a.GetNet(),.25,start_layer=1,end_layer=1)
    text(board,'GROOVEFORGE  PCB-P  v1  1.6mm',82,84.5,p.F_SilkS,.8)
    text(board,'SOCKETS / JST ABAJO',82,87,p.F_SilkS,.85)
    silkscreen_minimums(board)
    # Stable UUIDs preserve reviewable diffs when rebuilding.
    def identify(obj,key): obj.SetUuid(p.KIID(str(uuid.uuid5(uuid.NAMESPACE_URL,'grooveforge-console-pcb-p/'+key))))
    for fp in sorted(board.GetFootprints(),key=lambda f:f.GetReference()):
        key=fp.GetReference();identify(fp,key)
        for i,item in enumerate(fp.GetFields()): identify(item,key+'/field/'+str(i))
        for i,item in enumerate(fp.GraphicalItems()): identify(item,key+'/graphic/'+str(i))
        for i,item in enumerate(fp.Pads()): identify(item,key+'/pad/'+str(i))
    for family,objects in [('drawing',board.GetDrawings()),('track',board.GetTracks())]:
        for i,item in enumerate(objects): identify(item,family+'/'+str(i))
    p.SaveBoard(str(PCB),board)
    (DOCS/'layout-targets.json').write_text(json.dumps(targets,indent=2)+'\n')


def verify():
    board=p.LoadBoard(str(PCB));xml=ET.parse(DOCS/'pcb-p-layout-netlist.xml').getroot()
    fresh=ET.parse(DOCS/'pcb-p-netlist.xml').getroot()
    def parts(root):
        return {n.get('name'):sorted((v.get('ref'),v.get('pin')) for v in n.findall('node')) for n in root.findall('nets/net')}
    assert parts(xml)==parts(fresh)
    expected={name:set(values) for name,values in parts(xml).items()}
    actual={name:set() for name in expected};fps={f.GetReference():f for f in board.GetFootprints()}
    assert len(fps)==28 and len(expected)==12
    targets=json.loads((DOCS/'layout-targets.json').read_text());results=[]
    for c in xml.findall('components/comp'):
        fp=fps[c.get('ref')]
        assert fp.GetValue()==c.findtext('value')
        assert str(fp.GetFPID().GetUniStringLibId())==c.findtext('footprint')
    for ref,target in targets.items():
        fp=fps[ref];x,y,a,side=target
        assert math.dist(panel(fp.GetPosition()),(x,y))<.00001,ref
        assert abs((fp.GetOrientationDegrees()-a+180)%360-180)<.001,ref
        assert fp.GetLayer()==(p.F_Cu if side=='F' else p.B_Cu),ref
        for pad in fp.Pads():
            if pad.GetNumber(): actual[pad.GetNetname()].add((ref,pad.GetNumber()))
        results.append(dict(reference=ref,panel_mm=panel(fp.GetPosition()),side=side))
    assert actual==expected,(actual,expected)
    holes=[r for r in mechanics() if r['item']=='agujero']
    assert [(float(r['x_mm']),float(r['y_mm'])) for r in holes]==[(x,y) for x in (40,89.5,139,170,201) for y in (72,97)]
    for i,r in enumerate(holes,1):
        fp=fps[f'H{i}'];assert math.dist(panel(fp.GetPosition()),(float(r['x_mm']),float(r['y_mm'])))<1e-6
        pad=next(iter(fp.Pads()));assert pad.GetAttribute()==p.PAD_ATTRIB_NPTH and abs(p.ToMM(pad.GetDrillSize().x)-2.7)<1e-6
    for r in sensors():
        label=r['notes'].removeprefix('FSR ');i=('V1','V2','V3','V4','X1','X2').index(label)+1
        cx,cy=float(r['x_mm']),float(r['y_mm']);a,dx,dy=direction(label)
        fp=fps[f'FSR{i}'];pads={v.GetNumber():panel(v.GetPosition()) for v in fp.Pads()}
        centre=tuple((pads['1'][j]+pads['2'][j])/2 for j in (0,1))
        assert math.dist(centre,(cx+DISTANCE*dx,cy+DISTANCE*dy))<.00001,(label,centre,DISTANCE)
        slot=next(iter(fps[f'SLOT{next(j for j,s in enumerate(sensors(),1) if s["notes"]==r["notes"])}'].Pads()))
        assert slot.GetAttribute()==p.PAD_ATTRIB_NPTH and tuple(round(p.ToMM(v),3) for v in (slot.GetDrillSize().x,slot.GetDrillSize().y))==(3,8.8)
    for fp in fps.values():
        if not fp.GetReference().startswith(('FSR','J')): continue
        a,b,c,d=rect_panel(fp.GetBoundingBox(False,False));assert X0<=a<=c<=X1 and Y0<=b<=d<=Y1,(fp.GetReference(),a,b,c,d)
        for pad in fp.Pads():
            point=panel(pad.GetPosition())
            for r in sensors():
                # Copper radius bound includes the rectangular pad diagonal.
                radius=math.hypot(p.ToMM(pad.GetSize().x),p.ToMM(pad.GetSize().y))/2
                assert math.dist(point,(float(r['x_mm']),float(r['y_mm'])))>9.15+radius,(fp.GetReference(),'pad bajo disco',point)
    verify_sockets_purchase()
    assert len(board.Zones())==0
    for track in board.GetTracks():
        a,b=panel(track.GetStart()),panel(track.GetEnd())
        radius=p.ToMM(track.GetWidth(p.F_Cu) if isinstance(track,p.PCB_VIA) else track.GetWidth())/2
        for row in holes:
            hx,hy=float(row['x_mm']),float(row['y_mm'])
            dx,dy=b[0]-a[0],b[1]-a[1];length2=dx*dx+dy*dy
            u=max(0,min(1,((hx-a[0])*dx+(hy-a[1])*dy)/length2)) if length2 else 0
            assert math.hypot(hx-a[0]-u*dx,hy-a[1]-u*dy)>2.5+radius,(row,'reserva PEM')
        if isinstance(track,p.PCB_VIA): assert p.ToMM(track.GetDrillValue())>=.3
        else: assert p.ToMM(track.GetWidth())>=.25
    assert board.GetConnectivity().GetUnconnectedCount(True)==0
    (DOCS/'geometry-check.json').write_text(json.dumps(dict(result='PASS',transform='panel=(kicad.x+44,371-kicad.y)',footprints=results,holes_unchanged=True,nets=12,ground_zones=0,tail_mm=LENGTH,socket_offset_mm=DISTANCE,tail_bottom_run_mm=BOTTOM_RUN,tracks_and_vias=len(list(board.GetTracks()))),indent=2)+'\n')
    print('Geometria PASS: 6 discos, 6 ranuras, 6 sockets B, 6 JST B, 10 agujeros exactos; 12 redes',flush=True)


def verify_sockets_purchase():
    import sys
    sys.path.insert(0,str(ROOT/'tools'))
    from check_bom import check_components
    catalog=json.loads((ROOT/'tools/lcsc_catalog.json').read_text())
    purchases=rows(OUT/'assembly-sockets.csv')
    assert len(purchases)==6
    assert {r['Reference'] for r in purchases}=={f'XS{i}' for i in range(1,7)}
    assert {r['Sensor'] for r in purchases}=={f'FSR{i}' for i in range(1,7)}
    assert all(r['Side']=='B' for r in purchases)
    errors,pending=check_components(purchases,catalog)
    assert not errors and not pending,(errors,pending)
    order=rows(ROOT/'fab/pedido-lcsc-v1.csv')
    assert sum(int(r['Quantity']) for r in order if r['LCSC Part Number']=='C2897365')>=8


def finish():
    rules()
    run('pcb','drc','--exit-code-violations','--schematic-parity','--format','json','-o',DOCS/'pcb-p-drc.json',PCB)
    drc=json.loads((DOCS/'pcb-p-drc.json').read_text())
    assert not drc['violations'] and not drc['unconnected_items'] and not drc.get('schematic_parity',[])
    verify()
    fab=OUT/'fab';fab.mkdir(exist_ok=True)
    run('pcb','export','gerbers','-l','F.Cu,B.Cu,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts','-o',str(fab)+'/',PCB)
    run('pcb','export','drill','--excellon-separate-th','--generate-report','--report-path',fab/'pcb-p-drill-report.txt','-o',str(fab)+'/',PCB)
    run('pcb','export','svg','-l','F.Fab,F.Silkscreen,Dwgs.User,Cmts.User,Edge.Cuts','--sketch-pads-on-fab-layers','--mode-single','--black-and-white','--page-size-mode','2','--exclude-drawing-sheet','-o',DOCS/'pcb-p-mounting-1to1.svg',PCB)
    run('pcb','export','svg','-l','B.Fab,B.Silkscreen,Dwgs.User,Cmts.User,Edge.Cuts','--sketch-pads-on-fab-layers','--mode-single','--mirror','--black-and-white','--page-size-mode','2','--exclude-drawing-sheet','-o',DOCS/'pcb-p-bottom-1to1.svg',PCB)
    # ROOT = apps/hardware/kicad/consola; target is apps/design.
    step=ROOT.parents[2]/'design/3d-models/consola/pcb-p.step';step.parent.mkdir(parents=True,exist_ok=True)
    run('pcb','export','step','--force','--define-var',f'KICAD10_3DMODEL_DIR=/Applications/KiCad/KiCad.app/Contents/SharedSupport/3dmodels','-o',step,PCB)
    from drawings_p import write_drawings
    write_drawings(OUT,p.LoadBoard(str(PCB)),sensors(),[r for r in mechanics() if r['item']=='agujero'],
                   (R,T,H,FILM,INSERT,LENGTH,DISTANCE,BOTTOM_RUN))
    with zipfile.ZipFile(fab/'pcb-p-gerber-jlc.zip','w',zipfile.ZIP_DEFLATED) as z:
        for path in sorted(fab.iterdir()):
            if path.suffix in ('.gbr','.gtl','.gbl','.gto','.gbo','.gts','.gbs','.gm1','.drl'): z.write(path,path.name)
    write_manifest()


def write_manifest():
    import hashlib
    repo=ROOT.parents[3]
    paths=[PCB,OUT/'pcb-p.kicad_pro',OUT/'pcb-p.kicad_dru',OUT/'fp-lib-table',
           OUT/'pcb-p.kicad_sch',OUT/'sheets/fsr-passive-1.kicad_sch',OUT/'assembly-sockets.csv',
           OUT/'gf_consola.pretty/FSR402_DirectSolder_1x02_P2.54mm.kicad_mod',
           ROOT/'panel/pcb-outlines-console-v5.csv',ROOT/'fab/pedido-lcsc-v1.csv',ROOT/'tools/lcsc_catalog.json',
           ROOT.parents[2]/'design/3d-models/consola/pcb-p.step',DOCS/'pcb-p-drc.json',DOCS/'geometry-check.json']
    paths += sorted((OUT/'fab').iterdir())
    paths += sorted(DOCS.glob('*1to1.svg'))
    hashes={str(path.relative_to(repo)):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths if path.is_file()}
    (DOCS/'fabrication-manifest.json').write_text(json.dumps(dict(result='PASS',sha256=hashes),indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--verify-only',action='store_true');parser.add_argument('--finish-only',action='store_true');args=parser.parse_args()
    if args.verify_only: verify()
    else:
        if not args.finish_only: rules();build();rules()
        finish()

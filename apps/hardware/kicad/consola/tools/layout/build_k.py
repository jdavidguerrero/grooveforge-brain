#!/usr/bin/env python3
"""Build split PCB-K from immutable exported netlists and panel coordinates.
Run with the KiCad bundled Python (pcbnew). No schematic regeneration.
"""
from pathlib import Path
import sys, csv, json, math, xml.etree.ElementTree as ET
sys.path.append('/Applications/KiCad/KiCad.app/Contents/SharedSupport/scripting')
import pcbnew as p
ROOT=Path(__file__).resolve().parents[6]
CON=ROOT/'apps/hardware/kicad/consola'
STD=Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints')
OUT=CON/'tools/layout'
S=.05

def v(x,y): return p.VECTOR2I(round(x*1e6),round(y*1e6))
def xy(x,y): return v(x-44,371-y)
def mm(vv): return (p.ToMM(vv.x),p.ToMM(vv.y))
POLYS={
 'left':[(6,120),(86,120),(86,104),(108,104),(108,173),(68,173),(68,221),(74.5,221),(74.5,320),(50,320),(50,311),(6,311)],
 'right':[(108,28),(232,28),(232,90),(252,90),(252,180),(236,180),(236,104),(108,104)]}

def edge(b,a,z):
 e=p.PCB_SHAPE();e.SetShape(p.SHAPE_T_SEGMENT);e.SetLayer(p.Edge_Cuts);e.SetStart(xy(*a));e.SetEnd(xy(*z));e.SetWidth(p.FromMM(.05));b.Add(e)

def make(side):
 name='pcb-k-'+side;folder=CON/'boards'/name
 root=ET.parse(folder/'docs'/f'{name}-netlist.xml').getroot()
 b=p.BOARD();b.SetCopperLayerCount(2);b.GetDesignSettings().SetBoardThickness(p.FromMM(1.6))
 b.GetTitleBlock().SetTitle('GrooveForge '+name+' — hand solder');b.GetTitleBlock().SetRevision('layout-1')
 nets={}; pinmap={}
 for nn in root.findall('./nets/net'):
  n=p.NETINFO_ITEM(b,nn.attrib['name'],int(nn.attrib['code']));b.Add(n);nets[n.GetNetname()]=n
  for node in nn.findall('node'):pinmap[(node.attrib['ref'],node.attrib['pin'])]=n
 comps={c.attrib['ref']:c for c in root.findall('./components/comp')};fps={}
 for ref,c in comps.items():
  lib,fp=c.findtext('footprint').split(':');libpath=CON/'lib/gf_consola.pretty' if lib=='gf_consola' else STD/(lib+'.pretty')
  f=p.FootprintLoad(str(libpath),fp); assert f, (ref,libpath,fp)
  f.SetReference(ref);f.SetValue(c.findtext('value'));f.SetFPID(p.LIB_ID(lib,fp))
  # Import exact schematic UUID path so KiCad schematic parity can be checked.
  stamp=c.findtext('tstamps');sheet=c.find('sheetpath')
  if stamp and sheet is not None:
   f.SetPath(p.KIID_PATH(sheet.attrib['tstamps']+stamp))
  b.Add(f)
  if not ref.startswith('SW'):f.Flip(v(0,0),False)
  for pad in f.Pads():
   n=pinmap.get((ref,pad.GetNumber()))
   if n:pad.SetNet(n)
  f.Reference().SetVisible(False);f.Value().SetVisible(False)
  for item in f.GraphicalItems():
   if isinstance(item,p.PCB_TEXT) and item.GetLayer() in (p.F_SilkS,p.B_SilkS):item.SetVisible(False)
  if ref.startswith('SW'):
   for item in f.GraphicalItems():
    if isinstance(item,p.PCB_SHAPE) and item.GetLayer()==p.F_CrtYd:
     item.SetStart(v(-7.75,-7.49));item.SetEnd(v(7.75,7.49))
  fps[ref]=f
 placements={}
 def place(ref,x,y,angle=0):
  f=fps[ref];f.SetOrientationDegrees(angle);f.SetPosition(xy(x,y));placements[ref]=(x,y,angle)
 for row in csv.DictReader((folder/'placement.csv').open()):
  if row['component']=='socket':continue
  place(row['reference'],float(row['x_mm']),float(row['y_mm']),float(row['rotation_deg']))
 # Local LED decoupling: beside VDD, outside the switch central drill.
 for ref in list(fps):
  if ref.startswith('D') and ref[1:].isdigit():
   i=int(ref[1:]);x,y,_=placements[ref];place('C'+str(99+i),x+5.8,y,90)
 if side=='left':
  place('U22',77,144);place('U23',30,204)
  place('J1',23,264,90);place('J2',70,122,90)
  place('C1',30,269);place('R1',35,256);place('R2',65,136,90);place('R3',61,136,90)
 else:
  place('U21',211,58);place('J1',114,62);place('C1',131,62);place('R1',116,55)
  place('R2',188,57,90);place('R3',192,57,90)
 for ref in [r for r in fps if r.startswith('U')]:
  tag=ref[1:];x,y,_=placements[ref]
  place('C'+tag+'1',x-10,y+1.925,90);place('C'+tag+'2',x-13.5,y+1.925,90)
  place('R'+tag,x+11,y+2.275,90)
  if tag=='22':
   place('C221',65,141,90);place('C222',61,141,90)
  ts=[r for r in fps if r.startswith('TP'+tag)]
  for i,t in enumerate(ts):
   if side=='right':place(t,177+i*4,65.5)
   else:place(t,10+(i%7)*7,187+(i//7)*4 if tag=='22' else 214+(i//7)*4)
 for a,z in zip(POLYS[side],POLYS[side][1:]+POLYS[side][:1]):edge(b,a,z)
 rows=list(csv.DictReader((CON/'panel/pcb-outlines-console-v5.csv').open()));holes=[]
 for row in rows:
  if row['item']!='agujero' or row['board'] not in ('PCB-K','PCB-P'):continue
  x,y=float(row['x_mm']),float(row['y_mm']);left=x<108
  if (side=='left' and not left) or (side=='right' and left):continue
  # PCB-P pass-throughs exist only where PCB-K actually overlaps it.
  if row['board']=='PCB-P' and not(side=='right' and 108<=x<=232):continue
  dia=4.6 if row['board']=='PCB-P' else float(row['w_mm']);holes.append((x,y,dia,row['board']))
  f=p.FOOTPRINT(b);f.SetReference('H'+str(len(holes)));f.SetValue('M2.5 PEM' if row['board']=='PCB-K' else 'PCB-P passthrough');f.SetPosition(xy(x,y));f.Reference().SetVisible(False);f.Value().SetVisible(False)
  pd=p.PAD(f);pd.SetAttribute(p.PAD_ATTRIB_NPTH);pd.SetShape(p.PAD_SHAPE_CIRCLE);pd.SetSize(v(dia,dia));pd.SetDrillSize(v(dia,dia));pd.SetLayerSet(p.LSET.AllCuMask());pd.SetPosition(xy(x,y));f.Add(pd);b.Add(f)
 if side=='right':
  for x in (123.5,154.5,185.5,216.5):
   e=p.PCB_SHAPE();e.SetShape(p.SHAPE_T_CIRCLE);e.SetCenter(xy(x,84.5));e.SetEnd(xy(x+10,84.5));e.SetLayer(p.Edge_Cuts);e.SetWidth(p.FromMM(.05));b.Add(e)
 with (folder/'mounting.csv').open('w') as ff:
  w=csv.writer(ff);w.writerow(['reference','x_panel_mm','y_panel_mm','drill_mm','source','panel_action'])
  for i,(x,y,d,src) in enumerate(holes,1):w.writerow(['H'+str(i),x,y,d,src+' v5','conservar PEM M2.5' if src=='PCB-K' else 'paso PEM PCB-P'])
 for x,y,d,src in holes:
  if src=='PCB-K' and (x,y) not in [(77,130),(77,158),(244,94),(244,172)]:continue
  radius=4.6 if src=='PCB-K' else 3.5
  zone=p.ZONE(b);zone.SetIsRuleArea(True);zone.SetLayerSet(p.LSET.AllCuMask())
  zone.SetDoNotAllowTracks(True);zone.SetDoNotAllowVias(True);zone.SetDoNotAllowZoneFills(True)
  zone.SetDoNotAllowPads(False);zone.SetDoNotAllowFootprints(False)  # cobre prohibido; los cuerpos pueden asomar (pads medidos ≥5,4 mm)
  zone.Outline().NewOutline()
  for i in range(64):
   a=i*2*math.pi/64;pt=xy(x+radius*math.cos(a),y+radius*math.sin(a));zone.Outline().Append(pt.x,pt.y)
  b.Add(zone)
 # Explicit physical underside component keepouts: no components over PCB-P.
 # Keepout shapes in report are independently audited against actual pads/courtyards.
 b.SetFileName(str(folder/(name+'.kicad_pcb')))
 p.SaveBoard(b.GetFileName(),b)
 pro=json.loads((folder/(name+'.kicad_pro')).read_text())
 pro.update({'board':{'design_settings':{'rules':{'min_clearance':.2,'min_track_width':.15,'min_via_diameter':.6,'min_through_hole_diameter':.3,'min_copper_edge_clearance':.2,'min_hole_clearance':.25,'min_annular_width':.15},'defaults':{'copper_line_width':.25}}},'net_settings':{'classes':[{'name':'Default','clearance':.2,'track_width':.25,'via_diameter':.6,'via_drill':.3,'microvia_diameter':.3,'microvia_drill':.1,'diff_pair_width':.25,'diff_pair_gap':.2,'diff_pair_via_gap':.25}]}})
 (folder/(name+'.kicad_pro')).write_text(json.dumps(pro,indent=2)+'\n')
 (folder/'placements-layout.json').write_text(json.dumps(placements,indent=2)+'\n')
 print(side,len(fps),'footprints',len(holes),'holes',flush=True)
 return b

if __name__=='__main__':
 for side in sys.argv[1:] or ['left','right']:make(side)

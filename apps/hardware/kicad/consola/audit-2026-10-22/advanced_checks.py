"""Read-only geometric audit. Run with KiCad Python. Distances are mm, not compliance tests."""
import json, math
from pathlib import Path
import pcbnew as k
O=Path(__file__).resolve().parent
mm=k.ToMM
xy=lambda p:tuple(mm(p))
def distance_point_segment(p,a,b):
 d=(b[0]-a[0],b[1]-a[1]); n=d[0]**2+d[1]**2
 t=max(0,min(1,((p[0]-a[0])*d[0]+(p[1]-a[1])*d[1])/n)) if n else 0
 return math.dist(p,(a[0]+t*d[0],a[1]+t*d[1]))
results={}; inventory=json.loads((O/'inspection.json').read_text())
for name in inventory:
 b=k.LoadBoard(str(O.parent/'boards'/name/(name+'.kicad_pcb')))
 fs={f.GetReference():f for f in b.GetFootprints()}
 for f in inventory[name]['footprints']:
  p=fs[f['ref']].GetFPID(); f['footprint']=str(p.GetLibNickname())+':'+str(p.GetLibItemName())
 vias=[t for t in b.GetTracks() if isinstance(t,k.PCB_VIA)]
 edges=[]
 for d in b.GetDrawings():
  if d.GetLayer()!=k.Edge_Cuts:continue
  a,z=xy(d.GetStart()),xy(d.GetEnd())
  if d.GetShape()==k.SHAPE_T_SEGMENT:edges.append((a,z))
  elif d.GetShape()==k.SHAPE_T_RECT:
   corners=[a,(a[0],z[1]),z,(z[0],a[1])]
   edges.extend(zip(corners,corners[1:]+corners[:1]))
 holes=[(f.GetReference(),xy(p.GetPosition()),mm(p.GetDrillSize().x)/2) for f in fs.values() if f.GetReference().startswith('H') for p in f.Pads()]
 data={'mounting_holes':holes,'capacitors':[],'via_center_in_smd_pad':[],'via_drill_overlap_smd_pad':[],'decoupling':[],'opposite_ground_gaps':[],'zone_settings':[]}
 for z in b.Zones():
  data['zone_settings'].append({'net':z.GetNetname(),'layer':b.GetLayerName(z.GetLayer()),'pad_connection':z.GetPadConnection(),'thermal_gap':mm(z.GetThermalReliefGap()),'spoke_width':mm(z.GetThermalReliefSpokeWidth())})
 for f in fs.values():
  if f.GetReference().startswith('C'):
   c=xy(f.GetPosition()); body=[]
   for item in f.GraphicalItems():
    if item.GetLayer() in [k.F_Fab,k.B_Fab] and isinstance(item,k.PCB_SHAPE):
     body.extend([xy(item.GetStart()),xy(item.GetEnd())])
   # Bodies are axis aligned at 0/90 degrees here. Corner distances capture nearest straight exterior edge.
   corners=body or [c]
   gap=min(distance_point_segment(p,a,z) for p in corners for a,z in edges)
   near=min(((math.dist(c,pos),ref,pos,r) for ref,pos,r in holes),default=None)
   data['capacitors'].append({'ref':f.GetReference(),'value':f.GetValue(),'xy':c,'angle':f.GetOrientationDegrees(),'body_corners':corners,'body_to_outer_edge':gap,'nearest_mount_center':near,'pads':[{'pin':p.GetNumber(),'net':p.GetNetname(),'zone_override':p.GetLocalZoneConnection()} for p in f.Pads()]})
  for p in f.Pads():
   if p.GetAttribute()!=k.PAD_ATTRIB_SMD:continue
   for v in vias:
    box=p.GetBoundingBox(); w,h=mm(box.GetSize()); vx,vy=xy(v.GetPosition());px,py=xy(p.GetPosition())
    radius=mm(p.GetRoundRectCornerRadius()) if p.GetShape()==k.PAD_SHAPE_ROUNDRECT else min(w,h)/2 if p.GetShape()==k.PAD_SHAPE_OVAL else 0
    qx,qy=abs(vx-px)-(w/2-radius),abs(vy-py)-(h/2-radius)
    sd=math.hypot(max(qx,0),max(qy,0))+min(max(qx,qy),0)-radius
    if sd<mm(v.GetDrill())/2:
     data['via_drill_overlap_smd_pad'].append({'ref':f.GetReference(),'pad':p.GetNumber(),'net':p.GetNetname(),'xy':xy(v.GetPosition()),'drill':mm(v.GetDrill()),'center_inside':sd<=0,'drill_overlap_depth_mm':mm(v.GetDrill())/2-sd})
    if p.HitTest(v.GetPosition()):
     data['via_center_in_smd_pad'].append({'ref':f.GetReference(),'pad':p.GetNumber(),'net':p.GetNetname(),'xy':xy(v.GetPosition()),'drill':mm(v.GetDrill())})
 for u,c in ([('U1','C1')] if name=='pcb-a' else [('U1','C1'),('U2','C2')] if name=='pcb-k1' else []):
  up=next(p for p in fs[u].Pads() if p.GetNumber()=='9'); cp=next(p for p in fs[c].Pads() if p.GetNetname()=='/+3V3')
  gp=next(p for p in fs[c].Pads() if p.GetNetname()=='/GND')
  nv=min((math.dist(xy(v.GetPosition()),xy(gp.GetPosition())),xy(v.GetPosition())) for v in vias if v.GetNetname()=='/GND')
  data['decoupling'].append({'ic':u,'cap':c,'vdd_to_cap_pad_straight':math.dist(xy(up.GetPosition()),xy(cp.GetPosition())),'cap_ground_to_nearest_ground_via_center':nv})
 fills={layer:[z.GetFilledPolysList(layer) for z in b.Zones() if z.GetNetname()=='/GND' and z.GetLayer()==layer] for layer in [k.F_Cu,k.B_Cu]}
 for t in b.GetTracks():
  if isinstance(t,k.PCB_VIA) or t.GetNetname() in ['/GND','/+3V3','/+5V']:continue
  length=mm(t.GetLength()); steps=max(1,math.ceil(length/.2));a=t.GetStart();e=t.GetEnd();other=k.B_Cu if t.GetLayer()==k.F_Cu else k.F_Cu
  run=0; maxrun=0;start=None;best=None
  for i in range(steps+1):
   pt=k.VECTOR2I(round(a.x+(e.x-a.x)*i/steps),round(a.y+(e.y-a.y)*i/steps))
   ground=any(p.Contains(pt) for p in fills[other])
   if ground:run=0;start=None
   else:
    if start is None:start=xy(pt)
    run+=1
    if run>maxrun:maxrun=run;best=(start,xy(pt))
  gap=max(0,maxrun-1)*length/steps
  if gap>2:data['opposite_ground_gaps'].append({'net':t.GetNetname(),'track_layer':b.GetLayerName(t.GetLayer()),'max_gap_mm_approx':gap,'interval':best})
 data['opposite_ground_gaps'].sort(key=lambda x:x['max_gap_mm_approx'],reverse=True)
 results[name]=data
 print(name,'via-in-SMD',data['via_center_in_smd_pad'],'decoupling',data['decoupling'],'largest ground gaps',data['opposite_ground_gaps'][:5])
 print('caps closest edge',[(c['ref'],round(c['body_to_outer_edge'],3)) for c in sorted(data['capacitors'],key=lambda x:x['body_to_outer_edge'])[:8]])
(O/'advanced-checks.json').write_text(json.dumps(results,indent=2));(O/'inspection.json').write_text(json.dumps(inventory,indent=2))

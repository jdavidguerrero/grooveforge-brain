import pcbnew as p,json,math,heapq
from pathlib import Path
root=Path('apps/hardware/kicad/studio');out=Path(__file__).resolve().parent
def xy(v):return [round(p.ToMM(v.x),5),round(p.ToMM(v.y),5)]
def dist(a,b):return math.dist(a,b)
def pt(a):return p.VECTOR2I(round(a[0]*1e6),round(a[1]*1e6))
allres={}
for name in ['portadora','pcb-a','pcb-k']:
 b=p.LoadBoard(str(root/name/(name+'.kicad_pcb')));fps={f.GetReference():f for f in b.GetFootprints()};tracks=list(b.GetTracks());vias=[t for t in tracks if t.GetClass()=='PCB_VIA'];edges=[x for x in b.GetDrawings() if x.GetLayer()==p.Edge_Cuts];circles=[(xy(e.GetCenter()),p.ToMM(e.GetRadius())) for e in edges if e.GetShape()==p.SHAPE_T_CIRCLE]
 res={'fiducials':[r for r,f in fps.items() if 'fiducial' in str(f.GetFPID()).lower() or r.startswith('FID')], 'zone_settings':[{'layer':b.GetLayerName(z.GetLayer()),'net':z.GetNetname(),'pad_connection':z.GetPadConnection(),'thermal_gap':p.ToMM(z.GetThermalReliefGap()),'spoke_width':p.ToMM(z.GetThermalReliefSpokeWidth())} for z in b.Zones()], 'caps':{},'via_in_smd_pad':[]}
 for ref,f in fps.items():
  pads=list(f.Pads())
  for pad in pads:
   if pad.GetAttribute()==p.PAD_ATTRIB_SMD:
    for v in vias:
     if pad.HitTest(v.GetPosition()):res['via_in_smd_pad'].append({'ref':ref,'pad':pad.GetNumber(),'net':pad.GetNetname(),'via_at':xy(v.GetPosition()),'drill':p.ToMM(v.GetDrillValue())})
  if ref.startswith('C'):
   loc=xy(f.GetPosition());res['caps'][ref]={'value':f.GetValue(),'at':loc,'rot':f.GetOrientationDegrees(),'pads':[{'num':a.GetNumber(),'at':xy(a.GetPosition()),'net':a.GetNetname(),'connection_override':a.GetLocalZoneConnection()} for a in pads], 'nearest_circle':min([{'center':c,'radius':r,'center_to_rim_mm':dist(loc,c)-r} for c,r in circles],key=lambda x:x['center_to_rim_mm'],default=None)}
 if name=='portadora':
  def path(net,a,bb):
   g={}
   for t in tracks:
    if t.GetNetname()!=net or t.GetClass()=='PCB_VIA':continue
    s=tuple(xy(t.GetStart()));e=tuple(xy(t.GetEnd()));length=p.ToMM(t.GetLength());g.setdefault(s,[]).append((e,length));g.setdefault(e,[]).append((s,length))
   pq=[(0,tuple(a))];seen=set()
   while pq:
    l,n=heapq.heappop(pq)
    if n==tuple(bb):return l
    if n in seen:continue
    seen.add(n)
    for nxt,dl in g.get(n,[]):heapq.heappush(pq,(l+dl,nxt))
  def pad(ref,num):return next(a for a in fps[ref].Pads() if a.GetNumber()==num)
  res['decouple']={'U2.4_to_C7.1_trace_mm':path('/+3V3',xy(pad('U2','4').GetPosition()),xy(pad('C7','1').GetPosition()))}
  res['smd_ground_connections']={r:[(a.GetNumber(),a.GetLocalZoneConnection()) for a in f.Pads() if a.GetNetname()=='/GND'] for r,f in fps.items() if any(a.GetAttribute()==p.PAD_ATTRIB_SMD for a in f.Pads())}
  res['reference_coverage']={}
  for net in ['/I2S_MCLK','/I2S_BCLK','/I2S_LRCLK','/I2S_RX','/I2S_TX']:
   gaps=[];length=0;missing=0;vp=[]
   for t in tracks:
    if t.GetNetname()!=net:continue
    if t.GetClass()=='PCB_VIA':
     a=xy(t.GetPosition());near=sorted([(dist(a,xy(v.GetPosition())),xy(v.GetPosition())) for v in vias if v.GetNetname()=='/GND']);vp.append({'at':a,'nearest_GND_via':near[0] if near else None});continue
    a=xy(t.GetStart());bb=xy(t.GetEnd());ll=p.ToMM(t.GetLength());other=p.B_Cu if t.GetLayer()==p.F_Cu else p.F_Cu;polys=[z.GetFilledPolysList(other) for z in b.Zones() if z.GetLayer()==other and z.GetNetname()=='/GND'];n=max(1,math.ceil(ll/0.1));length+=ll;run=[]
    for i in range(n):
     loc=[a[j]+(bb[j]-a[j])*(i+.5)/n for j in [0,1]];present=any(z.Contains(pt(loc)) for z in polys)
     if not present:missing+=ll/n;run.append(loc)
     elif run:
      if len(run)*ll/n>=.3:gaps.append({'from':run[0],'to':run[-1],'mm':len(run)*ll/n,'reference_layer':b.GetLayerName(other)})
      run=[]
    if run and len(run)*ll/n>=.3:gaps.append({'from':run[0],'to':run[-1],'mm':len(run)*ll/n,'reference_layer':b.GetLayerName(other)})
   res['reference_coverage'][net]={'length_mm':length,'no_opposite_GND_zone_mm':missing,'gaps_over_0_3mm':gaps,'signal_vias':vp}
 allres[name]=res
(out/'measurements.json').write_text(json.dumps(allres,indent=2))
for n,r in allres.items():
 print(n,'fiducials',r['fiducials'],'zones',r['zone_settings'],'via-in-pad',r['via_in_smd_pad'])
 print('caps closest hole',sorted([(k,round(v['nearest_circle']['center_to_rim_mm'],2),v['nearest_circle']['center'],v['rot']) for k,v in r['caps'].items() if v['nearest_circle']],key=lambda x:x[1])[:8])
 if n=='portadora':print('decouple',r['decouple'],'ground',r['smd_ground_connections'],'coverage',r['reference_coverage'])

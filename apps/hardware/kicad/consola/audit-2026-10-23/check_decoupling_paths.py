"""Trace-only shortest path from VDD pad to bypass pad; does not model ground-plane impedance."""
from pathlib import Path
import pcbnew as k
import math,json,heapq
O=Path(__file__).resolve().parent
results=[]
for name,pairs in [('pcb-a',[('U1','C1')]),('pcb-k1',[('U1','C1'),('U2','C2')])]:
 b=k.LoadBoard(str(O.parent/'boards'/name/(name+'.kicad_pcb')));g={}
 def node(p,layer):return (p.x,p.y,layer)
 def edge(a,z,d):g.setdefault(a,[]).append((z,d));g.setdefault(z,[]).append((a,d))
 for t in b.GetTracks():
  if t.GetNetname()!='/+3V3':continue
  if isinstance(t,k.PCB_VIA):edge(node(t.GetPosition(),k.F_Cu),node(t.GetPosition(),k.B_Cu),0)
  else:edge(node(t.GetStart(),t.GetLayer()),node(t.GetEnd(),t.GetLayer()),k.ToMM(t.GetLength()))
 # Endpoint connections within each copper land. Existing tracks terminate at their junctions.
 terminals={}
 for f in b.GetFootprints():
  for p in f.Pads():
   if p.GetNetname()!='/+3V3':continue
   pn=(f.GetReference(),p.GetNumber());terminals[pn]=pn
   for n in list(g):
    if len(n)!=3:continue
    if p.IsOnLayer(n[2]) and p.HitTest(k.VECTOR2I(n[0],n[1])):
     edge(pn,n,k.ToMM(math.hypot(n[0]-p.GetPosition().x,n[1]-p.GetPosition().y)))
 for u,c in pairs:
  start=(u,'9');target=(c,'1');dist={start:0};previous={};queue=[(0,0,start)];i=0
  while queue:
   d,_,n=heapq.heappop(queue)
   if d>dist[n]:continue
   for v,w in g.get(n,[]):
    nd=d+w
    if nd<dist.get(v,float('inf')):
     dist[v]=nd;previous[v]=n;i+=1;heapq.heappush(queue,(nd,i,v))
  path=[];at=target
  while at in previous:
   path.append(at);at=previous[at]
  path.append(start);path.reverse()
  results.append({'path':path,'board':name,'ic':u,'cap':c,'trace_path_mm':dist.get(target)})
print(json.dumps(results,indent=2));(O/'decoupling-paths.json').write_text(json.dumps(results,indent=2))

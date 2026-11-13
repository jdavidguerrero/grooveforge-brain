#!/usr/bin/env python3
"""Acabado de K: metadatos, reservas, planos GND y limpieza de vías redundantes."""
from build_k import *

def finish(side):
 folder=CON/'boards'/('pcb-k-'+side);path=folder/(folder.name+'.kicad_pcb')
 b=p.LoadBoard(str(path))
 root=ET.parse(folder/'docs'/(folder.name+'-netlist.xml')).getroot()
 comps={c.attrib['ref']:c for c in root.findall('./components/comp')}
 for f in b.GetFootprints():
  ref=f.GetReference()
  if ref in comps:
   c=comps[ref];attrs=f.GetAttributes()&~(p.FP_EXCLUDE_FROM_BOM|p.FP_DNP)  # el esquema manda
   props={prop.attrib['name'] for prop in c.findall('property')}
   if 'dnp' in props:attrs|=p.FP_DNP
   if 'exclude_from_bom' in props:attrs|=p.FP_EXCLUDE_FROM_BOM
   f.SetAttributes(attrs)
   for field in c.findall('./fields/field'):
    f.SetField(field.attrib['name'],field.text or '')
    f.GetField(field.attrib['name']).SetVisible(False)
  else:f.SetAttributes(p.FP_BOARD_ONLY|p.FP_EXCLUDE_FROM_BOM|p.FP_EXCLUDE_FROM_POS_FILES)
  # Advertencia de compra en documentación de fabricación, no encima de otras piezas.
  for item in f.GraphicalItems():
   if isinstance(item,p.PCB_TEXT) and item.GetText()=='PADS 1/2 PROVISIONALES':item.SetLayer(p.B_Fab)
 if side=='left':
  # No mover el zócalo: su cobre acaba en x_panel=107,575. Borde a 108,2 => 0,625 mm.
  for item in b.GetDrawings():
   if item.GetLayer()!=p.Edge_Cuts or not isinstance(item,p.PCB_SHAPE):continue
   for get,setter in [(item.GetStart,item.SetStart),(item.GetEnd,item.SetEnd)]:
    pt=get()
    if abs(p.ToMM(pt.x)-64)<.0001:setter(v(64.2,p.ToMM(pt.y)))
 # Vías idénticas producidas al unir ramas de una red no constituyen taladros adicionales.
 seen=set()
 for t in list(b.GetTracks()):
  if not isinstance(t,p.PCB_VIA):continue
  k=(t.GetPosition().x,t.GetPosition().y,t.GetNetname())
  if k in seen:b.Remove(t)
  else:seen.add(k)
 for z in list(b.Zones()):
  if not z.GetIsRuleArea():b.Remove(z)
 # Conectores: GND macizo (unión mecánica y evita térmicos con un solo radio).
 for fp in b.GetFootprints():
  if fp.GetReference().startswith('J'):
   for pd in fp.Pads():
    if pd.GetNetname()=='GND':pd.SetLocalZoneConnection(p.ZONE_CONNECTION_FULL)
 outline=p.SHAPE_POLY_SET();outline.NewOutline()
 for x,y in POLYS[side]:
  pt=xy(108.2 if side=='left' and x==108 else x,y);outline.Append(pt.x,pt.y)
 outline.Inflate(-p.FromMM(.5),p.CORNER_STRATEGY_CHAMFER_ALL_CORNERS,p.FromMM(.01))
 for layer in [p.F_Cu,p.B_Cu]:
  zone=p.ZONE(b);zone.SetLayer(layer);zone.SetNet(b.FindNet('GND'))
  zone.SetLocalClearance(p.FromMM(.2));zone.SetMinThickness(p.FromMM(.15))
  zone.SetPadConnection(p.ZONE_CONNECTION_THERMAL);zone.SetThermalReliefGap(p.FromMM(.25));zone.SetThermalReliefSpokeWidth(p.FromMM(.25))
  zone.SetIslandRemovalMode(p.ISLAND_REMOVAL_MODE_ALWAYS)
  zone.Outline().NewOutline();chain=outline.Outline(0)
  for i in range(chain.PointCount()):pt=chain.CPoint(i);zone.Outline().Append(pt.x,pt.y)
  b.Add(zone)
 b.BuildConnectivity();filler=p.ZONE_FILLER(b);ok=filler.Fill(b.Zones());assert ok
 # Vías de unión GND: solo donde el disco de la vía cabe completo en el relleno de ambas caras.
 gz=[z for z in b.Zones() if not z.GetIsRuleArea()];gnd=b.FindNet('GND').GetNetCode();added=0
 bb=b.GetBoardEdgesBoundingBox();x0,x1,y0,y1=p.ToMM(bb.GetLeft()),p.ToMM(bb.GetRight()),p.ToMM(bb.GetTop()),p.ToMM(bb.GetBottom())
 padboxes=[]
 for fp in b.GetFootprints():
  for pd in fp.Pads():
   q=pd.GetBoundingBox();padboxes.append((p.ToMM(q.GetLeft())-.6,p.ToMM(q.GetRight())+.6,p.ToMM(q.GetTop())-.6,p.ToMM(q.GetBottom())+.6))
 def inside(x,y):
  if any(a<=x<=c and d<=y<=e for a,c,d,e in padboxes):return False
  pts=[(x,y)]+[(x+.36*math.cos(a*math.pi/4),y+.36*math.sin(a*math.pi/4)) for a in range(8)]
  return all(z.HitTestFilledArea(z.GetLayer(),v(*q),0) for z in gz for q in pts)
 yy=y0+1
 while yy<y1:
  xx=x0+1
  while xx<x1:
   if inside(xx,yy):
    t=p.PCB_VIA(b);t.SetPosition(v(xx,yy));t.SetWidth(p.FromMM(.6));t.SetDrill(p.FromMM(.3));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(p.F_Cu,p.B_Cu);t.SetNetCode(gnd);b.Add(t);added+=1
   xx+=5.0
  yy+=5.0
 b.BuildConnectivity();ok=p.ZONE_FILLER(b).Fill(b.Zones());assert ok
 # Unir islas restantes: union-find de islas F/B, pads GND y vías; añadir vías en rejilla fina.
 def islands():
  out=[]
  for z in gz:
   ps=z.GetFilledPolysList(z.GetLayer())
   for i in range(ps.OutlineCount()):
    one=p.SHAPE_POLY_SET();one.AddOutline(ps.Outline(i))
    for h in range(ps.HoleCount(i)):one.AddHole(ps.Hole(i,h))
    out.append((z.GetLayer(),one))
  return out
 def comps():
  isl=islands();par=list(range(len(isl)))
  def fnd(a):
   while par[a]!=a:par[a]=par[par[a]];a=par[a]
   return a
  def join(a,c):par[fnd(a)]=fnd(c)
  def at(layer,pt):return [k for k,(l,s) in enumerate(isl) if l==layer and s.Contains(pt)]
  for t in b.GetTracks():
   if t.GetNetCode()!=gnd:continue
   if isinstance(t,p.PCB_VIA):ks=at(p.F_Cu,t.GetPosition())+at(p.B_Cu,t.GetPosition())
   else:ks=at(t.GetLayer(),t.GetStart())+at(t.GetLayer(),t.GetEnd())
   for k in ks[1:]:join(ks[0],k)
  for fp in b.GetFootprints():
   for pd in fp.Pads():
    if pd.GetNetCode()!=gnd:continue
    ks=[k for L in (p.F_Cu,p.B_Cu) if pd.IsOnLayer(L) for k in at(L,pd.GetPosition())]
    for k in ks[1:]:join(ks[0],k)
  groups={}
  for k in range(len(isl)):groups.setdefault(fnd(k),[]).append(k)
  return isl,fnd,groups
 for rnd in range(40):
  isl,fnd,groups=comps()
  if len(groups)<=1:break
  main=max(groups.values(),key=lambda g:sum(isl[k][1].Area() for k in g));mainroot=fnd(main[0])
  done=False
  for g in sorted(groups.values(),key=len):
   if fnd(g[0])==mainroot:continue
   for k in g:
    q=isl[k][1].BBox();xa,xb,ya,yb=p.ToMM(q.GetLeft()),p.ToMM(q.GetRight()),p.ToMM(q.GetTop()),p.ToMM(q.GetBottom())
    yy=ya
    while yy<=yb and not done:
     xx=xa
     while xx<=xb and not done:
      if inside(xx,yy):
       other=[j for j,(l,s) in enumerate(isl) if l!=isl[k][0] and s.Contains(v(xx,yy))]
       if isl[k][1].Contains(v(xx,yy)) and any(fnd(j)!=fnd(k) for j in other):
        t=p.PCB_VIA(b);t.SetPosition(v(xx,yy));t.SetWidth(p.FromMM(.6));t.SetDrill(p.FromMM(.3));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(p.F_Cu,p.B_Cu);t.SetNetCode(gnd);b.Add(t);added+=1;done=True
      xx+=.5
     yy+=.5
    if done:break
   if done:break
  if not done:print(side,'islas sin vía posible',len(groups)-1,flush=True);break
  b.BuildConnectivity();ok=p.ZONE_FILLER(b).Fill(b.Zones());assert ok
 print(side,'vías de unión GND',added,'grupos',len(comps()[2]),flush=True)
 # Limpieza: vías de señal que solo tocan una capa y vías GND casi superpuestas (>=0,85 mm entre centros).
 tr=[t for t in b.GetTracks() if not isinstance(t,p.PCB_VIA)];removed=0
 def ends(net,pos,layer):return sum(1 for t in tr if t.GetNetCode()==net and t.GetLayer()==layer and (t.GetStart()==pos or t.GetEnd()==pos))
 kept=[]
 for t in list(b.GetTracks()):
  if not isinstance(t,p.PCB_VIA):continue
  pos=t.GetPosition()
  if t.GetNetCode()!=gnd:
   if ends(t.GetNetCode(),pos,p.F_Cu)==0 or ends(t.GetNetCode(),pos,p.B_Cu)==0:b.Remove(t);removed+=1
   continue
  if any(math.hypot(p.ToMM(pos.x-k.x),p.ToMM(pos.y-k.y))<.85 for k in kept):b.Remove(t);removed+=1;continue
  kept.append(pos)
 b.BuildConnectivity();ok=p.ZONE_FILLER(b).Fill(b.Zones());assert ok
 print(side,'vías retiradas',removed,flush=True)
 p.SaveBoard(str(path),b)
 print(side,'GND rellenado',len(b.GetTracks()),'pistas/vías',flush=True)

if __name__=='__main__':
 for side in sys.argv[1:] or ['left','right']:finish(side)

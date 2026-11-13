#!/usr/bin/env python3
"""Export conservative routing geometry and import independently checked tracks."""
from build_k import *
from PIL import Image,ImageDraw,ImageFilter
import subprocess

def prepare(side):
 name='pcb-k-'+side;folder=CON/'boards'/name;b=p.LoadBoard(str(folder/(name+'.kicad_pcb')))
 poly=[(x-44,371-y) for x,y in POLYS[side]];ox=min(x for x,y in poly)-1;oy=min(y for x,y in poly)-1
 W=round((max(x for x,y in poly)-ox+1)/S)+1;H=round((max(y for x,y in poly)-oy+1)/S)+1
 def px(x,y):return ((x-ox)/S,(y-oy)/S)
 mask=Image.new('L',(W,H));d=ImageDraw.Draw(mask);d.polygon([px(x,y) for x,y in poly],fill=255)
 # 0.65 mm erosion accommodates the 0.8 mm rail and grid error.
 for _ in range(13):mask=mask.filter(ImageFilter.MinFilter(3))
 d=ImageDraw.Draw(mask)
 for z in b.Zones():
  if z.GetIsRuleArea():
   expanded=p.SHAPE_POLY_SET(z.Outline());expanded.Inflate(p.FromMM(.45),p.CORNER_STRATEGY_ROUND_ALL_CORNERS,p.FromMM(.01));chain=expanded.Outline(0);d.polygon([px(*mm(chain.CPoint(i))) for i in range(chain.PointCount())],fill=0)
 shapes=[];pads={};meta={};fixed=[];drills=[];uesc=set()
 for f in b.GetFootprints():
  ref=f.GetReference()
  for pad in f.Pads():
   x,y=mm(pad.GetPosition());sx,sy=mm(pad.GetSize());ang=pad.GetOrientationDegrees()%180
   if abs(ang-90)<.01:sx,sy=sy,sx
   if pad.GetAttribute()==p.PAD_ATTRIB_NPTH:
    rr=sx/2+.8;d.ellipse([px(x-rr,y-rr),px(x+rr,y+rr)],fill=0);continue
   if pad.GetAttribute()==p.PAD_ATTRIB_PTH:drills.append((x,y,p.ToMM(pad.GetDrillSize().x)/2))
   net=pad.GetNetCode();l=2 if pad.GetAttribute()==p.PAD_ATTRIB_PTH else (0 if pad.IsOnLayer(p.F_Cu) else 1)
   rr=min(sx,sy)/2 if pad.GetShape() in (p.PAD_SHAPE_CIRCLE,p.PAD_SHAPE_OVAL) else 0
   shapes.append((net,l,x,y,sx/2,sy/2,rr))
   if net>0:
    if ref.startswith('U') and not pad.GetNetname().startswith('unconnected-'):
     cx,cy=mm(f.GetPosition());pin=int(pad.GetNumber());endx=cx+(-1 if x<cx else 1)*(6+(pin%2)*1.)
     fixed.append(('T',net,.15,l,x,y,endx,y));fixed.append(('V',net,endx,y))
     pads.setdefault(net,[]).append((endx,y,2));uesc.add((round(endx,4),round(y,4)))
    else:pads.setdefault(net,[]).append((x,y,l))
  if ref.startswith('D'):
   x,y=mm(f.GetPosition());d.rectangle([px(x-1.8-.65,y-1.6-.65),px(x+1.8+.65,y+1.6+.65)],fill=0)
 if side=='right':
  for xx in (123.5,154.5,185.5,216.5):
   x,y=xx-44,371-84.5;r=10.65;d.ellipse([px(x-r,y-r),px(x+r,y+r)],fill=0)
 maskpath=folder/'routing-mask.bin';maskpath.write_bytes(mask.tobytes())
 nets={n.GetNetCode():n.GetNetname() for n in b.GetNetsByNetcode().values()}
 def priority(item):
  k,pts=item;name=nets[k]
  if name=='+5V_LED':return (0,0)
  if name=='GND':return (3,0)
  # Critical I2C and power before ordinary key and diagnostic nets.
  if name in ['+3V3','SDA','SCL']:return (1,0)
  return (2,min(math.dist(a[:2],bb[:2]) for a in pts for bb in pts if a!=bb) if len(pts)>1 else 0)
 connections=[]
 gndc=[]
 for net,pts in pads.items():
  if nets[net]!='GND':continue
  # SMD GND pads: short spur to nearest through-hole GND (key pad) so no fill island is left alone.
  th=[q for q in pts if q[2]==2]
  for q in pts:
   if q[2]==2 or not th:continue
   tgt=min(th,key=lambda t:math.dist(t[:2],q[:2]))
   gndc.append((net,.25,*q,*tgt))
  # IC GND escapes must also reach a key's through-hole GND, not only their own decoupling.
  keys=[t for t in th if (round(t[0],4),round(t[1],4)) not in uesc]
  for q in th:
   if (round(q[0],4),round(q[1],4)) in uesc and keys:
    tgt=min(keys,key=lambda t:math.dist(t[:2],q[:2]));gndc.append((net,.25,*q,*tgt))
 for net,pts in sorted(pads.items(),key=priority):
  if len(pts)<2 or nets[net]=='GND':continue
  # Prim tree spans every physical pad, including duplicate switch numbers.
  reached=[pts[0]];todo=pts[1:]
  while todo:
   _,i,a=min((math.dist(a[:2],bb[:2]),i,a) for a in reached for i,bb in enumerate(todo))
   bb=todo.pop(i);reached.append(bb)
   connections.append((net,.8 if nets[net]=='+5V_LED' else .15,*a,*bb))
 connections+=sorted(gndc,key=lambda c:math.dist(c[2:4],c[5:7]))
 inp=folder/'routing-input.txt'
 with inp.open('w') as ff:
  ff.write(f'{W} {H} {S} {ox} {oy}\n{maskpath}\n{len(shapes)}\n')
  for row in shapes:ff.write(' '.join(map(str,row))+'\n')
  ff.write(str(len(drills))+'\n')
  for row in drills:ff.write(' '.join(map(str,row))+'\n')
  ff.write(str(len(fixed))+'\n')
  for row in fixed:ff.write(' '.join(map(str,row))+'\n')
  ff.write(str(len(connections))+'\n')
  for row in connections:ff.write(' '.join(map(str,row))+'\n')
 return inp

def import_routes(side):
 folder=CON/'boards'/('pcb-k-'+side);path=folder/(folder.name+'.kicad_pcb');b=p.LoadBoard(str(path))
 for t in list(b.GetTracks()):b.Remove(t)
 fails=[]
 for line in (folder/'routing-output.txt').read_text().splitlines():
  fields=line.split()
  if fields[0]=='FAIL':fails.append(line);continue
  net=int(fields[1])
  if fields[0]=='T':
   width,l,ax,ay,bx,by=map(float,fields[2:]);t=p.PCB_TRACK(b);t.SetStart(v(ax,ay));t.SetEnd(v(bx,by));t.SetWidth(p.FromMM(width));t.SetLayer(p.F_Cu if l==0 else p.B_Cu)
  else:
   x,y=map(float,fields[2:]);t=p.PCB_VIA(b);t.SetPosition(v(x,y));t.SetWidth(p.FromMM(.6));t.SetDrill(p.FromMM(.3));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(p.F_Cu,p.B_Cu)
  t.SetNetCode(net);b.Add(t)
 p.SaveBoard(str(path),b);print(side,'importado, failed:',len(fails))

if __name__=='__main__':
 action=sys.argv[1];sides=sys.argv[2:] or ['left','right']
 for side in sides:
  if action=='prepare':print(prepare(side))
  elif action=='import':import_routes(side)

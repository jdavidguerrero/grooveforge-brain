exec(open('/tmp/grooveforge-pcb-delta-20261022/measure.py').read().split('allres={}')[0])
b=p.LoadBoard(str(root/'portadora/portadora.kicad_pcb'));tracks=list(b.GetTracks());fps={f.GetReference():f for f in b.GetFootprints()}
def sd(v,a,c):
 d=[c[j]-a[j] for j in [0,1]];ll=sum(x*x for x in d);t=max(0,min(1,sum((v[j]-a[j])*d[j] for j in [0,1])/ll)) if ll else 0;return dist(v,[a[j]+t*d[j] for j in [0,1]])
for loc in [[118,106.717],[126,105.4],[117.779,106.065]]:
 print('location',loc,'opposite tracks',sorted([(sd(loc,xy(t.GetStart()),xy(t.GetEnd())),t.GetNetname(),xy(t.GetStart()),xy(t.GetEnd())) for t in tracks if t.GetClass()!='PCB_VIA' and t.GetLayer()==p.F_Cu])[:3]);print('nearest GNDpad',sorted([(dist(loc,xy(a.GetPosition())),r,a.GetNumber(),xy(a.GetPosition())) for r,f in fps.items() for a in f.Pads() if a.GetNetname()=='/GND'])[:2])
for ref in ['J10','J6','U1','C2','U2','C7','C4','D1','F1','J2','J1','J5']:
 f=fps[ref];print(ref,[(a.GetNumber(),a.GetNetname(),xy(a.GetPosition()),xy(a.GetSize()),xy(a.GetDrillSize())) for a in f.Pads() if ref not in ['J1','J2','J5'] or a.GetNetname()=='/ENCR_A'])
print('pad connection methods', [x for x in dir(fps['C2']) if 'Zone' in x])

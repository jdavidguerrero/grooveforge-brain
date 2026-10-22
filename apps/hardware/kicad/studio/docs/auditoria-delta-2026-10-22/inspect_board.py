import pcbnew,json,collections
from pathlib import Path
r=Path('apps/hardware/kicad/studio');out=Path(__file__).resolve().parent
def v(p):return [round(pcbnew.ToMM(p.x),5),round(pcbnew.ToMM(p.y),5)]
for n in ['portadora','pcb-a','pcb-k']:
 b=pcbnew.LoadBoard(str(r/n/f'{n}.kicad_pcb'));d={'footprints':{},'tracks':[],'zones':[]}
 for f in b.GetFootprints():
  d['footprints'][f.GetReference()]={'value':f.GetValue(),'lib':str(f.GetFPID().GetLibNickname())+":"+str(f.GetFPID().GetLibItemName()),'at':v(f.GetPosition()),'rot':f.GetOrientationDegrees(),'side':b.GetLayerName(f.GetLayer()),'lcsc':f.GetFieldText('LCSC') if f.HasField('LCSC') else '', 'pads':[{'num':p.GetNumber(),'net':p.GetNetname(),'at':v(p.GetPosition()),'size':v(p.GetSize()),'drill':v(p.GetDrillSize())} for p in f.Pads()]}
 for t in b.GetTracks():
  d['tracks'].append({'net':t.GetNetname(),'start':v(t.GetStart()),'end':v(t.GetEnd()),'width':(pcbnew.ToMM(t.GetWidth()) if t.GetClass() != 'PCB_VIA' else None),'length':pcbnew.ToMM(t.GetLength()),'layer':b.GetLayerName(t.GetLayer()),'type':t.GetClass()})
 for z in b.Zones():d['zones'].append({'net':z.GetNetname(),'layer':b.GetLayerName(z.GetLayer()),'clearance':pcbnew.ToMM(z.GetLocalClearance()),'filled':z.IsFilled()})
 bb=b.GetBoardEdgesBoundingBox();d['bounds']={'pos':v(bb.GetPosition()),'size':v(bb.GetSize())}
 (out/f'{n}-board.json').write_text(json.dumps(d,indent=2))
 print(n,'bounds',d['bounds'],'fp',len(d['footprints']),'tracks',len(d['tracks']),'zones',d['zones'])
 if n=='portadora':
  for net in ['/VBUS_IN','/+5V','/+3V3','/GND','/I2S_MCLK','/I2S_BCLK','/I2S_LRCLK','/I2S_RX','/I2S_TX']:
   ts=[t for t in d['tracks'] if t['net']==net];print(net,'widths',collections.Counter(t['width'] for t in ts if t['type']!='PCB_VIA'),'length',round(sum(t['length'] for t in ts if t['type']!='PCB_VIA'),2),'vias',sum(t['type']=='PCB_VIA' for t in ts))
  for ref in ['J1','J2','J15','J16','J13','J14','JP1']:print(ref,d['footprints'][ref])

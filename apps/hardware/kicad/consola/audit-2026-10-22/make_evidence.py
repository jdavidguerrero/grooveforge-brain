import csv,json,hashlib,html
from pathlib import Path
import pcbnew as k
O=Path(__file__).resolve().parent
j=json.loads((O/'advanced-checks.json').read_text())
with (O/'via-in-pad.csv').open('w') as out:
 w=csv.writer(out);w.writerow(['board','reference','pad','net','x_mm','y_mm','drill_mm'])
 for board,d in j.items():
  for v in d['via_center_in_smd_pad']:w.writerow([board,v['ref'],v['pad'],v['net'],*v['xy'],v['drill']])
parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1120" height="430" viewBox="0 0 1120 430"><rect width="1120" height="430" fill="#f8fafc"/><g font-family="Arial,sans-serif" fill="#172033"><text x="30" y="35" font-size="23" font-weight="bold">Vías dentro de pads de ensamblaje: tres ejemplos medidos</text><text x="30" y="62" font-size="14">28 centros dentro de pads + 11 invasiones de borde. Taladro rojo: Ø 0,30 mm.</text>']
for i,(board,ref,pin) in enumerate([('pcb-a','C24','1'),('pcb-k1','U2','4'),('pcb-k1','J1','6')]):
 b=k.LoadBoard(str(O.parent/'boards'/board/(board+'.kicad_pcb')))
 f=next(f for f in b.GetFootprints() if f.GetReference()==ref);p=next(p for p in f.Pads() if p.GetNumber()==pin)
 v=next(v for v in j[board]['via_center_in_smd_pad'] if v['ref']==ref and v['pad']==pin)
 box=p.GetBoundingBox();left=30+i*365;cx=left+165;cy=235;scale=90;px,py=k.ToMM(p.GetPosition());width,height=k.ToMM(box.GetSize())
 parts.append(f'<rect x="{left}" y="85" width="345" height="290" rx="12" fill="white" stroke="#cbd5e1"/><text x="{left+18}" y="115" font-size="18" font-weight="bold">{board.upper()} · {ref}.{pin}</text><text x="{left+18}" y="139" font-size="14">{html.escape(p.GetNetname())}</text><rect x="{cx-width*scale/2}" y="{cy-height*scale/2}" width="{width*scale}" height="{height*scale}" rx="8" fill="#edba58" stroke="#946716"/><circle cx="{cx+(v["xy"][0]-px)*scale}" cy="{cy+(v["xy"][1]-py)*scale}" r="{v["drill"]*scale/2}" fill="#d52e37"/><text x="{left+18}" y="350" font-size="13">Pad centro: ({px:.3f}; {py:.3f}) mm</text>')
parts.append('<text x="30" y="408" font-size="14">Vista en coordenadas CAD, sin espejo. Pad: envolvente de cobre; la apertura de pasta/máscara lo incluye.</text></g></svg>')
(O/'via-in-pad-evidence.svg').write_text(''.join(parts))
# Remove only the failed download created by this audit; it is HTML, not a datasheet.
p=O/'datasheets'/'SK6812MINI-EA.pdf'
if p.exists() and not p.read_bytes().startswith(b'%PDF'):p.unlink()
issues=[]
for name,d in json.loads((O/'inspection.json').read_text()).items():
 for path,h in d['hashes'].items():
  p=O.parent/'boards'/path
  if hashlib.sha256(p.read_bytes()).hexdigest()!=h:issues.append(str(p))
(O/'source-integrity.json').write_text(json.dumps({'changed_since_initial_audit':issues},indent=2))
print('Source files changed since initial audit:',issues)

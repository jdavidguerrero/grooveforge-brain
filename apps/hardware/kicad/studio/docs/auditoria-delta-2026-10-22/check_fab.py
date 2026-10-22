from pathlib import Path
import subprocess,zipfile,re,csv,json,hashlib,concurrent.futures
root=Path('apps/hardware/kicad/studio').resolve();out=Path(__file__).resolve().parent;cli='/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
def norm(b):
 s=b.decode().replace('\r\n','\n');s=re.sub(r'^.*(?:CreationDate|TF.CreationDate|G04 Created by|; DRILL file|; #@! TF.CreationDate).*\n','',s,flags=re.M);return s

def check(n):
 d=out/'regenerated'/n;d.mkdir(parents=True,exist_ok=True);pcb=root/n/f'{n}.kicad_pcb'
 cmds=[[cli,'pcb','export','gerbers','--layers','F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts','--subtract-soldermask','-o',str(d)+'/',str(pcb)],[cli,'pcb','export','drill','--format','excellon','--drill-origin','absolute','--excellon-units','mm','--excellon-separate-th','-o',str(d)+'/',str(pcb)]]
 for c in cmds:
  p=subprocess.run(c,capture_output=True,text=True)
  if p.returncode:return n,p.returncode,p.stderr
 z=zipfile.ZipFile(root/'fab'/n/f'{n}-gerbers.zip');res={}
 for f in z.namelist():
  if f.endswith('.gbrjob'):continue
  a=norm(z.read(f));b=norm((d/f).read_bytes());res[f]=a==b
  if a!=b:
   import difflib
   (d/(f+'.diff')).write_text(''.join(difflib.unified_diff(a.splitlines(True),b.splitlines(True))))
 board=json.loads((out/f'{n}-board.json').read_text());fps=board['footprints'];bom=root/'fab'/n/f'{n}-bom.csv';bstat={}
 if bom.exists():
  rows=list(csv.DictReader(bom.open()));refs={ref.strip():row for row in rows for ref in row['Designator'].split(',')};cpl=list(csv.DictReader((root/'fab'/n/f'{n}-cpl.csv').open()));bstat={'bom_lines':len(rows),'bom_parts':len(refs),'cpl_parts':len(cpl),'set_equal':set(refs)==set(r['Designator'] for r in cpl),'bom_mismatch':[],'pos_mismatch':[]}
  for ref,row in refs.items():
   f=fps.get(ref,{})
   if f.get('value')!=row['Comment'] or f.get('lcsc')!=row['LCSC Part #'] or f.get('lib','').split(':')[-1]!=row['Footprint']:bstat['bom_mismatch'].append(ref)
  for row in cpl:
   f=fps[row['Designator']]
   if abs(float(row['Mid X'].replace('mm',''))-f['at'][0])>0.0002 or abs(float(row['Mid Y'].replace('mm',''))+f['at'][1])>0.0002 or row['Layer']!=('Bottom' if f['side']=='B.Cu' else 'Top'):bstat['pos_mismatch'].append(row['Designator'])
 return n,res,bstat
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as ex:results=list(ex.map(check,['portadora','pcb-a','pcb-k']))
(out/'fab-comparison.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))

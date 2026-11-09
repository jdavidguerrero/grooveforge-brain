"""Read-only inspection. Run using KiCad's bundled Python; outputs stay in this audit folder."""
from pathlib import Path
import collections, csv, hashlib, json, re, subprocess, zipfile
import pcbnew

OUT = Path(__file__).resolve().parent
ROOT = OUT.parent / 'boards'
CLI = '/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
names = ['pcb-a', 'pcb-f', 'pcb-k1', 'pcb-k2', 'pcb-s']
mm = pcbnew.ToMM
summary = {}
for name in names:
    source = ROOT/name/(name+'.kicad_pcb')
    board = pcbnew.LoadBoard(str(source))
    box = board.GetBoardEdgesBoundingBox()
    entry = {'dimensions_mm': [mm(box.GetWidth()),mm(box.GetHeight())], 'footprints':[], 'nets':{}, 'hashes':{}}
    for fp in board.GetFootprints():
        entry['footprints'].append({'ref':fp.GetReference(),'value':fp.GetValue(),'footprint':str(fp.GetFPID().GetLibNickname())+':'+str(fp.GetFPID().GetLibItemName()),'xy':list(mm(fp.GetPosition())),'angle':fp.GetOrientationDegrees(),'bottom':fp.IsFlipped(),'lcsc':fp.GetFieldText('LCSC') if fp.HasField('LCSC') else '', 'pads':[{'pin':p.GetNumber(),'net':p.GetNetname(),'xy':list(mm(p.GetPosition())),'size':list(mm(p.GetSize())),'drill':list(mm(p.GetDrillSize()))} for p in fp.Pads()]})
    for t in board.GetTracks():
        n=entry['nets'].setdefault(t.GetNetname(),{'length_mm':0,'widths':{},'vias':0,'segments':[]})
        if isinstance(t,pcbnew.PCB_VIA): n['vias']+=1
        else:
            length=mm(t.GetLength());width=mm(t.GetWidth())
            n['length_mm']+=length;n['widths'][width]=n['widths'].get(width,0)+length
            n['segments'].append({'start':list(mm(t.GetStart())),'end':list(mm(t.GetEnd())),'width':width,'layer':board.GetLayerName(t.GetLayer()),'length':length})
    for path in [*list((ROOT/name).glob(name+'.kicad_*')),*list((ROOT/'fab'/name).glob('*'))]:
        entry['hashes'][str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
    dest=OUT/'regenerated'/name;dest.mkdir(parents=True,exist_ok=True)
    subprocess.run([CLI,'pcb','export','gerbers','--layers','F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts','--subtract-soldermask','-o',str(dest)+'/',str(source)],check=True,capture_output=True)
    subprocess.run([CLI,'pcb','export','drill','--format','excellon','--drill-origin','absolute','--excellon-units','mm','--excellon-separate-th','-o',str(dest)+'/',str(source)],check=True,capture_output=True)
    def norm(data):
        s=data.decode()
        return '\n'.join(l for l in s.splitlines() if not any(v in l for v in ('CreationDate','Created by KiCad','; DRILL file KiCad')))
    diff=[]
    with zipfile.ZipFile(ROOT/'fab'/name/(name+'-gerbers.zip')) as z:
        entry['zip_files']=z.namelist()
        for f in z.namelist():
            if f.endswith('.gbrjob'):
                a=json.loads(z.read(f));b=json.loads((dest/f).read_text())
                a.get('Header',{}).pop('CreationDate',None);b.get('Header',{}).pop('CreationDate',None)
                equal=a==b
            else: equal=(dest/f).exists() and norm(z.read(f))==norm((dest/f).read_bytes())
            if not equal: diff.append(f)
    entry['fabrication_differences']=diff
    if name!='pcb-s':
        bom=list(csv.DictReader((ROOT/'fab'/name/(name+'-bom.csv')).open()))
        cpl=list(csv.DictReader((ROOT/'fab'/name/(name+'-cpl.csv')).open()))
        br={ref.strip():row['LCSC Part #'] for row in bom for ref in row['Designator'].split(',')}
        cr={row['Designator']:row for row in cpl}
        expected={f['ref']:f for f in entry['footprints'] if f['lcsc']}
        issues=[]
        if set(br)!=set(cr) or set(br)!=set(expected):issues.append('BOM/CPL reference set mismatch')
        for ref,f in expected.items():
            if br.get(ref)!=f['lcsc']:issues.append(ref+': LCSC mismatch')
            row=cr.get(ref,{})
            if not row:continue
            if abs(float(row['Mid X'].removesuffix('mm'))-f['xy'][0])>0.001 or abs(float(row['Mid Y'].removesuffix('mm'))+f['xy'][1])>0.001:issues.append(ref+': center mismatch')
            if row['Layer']!=('Bottom' if f['bottom'] else 'Top'):issues.append(ref+': side mismatch')
        entry['assembly']={'parts':len(cr),'bom_lines':len(bom),'issues':issues}
    for side,layers in [('top','F.Cu,F.Silkscreen,Edge.Cuts'),('bottom','B.Cu,B.Silkscreen,Edge.Cuts')]:
        subprocess.run([CLI,'pcb','export','svg','--layers',layers,'--page-size-mode','2','--exclude-drawing-sheet','--mode-single','-o',str(OUT/(name+'-'+side+'.svg')),str(source)],check=True,capture_output=True)
    summary[name]=entry
    print(name,'dimensions',entry['dimensions_mm'],'fab differences',diff,'assembly',entry.get('assembly'))
    for net in ['/+5V','/+3V3','/GND','/SCL','/SDA','/LED_DIN']:
        if net in entry['nets']:
            n=entry['nets'][net];print(net,round(n['length_mm'],2),'mm',n['widths'],'vias',n['vias'])
(OUT/'inspection.json').write_text(json.dumps(summary,indent=2))

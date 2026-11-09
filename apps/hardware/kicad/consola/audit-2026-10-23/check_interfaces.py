"""Validate the four panel harnesses against exported schematic netlists."""
from pathlib import Path
import json
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parent
maps={}
for b in ['pcb-a','pcb-f','pcb-k1','pcb-k2','pcb-s']:
    maps[b]={(n.get('ref'),n.get('pin')):net.get('name') for net in ET.parse(ROOT/(b+'.xml')).findall('nets/net') for n in net.findall('node')}
result=[]
for a,ar,b,br,count in [('pcb-s','J1','pcb-a','J2',4),('pcb-a','J3','pcb-f','J1',6),('pcb-a','J4','pcb-k1','J1',10),('pcb-k1','J2','pcb-k2','J1',10)]:
    rows=[]
    for pin in range(1,count+1):
        an=maps[a].get((ar,str(pin)));bn=maps[b].get((br,str(pin)))
        rows.append({'pin':pin,'net_a':an,'net_b':bn,'match':an==bn})
    result.append({'a':a+'.'+ar,'b':b+'.'+br,'pins':rows,'pass':all(r['match'] for r in rows)})
    print(a,ar,'->',b,br,all(r['match'] for r in rows))
(ROOT/'interface-checks.json').write_text(json.dumps(result,indent=2))

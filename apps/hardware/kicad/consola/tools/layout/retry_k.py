#!/usr/bin/env python3
"""Prioriza conexiones pendientes sin alterar geometría ni reglas de cobre."""
from pathlib import Path
import sys,subprocess,random,json
D=Path(__file__).resolve().parent;C=D.parents[1]
side=sys.argv[1];folder=C/'boards'/('pcb-k-'+side)
rows=(folder/'routing-input.txt').read_text().splitlines();i=2
n=int(rows[i]);i+=1+n
n=int(rows[i]);i+=1+n
n=int(rows[i]);i+=1+n
n=int(rows[i]);i+=1
header=rows[:i];connections=rows[i:];assert len(connections)==n
output=folder/'routing-output.txt'
def key(l):
 a=l.split();return (int(a[0]),tuple(round(float(x),4) for x in a[2:4]),tuple(round(float(x),4) for x in a[5:7]))
def failkey(l):
 a=l.split();return (int(a[1]),tuple(round(float(x),4) for x in a[2:4]),tuple(round(float(x),4) for x in a[4:6]))
best=999;weights={};rng=random.Random(1978)
for trial in range(1,17):
 fails=[l for l in output.read_text().splitlines() if l.startswith('FAIL')]
 if len(fails)<best:
  best=len(fails);(folder/'routing-best.txt').write_text(output.read_text())
 if not fails:print(side,'DONE 0 pendientes',flush=True);break
 for l in fails:weights[failkey(l)]=weights.get(failkey(l),0)+1
 ranked=sorted(connections,key=lambda l:(-weights.get(key(l),0),rng.random() if trial%4==0 else connections.index(l)))
 (folder/'routing-retry.txt').write_text('\n'.join(header+ranked)+'\n')
 with (D/f'retry-{side}-{trial:02d}.log').open('w') as log:
  subprocess.run([str(D/'grid_router'),str(folder/'routing-retry.txt'),str(output)],stderr=log)
 print(side,'trial',trial,'pending',sum(l.startswith('FAIL') for l in output.read_text().splitlines()),'best',best,flush=True)
else:
 if sum(l.startswith('FAIL') for l in output.read_text().splitlines())>best:output.write_text((folder/'routing-best.txt').read_text())

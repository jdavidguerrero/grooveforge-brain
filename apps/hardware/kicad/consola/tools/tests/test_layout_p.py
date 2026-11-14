"""Cierre observable de entrega PCB-P: exports vigentes y ZIP completo, ver THEORY-layout."""
import hashlib
import json
from pathlib import Path
import zipfile

from schgen.sexp import parse, child, children

REPO=Path(__file__).resolve().parents[6]
OUT=REPO/'apps/hardware/kicad/consola/boards/pcb-p'


def test_pcb_p_fabrication_matches_verified_board():
    manifest=json.loads((OUT/'docs/fabrication-manifest.json').read_text())
    assert manifest['result']=='PASS'
    for filename,digest in manifest['sha256'].items():
        assert hashlib.sha256((REPO/filename).read_bytes()).hexdigest()==digest,filename
    report=json.loads((OUT/'docs/pcb-p-drc.json').read_text())
    assert report['violations']==report['unconnected_items']==report['schematic_parity']==[]
    assert json.loads((OUT/'pcb-p.kicad_pro').read_text())['board']['design_settings']['drc_exclusions']==[]
    with zipfile.ZipFile(OUT/'fab/pcb-p-gerber-jlc.zip') as archive:
        assert len(archive.namelist())==9
        for filename in archive.namelist():
            assert archive.read(filename)==(OUT/'fab'/filename).read_bytes()
    drill=(OUT/'fab/pcb-p-NPTH.drl').read_text()
    assert 'NonPlated' in drill and drill.count('G85')==6


def test_pcb_p_silkscreen_manufacturing_minimums():
    """Comprobar la entrega real, incluidos los contornos de las huellas."""
    board=parse((OUT/'pcb-p.kicad_pcb').read_text())
    items=list(board[1:])
    for fp in children(board,'footprint'):
        items.extend(fp[1:])
    texts=shapes=0
    for item in items:
        if not isinstance(item,list) or not children(item,'layer'):
            continue
        if child(item,'layer')[1] not in ('F.SilkS','B.SilkS'):
            continue
        if item[0] in ('gr_text','fp_text','property'):
            if children(item,'hide') and child(item,'hide')[1]=='yes':
                continue
            font=child(child(item,'effects'),'font')
            assert float(child(font,'size')[2])>=1.,item
            assert float(child(font,'thickness')[1])>=.15,item
            texts+=1
        elif children(item,'stroke'):
            assert float(child(child(item,'stroke'),'width')[1])>=.15,item
            shapes+=1
    assert texts==14 and shapes>0

"""Cierre observable de entrega PCB-P: exports vigentes y ZIP completo, ver THEORY-layout."""
import hashlib
import json
from pathlib import Path
import zipfile

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

"""Fake-only snapshot exported by gf-export-metadata; production uses list_params.

Regenerate: cmake --build <host-build>; copy <host-build>/synth-metadata.json here.
CTest metadata_snapshot compares this file to the compiled registry export.
"""
from copy import deepcopy
import json
from pathlib import Path

METADATA = json.loads(Path(__file__).with_name('synth-metadata.json').read_text())
ENGINE_LABELS = dict(zip(('prophet', 'dx7', 'dx7_6op', 'juno', 'ob6', 'model_d', 'arp2600', 'hybrid'),
                        ('VINTAGE POLY', 'FM 4-OP', 'DX7 6-OP', 'CHORUS POLY', 'SEM POLY', 'LADDER MONO', 'MODULAR', 'HYBRID')))
ENGINES = tuple(METADATA['engines'])


def list_pages(engine):
    if engine not in ENGINES:
        raise ValueError('unknown engine')
    return deepcopy(METADATA['engines'][engine])


def all_params(engine):
    return {p['id']: p for pg in list_pages(engine) for bank in pg['banks'] for p in bank}

"""Compile the actual registry, compare the fake, and exercise QML over real host audio."""
import json
import os
from pathlib import Path
import subprocess

import pytest
from PySide6.QtTest import QTest

from fm_algorithms import DX7_ALGORITHMS, _decode
from synth_catalog import METADATA, all_params
from fm_visuals import keyboard_scaling
from tools.real_synth_review import review, select_engine, until

ROOT=Path(__file__).resolve().parents[4]


@pytest.fixture(scope='session')
def real_host(tmp_path_factory):
    build=Path(os.environ.get('GF_SYNTH_HOST_BUILD',tmp_path_factory.mktemp('real-synth-build')))
    if 'GF_SYNTH_HOST_BUILD' not in os.environ:
        subprocess.run(['cmake','-S',str(ROOT/'apps/synth-host'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release'],capture_output=True,text=True,check=True)
        subprocess.run(['cmake','--build',str(build),'--target','gf-synth-host','gf-export-metadata','-j2'],capture_output=True,text=True,check=True)
    export=build/'pytest-metadata.json'
    subprocess.run([str(build/'gf-export-metadata'),str(export)],check=True)
    return build/'gf-synth-host',json.loads(export.read_text())


def test_fake_equals_compiled_real_metadata_and_algorithm_tables(real_host):
    _,actual=real_host
    assert METADATA==actual, 'Run host build, then copy build/synth-metadata.json to gui-console/synth-metadata.json'
    assert tuple(_decode(i,' '.join(f'{f:02x}' for f in row)) for i,row in enumerate(actual['dx7_algorithms'],1))==DX7_ALGORITHMS
    assert len(all_params('dx7_6op'))>=151
    for curve,points in enumerate(actual['dx7_scaling_reference']):
        assert points==[keyboard_scaling(note,39,63,45,curve,curve) for note in range(128)]


@pytest.mark.parametrize('engine_name', ['dx7_6op','dx7'])
def test_real_host_builds_every_fm_page_edits_and_switches_deterministically(app,real_host,engine_name):
    binary,actual=real_host
    with review(binary) as (gui,state,window,messages):
        synth=gui.synth_state
        state.navigate('SynthDx7')
        select_engine(synth,engine_name)
        assert synth.fmSupported and len(synth.fmOperators)==(6 if engine_name=='dx7_6op' else 4)
        pages=actual['engines'][engine_name]
        assert synth.current_pages==pages
        for page in pages:
            synth.selectPage(page['name'])
            for index,bank in enumerate(page['banks']):
                synth.chooseBank(index)
                assert [r['entry']['id'] for r in synth.cellsModel.rows if not r['entry']['empty']]==[p['id'] for p in bank]
                QTest.qWait(2)
                assert not window.grabWindow().isNull()
        for number in (1,4,6,32) if engine_name=='dx7_6op' else (1,5,8):
            synth.chooseAlgorithm(number)
            until(lambda:synth.algorithm['number']==number and not synth.client.pending,synth.flush_frame)
            key='algorithm' if engine_name=='dx7_6op' else 'osc.0.2'
            assert synth.values[key]==number-(1 if engine_name=='dx7_6op' else 0)
        for op in range(1,synth.operatorCount+1):
            synth.focusOperator(op)
            assert synth.selectedOperator==op
            if engine_name=='dx7_6op':
                synth.chooseBank(1)
                synth.turn(0,-1)
                until(lambda:not synth._outbound and not synth.client.pending,synth.flush_frame)
                assert synth.values[f'op{op}.r1']==98
        if engine_name=='dx7_6op':
            for key,value in [('op6.mode',1),('op6.coarse',3),('op6.fine',50),('op6.on',0)]:
                synth.request('set_param',part=synth.selectedPart,param=key,value=value)
            until(lambda:not synth.client.pending,synth.flush_frame)
            assert synth.fmOperators[5]['tuning']=='3162.28 Hz' and not synth.fmOperators[5]['on']
        # A full round trip, including selecting the same engine again, must discard previous values.
        for target in ('dx7_6op','dx7','dx7','dx7_6op',engine_name):
            select_engine(synth,target)
            assert set(synth.values)==set(all_params(target))
            assert synth.values=={id:p['default'] for id,p in all_params(target).items()}
        assert not messages


def test_normalized_fx_enum_keeps_fractional_steps_and_labels():
    from synth_state import relative_value, formatted
    from fake_synth_host import FakeSynth
    meta=all_params('dx7_6op')['fx.0.2']
    value=relative_value(meta,0,1,True)
    assert value==.5 and formatted(meta,value)=='Distortion'
    fake=FakeSynth()
    fake.request(dict(id=1,cmd='set_engine',part=1,engine='dx7_6op'))
    response,_=fake.request(dict(id=2,cmd='set_param',part=1,param=meta['id'],value=value))
    assert response['ok'] and response['value']==value
    response,_=fake.request(dict(id=3,cmd='set_param',part=1,param='groove.1.5',value=.6))
    assert response['ok'] and response['value']==1
    fake.request(dict(id=4,cmd='set_engine',part=1,engine='hybrid'))
    response,_=fake.request(dict(id=5,cmd='set_param',part=1,param='layer.transpose',value=-.5))
    assert response['ok'] and response['value']==-1

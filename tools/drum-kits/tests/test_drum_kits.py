from copy import deepcopy
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
import pytest
import soundfile as sf

from drum_kits import (store_sample, import_folder, render_engine, generate_variations,
                       atomic_json, empty_rack, convert, RATE, category, main)


def test_import_more_than_16_preserves_licence_and_decodes(tmp_path):
    source, data = tmp_path/'input', tmp_path/'data'
    source.mkdir()
    for k in range(17):
        sf.write(source/f'{k:02}-kick.wav', np.r_[np.zeros(300), np.ones(300)*.2, np.zeros(300)], 24000,
                 subtype=('PCM_16', 'PCM_24', 'PCM_32')[k%3])
    (source/'broken.wav').write_text('bad')
    events = []
    result = import_folder(data, source, licence_note='Tribe — licence pending', trim=True,
                           progress=lambda **e: events.append(e))
    assert result['imported'] == 17 and len(result['racks']) == 2
    assert len(result['skipped']) == 1 and events[-1]['done'] == 18
    for rack_path in result['racks']:
        rack = json.loads((data/rack_path).read_text())
        assert rack['sources'][0]['licence_note'] == 'Tribe — licence pending'
        assert rack['sources'][0]['kind'] == 'IMPORTED'
        assert len(rack['pads']) == 16 and len(rack['patterns']) == 8
        for pad in rack['pads']:
            for layer in pad['layers']:
                path = data/layer['path']
                assert sha256(path.read_bytes()).hexdigest() == layer['hash']
                info = sf.info(path)
                assert info.samplerate == RATE and info.subtype == 'FLOAT' and info.frames < 1800


def test_render_root_layers_variations_do_not_edit(tmp_path):
    result = render_engine(tmp_path, root='C#', genre='organic', layers=2)
    path = tmp_path/result['racks'][0]
    original = path.read_bytes()
    rack = json.loads(original)
    assert len([p for p in rack['pads'] if p['layers']]) == 13
    kick = rack['pads'][0]
    assert kick['recipe']['root'] == 'C#'
    assert kick['layers'][0]['velocity_max']+1 == kick['layers'][1]['velocity_min']
    variants = generate_variations(tmp_path, kick['recipe'], count=2)
    assert path.read_bytes() == original
    assert len(variants['variants']) == 2 and variants['root'] == 'C#'
    assert len({v['layer']['hash'] for v in variants['variants']}) == 2
    for v in variants['variants']:
        assert v['recipe']['root'] == 'C#'
        assert v['render']['afinar_hz'] == kick['layers'][0]['render']['afinar_hz']
    assert rack['pads'][10]['params']['choke'] == rack['pads'][11]['params']['choke'] == 1
    assert all(s['velocity'] == 0 for p in rack['patterns'] for l in p['lanes'] for s in l['steps'])


def test_content_hash_determinism_and_invalid(tmp_path):
    x = np.sin(np.arange(1000)*.03).astype('float32')*.2
    a = store_sample(tmp_path, x, 44100)
    b = store_sample(tmp_path, x, 44100)
    assert a == b
    raw=(tmp_path/a["path"]).read_bytes()
    assert raw[36:40]==b"fact" and raw[48:52]==b"data"  # no time-dependent PEAK chunk
    with pytest.raises(ValueError, match='nonfinite'):
        store_sample(tmp_path, [0, float('nan')])
    with pytest.raises(ValueError):
        convert(np.zeros((20, 3)), 48000)
    assert category('Conga low') == 'skin' and category('Hat open') == 'hat'
    target = tmp_path/'rack.json'
    atomic_json(target, empty_rack('one'))
    with pytest.raises(FileExistsError):
        atomic_json(target, empty_rack('two'))


def test_worker_cli_and_skipped_symlink(tmp_path):
    source, data = tmp_path/'source', tmp_path/'data'
    source.mkdir()
    sf.write(source/'snare.wav', np.ones(100)*.1, RATE)
    (source/'escape.wav').symlink_to('/not/a/sample.wav')
    job = tmp_path/'job.json'
    job.write_text(json.dumps(dict(cmd='import_folder', data_dir=str(data), path=str(source), licence_note='pending')))
    result = subprocess.run([sys.executable, str(Path(__file__).parents[1]/'drum_kits.py'), 'worker', '--job', str(job)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    lines = [json.loads(line) for line in result.stdout.splitlines()]
    assert lines[-1]['status'] == 'done' and lines[-1]['imported'] == 1
    assert len(lines[-1]['skipped']) == 1
    assert job.with_suffix('.result.json').exists()

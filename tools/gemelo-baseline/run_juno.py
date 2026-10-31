#!/usr/bin/env python3
"""Render the firmware Juno graph with a scalar PJRC noise shim, not a plugin."""
import argparse
import hashlib
import json
import math
import platform
import struct
import subprocess
import tempfile
import wave
from pathlib import Path
from run import ROOT, HERE, SRC, build

CASES = ('chord', 'pwm', 'glide', 'voice_steal', 'hpf', 'chorus_ii', 'chorus_i_ii', 'white_noise')
SOURCES = [HERE / 'shim/audio.cpp', HERE / 'juno_reference.cpp',
           SRC / 'engines/juno_106.cpp', SRC / 'audio/moog_ladder.cpp']

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compiler', default='c++')
    parser.add_argument('--compare', type=Path, help='saved reference directory; compare its listed cases byte for byte')
    args = parser.parse_args()
    out = args.out.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('output directory must be empty')
    out.mkdir(parents=True, exist_ok=True)
    baseline = None
    if args.compare:
        baseline = json.loads((args.compare / 'manifest.json').read_text())
    manifest = {'sample_rate': 44100, 'channels': 2, 'frames': 690 * 128,
                'source': 'firmware graph with single-instance software shim',
                'limitations': ['PJRC noise uses scalar modular implementation and harness seed order',
                                'no Studio effects or physical hardware', 'control ticks before each block'],
                'git_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'git_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()),
                'compiler': subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0],
                'architecture': platform.machine(), 'cases': []}
    # Record the full local source dependencies including headers rather than relying on a commit with dirty changes.
    manifest['source_sha256'] = {}
    files = SOURCES + [HERE / 'shim/Audio.h', HERE / 'shim/Arduino.h', HERE / 'run.py', HERE / 'run_juno.py']
    files += list((SRC / 'dsp').glob('*.h')) + list((SRC / 'audio').glob('*.h'))
    files += [SRC / 'engines/juno_106.h', SRC / 'engines/lfo_mod.h']
    for path in sorted(set(files)):
        manifest['source_sha256'][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='gf-juno-') as tmp:
        executable = Path(tmp) / 'juno'
        build(args.compiler, executable, SOURCES)
        for number, name in enumerate(CASES, 1):
            data = subprocess.check_output([str(executable), str(number)])
            repeat = subprocess.check_output([str(executable), str(number)])
            if data != repeat or len(data) != 690 * 128 * 2 * 2:
                raise RuntimeError(f'{name}: unstable output or incorrect length')
            samples = struct.unpack(f'<{len(data) // 2}h', data)
            attack = math.sqrt(sum(x*x for x in samples[4000:24000]) / 20000)
            tail = math.sqrt(sum(x*x for x in samples[-16000:]) / 16000)
            if attack < 100 or tail >= attack:
                raise RuntimeError(f'{name}: invalid attack or release ({attack}, {tail})')
            if baseline:
                expected = next((c for c in baseline['cases'] if c['name'] == name), None)
                if expected:
                    old_data = (args.compare / (name + '.raw')).read_bytes()
                    if hashlib.sha256(old_data).hexdigest() != expected['pcm_sha256'] or old_data != data:
                        raise RuntimeError(f'{name}: reference differs or saved PCM is corrupt')
            with wave.open(str(out / (name + '.wav')), 'wb') as wav:
                wav.setnchannels(2); wav.setsampwidth(2); wav.setframerate(44100); wav.writeframes(data)
            (out / (name + '.raw')).write_bytes(data)
            digest = hashlib.sha256(data).hexdigest()
            manifest['cases'].append({'name': name, 'pcm_sha256': digest, 'attack_rms': attack, 'tail_rms': tail})
            print(f'{name}: {digest}')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    if baseline and not all(case['name'] in CASES for case in baseline['cases']):
        raise RuntimeError('reference contains unknown cases')
    print(f'Juno reference: {out}')

if __name__ == '__main__':
    main()

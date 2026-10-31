#!/usr/bin/env python3
"""Build a temporary Juno diagnostic variant using the historical shim's block clock."""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent

def replace_once(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError('Juno source changed; review the diagnostic transformation before running it')
    return source.replace(old, new, 1)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path, help='new empty output directory')
    parser.add_argument('--compiler', default=os.environ.get('CXX', 'c++'))
    args = parser.parse_args()
    source_path = ROOT / 'apps/brain-plugin/src/juno_preview.cpp'
    source = source_path.read_text()
    source = replace_once(source, '    float mix = 0;\n    for (auto& item : voices_) {', '''    static unsigned frame = 0, previous_ms = 0;
    unsigned reference_dt = 0;
    if (frame % 128 == 0) {
        const unsigned target = uint32_t(uint64_t(frame) * 1000 / 44100);
        reference_dt = target - previous_ms; previous_ms = target;
    }
    ++frame;
    float mix = 0;
    for (auto& item : voices_) {''')
    source = replace_once(source, 'const float filter_env = update ? v.filter_env.update(1) : v.filter_env.level();\n        if (update) {',
                          'const float filter_env = reference_dt ? v.filter_env.update(reference_dt) : v.filter_env.level();\n        if (reference_dt) {')
    source = replace_once(source, '1.0f - std::exp(-1.0f / glide_ms_)', '1.0f - std::exp(-float(reference_dt) / glide_ms_)')
    with tempfile.TemporaryDirectory(prefix='gf-juno-clock-experiment-') as tmp:
        variant = Path(tmp) / 'juno_preview_variant.cpp'
        renderer = Path(tmp) / 'renderer'
        variant.write_text(source)
        command = [args.compiler, '-std=c++17', '-O2', f'-I{ROOT / "apps/brain-plugin/src"}',
                   f'-I{ROOT / "apps/firmware-teensy/src"}', f'-I{ROOT / "apps/bridge-protocol/include"}',
                   str(variant), str(ROOT / 'apps/firmware-teensy/src/dsp/lfo.cpp'),
                   str(HERE / 'juno_preview_reference.cpp'), '-o', str(renderer)]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([os.environ.get('PYTHON', 'python3'), str(HERE / 'compare_juno_preview.py'),
                        '--renderer', str(renderer), '--out', str(args.out)], cwd=ROOT, check=True)
        metadata = {'kind': 'temporary_diagnostic_not_plugin', 'sample_rate': 44100, 'block_samples': 128,
                    'limitations': ['filter envelope and glide use shim timing', 'LFO and other stages retain plugin timing',
                                    'not hardware audio or a production implementation'],
                    'source_sha256': hashlib.sha256(source_path.read_bytes()).hexdigest(),
                    'variant_sha256': hashlib.sha256(variant.read_bytes()).hexdigest(),
                    'compiler': subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0]}
        (args.out / 'experiment.json').write_text(json.dumps(metadata, indent=2) + '\n')

if __name__ == '__main__':
    main()

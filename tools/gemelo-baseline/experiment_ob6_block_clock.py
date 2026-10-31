#!/usr/bin/env python3
"""Compare a temporary OB-6 control-clock variant with the firmware-shim fixture."""
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
        raise RuntimeError('OB-6 source changed; review the diagnostic transformation')
    return source.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='new empty output directory')
    parser.add_argument('--compiler', default=os.environ.get('CXX', 'c++'))
    parser.add_argument('--component', choices=('both', 'filter', 'glide'), default='both')
    args = parser.parse_args()
    original = ROOT / 'apps/brain-plugin/src/ob6_preview.cpp'
    source = original.read_text()
    source = replace_once(source, '    const bool drift = drift_left_ <= 0;', '''    static unsigned frame = 0, previous_ms = 0;
    unsigned reference_dt = 0;
    if (frame % 128 == 0) {
        const unsigned target = uint32_t(uint64_t(frame) * 1000 / 44100);
        reference_dt = target - previous_ms; previous_ms = target;
    }
    ++frame;
    const bool drift = drift_left_ <= 0;''')
    original_block = """        if (update) {
            const float env = v.filter_env.update(1);
            const float glide = patch_.glide_ms >= 1 ? 1.0f - std::exp(-1.0f / patch_.glide_ms) : 1.0f;
            v.hz += (v.target_hz - v.hz) * glide;
"""
    filter_tick = 'reference_dt' if args.component in ('both', 'filter') else '(update ? 1u : 0u)'
    glide_tick = 'reference_dt' if args.component in ('both', 'glide') else '(update ? 1u : 0u)'
    variant_block = f"""        const unsigned filter_dt = {filter_tick};
        const unsigned glide_dt = {glide_tick};
        if (glide_dt) {{
            const float glide = patch_.glide_ms >= 1 ? 1.0f - std::exp(-float(glide_dt) / patch_.glide_ms) : 1.0f;
            v.hz += (v.target_hz - v.hz) * glide;
        }}
        if (filter_dt) {{
            const float env = v.filter_env.update(filter_dt);
"""
    source = replace_once(source, original_block, variant_block)
    with tempfile.TemporaryDirectory(prefix='gf-ob6-clock-experiment-') as temp:
        variant = Path(temp) / 'ob6_preview_variant.cpp'
        renderer = Path(temp) / 'renderer'
        variant.write_text(source)
        command = [args.compiler, '-std=c++17', '-O2', f'-I{ROOT / "apps/brain-plugin/src"}',
                   f'-I{ROOT / "apps/firmware-teensy/src"}', f'-I{ROOT / "apps/bridge-protocol/include"}',
                   str(variant), str(ROOT / 'apps/firmware-teensy/src/dsp/lfo.cpp'),
                   str(HERE / 'ob6_preview_reference.cpp'), '-o', str(renderer)]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([os.environ.get('PYTHON', 'python3'), str(HERE / 'compare_ob6_preview.py'),
                        '--renderer', str(renderer), '--out', str(args.out)], cwd=ROOT, check=True)
        metadata = {'kind': 'temporary_diagnostic_not_plugin', 'component': args.component, 'sample_rate': 44100, 'block_samples': 128,
                    'limitations': ['selected control components use shim block timing',
                                    'LFO, mixer and filter implementation retain plugin behavior',
                                    'not the hardware scheduler or a production implementation'],
                    'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest(),
                    'variant_sha256': hashlib.sha256(variant.read_bytes()).hexdigest(),
                    'compiler': subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0]}
        (args.out / 'experiment.json').write_text(json.dumps(metadata, indent=2) + '\n')


if __name__ == '__main__':
    main()

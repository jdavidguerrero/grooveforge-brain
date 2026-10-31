#!/usr/bin/env python3
"""Compare the portable OB-6 DSP with deterministic firmware-shim PCM; diagnostic only."""
import argparse
import hashlib
import json
import math
import platform
import struct
import subprocess
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REF = ROOT / 'apps/firmware-teensy/test/golden/ob6'
CASES = ('chord', 'notch', 'bandpass', 'sync', 'xmod', 'white_noise', 'voice_steal', 'glide_voice_steal')
FRAMES = 690 * 128


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('output directory must be empty')
    out.mkdir(parents=True, exist_ok=True)
    renderer = args.renderer.resolve()
    manifest_data = (REF / 'manifest.json').read_bytes()
    manifest = json.loads(manifest_data)
    report = {'schema': 1, 'sample_rate': 44100, 'frames': FRAMES, 'channels': 1,
              'reference': 'firmware graph using a software shim, not hardware',
              'candidate': 'Ob6Preview only; excludes JUCE and effects',
              'limitations': ['no latency or gain alignment', 'no pass/fail thresholds',
                              'different control scheduling, PCM quantization, filter and noise seeds'],
              'git_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'git_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()),
              'architecture': platform.machine(), 'renderer_sha256': sha(renderer.read_bytes()),
              'reference_manifest_sha256': sha(manifest_data), 'cases': []}
    for name in CASES:
        reference = (REF / f'{name}.raw').read_bytes()
        expected = next(c for c in manifest['cases'] if c['name'] == name)
        if len(reference) != FRAMES * 2 or sha(reference) != expected['pcm_sha256']:
            raise RuntimeError(f'{name}: invalid reference or hash')
        candidate = subprocess.check_output([str(renderer), name])
        if len(candidate) != len(reference) or candidate != subprocess.check_output([str(renderer), name]):
            raise RuntimeError(f'{name}: non-deterministic or incorrect length')
        x = struct.unpack(f'<{FRAMES}h', reference)
        y = struct.unpack(f'<{FRAMES}h', candidate)
        ref_energy = sum(v*v for v in x)
        got_energy = sum(v*v for v in y)
        error_energy = sum((a-b)**2 for a, b in zip(x, y))
        dot = sum(a*b for a, b in zip(x, y))
        peak = max(range(FRAMES), key=lambda i: abs(x[i] - y[i]))
        db = lambda energy: 20 * math.log10(max(math.sqrt(energy / FRAMES) / 32768, 1e-12))
        result = {'name': name, 'firmware_sha256': sha(reference), 'preview_sha256': sha(candidate),
                  'reference_rms_dbfs': db(ref_energy), 'candidate_rms_dbfs': db(got_energy),
                  'error_rms_dbfs': db(error_energy), 'peak_frame': peak,
                  'peak_error_dbfs': 20 * math.log10(max(abs(x[peak] - y[peak]) / 32768, 1e-12)),
                  'correlation': dot / math.sqrt(ref_energy * got_energy) if ref_energy and got_energy else 0}
        report['cases'].append(result)
        for label, data in [('firmware', reference), ('preview', candidate)]:
            with wave.open(str(out / f'{name}-{label}.wav'), 'wb') as w:
                w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(data)
        print(f'{name}: RMS error {result["error_rms_dbfs"]:.2f} dBFS, correlation {result["correlation"]:.4f}')
    (out / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Diagnostic only: {out}')


if __name__ == '__main__':
    main()

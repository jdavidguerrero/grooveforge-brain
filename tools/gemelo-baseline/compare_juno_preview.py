#!/usr/bin/env python3
"""Measure Juno preview against saved firmware-shim references; no pass threshold."""
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
GOLDEN = ROOT / 'apps/firmware-teensy/test/golden'
CASES = {**{name: 'juno106' for name in ('chord', 'pwm', 'glide', 'voice_steal', 'hpf', 'chorus_ii', 'chorus_i_ii')},
         'white_noise': 'juno106-noise', 'glide_voice_steal': 'juno106-extra',
         'pwm_initialized': 'juno106-extra'}
FRAMES = 690 * 128

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', type=Path, required=True, help='built gf_juno_preview_reference executable')
    parser.add_argument('--out', type=Path, required=True, help='new empty output directory')
    args = parser.parse_args()
    out = args.out.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('output directory must be empty')
    out.mkdir(parents=True, exist_ok=True)
    renderer = args.renderer.resolve()
    report = {'schema': 1, 'sample_rate': 44100, 'frames': FRAMES, 'channels': 2,
              'reference': 'software firmware graph with AudioStream shim, not hardware',
              'candidate': 'JunoPreview DSP only; not the full JUCE/FX signal chain',
              'limitations': ['no gain or latency alignment', 'no pass/fail thresholds',
                              'control scheduling and PCM quantization differ', 'noise phase depends on seed and runtime',
                              'historical glide fixture uses a free voice; glide_voice_steal uses an occupied voice',
                              'pwm_initialized uses an artificial preparation tick'],
              'git_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'git_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()),
              'architecture': platform.machine(), 'renderer_sha256': sha(renderer.read_bytes()), 'cases': []}
    for name, directory in CASES.items():
        ref_dir = GOLDEN / directory
        manifest_data = (ref_dir / 'manifest.json').read_bytes()
        manifest = json.loads(manifest_data)
        case = next(x for x in manifest['cases'] if x['name'] == name)
        reference = (ref_dir / f'{name}.raw').read_bytes()
        if sha(reference) != case['pcm_sha256'] or len(reference) != FRAMES * 4:
            raise RuntimeError(f'{name}: invalid reference or hash')
        candidate = subprocess.check_output([str(renderer), name])
        repeated = subprocess.check_output([str(renderer), name])
        if candidate != repeated or len(candidate) != len(reference):
            raise RuntimeError(f'{name}: renderer is non-deterministic or incorrect length')
        ref = struct.unpack(f'<{FRAMES * 2}h', reference)
        got = struct.unpack(f'<{FRAMES * 2}h', candidate)
        channels = []
        for ch in range(2):
            x, y = ref[ch::2], got[ch::2]
            ref_energy = sum(v*v for v in x)
            got_energy = sum(v*v for v in y)
            error_energy = sum((a-b)**2 for a, b in zip(x, y))
            dot = sum(a*b for a, b in zip(x, y))
            peak_frame = max(range(FRAMES), key=lambda i: abs(x[i] - y[i]))
            channels.append({'reference_rms_dbfs': 20 * math.log10(max(math.sqrt(ref_energy / FRAMES) / 32768, 1e-12)),
                             'candidate_rms_dbfs': 20 * math.log10(max(math.sqrt(got_energy / FRAMES) / 32768, 1e-12)),
                             'error_rms_dbfs': 20 * math.log10(max(math.sqrt(error_energy / FRAMES) / 32768, 1e-12)),
                             'error_peak_dbfs': 20 * math.log10(max(abs(x[peak_frame] - y[peak_frame]) / 32768, 1e-12)),
                             'peak_frame': peak_frame, 'peak_seconds': peak_frame / 44100,
                             'correlation': dot / math.sqrt(ref_energy * got_energy) if ref_energy and got_energy else 0})
        for kind, data in [('firmware', reference), ('preview', candidate)]:
            with wave.open(str(out / f'{name}-{kind}.wav'), 'wb') as w:
                w.setnchannels(2); w.setsampwidth(2); w.setframerate(44100); w.writeframes(data)
        report['cases'].append({'name': name, 'firmware_sha256': sha(reference), 'preview_sha256': sha(candidate),
                                'reference_manifest_sha256': sha(manifest_data), 'channels': channels})
        print(f'{name}: L RMS error {channels[0]["error_rms_dbfs"]:.2f} dBFS, correlation {channels[0]["correlation"]:.4f}')
    (out / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Diagnostic only: {out}')

if __name__ == '__main__':
    main()

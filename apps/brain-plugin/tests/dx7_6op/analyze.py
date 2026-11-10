#!/usr/bin/env python3
"""Analyze the reference tool's original test voices; numpy only, no network."""
import argparse
import csv
from pathlib import Path
import wave
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('renders', type=Path)
parser.add_argument('--csv', type=Path, required=True)
parser.add_argument('--wav', action='store_true')
args = parser.parse_args()
rows = []
for source in sorted(args.renders.glob('*_msfa.f32')):
    prefix = source.name.removesuffix('_msfa.f32')
    fs, kind, note = map(int, prefix.split('_'))
    a = np.fromfile(source, dtype=np.float32).astype(np.float64)
    target = source.with_name(prefix + '_gf.f32')
    b = np.fromfile(target, dtype=np.float32).astype(np.float64)
    assert len(a) == len(b) and np.isfinite(a).all() and np.isfinite(b).all()
    # Same 8192-sample Hann window near the held note's middle; no normalization hides level differences.
    start = int(fs * .4)
    window = np.hanning(8192)
    sa, sb = [np.abs(np.fft.rfft(x[start:start+8192] * window)) for x in (a, b)]
    rms_a, rms_b = [np.sqrt(np.mean(x*x)) for x in (a,b)]
    rows.append(dict(fs=fs, case=kind, note=note, msfa_rms_dbfs=20*np.log10(rms_a),
        gf_rms_dbfs=20*np.log10(rms_b), level_delta_db=20*np.log10(rms_b/rms_a),
        waveform_error=np.linalg.norm(a-b)/np.linalg.norm(a),
        spectrum_error=np.linalg.norm(sa-sb)/np.linalg.norm(sa),
        max_error=np.max(np.abs(a-b))))
    if args.wav:
        for name, x in [('msfa',a),('gf',b)]:
            with wave.open(str(source.with_name(prefix+'_'+name+'.wav')), 'wb') as f:
                f.setnchannels(1); f.setsampwidth(2); f.setframerate(fs)
                f.writeframes(np.rint(np.clip(x,-1,1)*32767).astype('<i2').tobytes())
assert rows, 'No reference renders found'
args.csv.parent.mkdir(parents=True, exist_ok=True)
with args.csv.open('w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=rows[0].keys()); writer.writeheader(); writer.writerows(rows)
for fs in [44100,48000]:
    for kind in range(7):
        group = [r for r in rows if r['fs']==fs and r['case']==kind]
        print(f'{fs} case {kind}: Δlevel {min(r["level_delta_db"] for r in group):+.6f}…'
              f'{max(r["level_delta_db"] for r in group):+.6f} dB; '
              f'spectrum error ≤{max(r["spectrum_error"] for r in group):.6%}')

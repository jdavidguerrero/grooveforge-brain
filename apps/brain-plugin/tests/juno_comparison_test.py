#!/usr/bin/env python3
"""Smoke-test the full diagnostic corpus without prescribing audio equivalence."""
import json
import math
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

root = Path(__file__).resolve().parents[3]
script = root / 'tools/gemelo-baseline/compare_juno_preview.py'
expected = {'chord', 'pwm', 'glide', 'voice_steal', 'hpf', 'chorus_ii', 'chorus_i_ii', 'white_noise', 'glide_voice_steal', 'pwm_initialized'}
with tempfile.TemporaryDirectory(prefix='gf-juno-comparison-test-') as temporary:
    out = Path(temporary) / 'result'
    subprocess.run([sys.executable, str(script), '--renderer', sys.argv[1], '--out', str(out)], check=True,
                   stdout=subprocess.DEVNULL)
    report = json.loads((out / 'comparison.json').read_text())
    cases = report['cases']
    assert len(cases) == len(expected) and {case['name'] for case in cases} == expected
    for case in cases:
        assert len(case['channels']) == 2
        for channel in case['channels']:
            assert all(math.isfinite(value) for value in channel.values())
            assert 0 <= channel['peak_frame'] < 690 * 128
        for side in ('firmware', 'preview'):
            with wave.open(str(out / f"{case['name']}-{side}.wav"), 'rb') as audio:
                assert audio.getnchannels() == 2 and audio.getframerate() == 44100 and audio.getnframes() == 690 * 128

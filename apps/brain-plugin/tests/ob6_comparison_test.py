#!/usr/bin/env python3
"""Check the diagnostic corpus, without defining an equivalence threshold."""
import json
import math
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

root = Path(__file__).resolve().parents[3]
script = root / 'tools/gemelo-baseline/compare_ob6_preview.py'
expected = {'chord', 'notch', 'bandpass', 'sync', 'xmod', 'white_noise', 'voice_steal', 'glide_voice_steal'}
with tempfile.TemporaryDirectory(prefix='gf-ob6-comparison-') as temp:
    out = Path(temp) / 'result'
    subprocess.run([sys.executable, str(script), '--renderer', sys.argv[1], '--out', str(out)], check=True, stdout=subprocess.DEVNULL)
    report = json.loads((out / 'comparison.json').read_text())
    assert len(report['cases']) == len(expected) and {case['name'] for case in report['cases']} == expected
    for case in report['cases']:
        assert all(math.isfinite(case[key]) for key in ('reference_rms_dbfs', 'candidate_rms_dbfs', 'error_rms_dbfs', 'peak_error_dbfs', 'correlation'))
        assert 0 <= case['peak_frame'] < 690 * 128
        for side in ('firmware', 'preview'):
            with wave.open(str(out / f"{case['name']}-{side}.wav"), 'rb') as audio:
                assert audio.getnchannels() == 1 and audio.getframerate() == 44100 and audio.getnframes() == 690 * 128

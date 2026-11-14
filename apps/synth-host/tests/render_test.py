#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

with tempfile.TemporaryDirectory(prefix="gf-render-", dir="/tmp") as root:
    reports = []
    for index, block in enumerate((128, 128, 256, 512)):
        path = Path(root) / f"{index}.wav"
        run = subprocess.run([sys.argv[1], "--render", str(path), "--seconds", "3", "--block", str(block)],
                             capture_output=True, text=True, check=True)
        result = json.loads(run.stdout)
        with wave.open(str(path)) as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()) == (2, 2, 48000, 144000)
            pcm = wav.readframes(wav.getnframes())
        assert hashlib.sha256(pcm).hexdigest() == result["pcm_sha256"]
        assert result["nonfinite_samples"] == 0 and result["rms"] > 1e-5
        reports.append(result)
    assert reports[0]["pcm_sha256"] == reports[1]["pcm_sha256"] == reports[2]["pcm_sha256"] == reports[3]["pcm_sha256"], reports
    print("Offline deterministic PCM SHA-256:", reports[0]["pcm_sha256"])
    print("Repeat and blocks 128/256/512: identical; four parts wet; nonfinite=0")
    print(json.dumps(reports[0]))

#!/usr/bin/env python3
"""Build and render the pre-refactor Prophet harness at its nominal 44.1 kHz."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import platform
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SRC = ROOT / "apps/firmware-teensy/src"
CASES = ("saw_chord", "pulse_pwm", "poly_mod", "glide", "resonance", "voice_steal")
SOURCES = [HERE / "shim/audio.cpp", HERE / "reference.cpp", SRC / "engines/prophet_5.cpp",
           SRC / "engines/prophet_osc_core.cpp", SRC / "audio/moog_ladder.cpp"]
HEADERS = [HERE / "shim/Arduino.h", HERE / "shim/Audio.h", SRC / "engines/prophet_5.h",
           SRC / "engines/prophet_osc_core.h", SRC / "engines/lfo_mod.h", SRC / "audio/moog_ladder.h",
           SRC / "audio/analog_vca.h", SRC / "dsp/analog_env.h", SRC / "dsp/voice_spread.h",
           SRC / "dsp/zdf_ladder.h", SRC / "dsp/polyblep_core.h", SRC / "dsp/fast_math.h"]
ORACLES = [HERE / "run.py", HERE / "test_mixer.cpp"]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def build(compiler: str, output: Path, sources: list[Path]) -> None:
    subprocess.run([compiler, "-std=c++17", "-O2", f"-I{HERE / 'shim'}", f"-I{SRC}",
                    *(str(p) for p in sources), "-o", str(output)], check=True, cwd=ROOT)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True, help="new, empty output directory")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--compare", type=Path, help="compare non-resonant cases with a saved baseline")
    args = parser.parse_args()
    output = args.out.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error("output directory must be empty; existing reference data is never overwritten")
    output.mkdir(parents=True, exist_ok=True)
    baseline = {}
    if args.compare:
        saved = json.loads((args.compare / "manifest.json").read_text())
        baseline = {case["name"]: case["pcm_sha256"] for case in saved["cases"]}
    with tempfile.TemporaryDirectory(prefix="gf-baseline-") as temp:
        reference, mixer = Path(temp) / "reference", Path(temp) / "test_mixer"
        build(args.compiler, mixer, [HERE / "shim/audio.cpp", HERE / "test_mixer.cpp"])
        subprocess.run([str(mixer)], check=True)
        build(args.compiler, reference, SOURCES)
        manifest = {
            "schema": 1, "kind": "comparison_render" if args.compare else "reference_render", "source": "software_shim",
            "limitations": ["Prophet engine output only; no presets, Studio FX or physical hardware",
                            "control ticks simulated before each 128-sample block", "not hardware-validated"],
            "git_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "git_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()),
            "compiler": subprocess.check_output([args.compiler, "--version"], text=True).splitlines()[0],
            "architecture": platform.machine(), "sample_rate": 44100, "channels": 1,
            "audio_format": "signed PCM16 little-endian", "frames": 690 * 128,
            "source_sha256": {str(p.relative_to(ROOT)): sha(p.read_bytes()) for p in SOURCES + HEADERS + ORACLES
                              + [p for p in (SRC / "dsp/ladder_stage_core.h", SRC / "dsp/prophet_control_core.h")
                                 if p.exists()]},
            "cases": [],
        }
        if args.compare:
            manifest["reference_manifest_sha256"] = sha((args.compare / "manifest.json").read_bytes())
        for number, name in enumerate(CASES, 1):
            data = subprocess.check_output([str(reference), str(number)])
            repeat = subprocess.check_output([str(reference), str(number)])
            if data != repeat or len(data) != 690 * 128 * 2:
                raise RuntimeError(f"{name}: non-deterministic render or incorrect size")
            samples = struct.unpack(f"<{len(data) // 2}h", data)
            attack = math.sqrt(sum(x*x for x in samples[2000:12000]) / 10000)
            tail = math.sqrt(sum(x*x for x in samples[-8000:]) / 8000)
            if attack < 100 or tail >= attack:
                raise RuntimeError(f"{name}: invalid attack/release: {attack}, {tail}")
            if args.compare:
                expected_bytes = (args.compare / f"{name}.raw").read_bytes()
                if len(expected_bytes) != len(data) or sha(expected_bytes) != baseline.get(name):
                    raise RuntimeError(f"{name}: incompatible or modified baseline")
                if name == "resonance" and data != expected_bytes:
                    raise RuntimeError("resonance changed: spectral/envelope comparison is not implemented yet")
                expected = struct.unpack(f"<{len(data) // 2}h", expected_bytes)
                squared_error = sum((a - b) ** 2 for a, b in zip(samples, expected))
                squared_signal = sum(a * a for a in expected)
                rms_relative = math.sqrt(squared_error / max(squared_signal, 1))
                peak_fs = max(abs(a - b) for a, b in zip(samples, expected)) / 32768.0
                if rms_relative > 1e-4 or peak_fs > 1e-3:
                    raise RuntimeError(f"{name}: golden failed: relative RMS={rms_relative}, peak FS={peak_fs}")
            (output / f"{name}.raw").write_bytes(data)
            with wave.open(str(output / f"{name}.wav"), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(44100)
                wav.writeframes(data)
            manifest["cases"].append({"name": name, "pcm_sha256": sha(data),
                                      "attack_rms_pcm": attack, "tail_rms_pcm": tail})
            print(f"{name}: {sha(data)}")
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        if args.compare:
            print("Five numerical comparisons passed; resonance matched byte-for-byte.")
    print(f"Reference renders and provenance: {output}")


if __name__ == "__main__":
    main()

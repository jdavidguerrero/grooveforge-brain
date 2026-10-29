"""Tests del snapshot del laboratorio (apps/docs/produccion/protocolo-captura.md §6)."""
import hashlib
import json
import sys
import wave
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gf_snapshot as gs  # noqa: E402


def make_sources(tmp_path):
    ds, live = tmp_path / "ds", tmp_path / "live"
    (ds / "midi" / "bass").mkdir(parents=True)
    (ds / "midi" / "bass" / "a.mid").write_bytes(b"MThd-a")
    (ds / "patches" / "brain" / "juno_106").mkdir(parents=True)
    (ds / "patches" / "brain" / "juno_106" / "p.json").write_text(json.dumps({"name": "P", "raw": "00"}))
    (ds / "midi" / ".DS_Store").write_bytes(b"x")
    (live / "Samples" / "Recorded").mkdir(parents=True)
    (live / "Lab.als").write_bytes(b"gz")
    with wave.open(str(live / "Samples" / "Recorded" / "t.wav"), "wb") as w:
        w.setnchannels(2); w.setsampwidth(3); w.setframerate(44100); w.writeframes(b"\0" * 6 * 4410)
    return ds, live


def test_manifiesto_con_hashes_y_audio_real(tmp_path):
    ds, live = make_sources(tmp_path)
    m = gs.snapshot(ds, live, ds / "snapshots" / "x", "", False, None)
    files = {f["path"]: f for f in m["archivos"]}
    assert files["dataset/midi/bass/a.mid"]["sha256"] == hashlib.sha256(b"MThd-a").hexdigest()
    assert "dataset/midi/.DS_Store" not in files
    assert files["live/Lab.als"]["bytes"] == 2
    assert files["live/recorded/t.wav"]["audio"] == {"sample_rate": 44100, "bits": 24, "channels": 2, "seconds": 0.1}
    assert m["firmware_cargado"] == "unknown"
    assert json.loads((ds / "snapshots" / "x" / "manifest.json").read_text())["archivos"] == m["archivos"]


def test_un_snapshot_no_se_sobrescribe(tmp_path):
    ds, live = make_sources(tmp_path)
    gs.snapshot(ds, live, ds / "snapshots" / "x", "", False, None)
    with pytest.raises(SystemExit):
        gs.snapshot(ds, live, ds / "snapshots" / "x", "", False, None)


def test_libreria_vs_brain_compara_el_raw(tmp_path):
    lib, brain = tmp_path / "lib", tmp_path / "brain"
    for root, raws in ((lib, {"a": "01", "b": "02", "c": "03"}), (brain, {"a": "01", "b": "FF", "d": "04"})):
        root.mkdir()
        for k, raw in raws.items():
            (root / f"{k}.json").write_text(json.dumps({"raw": raw}))
    assert gs.library_vs_brain(lib, brain) == {"solo_libreria": ["c.json"], "solo_brain": ["d.json"],
                                               "raw_distinto": ["b.json"]}

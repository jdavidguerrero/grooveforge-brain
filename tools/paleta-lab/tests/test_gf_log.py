"""Tests del registro de eventos v2 y la audición a ciegas (apps/docs/produccion/motor-asistencia.md §5, §9)."""
import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gf_log  # noqa: E402
import make_audicion as mau  # noqa: E402


@pytest.fixture
def ds(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path))
    return tmp_path


def prep(ds, aid="a1", letters="ABC"):
    gf_log.auditions_dir().mkdir(parents=True, exist_ok=True)
    opts = [{"letter": l, "id": f"opt{i}", "kind": "keep_current" if i == 0 else "option"} for i, l in enumerate(letters)]
    (gf_log.auditions_dir() / f"{aid}.json").write_text(json.dumps({"options": opts, "pendiente_id": "p1"}))


def test_solo_se_agrega(ds):
    gf_log.open_pending("el bajo se come el kick")
    first = (gf_log.events_dir()).glob("*.jsonl").__next__().read_text()
    gf_log.open_pending("los pads muy brillantes")
    assert (gf_log.events_dir()).glob("*.jsonl").__next__().read_text().startswith(first)
    assert len(gf_log.load_events()) == 2


def test_pendiente_sin_criterio_queda_missing_y_cambia_de_estado(ds):
    e = gf_log.open_pending("timbre de acordes")
    assert e["structured"]["criterion"] == "missing"
    gf_log.set_pending(e["pendiente_id"], "postponed")
    assert gf_log.pending_state(gf_log.load_events())[e["pendiente_id"]]["state"] == "postponed"


def test_sin_frase_es_missing_no_uncertain(ds):
    prep(ds)
    e = gf_log.record_audition("a1", "", None, [], [])
    assert e["response"] == "missing" and "decision" not in e
    assert all(o["listen"] == "unknown" for o in e["options"])


def test_escucha_desconocida_no_es_no_escuchada(ds):
    prep(ds)
    e = gf_log.record_audition("a1", "la B", "prefer", ["B"], ["B"])
    assert [o["listen"] for o in e["options"]] == ["unknown", "confirmed", "unknown"]
    assert "not_auditioned" not in {o["listen"] for o in e["options"]}


def test_varias_utiles_y_empates_no_son_preferencias(ds):
    prep(ds)
    gf_log.record_audition("a1", "A y C sirven", "multiple_useful", ["A", "C"], ["A", "B", "C"])
    gf_log.record_audition("a1", "no sé", "uncertain", [], ["A", "B", "C"])
    assert gf_log.preferences(gf_log.load_events()) == []
    gf_log.record_audition("a1", "la B, más redonda", "prefer", ["B"], ["A", "B"])
    assert gf_log.preferences(gf_log.load_events()) == [("opt1", "opt0")]   # C no se confirmó: no perdió


def test_prefer_lleva_una_sola(ds):
    prep(ds)
    with pytest.raises(SystemExit):
        gf_log.record_audition("a1", "A y B", "prefer", ["A", "B"], ["A", "B"])


def test_asset_exact_solo_si_el_hash_coincide(ds):
    (ds / "midi").mkdir()
    (ds / "midi" / "x.mid").write_bytes(b"uno")
    snap = ds / "snapshots" / "s1"
    snap.mkdir(parents=True)
    import hashlib
    (snap / "manifest.json").write_text(json.dumps({"creado": "2026-10-29T10:00:00", "archivos": [
        {"path": "dataset/midi/x.mid", "sha256": hashlib.sha256(b"uno").hexdigest()}]}))
    assert gf_log.asset("midi/x.mid")["match"] == "exact"
    (ds / "midi" / "x.mid").write_bytes(b"dos")
    assert gf_log.asset("midi/x.mid")["match"] == "unknown"


def test_cc_de_capa_ida_y_vuelta():
    for count in (8, 21, 32):
        for slot in range(1, count + 1):
            v = mau.cc_for_layer(slot, count)
            assert min(count, 1 + (v - 1) * count // 127) == slot


def test_bajo_del_estudio_en_la_dorica_y_fuera_de_choque():
    import make_estudio_clips as mec
    dorian = {0, 2, 3, 5, 7, 9, 10}                                # La dórica (kumoi: Am – D)
    notes = mec.clips()["bass_octavas_kumoi"]
    assert notes and all((n - 33) % 12 in dorian for _, _, n, _ in notes)
    assert {(n - 33) % 12 for t, _, n, _ in notes if t // (4 * 480) in (1, 3)} <= {5, 0, 10}   # Re y su 5.ª / 8.ª


def test_audicion_solo_midi(ds, tmp_path):
    (ds / "midi").mkdir()
    import make_test_clips as mtc
    mtc.write_mid(ds / "midi" / "x.mid", [(0, 480, 45, 90)], 124, "x")
    layers = [("UNO", b"\x01"), ("DOS", b"\x02"), ("TRES", b"\x03")]
    prep = mau.prepare(tmp_path / "aud", "x.mid", ["UNO", "TRES"], "UNO", layers, "p1", {"als": "set.als"}, seed=1)
    assert sorted(p.name for p in (tmp_path / "aud").iterdir()) == ["A.mid", "B.mid"]
    assert {o["patch"]["name"] for o in prep["options"]} == {"UNO", "TRES"}
    assert [o["kind"] for o in prep["options"] if o["patch"]["name"] == "UNO"] == ["keep_current"]
    with pytest.raises(SystemExit):
        mau.prepare(tmp_path / "aud", "x.mid", ["UNO"], "UNO", layers, "p1", {}, seed=1)

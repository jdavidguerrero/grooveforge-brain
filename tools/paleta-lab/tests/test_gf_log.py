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
    assert all((n - 33) % 12 in dorian for _, _, n, _ in mec.clips()["pluck_arp16_kumoi"])
    assert {(n - 33) % 12 for t, _, n, _ in notes if t // (4 * 480) in (1, 3)} <= {5, 0, 10}   # Re y su 5.ª / 8.ª


def test_audicion_solo_midi(ds, tmp_path):
    (ds / "midi").mkdir()
    import make_test_clips as mtc
    mtc.write_mid(ds / "midi" / "x.mid", [(0, 480, 45, 90)], 124, "x")
    layers = [("UNO", b"\x01"), ("DOS", b"\x02"), ("TRES", b"\x03")]
    prep = mau.prepare(tmp_path / "aud", "x.mid", ["UNO", "TRES"], "UNO", layers, "p1", {"als": "set.als"}, seed=1)
    assert sorted(p.name for p in (tmp_path / "aud").iterdir()) == ["A.mid", "B.mid"]   # sin "(sin)": las dos son capas
    assert {o["patch"]["name"] for o in prep["options"]} == {"UNO", "TRES"}
    assert [o["kind"] for o in prep["options"] if o["patch"]["name"] == "UNO"] == ["keep_current"]
    with pytest.raises(SystemExit):
        mau.prepare(tmp_path / "aud", "x.mid", ["UNO"], "UNO", layers, "p1", {}, seed=1)


def test_capa_solo_suena_solo_a():
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "preset-librarian"))
    import gf_presets as gp
    import make_solo_layers as msl
    lib = Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1" / "patches" / "brain" / "prophet_5"
    files = sorted(lib.glob("*.json")) if lib.is_dir() else []
    if not files:
        pytest.skip("sin librería local")
    pre = bytes.fromhex(json.loads(files[0].read_text())["raw"])
    fill = bytes.fromhex(json.loads(sorted((lib.parent / "juno_106").glob("*.json"))[0].read_text())["raw"])
    lay = gp.decode_layer(msl.solo_layer(pre, fill, "S PRUEBA"))
    assert lay["morph"] == 0 and lay["gain_b_db"] == -60
    assert (lay["engine_a"], lay["engine_b"]) == ("prophet_5", "juno_106")   # el firmware exige engines distintos
    with pytest.raises(ValueError):
        msl.solo_layer(pre, pre, "S MAL")
    assert lay["a"]["patch"] == gp.decode(pre)["patch"]


def test_roll_acelerado_dobla_la_densidad_cada_dos_compases():
    import make_estudio_clips as mec
    import make_perc_patterns as mp
    roll = mec.roll_acelerado()["roll_acelerado_brasil"]
    caja = [t for t, _, n, _ in roll if n == mp.ROLES["caja"]]
    bar = 4 * 480
    assert [sum(1 for t in caja if 2 * k * bar <= t < 2 * (k + 1) * bar) for k in range(4)] == [8, 16, 32, 64]
    vels = [v for t, _, n, v in roll if n == mp.ROLES["caja"]]
    assert vels[0] < vels[-1] <= 127
    tribe = [n for _, _, n, _ in mec.roll_acelerado()["roll_acelerado_tribe"] if n in (71, 72)]
    assert all(a != b for a, b in zip(tribe, tribe[1:]))                    # manos alternando golpe a golpe


def ficha(ds):
    (ds / "metadata" / "sesiones").mkdir(parents=True)
    (ds / "metadata" / "sesiones" / "2026-10-28_bass.json").write_text(json.dumps({
        "rol": "bass", "decisiones": [{"a": "GF AFRO SUB", "b": "ROLLER SUB", "elegido": "ambos (papeles distintos)",
                                       "por_que": "uno sub, otro mid"}],
        "prefiltro": {"probados": ["GF AFRO SUB", "WOODBASS (capa)"], "pasan": {"preset": "GF AFRO SUB"}},
        "elegidos": [{"sonido": "GF AFRO SUB", "ganador": True}, {"sonido": "ROLLER SUB"}],
        "rechazados": [{"sonido": "WOODBASS", "por_que": "flojo"}]}))


def test_migracion_idempotente_y_sin_inventar(ds):
    import migrate_v1 as mv
    ficha(ds)
    _, new = mv.migrate(write=True)
    assert len(new) == 3
    _, again = mv.migrate(write=True)
    assert again == [] and len(gf_log.load_events()) == 3
    dec = {e["migrated"]["source"].split("#")[1]: e for e in gf_log.load_events()}
    assert dec["decisiones[0]"]["decision"]["state"] == "multiple_useful"            # "ambos" no es preferencia
    assert dec["elegidos"]["decision"]["state"] == "prefer"
    for e in gf_log.load_events():
        assert all(o["patch"]["match"] in ("name_match_only", "unknown") for o in e["options"])   # nunca exact
        assert "not_auditioned" not in {o["listen"] for o in e["options"]}
    assert gf_log.preferences(gf_log.load_events()) == [("opt1", "opt2"), ("opt1", "opt3")]


def test_quitar_no_es_rechazo_y_las_reglas_las_confirma_juan(ds):
    u = gf_log.record_use("p", "removed", "pad")
    assert u["reason"] == "missing"
    r = gf_log.record_rule("ganan los que dejan espacio", "tendencia_observada", "mezcla completa", [])
    assert r["status"] == "hypothesis"
    with pytest.raises(SystemExit):
        gf_log.set_rule(r["event_id"], "confirmed", "")
    with pytest.raises(SystemExit):
        gf_log.record_rule("riser + roll", "preferencia_declarada", "breaks", [])
    gf_log.set_rule(r["event_id"], "confirmed", "sí, así lo siento")
    assert gf_log.rule_state(gf_log.load_events())[r["event_id"]]["status"] == "confirmed"
    assert gf_log.stats(gf_log.load_events())["reglas"]["confirmed"] == 1

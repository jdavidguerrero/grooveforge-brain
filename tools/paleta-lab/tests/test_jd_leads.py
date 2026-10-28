"""Tests de los presets JD LEAD (apps/docs/produccion/lab-leads.md §5): los cambios contra la estridencia."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import make_jd_leads as mj  # noqa: E402
from make_jd_leads import g  # noqa: E402


def blank(**over) -> dict:
    """Un patch de formato 5 con valores 'estridentes', como los originales."""
    p = {k: g._nest([0.0 if t == "f" else 0] * g._count(s), s) if s else 0 for k, t, s in g._layout(g.FORMAT)}
    p.update(attack_ms=3.0, sustain=0.8, release_ms=400.0, cutoff_hz=2500.0, reso_norm=0.25)
    p["lfo_amt"] = [0.3, 0.08, 0.0, 0.0]
    p["osc"][0][0], p["osc"][0][4], p["osc"][0][6] = 3.0, 1.0, 50.0
    p["mod"][0][0] = 20.0
    p["groove"][mj.UNISON][:2] = [2.0, 18.0]
    p.update(over)
    return p


def common(p):
    assert 10 <= p["attack_ms"] <= 20 and p["sustain"] <= 0.6 and p["release_ms"] <= 250
    assert p["cutoff_hz"] <= 1400 and p["reso_norm"] <= 0.2 and p["f_decay_ms"] <= 250
    assert p["lfo_amt"][mj.DEST_PITCH] == 0 and p["vel_cut"] >= 50


def test_molde_comun_y_cambios_por_preset():
    for fn in (mj.melodic, mj.body, mj.desert_a, mj.desert_b):
        common(fn(blank()))
    m = mj.melodic(blank())
    assert m["groove"][mj.UNISON][:2] == [1.0, 10.0] and m["groove"][mj.MACRO][mj.GRIT] == 30
    for fn in (mj.body, mj.desert_b):
        p = fn(blank())
        assert p["osc"][0][0] == 0 and p["osc"][0][4] == 0 and p["mod"][0][0] == 0      # sin sync ni poly-mod
    a = mj.desert_a(blank())
    assert a["osc"][0][6] < 50 and a["lfo_amt"][mj.DEST_CUTOFF] < 0.3
    src = blank()
    mj.melodic(src)
    assert src["cutoff_hz"] == 2500                                                     # no toca el original


def test_build_escribe_presets_y_capa(tmp_path):
    lib = tmp_path / "brain"
    for engine, name in (("ob_6", "GF MELODIC LEAD"), ("prophet_5", "JD RUN LEAD BODY")):
        (lib / engine).mkdir(parents=True)
        raw = g.encode(g.ENGINES.index(engine), name, blank())
        (lib / engine / f"{g.slug(name)}.json").write_text(json.dumps(g.entry_from_raw(raw)))
    half = lambda e, n: g.encode(e, n, blank())                                          # noqa: E731
    head = g.LAYER_MAGIC + bytes([g.LAYER_FORMAT]) + b"DESERT LEAD".ljust(12, b"\0") + bytes([3, 2, 50, 0, 0, 0, 253])
    raw = head + half(3, "HY DESERT LEAD A") + half(2, "HY DESERT LEAD B")
    (lib / "layers").mkdir()
    (lib / "layers" / "desert-lead.json").write_text(json.dumps(g.layer_entry_from_raw(raw)))
    out = mj.build(lib)
    assert set(out) == {"ob_6/jd-lead-melodic.json", "prophet_5/jd-lead-body.json", "layers/jd-desert-ld.json"}
    for rel, e in out.items():
        assert e["role"] == "lead" and e["origin"] == "propio"
        if rel.startswith("layers/"):
            d = g.decode_layer(bytes.fromhex(e["raw"]))
            assert (d["name"], d["a"]["name"], d["b"]["name"]) == ("JD DESERT LD", "JD DESERT A", "JD DESERT B")
            assert (d["morph"], d["gain_b_db"]) == (50, -3)                                # la mezcla de la capa no cambia
            common(d["a"]["patch"]); common(d["b"]["patch"])
        else:
            d = g.decode(bytes.fromhex(e["raw"]))
            assert d["name"] == e["name"] and d["format"] == g.FORMAT
            common(d["patch"])

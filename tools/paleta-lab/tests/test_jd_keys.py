"""Tests de los presets JD KEYS (apps/docs/produccion/lab-acordes.md §8): los rasgos del Electric."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import make_jd_keys as mk  # noqa: E402
from make_jd_leads import g  # noqa: E402
from test_jd_leads import blank  # noqa: E402


def keys_traits(p):
    assert p["sustain"] <= 0.2 and p["decay_ms"] >= 2000                 # decae solo (rasgo 4)
    assert p["vel_cut"] >= 60 and p["env_amt"] > 0 and p["f_sustain"] == 0 and p["f_decay_ms"] <= 500   # golpe y bark
    assert p["cutoff_hz"] <= 1000 and not any(p["lfo_amt"]) and not any(p["sfx_on"])


def test_juno_y_prophet():
    j = mk.juno(blank())
    keys_traits(j)
    assert j["osc"][0][4] == mk.JUNO_CHORUS_I and j["osc"][0][0] == mk.JUNO_WAVE_PULSE
    p = mk.prophet(blank())
    keys_traits(p)
    a, b, mix = p["osc"][0], p["osc"][1], p["osc"][2]
    assert b[5] == 1 and b[4] == b[6] == 0                                # B solo triángulo: el cuerpo
    assert a[0] == 24 and a[4] == 0 and mix[0] < mix[1] / 4               # A arriba, sin sync y bajito: el clic
    src = blank()
    mk.juno(src)
    assert src["cutoff_hz"] == 2500                                       # no toca el original


def test_tine_se_apaga_rapido():
    t = mk.tine(blank())
    assert t["sustain"] == 0 and t["decay_ms"] + t["release_ms"] <= 300
    assert t["osc"][0][2] == 5 and t["osc"][1][0] == mk.TINE_RATIO       # algoritmo 5, modulador 2 a 14:1
    for op in t["osc"]:
        assert op[6] == 0 and op[5] <= 200 and op[7] <= 200              # cada operador: S 0, D y R cortos
    assert t["osc"][2][1] == t["osc"][3][1] == 0                         # la otra pila, muda


def test_build_escribe_presets_y_capa(tmp_path):
    lib = tmp_path / "brain"
    for engine, name in (("juno_106", "PLUCK KEYS"), ("prophet_5", "WARM KEYS"), ("dx7", "E.PIANO")):
        (lib / engine).mkdir(parents=True)
        raw = g.encode(g.ENGINES.index(engine), name, blank())
        (lib / engine / f"{g.slug(name)}.json").write_text(json.dumps(g.entry_from_raw(raw)))
    out = mk.build(lib)
    assert set(out) == {"juno_106/jd-keys-juno.json", "prophet_5/jd-keys-prophet.json", "layers/jd-keys-capa.json"}
    for rel, e in out.items():
        assert e["role"] == "chords" and e["origin"] == "propio"
        if rel.startswith("layers/"):
            d = g.decode_layer(bytes.fromhex(e["raw"]))
            assert (d["name"], d["engine_a"], d["engine_b"]) == ("JD KEYS CAPA", "juno_106", "dx7")
            assert (d["a"]["name"], d["b"]["name"], d["gain_b_db"]) == ("JD KEYS BODY", "JD KEYS TINE", mk.CAPA_GAIN_B_DB)
            assert d["a"]["patch"] == g.decode(bytes.fromhex(out["juno_106/jd-keys-juno.json"]["raw"]))["patch"]
        else:
            d = g.decode(bytes.fromhex(e["raw"]))
            assert d["name"] == e["name"] and d["format"] == g.FORMAT
            keys_traits(d["patch"])

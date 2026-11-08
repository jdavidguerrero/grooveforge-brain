"""Tests de las atmósferas de agua y aire (apps/docs/produccion/motor-pads.md §5.1)."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import make_jd_agua as mk  # noqa: E402
from make_jd_leads import g  # noqa: E402
from test_jd_leads import blank  # noqa: E402


def test_solo_ruido_y_lo_mueve_el_lfo():
    for _, receta in mk.SOLOS:
        p = mk.arp_ruido(blank(), **receta)
        assert p["osc"][0][3] == 100 and p["osc"][0][4] == 0 and p["osc"][1][4] == 0      # solo ruido
        assert p["sustain"] == 1 and p["attack_ms"] >= 500 and p["env_amt"] == 0
        assert p["lfo_amt"][0] > 0 and 0.05 <= p["lfo_rate_hz"] <= 20
        assert not any(p["sfx_on"])
    for _, _, _, ra, rb in mk.CAPAS:
        b = mk.ob6_ruido(blank(), **rb)
        assert b["osc"][0][3] == 100 and b["osc"][0][2] == b["osc"][1][2] == b["osc"][0][1] == 0


def test_gotas_y_rio_usan_sample_and_hold():
    recetas = dict(mk.SOLOS)
    assert recetas["JD GOTAS"]["lfo_shape"] == mk.SH and recetas["JD GOTAS"]["reso"] >= 0.8
    assert recetas["JD AIRE"]["lfo_shape"] == mk.SINE and recetas["JD AIRE"]["lfo_rate"] <= 0.2
    rio = next(c for c in mk.CAPAS if c[0] == "JD RIO")
    assert rio[3]["lfo_shape"] == mk.SH and rio[4]["lfo_rate"] < 0.1


def test_build_escribe_dos_presets_y_dos_capas(tmp_path):
    lib = tmp_path
    for engine, name in (("arp_2600", "GF NOISE RISER"), ("ob_6", "GF DARK DRONE")):
        (lib / engine).mkdir(parents=True)
        raw = g.encode(g.ENGINES.index(engine), name, blank())
        (lib / engine / f"{g.slug(name)}.json").write_text(json.dumps(g.entry_from_raw(raw)))
    out = mk.build(lib)
    assert set(out) == {"arp_2600/jd-aire.json", "arp_2600/jd-gotas.json", "layers/jd-rio.json", "layers/jd-cascada.json",
                        "arp_2600/jd-caida.json", "arp_2600/jd-swell.json"}
    capa = out["layers/jd-rio.json"]
    assert capa["engine_a"] == "arp_2600" and capa["engine_b"] == "ob_6" and capa["role"] == "atmos"
    assert g.decode(bytes.fromhex(out["arp_2600/jd-aire.json"]["raw"]))["name"] == "JD AIRE"


def test_fx_caida_se_cierra_y_swell_crece_y_corta():
    r = dict(mk.FX)
    c = mk.fx_ruido(blank(), **r["JD CAIDA"])
    assert c["attack_ms"] <= 10 and c["sustain"] == 0 and c["env_amt"] > 0 and c["f_sustain"] == 0 and c["f_decay_ms"] >= 2000
    s = mk.fx_ruido(blank(), **r["JD SWELL"])
    assert 1500 <= s["attack_ms"] <= 2100 and s["f_attack_ms"] == s["attack_ms"] and s["release_ms"] <= 100
    for p in (c, s):
        assert p["osc"][0][3] == 100 and p["osc"][0][4] == 0 and not any(p["lfo_amt"])

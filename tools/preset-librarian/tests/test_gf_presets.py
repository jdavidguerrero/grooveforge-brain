"""Tests del bibliotecario (sin Brain conectado): formato binario, ida y vuelta, renombrar y fichas."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gf_presets as gp  # noqa: E402


def sample_patch(fmt=gp.FORMAT):
    patch = {}
    for key, typ, shape in gp._layout(fmt):
        n = gp._count(shape)
        if key in ("sfx_on", "groove_on"):
            vals = [bool(k % 2) for k in range(n)]
        elif typ == "B":
            vals = [k % 3 for k in range(n)]
        else:
            vals = [round(0.25 * k + 1.5, 6) for k in range(n)]
        patch[key] = vals[0] if shape == () else gp._nest(vals, shape)
    return patch


def test_tamanos_coinciden_con_el_firmware():
    for fmt, size in gp.SIZE.items():
        raw = gp.encode(2, "X", sample_patch(fmt), fmt)
        assert len(raw) == size


def test_ida_y_vuelta_exacta():
    raw = gp.encode(3, "JD HORIZON NOTCH", sample_patch())
    d = gp.decode(raw)
    assert d["engine"] == 3 and d["name"] == "JD HORIZON NOTCH" and d["format"] == gp.FORMAT
    assert gp.encode(d["engine"], d["name"], d["patch"]) == raw
    assert d["patch"]["vel_cut"] == sample_patch()["vel_cut"]


def test_formatos_viejos_se_leen():
    raw = gp.encode(1, "VIEJO", sample_patch(2), 2)
    d = gp.decode(raw)
    assert "groove" not in d["patch"] and d["format"] == 2


def test_renombrar_y_slug():
    raw = gp.encode(0, "USER 01", sample_patch())
    raw2 = gp.rename(raw, "MI BAJO FUNK CON GHOST")      # se corta a 16
    assert gp.decode(raw2)["name"] == "MI BAJO FUNK CON"
    assert raw2[5 + gp.NAME_LEN:] == raw[5 + gp.NAME_LEN:]
    assert gp.slug("JD RUN FUNKBASS") == "jd-run-funkbass"


def test_export_conserva_la_ficha(tmp_path):
    raw = gp.encode(1, "MI STAB", sample_patch())
    old = {"role": "chords", "tags": ["disco"], "notes": "con la tambora", "rating": 4,
           "permissions": {"train": False, "publish": True}}
    e = gp.entry_from_raw(raw, old)
    assert e["role"] == "chords" and e["tags"] == ["disco"] and e["engine"] == "juno_106"
    assert e["rating"] == 4 and e["permissions"]["train"] is False          # la ficha se conserva entera
    assert gp.entry_from_raw(gp.encode(1, "USER 03", sample_patch()))["origin"] == "propio"
    assert gp.entry_from_raw(gp.encode(1, "JD VELVET JUNO", sample_patch()))["origin"] == "fabrica-brain"
    f = tmp_path / "mi-stab.json"
    f.write_text(json.dumps({**e, "name": "MI STAB 2"}))
    engine, raw2 = gp.load_entry(str(f))                   # el nombre del JSON manda al subir
    assert engine == 1 and gp.decode(raw2)["name"] == "MI STAB 2"


def test_lugares_de_banco():
    assert gp.slot(0) == "A-01" and gp.slot(15) == "A-16" and gp.slot(16) == "B-01" and gp.slot(63) == "D-16"

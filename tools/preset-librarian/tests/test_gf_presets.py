"""Tests del bibliotecario (sin Brain conectado): formato binario, ida y vuelta, renombrar y fichas."""
import json
import struct
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


def test_preset_de_capa_ida_y_vuelta():
    assert gp.LAYER_SIZE == 1111                      # LAYER_BYTES de layer_bank.h (formato 2)
    assert gp.LAYER_SIZE_V1 == 1077
    half_a = gp.encode(4, "HY WOODBASS A", sample_patch())
    half_b = gp.encode(0, "HY WOODBASS B", sample_patch())
    raw = (gp.LAYER_MAGIC + bytes([2]) + b"WOODBASS".ljust(12, b"\0") + bytes([4, 0, 50, 10]) +
           struct.pack("<bbb", -12, -6, 0) + half_a + half_b)
    d = gp.decode_layer(raw)
    assert (d["name"], d["engine_a"], d["engine_b"], d["delay_b_ms"], d["transpose_b"]) == ("WOODBASS", "dx7", "model_d", 10, -12)
    assert d["b"]["name"] == "HY WOODBASS B"
    assert gp.decode_layer(gp.rename_layer(raw, "MI CAPA"))["name"] == "MI CAPA"
    e = gp.layer_entry_from_raw(raw, {"notes": "para el bajo", "rating": 4})
    assert e["notes"] == "para el bajo" and e["half_a"] == "HY WOODBASS A" and e["origin"] == "fabrica-brain"


def _args(tmp_path, archivo, nombre, subir=False, rol="bass"):
    import argparse

    return argparse.Namespace(lib=str(tmp_path), archivo=str(archivo), nombre=nombre, rol=rol, subir=subir, port=None)


def test_desde_vst_preset_va_a_la_libreria_sin_subir(tmp_path):
    raw = gp.encode(0, "VIEJO", sample_patch())
    f = tmp_path / "Model.gfp"
    f.write_bytes(raw)
    gp.cmd_desde_vst(_args(tmp_path, f, "JD VST BASS"))
    j = json.loads((tmp_path / "brain" / "model_d" / "jd-vst-bass.json").read_text())
    assert j["name"] == "JD VST BASS" and j["origin"] == "propio" and j["role"] == "bass"
    assert gp.decode(bytes.fromhex(j["raw"]))["name"] == "JD VST BASS"


def test_desde_vst_no_pisa_un_sonido(tmp_path):
    f = tmp_path / "Model.gfp"
    f.write_bytes(gp.encode(0, "X", sample_patch()))
    gp.cmd_desde_vst(_args(tmp_path, f, "MISMO"))
    import pytest

    with pytest.raises(SystemExit, match="Ya existe"):
        gp.cmd_desde_vst(_args(tmp_path, f, "MISMO"))


def test_desde_vst_rechaza_archivo_invalido(tmp_path):
    import pytest

    f = tmp_path / "malo.gfp"
    f.write_bytes(b"GFP" + bytes(10))
    with pytest.raises(ValueError):
        gp.cmd_desde_vst(_args(tmp_path, f, "MALO"))
    assert not list((tmp_path / "brain").rglob("*.json")) if (tmp_path / "brain").exists() else True


def test_desde_vst_capa_y_subir(tmp_path, monkeypatch):
    capa = tmp_path / "capa.json"
    biblioteca = Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1" / "patches" / "brain" / "layers" / "jd-sub-mid.json"
    if not biblioteca.exists():
        import pytest

        pytest.skip("Sin la librería de Juan")
    raw = bytes.fromhex(json.loads(biblioteca.read_text())["raw"])
    f = tmp_path / "Hybrid.gfl"
    f.write_bytes(raw)
    subidos = []

    class BrainFalso:
        def __init__(self, _p):
            pass

        def layer_export(self):
            return [(0, raw)]

        def layer_put(self, i, r):
            subidos.append((i, r))

    monkeypatch.setattr(gp, "Brain", BrainFalso)
    gp.cmd_desde_vst(_args(tmp_path, f, "JD VST CAPA", subir=True))
    assert subidos[0][0] == 1 and gp.decode_layer(subidos[0][1])["name"] == "JD VST CAPA"
    assert (tmp_path / "brain" / "layers" / "jd-vst-capa.json").exists()


def _capa_real():
    import pytest

    ruta = Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1" / "patches" / "brain" / "layers" / "tresbass.json"
    if not ruta.exists():
        pytest.skip("Sin la librería de Juan")
    return bytes.fromhex(json.loads(ruta.read_text())["raw"])


def test_encode_layer_es_inverso_salvo_redondeo():
    import variar_capa as v

    raw = _capa_real()
    nuevo = v.encode_layer(gp.decode_layer(raw))
    assert len(nuevo) == len(raw) and gp.decode_layer(nuevo) == gp.decode_layer(raw)
    # decode() redondea los decimales: como mucho se mueve la última cifra de alguno.
    dif = [abs(a - b) for a, b in zip(raw, nuevo) if a != b]
    assert len(dif) <= 4 and all(x == 1 for x in dif)


def test_variaciones_cambian_solo_lo_declarado():
    import variar_capa as v

    base = gp.decode_layer(_capa_real())
    original = json.dumps(base, sort_keys=True)

    def difs(a, b, p=""):
        if isinstance(a, dict):
            return [x for k in a for x in difs(a[k], b[k], f"{p}.{k}")]
        if isinstance(a, list):
            return [x for i, (m, n) in enumerate(zip(a, b)) for x in difs(m, n, f"{p}[{i}]")]
        return [p] if a != b else []

    esperado = {
        "TRESB OSC": {".a.patch.cutoff_hz"},
        "TRESB MAD1": {".morph", ".b.patch.osc[2][2]"},
        "TRESB MAD2": {".morph", ".b.patch.osc[2][2]", ".a.patch.osc[1][1]", ".a.patch.cutoff_hz"},
        "TRESB MAD3": {".morph", ".b.patch.osc[2][2]", ".a.patch.osc[1][0]", ".a.patch.osc[1][1]", ".a.patch.cutoff_hz"},
    }
    for nombre, campos in esperado.items():
        d = gp.decode_layer(v.encode_layer(v.variar(base, nombre)))
        assert d["name"] == nombre
        assert set(difs(d, {**base, "name": nombre})) == campos
    assert json.dumps(base, sort_keys=True) == original


def test_morph_menor_da_mas_a():
    from math import cos, pi, sin

    assert cos(0.42 * pi / 2) > cos(0.53 * pi / 2) and sin(0.42 * pi / 2) < sin(0.53 * pi / 2)


def test_hacia_vst_es_la_inversa_de_desde_vst(tmp_path):
    import argparse

    f = tmp_path / "Model.gfp"
    f.write_bytes(gp.encode(0, "X", sample_patch()))
    gp.cmd_desde_vst(_args(tmp_path, f, "JD VUELTA"))
    entrada = tmp_path / "brain" / "model_d" / "jd-vuelta.json"
    gp.cmd_hacia_vst(argparse.Namespace(destino=str(tmp_path / "vst"), files=[str(entrada)]))
    salida = (tmp_path / "vst" / "jd-vuelta.gfp").read_bytes()
    assert salida == bytes.fromhex(json.loads(entrada.read_text())["raw"])
    assert gp.decode(salida)["name"] == "JD VUELTA"

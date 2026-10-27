"""Tests de la ficha de sesión (sin tocar el dataset real: GF_DATASET apunta a una carpeta temporal)."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gf_session as gs  # noqa: E402


def test_flujo_de_una_sesion(tmp_path, monkeypatch, capsys):
    monkeypatch.setenv("GF_DATASET", str(tmp_path))
    gs.main(["new", "bass", "JD RUN FUNKBASS", "GF POCKET BASS", "--fecha", "2026-10-28", "--ganancia", "11 h"])
    gs.main(["decide", "JD RUN FUNKBASS", "GF POCKET BASS", "a", "las ghost notes se sienten", "--contexto", "afro 124"])
    gs.main(["decide", "JD RUN FUNKBASS", "GF POCKET BASS", "GF POCKET BASS", "más redondo con el kick"])
    gs.main(["reject", "HY WOODBASS B", "tapa la tambora"])
    gs.main(["permiso", "LOOP TRIBE 03", "tribe-instruments", "--no-entrenar"])
    d = json.loads((tmp_path / "metadata" / "sesiones" / "2026-10-28_bass.json").read_text())
    assert d["montaje"]["ganancia_interfaz"] == "11 h" and d["montaje"]["bpm"] == 124
    assert d["decisiones"][0]["elegido"] == "a" and d["decisiones"][0]["contexto"] == "afro 124"
    assert d["decisiones"][1]["elegido"] == "b"                     # por nombre
    assert d["rechazados"][0]["sonido"] == "HY WOODBASS B"
    assert d["origen_permisos"]["LOOP TRIBE 03"] == {"origen": "tribe-instruments", "entrenar": False, "publicar": True}
    gs.main(["show"])
    out = capsys.readouterr().out
    assert "✔ JD RUN FUNKBASS > GF POCKET BASS" in out and "✘ HY WOODBASS B" in out


def test_dos_sesiones_del_mismo_rol_el_mismo_dia(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path))
    gs.main(["new", "pad", "--fecha", "2026-10-28"])
    gs.main(["new", "pad", "--fecha", "2026-10-28"])
    names = sorted(f.name for f in (tmp_path / "metadata" / "sesiones").glob("*.json"))
    assert names == ["2026-10-28_pad.json", "2026-10-28_pad_2.json"]

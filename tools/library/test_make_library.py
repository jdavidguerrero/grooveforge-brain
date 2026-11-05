from __future__ import annotations

import json
from pathlib import Path

import mido
import pytest

from make_library import _desglosar_patron, construir


def _midi(ruta: Path, nota: int = 60) -> None:
    ruta.parent.mkdir(parents=True, exist_ok=True)
    m = mido.MidiFile()
    t = mido.MidiTrack()
    t += [mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(124)), mido.Message("note_on", note=nota, velocity=90, time=0), mido.Message("note_off", note=nota, time=480)]
    m.tracks.append(t)
    m.save(ruta)


def _binario(ruta: Path, contenido: bytes = b"wav") -> None:
    ruta.parent.mkdir(parents=True, exist_ok=True)
    ruta.write_bytes(contenido)


@pytest.fixture
def dataset(tmp_path):
    d = tmp_path / "ds"
    r = d / "renders/perc-synth"
    _binario(r / "v0.4/organic_A_menor_122/golpes/kick/51_kick_base.wav", b"kick")
    (r / "v0.4/manifiesto.jsonl").write_text(
        json.dumps({"archivo": "organic_A_menor_122/golpes/kick/51_kick_base.wav", "material": "piel", "seed_timbre": 1, "afinar_hz": 55.0}) + "\n"
    )
    _binario(r / "v0.4/organic_A_menor_122/loop_organic_A_menor_122.wav", b"loop")
    _binario(r / "v0.4/organic_A_menor_122/audicion/kick.wav", b"aud")
    _binario(r / "v0.3/variantes/madera/kick/51_kick_madera_v01.wav", b"mad")
    _binario(r / "v0.2/kits/808_piel/51_kick_01.wav", b"viejo")
    _midi(d / "midi/perc/organic_house/top/peak/v2/cumbia_afro_v2_peak_tribe.mid")
    _midi(d / "midi/perc/favoritos/organic_house/top/peak/v2/cumbia_afro_v2_peak_tribe.mid")
    _midi(d / "midi/perc/organic_house/fills/entrada/top/mapale_entrada_2bar_tribe.mid", 61)
    _midi(d / "midi/adn/lead/lab/f1_menores/eolia__corta__organic.mid", 62)
    _midi(d / "midi/adn/contexto/A_tierra/perc_tribe.mid", 63)
    _midi(d / "midi/bass/bass_ghost_notes.mid", 64)
    _binario(d / "kits/racks/GF Brasil.adg", b"rack")
    _binario(d / "snapshots/2026-10-29/dataset/copia.mid", b"no")
    return d


def _catalogo(lib: Path) -> dict[str, dict]:
    filas = (json.loads(x) for x in (lib / "_catalogo/catalogo.jsonl").read_text().splitlines())
    return {f["ruta"]: f for f in filas}


def test_estructura(dataset, tmp_path):
    lib = tmp_path / "lib"
    construir(dataset, lib)
    for ruta in (
        "01 One Shots/Kick/Organic/kick_organic_Amenor_base.wav",
        "01 One Shots/Kick/Madera/kick_madera_v01.wav",
        "03 Drums MIDI/Tops/Organic House/Tribe/cumbia_afro_v2_peak_tribe.mid",
        "03 Drums MIDI/Favoritos/Organic House/Tops/cumbia_afro_v2_peak_tribe.mid",
        "03 Drums MIDI/Fills/Organic House/Tribe/Tops/mapale_entrada_2bar_tribe.mid",
        "04 Melodic MIDI/Leads/f1_menores/eolia__corta__organic.mid",
        "05 Loops/Organic/loop_organic_A_menor_122.wav",
        "06 Racks/GF Brasil.adg",
        "07 Contexto/Contexto/A_tierra/perc_tribe.mid",
        "07 Contexto/Pruebas/bass_ghost_notes.mid",
        "08 Audiciones/Organic/kick_organic.wav",
    ):
        assert (lib / ruta).exists(), ruta
    assert (lib / "00 Elegidos").is_dir()


def test_excluye_snapshots_y_v02(dataset, tmp_path):
    lib = tmp_path / "lib"
    construir(dataset, lib)
    origenes = [f["origen"] for f in _catalogo(lib).values()]
    assert not any("snapshots" in o or "v0.2" in o for o in origenes)


def test_top_y_perc_con_igual_nombre_no_chocan(dataset, tmp_path):
    _midi(dataset / "midi/perc/organic_house/fills/entrada/perc/mapale_entrada_2bar_tribe.mid", 72)
    _midi(dataset / "midi/perc/favoritos/organic_house/perc/peak/v2/cumbia_afro_v2_peak_tribe.mid", 73)
    lib = tmp_path / "lib"
    construir(dataset, lib)
    assert (lib / "03 Drums MIDI/Fills/Organic House/Tribe/Perc/mapale_entrada_2bar_tribe.mid").exists()
    assert (lib / "03 Drums MIDI/Favoritos/Organic House/Perc/cumbia_afro_v2_peak_tribe.mid").exists()
    assert not [p for p in lib.rglob("*.mid") if "__" in p.stem and "eolia" not in p.stem]


def test_catalogo_para_ia(dataset, tmp_path):
    lib = tmp_path / "lib"
    construir(dataset, lib)
    c = _catalogo(lib)
    k = c["01 One Shots/Kick/Organic/kick_organic_Amenor_base.wav"]
    assert (k["tonalidad"], k["bpm"], k["material"], k["autoria"]) == ("A menor", 122, "piel", "generado")
    assert k["sintesis"]["afinar_hz"] == 55.0 and not Path(k["ruta"]).is_absolute()
    assert c["03 Drums MIDI/Favoritos/Organic House/Tops/cumbia_afro_v2_peak_tribe.mid"]["decision_juan"] == "favorito"
    top = c["03 Drums MIDI/Tops/Organic House/Tribe/cumbia_afro_v2_peak_tribe.mid"]
    assert top["decision_juan"] == "favorito" and top["seccion"] == "peak" and top["version"] == 2
    assert c["03 Drums MIDI/Fills/Organic House/Tribe/Tops/mapale_entrada_2bar_tribe.mid"]["decision_juan"] is None
    assert top["midi"]["notas"] == 1 and top["midi"]["bpm_archivo"] == 124
    assert c["06 Racks/GF Brasil.adg"]["uso_ia"] == "excluido"
    assert c["08 Audiciones/Organic/kick_organic.wav"]["uso_ia"] == "excluido"


def test_grupo_une_variantes_del_mismo_patron():
    a, b, c = (_desglosar_patron(x) for x in ("cumbia_afro_intro_tribe", "cumbia_afro_v3_peak_808", "cumbia_afro_v2_main_q_djembe"))
    assert a["patron"] == b["patron"] == c["patron"] == "cumbia_afro"
    assert {_desglosar_patron(x)["patron"] for x in ("baiao_tribe", "baiao_q_tribe", "baiao_break_tribe", "baiao_remate_2beat_tribe")} == {"baiao"}
    assert (a["version"], b["version"], b["rack"]) == (1, 3, "808")


def test_segunda_corrida_no_copia(dataset, tmp_path):
    lib = tmp_path / "lib"
    primera = construir(dataset, lib)
    assert primera["copiados"] == primera["archivos"]
    segunda = construir(dataset, lib)
    assert segunda["copiados"] == 0 and segunda["protegidos"] == []


def test_edicion_de_juan_se_conserva_siempre(dataset, tmp_path):
    lib = tmp_path / "lib"
    construir(dataset, lib)
    ruta = lib / "06 Racks/GF Brasil.adg"
    ruta.write_bytes(b"editado")
    for _ in range(2):
        r = construir(dataset, lib)
        assert ruta.read_bytes() == b"editado" and "06 Racks/GF Brasil.adg" in r["protegidos"]


def test_origen_actualizado_si_no_hubo_edicion(dataset, tmp_path):
    lib = tmp_path / "lib"
    construir(dataset, lib)
    (dataset / "kits/racks/GF Brasil.adg").write_bytes(b"rack v2")
    construir(dataset, lib)
    assert (lib / "06 Racks/GF Brasil.adg").read_bytes() == b"rack v2"


def test_archivos_propios_no_se_borran_y_dataset_intacto(dataset, tmp_path):
    lib = tmp_path / "lib"
    construir(dataset, lib)
    propio = lib / "00 Elegidos/mi_kick.wav"
    propio.write_bytes(b"mio")
    antes = {p: p.read_bytes() for p in dataset.rglob("*") if p.is_file()}
    construir(dataset, lib)
    assert propio.read_bytes() == b"mio"
    assert antes == {p: p.read_bytes() for p in dataset.rglob("*") if p.is_file()}


def test_colision_con_otro_contenido_no_pierde_nada(dataset, tmp_path):
    _midi(dataset / "midi/perc/organic_house/top/main/v1/cumbia_afro_v2_peak_tribe.mid", 70)
    lib = tmp_path / "lib"
    construir(dataset, lib)
    carpeta = lib / "03 Drums MIDI/Tops/Organic House/Tribe"
    assert len(list(carpeta.glob("cumbia_afro_v2_peak_tribe*.mid"))) == 2

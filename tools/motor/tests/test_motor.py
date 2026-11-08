from __future__ import annotations

import copy
import json
import re
import sys
from pathlib import Path
import urllib.error
import urllib.request
import threading

import pytest
import soundfile as sf

from gf_motor.bajo import COMPASES, FAMILIAS, PASOS_KICK, fundamental, patron
from gf_motor.borrador import COLORES, PLANTILLAS, clip_kick, clips_guia
from gf_motor.drums import CLAVE, CLAP, HAT_ABIERTO, HAT_CERRADO, SNARE, analizar, clip_drums, opciones as opciones_drums
from gf_motor.edicion import diferencia
from gf_motor.brain import cc_de_capa
from gf_motor.tops import candidatos, clip_loop, encaje, leer_midi
from gf_motor.motivo import COLOR, RITMOS_FIJOS, TOPE_OCUPACION, motivo as crear_motivo, ocupacion, opciones as ritmos_motivo
from gf_motor.motor import MODOS
from gf_motor.acordes import NOMBRES, PROGRESIONES, REG_MAX, REG_MIN, choques, clases_acorde, clip_acordes, nombre_acorde, nombres_progresion, patrones as patrones_acordes
from gf_motor.live import ClienteLive, ErrorLive
from gf_motor.motor import Motor, SetProtegido, gf_log
from gf_motor.servidor import crear_servidor
import gzip
import xml.etree.ElementTree as ET

from gf_motor.rack import crear_rack
from live_simulado import LiveSimulado


_PAD = """<DrumBranchPreset Id="9"><Name Value="" /><DevicePresets><MultiSamplePart><Name Value="Viejo" /><SampleRef><FileRef><RelativePathType Value="6" /><RelativePath Value="Samples/Viejo.aif" /><Path Value="/x/Viejo.aif" /><OriginalFileSize Value="5" /><OriginalCrc Value="7" /></FileRef><LastModDate Value="1" /><SourceContext><SourceContext Id="0"><OriginalFileRef><FileRef Id="1"><Path Value="/y/Viejo.aif" /></FileRef></OriginalFileRef></SourceContext></SourceContext><DefaultDuration Value="1000" /><DefaultSampleRate Value="44100" /></SampleRef></MultiSamplePart></DevicePresets><ZoneSettings><ReceivingNote Value="92" /></ZoneSettings></DrumBranchPreset>"""


def _plantilla(ruta):
    xml = f'<Ableton><GroupDevicePreset><Device><DrumGroupDevice Id="0"><UserName Value="" /></DrumGroupDevice></Device><BranchPresets>{_PAD}{_PAD}</BranchPresets></GroupDevicePreset></Ableton>'
    ruta.write_bytes(gzip.compress(xml.encode()))
    return ruta


class BrainFalso:
    def __init__(self):
        self.enviados = []

    def capas(self):
        return ["ACID ARP", "JD SUB MID", "KALIMBA", "ROLLER SUB", "WOODBASS", "OTRA"]

    def elegir_capa(self, indice, total):
        self.enviados.append((indice, total))


@pytest.fixture
def entorno(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path / "dataset"))
    monkeypatch.setenv("GF_RACK_PLANTILLA", str(_plantilla(tmp_path / "molde.adg")))
    live = LiveSimulado()
    (tmp_path / "GrooveForge_Library").mkdir()
    motor = Motor(ClienteLive(puerto=live.puerto), tmp_path / "GrooveForge_Library", BrainFalso())
    yield live, motor
    live.cerrar()


def _eventos(tipo):
    return [e for e in gf_log.load_events() if e["type"] == tipo]


def nota(inicio, pitch=60, vel=110, dur=0.25):
    return {"pitch": pitch, "start_beat": inicio, "duration_beats": dur, "velocity": vel}


# ── Plantilla ──
def test_plantilla_organic_dura_lo_del_clip_fantasma():
    assert sum(c for _, c in PLANTILLAS["organic"]) == 212


def test_bloques_contiguos_en_mayuscula_y_con_color():
    g = clips_guia("organic")
    assert all(a["inicio"] + a["longitud"] == b["inicio"] for a, b in zip(g, g[1:]))
    assert all(c["nombre"].isupper() and c["color"] == COLORES[c["nombre"]] for c in g)
    assert len({c["color"] for c in g}) == 5


def test_kick_son_16_compases_a_negras():
    c = clip_kick(36)
    assert c["inicio"] == 0 and c["longitud"] == 64 and len(c["notas"]) == 64
    assert {n["pitch"] for n in c["notas"]} == {36}


# ── Diferencias ──
def test_sin_cambios():
    assert diferencia([nota(0)], [nota(0)])["sin_cambios"]


def test_detecta_cada_tipo_de_cambio():
    d = diferencia([nota(0), nota(1), nota(2), nota(3)], [nota(0, vel=90), nota(1.1), nota(3, dur=0.5), nota(4)])
    assert [v["a"] for v in d["velocidad"]] == [90]
    assert [(m["de"], m["a"]) for m in d["movidas"]] == [(1, 1.1)]
    assert [n["start_beat"] for n in d["quitadas"]] == [2]
    assert [n["start_beat"] for n in d["anadidas"]] == [4]
    assert [x["a"] for x in d["duracion"]] == [0.5]


def test_otra_altura_no_es_movida():
    d = diferencia([nota(0)], [nota(0.1, pitch=62)])
    assert not d["movidas"] and len(d["quitadas"]) == len(d["anadidas"]) == 1


# ── Cliente ──
def test_leer_clip_ignora_el_mas_cercano(entorno):
    live, motor = entorno
    motor.live.escribir_clip(1, 16.0, 4.0, [nota(0)], "x")
    assert motor.live.leer_clip(1, 0.0) is None
    assert motor.live.leer_clip(1, 16.0)["notes"] == [nota(0)]


def test_sin_conexion_da_error_claro():
    with pytest.raises(ErrorLive, match="GFMotor no responde"):
        ClienteLive(puerto=1, timeout=0.2).sesion()


def test_dos_pistas_iguales_detienen(entorno):
    live, motor = entorno
    live.pistas.append({"name": "KICK", "clips": []})
    motor.partida("organic", "A", "menor", 122)
    with pytest.raises(ErrorLive, match="2 pistas"):
        motor.borrador_kick()
    assert "write_clip" not in live.comandos


# ── Flujo y reglas ──
def test_partida_no_toca_live(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    assert live.comandos == [] and live.tempo == 124.0


def test_guia_pone_tempo_y_bloques(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.crear_guia()
    guia = next(p for p in live.pistas if p["name"] == "GUIA")
    assert live.tempo == 122
    assert [c["name"] for c in guia["clips"]] == ["INTRO", "BUILD", "DROP", "BREAK", "BUILD", "DROP", "OUTRO"]
    assert [c["color"] for c in guia["clips"]][:3] == [COLORES["INTRO"], COLORES["BUILD"], COLORES["DROP"]]


def test_template_original_no_se_escribe(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path / "d"))
    live = LiveSimulado("/x/TEMPLATE Project/TEMPLATE.als")
    motor = Motor(ClienteLive(puerto=live.puerto), tmp_path)
    motor.partida("organic", "A", "menor", 122)
    for paso in (motor.crear_guia, motor.borrador_kick):
        with pytest.raises(SetProtegido):
            paso()
    assert live.tempo == 124.0 and len(live.pistas) == 2
    live.cerrar()


def test_borrador_usa_kick_vacio_y_no_toca_ghost(entorno):
    live, motor = entorno
    live.pistas[0]["clips"] = [{"start": 0.0, "length": 848.0, "notes": [nota(0)], "name": ""}]
    fantasma = copy.deepcopy(live.pistas[0])
    motor.partida("organic", "A", "menor", 122)
    motor.borrador_kick()
    assert live.pistas[1]["clips"] and live.pistas[0] == fantasma


def test_regla_3_edicion_no_se_sobrescribe(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.borrador_kick()
    live.pistas[1]["clips"][0]["notes"] = [nota(0, pitch=36, vel=64)]
    editada = copy.deepcopy(live.pistas[1])
    motor.borrador_kick()
    assert live.pistas[1] == editada
    assert live.pistas[-1]["name"] == "KICK propuesta" and live.pistas[-1]["clips"]
    motor.borrador_kick()
    assert live.pistas[-1]["name"] == "KICK propuesta 2"


def test_revisar_registra_y_no_escribe(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.borrador_kick()
    clip = live.pistas[1]["clips"][0]
    clip["notes"] = clip["notes"][1:]
    live.comandos.clear()
    resultado = motor.revisar_edicion()["ultima_revision"]
    assert resultado[0]["estado"] == "leido" and len(resultado[0]["quitadas"]) == 1
    assert set(live.comandos) <= {"get_session", "read_clip"}
    assert _eventos("motor_edicion")[0]["contexto"]["bpm"] == 122
    clip["start"] = 999.0
    assert motor.revisar_edicion()["ultima_revision"][0]["estado"] == "movido_o_borrado"


def _con_rack(motor, ids=("1A", "2B", "1C")):
    motor.partida("organic", "A", "menor", 122)
    motor.borrador_kick()
    motor.generar_kicks()
    motor.generar_kicks()
    for i in ids:
        e = motor.alternar_kick(i)
    return e


def test_generar_no_crea_rack_y_tandas_tienen_ids_propios(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.generar_kicks()
    e = motor.generar_kicks()
    assert [o["id"] for o in e["opciones"][0]] == [f"1{x}" for x in "ABCDEFGH"]
    assert [o["id"] for o in e["opciones"][1]][0] == "2A"
    assert [o["caracter"] for o in e["opciones"][0]] == ["CUERPO MIX SECA", "CUERPO PIEL CORTA", "CUERPO PIEL LIGERA", "INICIAL MADERA CORTA", "INICIAL PIEL CORTA", "INICIAL MIX", "INICIAL MADERA SECA", "INICIAL PIEL SECA"]
    audios = [sf.read(o["ruta"])[0] for o in e["opciones"][0]]
    assert all(abs(a).max() > 0.1 for a in audios)
    assert e["opciones"][0][0]["seed_timbre"] != e["opciones"][1][0]["seed_timbre"]
    assert not list(motor.carpeta_live.rglob("*.adg")) and e["rack"] is None


def test_contador_quitar_y_rack_solo_al_tercero(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.generar_kicks()
    motor.generar_kicks()
    assert motor.alternar_kick("1A")["seleccion"] == ["1A"]
    motor.alternar_kick("2C")
    assert motor.alternar_kick("1A")["seleccion"] == ["2C"]
    motor.alternar_kick("1B")
    assert motor.estado["rack"] is None and not entorno[0].cargas
    e = motor.alternar_kick("2A")
    assert e["seleccion"] == ["2C", "1B", "2A"] and e["rack"]["cargado"] is True
    with pytest.raises(ValueError, match="Nueva selección"):
        motor.alternar_kick("1C")


def test_rack_mezcla_tandas_y_se_pone_en_la_pista(entorno):
    live, motor = entorno
    e = _con_rack(motor)
    r = e["rack"]
    assert [(p["id"], p["nota"]) for p in r["pads"]] == [("1A", 36), ("2B", 37), ("1C", 38)]
    raiz = ET.fromstring(gzip.decompress(open(r["ruta"], "rb").read()))
    pads = raiz.findall("GroupDevicePreset/BranchPresets/DrumBranchPreset")
    assert [p.find("ZoneSettings/ReceivingNote").get("Value") for p in pads] == ["92", "91", "90"]
    assert [p.find(".//SampleRef/FileRef/Path").get("Value") for p in pads] == [str(motor.ruta_opcion(i)) for i in ("1A", "2B", "1C")]
    assert len({p.get("Id") for p in pads}) == 3
    partes, pista = live.cargas[0]
    assert pista == "KICK" and partes[0] == "GrooveForge_Library" and partes[-1] == Path(r["ruta"]).name
    evento = _eventos("motor_seleccion")[0]
    assert evento["seleccion"] == ["1A", "2B", "1C"] and len(evento["presentadas"]) == 16


def test_si_no_carga_el_rack_queda_y_se_explica(entorno):
    live, motor = entorno
    live.error_carga = "Live no encontró el rack"
    r = _con_rack(motor)["rack"]
    assert r["cargado"] is False and "no encontró" in r["motivo"] and Path(r["ruta"]).exists()


def test_template_no_recibe_rack(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path / "d"))
    monkeypatch.setenv("GF_RACK_PLANTILLA", str(_plantilla(tmp_path / "m.adg")))
    live = LiveSimulado("/x/TEMPLATE.als")
    (tmp_path / "L").mkdir()
    motor = Motor(ClienteLive(puerto=live.puerto), tmp_path / "L", BrainFalso())
    motor.partida("organic", "A", "menor", 122)
    motor.generar_kicks()
    for i in ("1A", "1B", "1C"):
        e = motor.alternar_kick(i)
    assert not live.cargas and e["rack"]["cargado"] is False
    live.cerrar()


def test_usar_mueve_la_nota_en_un_clip_sin_editar(entorno):
    live, motor = entorno
    _con_rack(motor)
    e = motor.usar_kick("2B")
    assert e["elegido"]["colocado"] is True
    assert {n["pitch"] for n in live.pistas[1]["clips"][0]["notes"]} == {37}
    assert motor.revisar_edicion()["ultima_revision"][0]["sin_cambios"]
    assert _eventos("motor_eleccion")[0]["elegido"] == "2B"
    assert list(motor.carpeta_live.glob("00 Elegidos/*2B.wav"))
    with pytest.raises(ValueError, match="no está en este rack"):
        motor.usar_kick("2A")


def test_usar_no_toca_un_clip_editado(entorno):
    live, motor = entorno
    _con_rack(motor)
    clip = live.pistas[1]["clips"][0]
    clip["notes"] = clip["notes"][:-1]
    copia = copy.deepcopy(clip)
    assert motor.usar_kick("2B")["elegido"]["colocado"] is False
    assert live.pistas[1]["clips"][0] == copia


def test_nueva_seleccion_crea_otro_rack(entorno):
    _, motor = entorno
    primero = _con_rack(motor)["rack"]["nombre"]
    assert motor.nueva_seleccion()["seleccion"] == []
    for i in ("2A", "2B", "2C"):
        e = motor.alternar_kick(i)
    assert e["rack"]["nombre"] != primero and len(e["racks"]) == 2


class _Nota:
    def __init__(self, pitch, start_time, duration, velocity):
        self.pitch, self.start_time, self.duration, self.velocity = pitch, start_time, duration, velocity


class _Clip:
    def __init__(self, length, start_time=0.0):
        self.length, self.start_time, self.name, self.color, self.notas = length, start_time, "", None, ()
        self.end_time, self.start_marker = start_time + length, 0.0

    def add_new_notes(self, specs):
        self.notas = specs

    def get_notes_extended(self, _p, _s, _t, _d):
        return self.notas


class _Ranura:
    def __init__(self):
        self.clip = None

    @property
    def has_clip(self):
        return self.clip is not None

    def create_clip(self, largo):
        self.clip = _Clip(largo)

    def delete_clip(self):
        self.clip = None


class _Pista:
    has_midi_input = True

    def __init__(self, name):
        self.name, self.clip_slots, self.arrangement_clips = name, [_Ranura(), _Ranura()], []
        self.devices = []

    def delete_device(self, i):
        del self.devices[i]

    def duplicate_clip_to_arrangement(self, clip, tiempo):
        if tiempo < 0:
            raise RuntimeError("fuera de rango")
        copia = _Clip(clip.length, tiempo)
        copia.name, copia.color, copia.notas = clip.name, clip.color, clip.notas
        self.arrangement_clips.insert(0, copia)


class _Vista:
    selected_track = None


class _Cancion:
    def __init__(self, *nombres):
        self.tracks = [_Pista(n) for n in nombres]
        self.tempo, self.file_path, self.view, self.pasos = 120.0, "", _Vista(), []

    def begin_undo_step(self):
        self.pasos.append("inicio")

    def end_undo_step(self):
        self.pasos.append("fin")

    def create_midi_track(self, i):
        self.tracks.insert(i, _Pista(""))


def _comandos():
    sys.path.insert(0, str(Path(__file__).parents[1] / "remote_script"))
    from GFMotor import comandos

    return comandos


def test_superficie_escribe_y_lee_con_nombre_y_color():
    c = _comandos()
    s = _Cancion("KICK")
    m = {"track_index": 0, "start_beats": 8.0, "length_beats": 4.0, "clip_name": "INTRO", "color": 0x1D9E75, "notes": [nota(1)]}
    c.escribir_clip(s, m, _Nota)
    pista = s.tracks[0]
    assert all(not r.has_clip for r in pista.clip_slots) and s.pasos == ["inicio", "fin"]
    assert c.listar_clips(s, {"track_index": 0})["clips"] == [{"start": 8.0, "length": 4.0, "name": "INTRO", "end": 12.0, "marker": 0.0}]
    assert pista.arrangement_clips[0].color == 0x1D9E75
    assert c.leer_clip(s, {"track_index": 0, "start_beats": 8.0})["clip"]["notes"][0]["start_beat"] == 1
    assert c.leer_clip(s, {"track_index": 0, "start_beats": 0.0})["clip"] is None


def test_superficie_no_deja_clip_temporal_si_falla():
    c = _comandos()
    s = _Cancion("KICK")
    with pytest.raises(RuntimeError):
        c.escribir_clip(s, {"track_index": 0, "start_beats": -1.0, "length_beats": 4.0, "clip_name": "X", "notes": []}, _Nota)
    assert not s.tracks[0].clip_slots[0].has_clip and s.pasos == ["inicio", "fin"]


def test_superficie_crea_pista_y_rechaza_indices_malos():
    c = _comandos()
    s = _Cancion("A")
    assert c.crear_pista(s, {"track_name": "GUIA"})["track_index"] == 1 and s.tracks[1].name == "GUIA"
    with pytest.raises(c.ErrorComando):
        c.listar_clips(s, {"track_index": 9})
    assert c.sesion(s, {})["session"]["tracks"][1] == {"index": 1, "name": "GUIA"}


def test_superficie_carga_en_la_pista_o_pide_reintento():
    c = _comandos()
    s = _Cancion("KICK", "OTRA")

    class I:
        def __init__(self, name, children=(), is_loadable=False):
            self.name, self.children, self.is_loadable = name, children, is_loadable

    class Navegador:
        cargado = None
        user_folders = [I("Lib", [I("R.adg", is_loadable=True)])]

        def load_item(self, item):
            self.cargado = item

    n = Navegador()
    assert c.intentar_cargar(s, n, ["Lib", "Falta.adg"], "KICK") is False and n.cargado is None
    assert c.intentar_cargar(s, n, ["Lib", "R.adg"], "KICK") is True
    assert s.view.selected_track.name == "KICK" and n.cargado.name == "R.adg"
    with pytest.raises(c.ErrorComando):
        c.intentar_cargar(s, n, ["Lib", "R.adg"], "NO")


def test_superficie_real_responde_por_socket(monkeypatch):
    """Recorre red, cola e hilo principal con los módulos de Live simulados."""
    import socket as sk
    import time
    import types

    class Base:
        def __init__(self, c):
            self.c = c

        def song(self):
            return self.c

        def log_message(self, _m):
            pass

        def update_display(self):
            pass

        def disconnect(self):
            pass

    class Rack:
        name, children, is_loadable = "R.adg", (), True

    class Carpeta:
        name, is_loadable = "Lib", False
        children = (Rack(),)

    cargados = []
    navegador = types.SimpleNamespace(user_folders=[Carpeta()], load_item=cargados.append)
    live = types.ModuleType("Live")
    live.Application = types.SimpleNamespace(get_application=lambda: types.SimpleNamespace(browser=navegador))
    clip = types.ModuleType("Live.Clip")
    clip.MidiNoteSpecification = _Nota
    fw = types.ModuleType("_Framework")
    cs = types.ModuleType("_Framework.ControlSurface")
    cs.ControlSurface = Base
    for n, m in (("Live", live), ("Live.Clip", clip), ("_Framework", fw), ("_Framework.ControlSurface", cs)):
        monkeypatch.setitem(sys.modules, n, m)
    _comandos()
    from GFMotor import superficie

    with sk.socket() as libre:
        libre.bind(("127.0.0.1", 0))
        puerto = libre.getsockname()[1]
    monkeypatch.setattr(superficie, "PUERTO", puerto)
    s = _Cancion("KICK")
    sup = superficie.SuperficieGFMotor(s)
    vivo = [True]

    def bombear():
        while vivo[0]:
            sup.update_display()
            time.sleep(0.01)

    threading.Thread(target=bombear, daemon=True).start()
    try:
        cli = ClienteLive(puerto=puerto, timeout=5)
        assert cli.llamar("ping")["ok"]
        assert cli.indice_pista("KICK") == 0
        cli.poner_tempo(122)
        assert s.tempo == 122
        assert cli.pista_sin_clips(0)
        cli.escribir_clip(0, 0.0, 4.0, [nota(0, pitch=36)], "KICK", 0x1D9E75)
        assert not cli.pista_sin_clips(0)
        assert cli.leer_clip(0, 0.0)["notes"][0]["pitch"] == 36
        cli.cargar_rack(["Lib", "R.adg"], "KICK")
        assert cargados and s.view.selected_track.name == "KICK"
        with pytest.raises(ErrorLive, match="desconocido"):
            cli._ok("borrar_todo")
    finally:
        vivo[0] = False
        sup.disconnect()


def test_rack_nuevo_reemplaza_solo_los_del_motor():
    import types

    c = _comandos()
    s = _Cancion("KICK")
    pista = s.tracks[0]
    nombres = ["KICK organic A menor a383 R1", "Mi rack KICK", "KICK organic A menor aeb5 R2", "KICK", "EQ Eight"]
    pista.devices = [types.SimpleNamespace(name=n) for n in nombres]
    rack = types.SimpleNamespace(name="KICK organic C# dorico dbc8 R1.adg", children=(), is_loadable=True)
    carpeta = types.SimpleNamespace(name="Lib", children=(rack,), is_loadable=False)

    def cargar(_item):
        pista.devices.append(types.SimpleNamespace(name="KICK organic C# dorico dbc8 R1"))

    navegador = types.SimpleNamespace(user_folders=[carpeta], load_item=cargar)
    assert c.intentar_cargar(s, navegador, ["Lib", rack.name], "KICK")
    assert [d.name for d in pista.devices] == ["Mi rack KICK", "KICK", "EQ Eight", "KICK organic C# dorico dbc8 R1"]
    assert s.pasos == ["inicio", "fin"]


def test_si_no_encuentra_el_rack_nuevo_no_borra_nada():
    import types

    c = _comandos()
    s = _Cancion("KICK")
    viejo = types.SimpleNamespace(name="KICK organic A menor a383 R1")
    s.tracks[0].devices = [viejo]
    navegador = types.SimpleNamespace(user_folders=[], load_item=lambda _i: None)
    assert c.intentar_cargar(s, navegador, ["Lib", "X.adg"], "KICK") is False
    assert s.tracks[0].devices == [viejo]


# ── Bajo ──
def _layer_from_cc(valor, total):
    return min(1 + (valor - 1) * total // 127, total)


def test_cc_de_capa_es_la_inversa_del_firmware():
    for total in range(1, 33):
        for i in range(total):
            v = cc_de_capa(i, total)
            assert 1 <= v <= 127 and _layer_from_cc(v, total) == i + 1


@pytest.mark.parametrize("familia", list(FAMILIAS))
@pytest.mark.parametrize("tanda", [1, 2, 3])
def test_patron_cumple_las_reglas(familia, tanda):
    c = patron(familia, "A", tanda)
    n = c["notas"]
    assert c["longitud"] == COMPASES * 4 and n
    assert all(int(x["start_beat"] * 4) % 16 not in PASOS_KICK for x in n)
    assert all(1 <= x["velocity"] <= 127 for x in n)
    orden = sorted(n, key=lambda x: x["start_beat"])
    assert all(a["start_beat"] + a["duration_beats"] <= b["start_beat"] + 1e-6 for a, b in zip(orden, orden[1:]))
    tonica = fundamental("A")
    assert tonica == 33
    principal = sum(x["pitch"] in (tonica, tonica + 12) for x in n)
    assert principal >= 0.9 * len(n) and all(tonica - 1 <= x["pitch"] <= tonica + 12 for x in n)


def test_tandas_cambian_y_son_repetibles():
    assert patron("TAMBORA", "A", 1) == patron("TAMBORA", "A", 1)
    assert patron("TAMBORA", "A", 1) != patron("TAMBORA", "A", 2)


def test_familias_se_diferencian():
    firmas = {f: frozenset(int(x["start_beat"] * 4) % 16 for x in patron(f, "A", 1)["notas"]) for f in FAMILIAS}
    assert len(set(firmas.values())) == len(FAMILIAS)


def test_fundamental_en_registro_o_cercana():
    assert all(28 <= fundamental(r) <= 39 for r in ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"))
    assert fundamental("E") == 28 and fundamental("C") == 36


def test_poner_patron_crea_bass_y_reemplaza_si_esta_intacto(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.generar_patrones()
    motor.poner_patron("11")
    bass = next(p for p in live.pistas if p["name"] == "BASS")
    primero = copy.deepcopy(bass["clips"][0]["notes"])
    e = motor.poner_patron("13")
    assert bass["clips"][0]["notes"] != primero and e["bajo"]["pista"] == "BASS" and len(bass["clips"]) == 1


def test_poner_patron_no_pisa_lo_editado(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.generar_patrones()
    motor.poner_patron("11")
    bass = next(p for p in live.pistas if p["name"] == "BASS")
    bass["clips"][0]["notes"] = bass["clips"][0]["notes"][1:]
    copia = copy.deepcopy(bass)
    e = motor.poner_patron("12")
    assert bass == copia and e["bajo"]["pista"] == "BASS propuesta"


def test_bass_ajeno_no_se_pisa(entorno):
    live, motor = entorno
    live.pistas.append({"name": "BASS", "clips": [{"start": 0.0, "length": 4.0, "notes": [nota(0)], "name": "mio", "color": None}]})
    motor.partida("organic", "A", "menor", 122)
    motor.generar_patrones()
    assert motor.poner_patron("11")["bajo"]["pista"] == "BASS propuesta"
    assert live.pistas[2]["clips"][0]["name"] == "mio"


def test_capas_de_bajo_con_jd_primero_y_cc(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    e = motor.cargar_capas()
    assert [c["nombre"] for c in e["bajo"]["capas"]] == ["JD SUB MID", "ROLLER SUB", "WOODBASS"]
    motor.usar_capa("ROLLER SUB")
    assert motor.brain.enviados == [(3, 6)] and motor.estado["bajo"]["capa"] == "ROLLER SUB"
    assert _eventos("motor_eleccion")[-1]["parte"] == "sonido_bajo"
    with pytest.raises(ValueError):
        motor.usar_capa("KALIMBA")


def test_revisar_bajo_registra_y_no_escribe(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.generar_patrones()
    motor.poner_patron("11")
    live.pistas[-1]["clips"][0]["notes"] = live.pistas[-1]["clips"][0]["notes"][1:]
    live.comandos.clear()
    r = motor.revisar_bajo()["bajo"]["revision"]
    assert r["estado"] == "leido" and len(r["quitadas"]) == 1
    assert set(live.comandos) <= {"get_session", "read_clip"}


def test_template_no_recibe_patron(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path / "d"))
    live = LiveSimulado("/x/TEMPLATE.als")
    (tmp_path / "L").mkdir()
    motor = Motor(ClienteLive(puerto=live.puerto), tmp_path / "L", BrainFalso())
    motor.partida("organic", "A", "menor", 122)
    motor.generar_patrones()
    with pytest.raises(SetProtegido):
        motor.poner_patron("11")
    live.cerrar()


# ── Drums ──
_NOTAS_DRUMS = {"clave": 47, "snare": 48, "clap": 49, "hat_cerrado": 52, "hat_abierto": 53}


def _drums_listo(motor):
    motor.partida("organic", "A", "menor", 122)
    motor.generar_drums()
    for g in ("clave", "snare", "clap"):
        motor.elegir_drum(f"{g}-piel-1")
    return motor.elegir_drum("hats-madera-1")


def test_generar_drums_tres_materiales_por_voz(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    d = motor.generar_drums()["drums"]
    assert len(d["opciones"]) == 12 and {o["material"] for o in d["opciones"]} == {"MADERA", "PIEL", "808"}
    for o in d["opciones"]:
        for ruta in o["archivos"].values():
            y, _ = sf.read(ruta)
            assert abs(y).max() > 0.05
    hats = next(o for o in d["opciones"] if o["grupo"] == "HATS")
    assert set(hats["archivos"]) == {"hat_cerrado", "hat_abierto"}
    assert d["rack"] is None and not list(motor.carpeta_live.rglob("*.adg"))


def test_rack_de_drums_notas_choke_y_pista(entorno):
    live, motor = entorno
    d = _drums_listo(motor)["drums"]
    r = d["rack"]
    assert r["cargado"] and r["pista"] == "DRUMS" and live.cargas[-1][1] == "DRUMS"
    assert any(p["name"] == "DRUMS" for p in live.pistas)
    assert {p["voz"]: p["nota"] for p in r["pads"]} == _NOTAS_DRUMS
    raiz = ET.fromstring(gzip.decompress(open(r["ruta"], "rb").read()))
    pads = raiz.findall("GroupDevicePreset/BranchPresets/DrumBranchPreset")
    assert sorted(128 - int(p.find("ZoneSettings/ReceivingNote").get("Value")) for p in pads) == sorted(_NOTAS_DRUMS.values())
    assert _eventos("motor_seleccion")[-1]["parte"] == "drums"


def test_cambiar_una_voz_crea_el_siguiente_rack(entorno):
    live, motor = entorno
    primero = _drums_listo(motor)["drums"]["rack"]["nombre"]
    d = motor.elegir_drum("snare-808-1")["drums"]
    assert d["rack"]["nombre"] != primero and len(d["racks"]) == 2 and len(live.cargas) == 2
    assert next(p for p in d["rack"]["pads"] if p["voz"] == "snare")["material"] == "808"


def test_no_hay_rack_hasta_elegir_las_cuatro(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.generar_drums()
    motor.elegir_drum("clave-piel-1")
    motor.elegir_drum("clave-madera-1")
    assert motor.estado["drums"]["rack"] is None and motor.estado["drums"]["seleccion"] == {"CLAVE": "clave-madera-1"}
    with pytest.raises(ValueError):
        motor.elegir_drum("no-existe")


def test_tonalidad_afecta_solo_lo_nuevo(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.generar_drums()
    motor.cambiar_tonalidad("A", "lidia_dominante")
    d = motor.generar_drums()["drums"]
    assert d["opciones"][0]["modo"] == "menor" and d["opciones"][-1]["modo"] == "lidia_dominante"
    with pytest.raises(ValueError):
        motor.cambiar_tonalidad("A", "inventado")
    assert _eventos("motor_tonalidad")[0]["modo"] == "lidia_dominante"


def test_template_no_recibe_rack_de_drums(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path / "d"))
    monkeypatch.setenv("GF_RACK_PLANTILLA", str(_plantilla(tmp_path / "m.adg")))
    live = LiveSimulado("/x/TEMPLATE.als")
    (tmp_path / "L").mkdir()
    motor = Motor(ClienteLive(puerto=live.puerto), tmp_path / "L", BrainFalso())
    r = _drums_listo(motor)["drums"]["rack"]
    assert not r["cargado"] and not live.cargas and len(live.pistas) == 2
    live.cerrar()


def test_rack_real_con_choke_si_existe_la_plantilla(tmp_path, monkeypatch):
    from gf_motor.rack import plantilla_por_defecto

    monkeypatch.delenv("GF_RACK_PLANTILLA", raising=False)
    if not plantilla_por_defecto().exists():
        pytest.skip("Sin la plantilla de Juan")
    a, b = tmp_path / "a.wav", tmp_path / "b.wav"
    sf.write(a, [0.1] * 4800, 48000, subtype="PCM_24")
    sf.write(b, [0.1] * 4800, 48000, subtype="PCM_24")
    destino = tmp_path / "r.adg"
    assert crear_rack([a, b], "R", destino, notas=[48, 52], choke={52: 1}) == [48, 52]
    raiz = ET.fromstring(gzip.decompress(destino.read_bytes()))
    pads = raiz.findall("GroupDevicePreset/BranchPresets/DrumBranchPreset")
    assert [p.find("ZoneSettings/ChokeGroup").get("Value") for p in pads] == ["0", "1"]
    assert [p.find("ZoneSettings/ReceivingNote").get("Value") for p in pads] == ["80", "76"]


def test_nombre_de_rack_drums_se_reemplaza():
    import types

    c = _comandos()
    s = _Cancion("DRUMS")
    p = s.tracks[0]
    p.devices = [types.SimpleNamespace(name=n) for n in ("DRUMS organic A lidia_dominante abcd R1", "Mi kit", "GF Brasil")]
    rack = types.SimpleNamespace(name="DRUMS organic A menor abcd R2.adg", children=(), is_loadable=True)
    carpeta = types.SimpleNamespace(name="Lib", children=(rack,), is_loadable=False)
    navegador = types.SimpleNamespace(user_folders=[carpeta], load_item=lambda _i: p.devices.append(types.SimpleNamespace(name="DRUMS organic A menor abcd R2")))
    assert c.intentar_cargar(s, navegador, ["Lib", rack.name], "DRUMS")
    assert [d.name for d in p.devices] == ["Mi kit", "GF Brasil", "DRUMS organic A menor abcd R2"]


# ── Patrones de drums ──
def _compases(pasos, compases=16, pitch=33):
    return [{"pitch": pitch, "start_beat": (c * 16 + p) / 4, "duration_beats": 0.2, "velocity": 90} for c in range(compases) for p in pasos]


def _analisis_real():
    # Tresillo editado por Juan: 3, 6, 11, 14, ghosts en 1, 5 y 9 cada 4 compases, y un remate de kick.
    kick = _compases((0, 4, 8, 12)) + [{"pitch": 36, "start_beat": 63.5, "duration_beats": 0.2, "velocity": 90}]
    bajo = _compases((3, 6, 11, 14))
    for inicio, p in ((0, 5), (1, 9), (3, 1)):
        bajo += [n for n in _compases((p,)) if int(n["start_beat"] // 4) % 4 == inicio]
    return analizar(kick, bajo)


def test_analisis_lee_kick_bajo_ghosts_y_huecos():
    a = _analisis_real()
    assert a["kick"] == [0, 4, 8, 12] and a["bajo"] == [3, 6, 11, 14]
    assert a["ghosts"] == [1, 5, 9] and a["libres"] == [2, 7, 10, 13, 15]


def test_opciones_de_clave_segun_kick_y_bajo():
    op = {o["nombre"]: o["clave"] for o in opciones_drums(_analisis_real())}
    assert op["RESPUESTA"] == [2, 7, 10, 13]
    assert op["SON 3-2"] == [0, 3, 6, 10, 12]
    assert op["CONTRA"] == [7, 13, 15]
    a = _analisis_real()
    ocupados = set(a["kick"]) | set(a["bajo"]) | set(a["ghosts"])
    assert not ocupados & set(op["RESPUESTA"]) and not ocupados & set(op["CONTRA"])


def test_cambiar_el_bajo_cambia_las_claves():
    kick = _compases((0, 4, 8, 12))
    a = {o["nombre"]: o["clave"] for o in opciones_drums(analizar(kick, _compases((2, 10))))}
    b = {o["nombre"]: o["clave"] for o in opciones_drums(analizar(kick, _compases((3, 6, 11, 14))))}
    assert a["RESPUESTA"] != b["RESPUESTA"] and a["CONTRA"] != b["CONTRA"] and a["SON 3-2"] == b["SON 3-2"]


def test_sin_clips_asume_cuatro_por_cuatro_y_sin_huecos_descarta():
    a = analizar([], [])
    assert a["kick"] == [0, 4, 8, 12] and a["bajo"] == [] and {o["nombre"] for o in opciones_drums(a)} == {"RESPUESTA", "SON 3-2"}
    lleno = analizar(_compases(range(16)), [])
    assert [o["nombre"] for o in opciones_drums(lleno)] == ["SON 3-2"]


def test_clip_de_drums_notas_del_rack_y_16_compases():
    c = clip_drums({"nombre": "RESPUESTA", "clave": [2, 7, 10, 13]})
    assert c["longitud"] == 64 and {n["pitch"] for n in c["notas"]} == {CLAVE, SNARE, CLAP, HAT_CERRADO, HAT_ABIERTO}
    assert all(0 <= n["start_beat"] < 64 and 1 <= n["velocity"] <= 127 for n in c["notas"])
    claves = sorted({round(n["start_beat"] * 4) % 16 for n in c["notas"] if n["pitch"] == CLAVE})
    assert claves == [2, 7, 10, 13]
    assert sum(n["pitch"] == SNARE for n in c["notas"]) == 4
    assert c == clip_drums({"nombre": "RESPUESTA", "clave": [2, 7, 10, 13]})


def _con_kick_y_bajo(live, motor):
    motor.partida("organic", "A", "menor", 122)
    live.pistas[1]["clips"] = [{"start": 0.0, "length": 64.0, "notes": _compases((0, 4, 8, 12), pitch=36), "name": "KICK", "color": None}]
    live.pistas.append({"name": "BASS", "clips": [{"start": 0.0, "length": 64.0, "notes": _compases((3, 6, 11, 14)), "name": "BASS", "color": None}]})


def test_proponer_solo_lee_live(entorno):
    live, motor = entorno
    _con_kick_y_bajo(live, motor)
    antes = copy.deepcopy(live.pistas)
    live.comandos.clear()
    p = motor.proponer_patrones_drums()["drums"]["patrones"]
    assert live.pistas == antes and set(live.comandos) <= {"get_session", "list_clips", "read_clip"}
    assert p["analisis"]["bajo"] == [3, 6, 11, 14] and len(p["opciones"]) == 3


def test_poner_patron_drums_reemplaza_intacto_y_respeta_lo_editado(entorno):
    live, motor = entorno
    _con_kick_y_bajo(live, motor)
    live.pistas.append({"name": "DRUMS", "clips": []})
    motor.proponer_patrones_drums()
    motor.poner_patron_drums("RESPUESTA")
    d = next(p for p in live.pistas if p["name"] == "DRUMS")
    assert d["clips"][0]["name"] == "DRUMS RESPUESTA"
    motor.poner_patron_drums("CONTRA")
    assert len(d["clips"]) == 1 and d["clips"][0]["name"] == "DRUMS CONTRA"
    d["clips"][0]["notes"] = d["clips"][0]["notes"][1:]
    copia = copy.deepcopy(d)
    e = motor.poner_patron_drums("SON 3-2")
    assert d == copia and e["drums"]["patrones"]["pista"] == "DRUMS propuesta"
    with pytest.raises(ValueError):
        motor.poner_patron_drums("NO EXISTE")


def test_clip_ajeno_en_drums_no_se_pisa(entorno):
    live, motor = entorno
    _con_kick_y_bajo(live, motor)
    live.pistas.append({"name": "DRUMS", "clips": [{"start": 0.0, "length": 8.0, "notes": [nota(0, pitch=48)], "name": "mio", "color": None}]})
    motor.proponer_patrones_drums()
    assert motor.poner_patron_drums("CONTRA")["drums"]["patrones"]["pista"] == "DRUMS propuesta"
    assert next(p for p in live.pistas if p["name"] == "DRUMS")["clips"][0]["name"] == "mio"


# ── Tops ──
def _mid(ruta, notas, tpb=480):
    import mido

    ruta.parent.mkdir(parents=True, exist_ok=True)
    m = mido.MidiFile(ticks_per_beat=tpb)
    p = mido.MidiTrack()
    m.tracks.append(p)
    ev = []
    for b, d, n, v in notas:
        ev += [(round(b * tpb), 1, n, v), (round((b + d) * tpb), 0, n, 0)]
    ahora = 0
    for tiempo, on, n, v in sorted(ev, key=lambda x: (x[0], x[1])):
        p.append(mido.Message("note_on" if on else "note_off", note=n, velocity=v, time=tiempo - ahora))
        ahora = tiempo
    m.save(ruta)


def _catalogo_tops(lib):
    filas = []

    def fila(i, nombre, rol="perc", grupo="patron:a", seccion="peak", decision=None, rack="tribe", genero="organic_house", sha=None, version=1):
        ruta = f"03 Drums MIDI/x/{nombre}.mid"
        _mid(lib / ruta, [(0.269, 0.25, 53, 100), (15.5, 0.25, 53, 60)])
        filas.append({"id": f"i{i}", "ruta": ruta, "tipo": "midi", "rol": rol, "genero": genero, "rack": rack, "grupo": grupo, "seccion": seccion, "version": version, "decision_juan": decision, "sha256": sha or f"s{i}"})

    fila(1, "resto", seccion="peak")
    fila(2, "fav", decision="favorito", grupo="patron:b")
    fila(3, "sel_main", decision="seleccion", seccion="main", grupo="patron:c")
    fila(4, "sel_peak", decision="seleccion", grupo="patron:c")
    fila(5, "sel_peak2", decision="seleccion", grupo="patron:c", version=2)
    fila(6, "sel_peak3", decision="seleccion", grupo="patron:c", version=3)
    fila(7, "copia", decision="seleccion", grupo="patron:d", sha="dup")
    fila(8, "copia2", decision="seleccion", grupo="patron:d", sha="dup")
    fila(9, "otro_rack", rack="808")
    fila(10, "otro_genero", genero="afro_house")
    fila(11, "top", rol="top")
    (lib / "_catalogo").mkdir(parents=True, exist_ok=True)
    (lib / "_catalogo" / "catalogo.jsonl").write_text("".join(json.dumps(f) + "\n" for f in filas))


def test_candidatos_orden_diversidad_y_filtros(tmp_path):
    _catalogo_tops(tmp_path)
    c = candidatos(tmp_path, "perc", "organic")
    assert [x["nombre"] for x in c] == ["copia", "sel_peak", "sel_peak2", "fav", "resto"]
    assert all(x["nombre"] not in ("otro_rack", "otro_genero", "top") for x in c)
    assert [x["nombre"] for x in candidatos(tmp_path, "top", "organic")] == ["top"]
    with pytest.raises(ValueError):
        candidatos(tmp_path, "perc", "inventado")
    with pytest.raises(ValueError, match="catálogo"):
        candidatos(tmp_path / "vacio", "perc", "organic")


def test_leer_midi_conserva_microtiming_y_largo(tmp_path):
    r = tmp_path / "a.mid"
    _mid(r, [(0.269, 0.25, 53, 56), (15.5, 0.25, 53, 86), (15.5, 0.25, 44, 70)])
    notas, largo = leer_midi(r)
    assert largo == 16 and notas[0] == {"pitch": 53, "start_beat": 0.2687, "duration_beats": 0.25, "velocity": 56} or abs(notas[0]["start_beat"] - 0.269) < 0.002
    assert len(notas) == 3 and {n["pitch"] for n in notas} == {53, 44}


def test_microtiming_no_alarga_el_loop(tmp_path):
    r = tmp_path / "a.mid"
    _mid(r, [(0.0, 0.25, 53, 90), (15.9, 0.2, 53, 90)])
    assert leer_midi(r)[1] == 16
    r2 = tmp_path / "b.mid"
    _mid(r2, [(0.0, 0.25, 53, 90), (16.03, 0.2, 53, 90)])
    assert leer_midi(r2)[1] == 16
    r3 = tmp_path / "c.mid"
    _mid(r3, [(0.0, 0.25, 53, 90), (18.0, 0.2, 53, 90)])
    assert leer_midi(r3)[1] == 20


def test_clip_loop_repite_hasta_16_compases():
    notas = [{"pitch": 53, "start_beat": 0.269, "duration_beats": 0.25, "velocity": 90}]
    c = clip_loop("x", notas, 16.0)
    assert c["longitud"] == 64 and [n["start_beat"] for n in c["notas"]] == [0.269, 16.269, 32.269, 48.269]
    assert len(clip_loop("x", notas, 4.0)["notas"]) == 16


def test_encaje_informa_sin_descartar():
    n = [{"pitch": 1, "start_beat": b, "duration_beats": 0.1, "velocity": 100} for b in (0.0, 0.75, 1.5, 2.25)] + [{"pitch": 1, "start_beat": 3.25, "duration_beats": 0.1, "velocity": 40}]
    assert encaje(n, [0], [3], [6, 9]) == {"kick": 25, "bajo": 25, "clave": 50, "golpes": 4}
    assert encaje([], [0], [], [])["golpes"] == 0


def test_tops_proponer_solo_lee_y_poner_respeta_lo_editado(entorno):
    live, motor = entorno
    _catalogo_tops(motor.carpeta_live)
    motor.partida("organic", "A", "lidia_dominante", 122)
    live.pistas[1]["clips"] = [{"start": 0.0, "length": 64.0, "notes": _compases((0, 4, 8, 12)), "name": "KICK", "color": None}]
    antes = copy.deepcopy(live.pistas)
    live.comandos.clear()
    op = motor.proponer_tops("perc")["tops"]["perc"]["opciones"]
    assert live.pistas == antes and set(live.comandos) <= {"get_session", "list_clips", "read_clip"}
    assert op[0]["encaje"]["golpes"] > 0
    motor.poner_top("perc", op[0]["id"])
    perc = next(p for p in live.pistas if p["name"] == "PERC")
    assert perc["clips"][0]["length"] == 64 and len(perc["clips"][0]["notes"]) == 8
    assert abs(perc["clips"][0]["notes"][0]["start_beat"] - 0.269) < 0.002
    motor.poner_top("perc", op[1]["id"])
    assert len(perc["clips"]) == 1 and perc["clips"][0]["name"] == op[1]["nombre"]
    perc["clips"][0]["notes"] = perc["clips"][0]["notes"][1:]
    copia = copy.deepcopy(perc)
    assert motor.poner_top("perc", op[0]["id"])["tops"]["perc"]["pista"] == "PERC propuesta"
    assert perc == copia
    assert not any("TRIBE" in p["name"].upper() for p in live.pistas)
    with pytest.raises(ValueError):
        motor.poner_top("otro", "x")


def test_top_va_en_su_pista(entorno):
    live, motor = entorno
    _catalogo_tops(motor.carpeta_live)
    motor.partida("organic", "A", "menor", 122)
    op = motor.proponer_tops("top")["tops"]["top"]["opciones"]
    motor.poner_top("top", op[0]["id"])
    assert any(p["name"] == "TOP" and p["clips"] for p in live.pistas)


def test_sesion_antigua_sin_tops_se_completa(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    ruta = motor._carpeta() / "estado.json"
    viejo = json.loads(ruta.read_text())
    del viejo["tops"]
    ruta.write_text(json.dumps(viejo))
    assert Motor(ClienteLive(puerto=live.puerto), motor.carpeta_live, BrainFalso()).estado["tops"]["perc"]["opciones"] == []


# ── Motivo ──
_LIBRES, _CLAVE = [2, 7, 10, 13, 15], [0, 3, 6, 10, 12]


def _clips_motivo(raiz="A", modo="lidia_dominante", tanda=1):
    return {n: crear_motivo(n, r, raiz, modo, MODOS[modo], tanda) for n, r in ritmos_motivo(_LIBRES, _CLAVE).items()}


@pytest.mark.parametrize("modo", list(COLOR))
@pytest.mark.parametrize("raiz", ["A", "F", "C#"])
def test_motivo_en_escala_registro_y_con_nota_de_color(raiz, modo):
    from gf_perc.tonalidad import clase_de

    escala = {(clase_de(raiz) + g) % 12 for g in MODOS[modo]}
    color = (clase_de(raiz) + MODOS[modo][COLOR[modo]]) % 12
    for tanda in (1, 2, 3):
        for nombre, c in _clips_motivo(raiz, modo, tanda).items():
            assert c["longitud"] == 64
            alturas = [n["pitch"] for n in c["notas"]]
            assert all(p % 12 in escala for p in alturas), nombre
            assert max(alturas) <= 76 and min(alturas) >= 45, nombre
            assert color in {p % 12 for p in alturas}, nombre


def test_motivo_tiene_silencios_y_no_se_extiende():
    for tanda in (1, 2):
        for nombre, c in _clips_motivo(tanda=tanda).items():
            assert ocupacion(c) <= TOPE_OCUPACION, nombre
            orden = sorted(c["notas"], key=lambda n: n["start_beat"])
            assert all(a["start_beat"] + a["duration_beats"] <= b["start_beat"] + 1e-6 for a, b in zip(orden, orden[1:])), nombre


def test_motivo_forma_aaab():
    for nombre, c in _clips_motivo().items():
        bloques = [[(round(n["start_beat"] - b * 8, 4), n["pitch"]) for n in c["notas"] if b * 8 <= n["start_beat"] < (b + 1) * 8] for b in range(8)]
        assert bloques[0] == bloques[1] == bloques[2], nombre
        assert bloques[3] != bloques[0], nombre
        assert bloques[3] == bloques[7] and bloques[4] == bloques[0], nombre
        resuelve = bloques[3][-1][1]
        assert (resuelve - 57) % 12 in (0, 7), nombre


def test_ritmos_fijos_no_empiezan_con_el_kick():
    for nombre, r in RITMOS_FIJOS.items():
        assert all(p % 4 != 0 for p, _ in r), nombre


def test_respuesta_vive_en_los_huecos_y_con_clave_sigue_la_clave():
    r = ritmos_motivo(_LIBRES, _CLAVE)
    assert {p % 16 for p, _ in r["RESPUESTA"]} <= set(_LIBRES)
    assert {p % 16 for p, _ in r["CON CLAVE"]} <= set(_CLAVE)
    solo = ritmos_motivo([2], [])
    assert set(solo) == set(RITMOS_FIJOS)


def test_motivo_repetible_y_otra_tanda_cambia_contorno():
    a, b = _clips_motivo(tanda=1), _clips_motivo(tanda=1)
    assert a == b
    distintos = sum(a[n]["notas"] != _clips_motivo(tanda=t)[n]["notas"] for n in a for t in (2, 3, 4))
    assert distintos > 0


def _groove_para_motivo(live, motor):
    _con_kick_y_bajo(live, motor)
    live.pistas.append({"name": "DRUMS", "clips": [{"start": 0.0, "length": 64.0, "notes": _compases((0, 3, 6, 10, 12), pitch=47), "name": "D", "color": None}]})


def test_generar_motivos_solo_lee_y_usa_el_groove(entorno):
    live, motor = entorno
    _groove_para_motivo(live, motor)
    antes = copy.deepcopy(live.pistas)
    live.comandos.clear()
    m = motor.generar_motivos()["motivo"]
    assert live.pistas == antes and set(live.comandos) <= {"get_session", "list_clips", "read_clip"}
    assert {o["nombre"] for o in m["opciones"]} == {"CORTO", "MIXTO", "LARGO", "RESPUESTA", "CON CLAVE"}
    assert _eventos("motor_opciones")[-1]["parte"] == "motivo"


def test_poner_motivo_reemplaza_intacto_y_respeta_lo_editado(entorno):
    live, motor = entorno
    _groove_para_motivo(live, motor)
    motor.generar_motivos()
    motor.poner_motivo("1-CORTO")
    p = next(x for x in live.pistas if x["name"] == "LEAD")
    motor.poner_motivo("1-LARGO")
    assert len(p["clips"]) == 1 and p["clips"][0]["name"] == "LEAD LARGO"
    p["clips"][0]["notes"] = p["clips"][0]["notes"][1:]
    copia = copy.deepcopy(p)
    assert motor.poner_motivo("1-MIXTO")["motivo"]["pista"] == "LEAD propuesta" and p == copia
    with pytest.raises(ValueError):
        motor.poner_motivo("nada")


def test_capas_de_motivo(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.brain.capas = lambda: ["KALIMBA", "JD SUB MID", "JD PLK WOOD"]
    e = motor.cargar_capas_motivo()
    assert [c["nombre"] for c in e["motivo"]["capas"]] == ["JD PLK WOOD", "KALIMBA"]
    motor.usar_capa_motivo("KALIMBA")
    assert motor.brain.enviados == [(0, 3)]
    with pytest.raises(ValueError):
        motor.usar_capa_motivo("JD SUB MID")


def test_motivo_en_template_no_escribe(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path / "d"))
    live = LiveSimulado("/x/TEMPLATE.als")
    (tmp_path / "L").mkdir()
    motor = Motor(ClienteLive(puerto=live.puerto), tmp_path / "L", BrainFalso())
    motor.partida("organic", "A", "menor", 122)
    motor.generar_motivos()
    with pytest.raises(SetProtegido):
        motor.poner_motivo("1-CORTO")
    live.cerrar()


# ── Clips partidos ──
def test_notas_en_arreglo_clip_partido_con_marcador():
    from gf_motor.motor import notas_en_arreglo

    notas = [nota(b) for b in (0, 8, 16, 20, 31, 40)]
    c = {"start": 16.0, "length": 64.0, "end": 32.0, "marker": 16.0, "notes": notas}
    assert [n["start_beat"] for n in notas_en_arreglo(c, 32.0)] == [16.0, 20.0, 31.0]


def test_notas_en_arreglo_sin_datos_del_script_usa_el_siguiente_clip():
    from gf_motor.motor import notas_en_arreglo

    c = {"start": 0.0, "length": 64.0, "notes": [nota(0), nota(15), nota(16), nota(40)]}
    assert [n["start_beat"] for n in notas_en_arreglo(c, 16.0)] == [0.0, 15.0]
    assert [n["start_beat"] for n in notas_en_arreglo(c, None)] == [0.0, 15.0, 16.0, 40.0]
    assert notas_en_arreglo({"start": 0.0, "length": 4.0, "notes": None}, None) == []


def test_clip_partido_no_duplica_notas_al_registrar(entorno):
    import mido

    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    notas = [nota(b, pitch=57) for b in (0, 1, 2, 3)]
    live.pistas.append({"name": "MOTIVO", "clips": [{"start": 0.0, "length": 4.0, "notes": notas, "name": "a", "color": None}, {"start": 2.0, "length": 4.0, "notes": notas, "name": "b", "color": None}]})
    r = motor.registrar("X")["registros"][-1]
    m = mido.MidiFile(next(x for x in r["midis"] if "motivo" in x))
    assert sum(x.type == "note_on" and x.velocity > 0 for x in m.tracks[0]) == 2 + 4


# ── Acordes ──
_LD = (0, 2, 4, 6, 7, 9, 10)  # lidia dominante


def test_acordes_nombres_en_la_lidia_dominante():
    assert nombres_progresion("I-II-VI-II", "A", _LD) == "A – B – F#m – B"
    assert nombres_progresion("I-II", "A", _LD) == "A – B"
    assert nombres_progresion("I-II-V-I", "A", _LD) == "A – B – Em – A"
    assert nombre_acorde(clases_acorde(9, _LD, 0, True)) == "A7"
    assert nombre_acorde(clases_acorde(9, _LD, 1, True)) == "B7"
    assert nombre_acorde(clases_acorde(9, _LD, 5, True)) == "F#m7"
    assert nombre_acorde(clases_acorde(9, _LD, 4, True)) == "Em(maj7)"


@pytest.mark.parametrize("raiz", ["A", "F", "C#"])
@pytest.mark.parametrize("escala", [_LD, (0, 2, 3, 5, 7, 8, 10), (0, 2, 4, 5, 7, 9, 11)])
@pytest.mark.parametrize("prog", list(PROGRESIONES))
def test_acordes_en_escala_y_registro(raiz, escala, prog):
    from gf_motor.bajo import _NOTAS

    casa = {(_NOTAS[raiz] + g) % 12 for g in escala}
    for nombre, definicion in patrones_acordes(_LIBRES, _CLAVE).items():
        c = clip_acordes(prog, nombre, definicion, raiz, escala)
        assert c["longitud"] == 64 and c["notas"]
        for n in c["notas"]:
            assert n["pitch"] % 12 in casa, nombre
            assert REG_MIN <= n["pitch"] <= REG_MAX + 12, (nombre, n["pitch"])
            assert 0 <= n["start_beat"] < 64 and 1 <= n["velocity"] <= 127


def test_voicing_cercano_entre_acordes():
    c = clip_acordes("I-II", "BLOQUE TRESILLO", patrones_acordes([], [])["BLOQUE TRESILLO"], "A", _LD)
    a = sorted(n["pitch"] for n in c["notas"] if n["start_beat"] == 0)
    b = sorted(n["pitch"] for n in c["notas"] if n["start_beat"] == 4)
    assert len(a) == len(b) == 3 and sum(abs(x - y) for x, y in zip(a, b)) <= 8


def test_tipos_de_voicing():
    p = patrones_acordes(_LIBRES, _CLAVE)
    def primer(nombre):
        c = clip_acordes("I-II", nombre, p[nombre], "A", _LD)
        return sorted(n["pitch"] for n in c["notas"] if n["start_beat"] == c["notas"][0]["start_beat"])
    assert len(primer("BLOQUE TRESILLO")) == 3
    assert len(primer("SEPTIMAS CONTRATIEMPO")) == 4
    d = primer("DECIMAS TRESILLO")
    assert len(d) == 2 and d[1] - d[0] == 16  # décima mayor sobre A
    q = primer("1-5-8 TRESILLO")
    assert [x - q[0] for x in q] == [0, 7, 12]


@pytest.mark.parametrize("nombre,voces", [("BLOQUE LARGO", 3), ("SEPTIMAS LARGAS", 4), ("DECIMAS LARGAS", 2)])
def test_acordes_largos_un_acorde_sostenido_por_compas(nombre, voces):
    d = patrones_acordes([], [])[nombre]
    c = clip_acordes("I-II", nombre, d, "A", _LD)
    assert len(c["notas"]) == 16 * voces
    assert all(n["start_beat"] % 4 == 0 and 3.9 <= n["duration_beats"] < 4 for n in c["notas"])
    # Sin anticipación: el compás 4 sigue siendo B, no el A que viene.
    b = {n["pitch"] for n in c["notas"] if n["start_beat"] == 4}
    assert {n["pitch"] for n in c["notas"] if n["start_beat"] == 12} == b


def test_ritmos_leen_el_groove():
    p = patrones_acordes(_LIBRES, _CLAVE)
    assert p["SEPTIMAS HUECOS"][1] == tuple(_LIBRES[:4]) and p["SEPTIMAS CLAVE"][1] == tuple(_CLAVE)
    assert not {"SEPTIMAS HUECOS", "SEPTIMAS CLAVE"} & set(patrones_acordes([2], []))


def test_cierre_adelanta_el_siguiente_acorde():
    d = patrones_acordes([], [])["BLOQUE TRESILLO"]
    c = clip_acordes("I-II", "BLOQUE TRESILLO", d, "A", _LD)
    # Compás 4 (B) anticipa el A del compás 5 en su último golpe; el compás 2 (B) no.
    a = {n["pitch"] for n in c["notas"] if n["start_beat"] == 0}
    cierre = {n["pitch"] for n in c["notas"] if n["start_beat"] == (3 * 16 + 14) / 4}
    assert cierre == a
    normal = {n["pitch"] for n in c["notas"] if n["start_beat"] == (1 * 16 + 14) / 4}
    assert normal != a


def test_choques_detecta_semitono_sin_contar_notas_del_acorde():
    m = [{"pitch": 63, "start_beat": 0.0, "duration_beats": 1.0, "velocity": 90}, {"pitch": 61, "start_beat": 1.0, "duration_beats": 1.0, "velocity": 90}]
    r = choques(m, "I-II", False, "A", _LD)  # Re# roza Mi en A; Do# es parte de A.
    assert r["porcentaje"] == 50 and r["frecuentes"] == ["D# sobre A"]
    assert choques([], "I-II", False, "A", _LD)["porcentaje"] is None
    m2 = [{"pitch": 67, "start_beat": 0.0, "duration_beats": 1.0, "velocity": 90}]
    assert choques(m2, "I-II", False, "A", _LD)["porcentaje"] == 0
    assert choques(m2, "I-II", True, "A", _LD)["porcentaje"] == 0


def _set_acordes(live, motor):
    _groove_para_motivo(live, motor)
    motor.cambiar_tonalidad("A", "lidia_dominante")
    live.pistas.append({"name": "MELODY", "clips": [{"start": 0.0, "length": 64.0, "notes": _compases((3, 6), pitch=63), "name": "m", "color": None}]})


def test_proponer_acordes_solo_lee_y_usa_melodia(entorno):
    live, motor = entorno
    _set_acordes(live, motor)
    antes = copy.deepcopy(live.pistas)
    live.comandos.clear()
    a = motor.proponer_acordes()["acordes"]
    assert live.pistas == antes and set(live.comandos) <= {"get_session", "list_clips", "read_clip"}
    assert a["melodia"] == "MELODY" and len(a["opciones"]) == 3 * 10
    assert {o["acordes"] for o in a["opciones"]} == {"A – B – F#m – B", "A – B", "A – B – Em – A"}
    assert all(o["choques"]["porcentaje"] is not None for o in a["opciones"])


def test_proponer_acordes_prefiere_lead(entorno):
    live, motor = entorno
    _set_acordes(live, motor)
    live.pistas.append({"name": "LEAD", "clips": [{"start": 0.0, "length": 64.0, "notes": _compases((2,), pitch=64), "name": "l", "color": None}]})
    assert motor.proponer_acordes()["acordes"]["melodia"] == "LEAD"


def test_proponer_acordes_sin_melodia(entorno):
    live, motor = entorno
    _groove_para_motivo(live, motor)
    a = motor.proponer_acordes()["acordes"]
    assert a["melodia"] is None and a["opciones"][0]["choques"]["porcentaje"] is None


def test_poner_acordes_reemplaza_intacto_y_respeta_lo_editado(entorno):
    live, motor = entorno
    _set_acordes(live, motor)
    motor.proponer_acordes()
    motor.poner_acordes("I-II|BLOQUE TRESILLO")
    p = next(x for x in live.pistas if x["name"] == "CHORDS")
    motor.poner_acordes("I-II-V-I|ARCO")
    assert len(p["clips"]) == 1 and p["clips"][0]["name"] == "I-II-V-I ARCO"
    p["clips"][0]["notes"] = p["clips"][0]["notes"][1:]
    copia = copy.deepcopy(p)
    assert motor.poner_acordes("I-II|ARCO")["acordes"]["pista"] == "CHORDS propuesta" and p == copia
    with pytest.raises(ValueError):
        motor.poner_acordes("nada")
    assert _eventos("motor_borrador")[-1]["parte"] == "acordes"


# ── Estado y registro ──
def test_la_sesion_sobrevive_a_reiniciar_el_motor(entorno, tmp_path):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.generar_patrones()
    motor.poner_patron("13")
    otro = Motor(ClienteLive(puerto=live.puerto), motor.carpeta_live, BrainFalso())
    assert otro.estado["id"] == motor.estado["id"] and otro.estado["bajo"]["actual"] == "13"
    assert otro.revisar_bajo()["bajo"]["revision"]["sin_cambios"]


def test_abre_la_sesion_mas_reciente_y_tolera_archivos_danados(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    primera = motor.estado["id"]
    motor.partida("organic", "D", "menor", 124)
    segunda = motor.estado["id"]
    assert primera != segunda
    otro = Motor(ClienteLive(puerto=live.puerto), motor.carpeta_live, BrainFalso())
    assert otro.estado["id"] == segunda
    (gf_log.dataset_dir() / "metadata" / "motor" / segunda / "estado.json").write_text("{roto")
    assert Motor(ClienteLive(puerto=live.puerto), motor.carpeta_live, BrainFalso()).estado["id"] == primera


def test_sin_sesiones_arranca_vacio(entorno):
    live, motor = entorno
    assert motor.estado is None


def test_estado_antiguo_sin_bajo_se_completa(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    ruta = motor._carpeta() / "estado.json"
    viejo = json.loads(ruta.read_text())
    del viejo["bajo"], viejo["registros"]
    ruta.write_text(json.dumps(viejo))
    otro = Motor(ClienteLive(puerto=live.puerto), motor.carpeta_live, BrainFalso())
    assert otro.estado["bajo"]["tandas"] == 0 and otro.estado["registros"] == []


def test_registrar_guarda_todo_sin_tocar_live(entorno):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.borrador_kick()
    live.pistas.append({"name": "BASS", "clips": [{"start": 8.0, "length": 4.0, "notes": [nota(0, pitch=33)], "name": "mio", "color": None}]})
    antes = copy.deepcopy(live.pistas)
    live.comandos.clear()
    r = motor.registrar("TRESB MAD2", "rack R1", "más madera")["registros"][-1]
    assert live.pistas == antes and set(live.comandos) <= {"get_session", "list_clips", "read_clip"}
    guardado = json.loads(Path(r["archivo"]).read_text())
    bass = next(p for p in guardado["pistas"] if p["name"] == "BASS")
    assert bass["clips"][0]["notes"][0]["pitch"] == 33 and guardado["sonido_bajo"] == "TRESB MAD2"
    assert r["pistas"]["KICK"] == 64 and r["pistas"]["BASS"] == 1 and r["sin_guardar"]
    assert len(r["midis"]) == 2
    import mido

    m = mido.MidiFile(next(x for x in r["midis"] if "bass" in x))
    tiempo = 0
    for msg in m.tracks[0]:
        tiempo += msg.time
        if msg.type == "note_on":
            assert msg.note == 33 and tiempo == 8 * 480
    assert _eventos("motor_registro")[0]["nota"] == "más madera"


def test_registrar_no_pisa_y_acumula(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.registrar("A")
    motor.registrar("B")
    assert [r["sonido_bajo"] for r in motor.estado["registros"]] == ["A", "B"]


def test_registrar_funciona_sin_ssd(entorno, tmp_path):
    live, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.borrador_kick()
    motor.carpeta_live = tmp_path / "no" / "Lib"
    r = motor.registrar("X")["registros"][-1]
    assert r["midis"] == [] and Path(r["archivo"]).exists()


def test_buscar_item_en_el_navegador():
    sys.path.insert(0, str(Path(__file__).parents[1] / "remote_script"))
    from GFMotor.buscar import buscar_item

    class I:
        def __init__(self, name, children=(), is_loadable=False):
            self.name, self.children, self.is_loadable = name, children, is_loadable

    rack = I("R1.adg", is_loadable=True)
    raiz = [I("Otra"), I("GrooveForge_Library", [I("00 Motor", [I("KICK", [rack])])])]
    assert buscar_item(raiz, ["GrooveForge_Library", "00 Motor", "KICK", "R1.adg"]) is rack
    rack.name = "R1"
    assert buscar_item(raiz, ["GrooveForge_Library", "00 Motor", "KICK", "R1.adg"]) is rack
    assert buscar_item(raiz, ["GrooveForge_Library", "00 Motor", "KICK", "R2.adg"]) is None
    assert buscar_item(raiz, ["GrooveForge_Library", "00 Motor"]) is None


def test_sin_ssd_avisa(entorno, tmp_path):
    _, motor = entorno
    motor.carpeta_live = tmp_path / "no_existe" / "Library"
    motor.partida("organic", "A", "menor", 122)
    with pytest.raises(ValueError, match="conecta el SSD"):
        motor.generar_kicks()


def test_rack_real_si_existe_la_plantilla(tmp_path):
    from gf_motor.rack import plantilla_por_defecto

    import os
    os.environ.pop("GF_RACK_PLANTILLA", None)
    molde = plantilla_por_defecto()
    if not molde.exists():
        pytest.skip("Sin la plantilla de Juan")
    wav = tmp_path / "k.wav"
    sf.write(wav, [0.1] * 4800, 48000, subtype="PCM_24")
    destino = tmp_path / "r.adg"
    assert crear_rack([wav], "R", destino) == [36]
    raiz = ET.fromstring(gzip.decompress(destino.read_bytes()))
    pad = raiz.find("GroupDevicePreset/BranchPresets/DrumBranchPreset")
    assert pad.find(".//SampleRef/FileRef/Path").get("Value") == str(wav)
    assert pad.find(".//SampleRef/DefaultDuration").get("Value") == "4800"
    simpler = pad.find(".//OriginalSimpler")
    assert simpler.find("Pitch/TransposeKey/Manual").get("Value") == "0"
    assert simpler.find("Pitch/TransposeFine/Manual").get("Value") == "0"


def test_borradores_guardados_sobreviven_a_cerrar_el_motor(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.borrador_kick()
    guardado = json.loads((motor._carpeta() / "estado.json").read_text())
    assert guardado["borradores"][0]["clips"][0]["notas"][0]["pitch"] == 36


def test_registro_compatible_con_stats(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    assert isinstance(gf_log.stats(gf_log.load_events()), dict)


# ── Servidor ──
@pytest.fixture
def web(entorno):
    _, motor = entorno
    srv = crear_servidor(motor, 0)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{srv.server_address[1]}", motor
    srv.shutdown()
    srv.server_close()


def _post(url, datos=None, **cabeceras):
    pet = urllib.request.Request(url, json.dumps(datos or {}).encode(), {"Content-Type": "application/json", **cabeceras})
    return json.loads(urllib.request.urlopen(pet, timeout=5).read())


def test_servidor_flujo_y_error(web):
    url, _ = web
    with pytest.raises(urllib.error.HTTPError) as e:
        _post(f"{url}/api/guia")
    assert e.value.code == 400
    s = _post(f"{url}/api/partida", {"genero": "organic", "raiz": "A", "modo": "menor", "bpm": 122})["sesion"]
    assert s["partida"]["bpm"] == 122
    estado = json.loads(urllib.request.urlopen(f"{url}/api/estado", timeout=5).read())
    assert estado["live"]["conectado"] and "SC GHOST" in estado["live"]["pistas"]


def test_servidor_rechaza_otra_web(web):
    url, motor = web
    for cabeceras in ({"Origin": "https://malicioso.example"}, {"Host": "malicioso.example"}):
        with pytest.raises(urllib.error.HTTPError) as e:
            _post(f"{url}/api/partida", {"genero": "organic", "raiz": "A", "modo": "menor", "bpm": 122}, **cabeceras)
        assert e.value.code == 403
    pet = urllib.request.Request(f"{url}/api/partida", b"{}", {"Content-Type": "text/plain"})
    with pytest.raises(urllib.error.HTTPError):
        urllib.request.urlopen(pet, timeout=5)
    assert motor.estado is None


# ── Pads y atmósfera (motor-pads.md) ──────────────────────────────────
from gf_motor.pads import ALTO_MIN, GUIA_MIN, PATRONES_ATMOS, PATRONES_PAD, clip_pad  # noqa: E402


@pytest.mark.parametrize("escala", [_LD, (0, 2, 3, 5, 7, 8, 10)])
@pytest.mark.parametrize("prog", [None, *PROGRESIONES])
def test_pads_en_escala_y_dentro_del_clip(escala, prog):
    from gf_motor.bajo import _NOTAS

    casa = {(_NOTAS["A"] + g) % 12 for g in escala}
    for parte, patrones in (("pad", PATRONES_PAD), ("atmos", PATRONES_ATMOS)):
        for nombre in patrones:
            c = clip_pad(parte, nombre, prog, "A", escala)
            assert c["longitud"] == 64 and c["notas"], nombre
            for n in c["notas"]:
                assert n["pitch"] % 12 in casa, (nombre, n)
                assert 0 <= n["start_beat"] and n["start_beat"] + n["duration_beats"] <= 64, (nombre, n)
                assert n["duration_beats"] >= 3.5 and 1 <= n["velocity"] <= 127, (nombre, n)


def test_pad_no_pisa_el_registro_de_los_acordes():
    alto = clip_pad("pad", "ABIERTO ALTO", "I-II", "A", _LD)["notas"]
    assert min(n["pitch"] for n in alto) >= ALTO_MIN > REG_MAX - 1
    dron = clip_pad("pad", "DRON", "I-II", "A", _LD)["notas"]
    assert sorted(n["pitch"] for n in dron) == [45, 52] and dron[0]["duration_beats"] > 60  # La2 + Mi3, 16 compases


def test_linea_guia_solo_terceras_y_septimas():
    notas = clip_pad("pad", "LINEA GUIA", "I-II", "A", _LD)["notas"]
    assert {n["pitch"] % 12 for n in notas} == {1, 7, 3, 9}  # A7: Do# y Sol · B7: Re# y La
    assert min(n["pitch"] for n in notas) >= GUIA_MIN
    # Do# → Re#: la voz de terceras sube un semitono... o baja diez; aquí se mueve lo mínimo.
    terceras = sorted((n for n in notas if n["pitch"] % 12 in (1, 3)), key=lambda n: n["start_beat"])
    assert abs(terceras[1]["pitch"] - terceras[0]["pitch"]) == 2


def test_nube_fuera_del_pulso_y_con_semilla():
    a = clip_pad("atmos", "NUBE", "I-II", "A", _LD, tanda=1)["notas"]
    assert a == clip_pad("atmos", "NUBE", "I-II", "A", _LD, tanda=1)["notas"]
    assert a != clip_pad("atmos", "NUBE", "I-II", "A", _LD, tanda=2)["notas"]
    assert all(n["start_beat"] % 1 != 0 for n in a)
    assert {n["pitch"] % 12 for n in a} <= {9, 4, 3}  # La, Mi y Re#


def test_swell_entra_nota_a_nota():
    notas = clip_pad("atmos", "SWELL", "I-II", "A", _LD)["notas"]
    primeras = sorted(n["start_beat"] for n in notas if n["start_beat"] < 16)
    assert primeras == [0, 4, 8, 12]
    assert all(n["start_beat"] + n["duration_beats"] <= 16 for n in notas if n["start_beat"] < 16)


def test_proponer_y_poner_pads_respeta_lo_editado(entorno):
    live, motor = entorno
    _set_acordes(live, motor)
    motor.proponer_acordes()
    motor.poner_acordes("I-II|SEPTIMAS LARGAS")
    live.comandos.clear()
    s = motor.proponer_pads()["pads"]
    assert not live.comandos and s["progresion"] == "I-II" and s["tanda"] == 1
    assert [o["id"] for o in s["pad"]["opciones"]] == list(PATRONES_PAD)
    motor.poner_pad("pad", "DRON")
    motor.poner_pad("atmos", "NUBE")
    pad = next(x for x in live.pistas if x["name"] == "PAD")
    atmos = next(x for x in live.pistas if x["name"] == "ATMOS")
    assert pad["clips"][0]["name"] == "PAD DRON" and atmos["clips"][0]["name"] == "ATMOS NUBE"
    motor.poner_pad("pad", "LINEA GUIA")
    assert len(pad["clips"]) == 1 and pad["clips"][0]["name"] == "PAD LINEA GUIA"
    pad["clips"][0]["notes"] = pad["clips"][0]["notes"][1:]
    copia = copy.deepcopy(pad)
    assert motor.poner_pad("pad", "DRON")["pads"]["pad"]["pista"] == "PAD propuesta" and pad == copia
    assert motor.proponer_pads()["pads"]["tanda"] == 2
    with pytest.raises(ValueError):
        motor.poner_pad("pad", "nada")
    with pytest.raises(ValueError):
        motor.poner_pad("otra", "DRON")
    assert _eventos("motor_borrador")[-1]["parte"] == "pad"


def test_pads_sin_acordes_usan_la_tonica(entorno):
    live, motor = entorno
    _groove_para_motivo(live, motor)
    s = motor.proponer_pads()["pads"]
    assert s["progresion"] is None
    motor.poner_pad("pad", "ABIERTO ALTO")
    notas = next(x for x in live.pistas if x["name"] == "PAD")["clips"][0]["notes"]
    clases = {n["pitch"] % 12 for n in notas}
    assert len(clases) == 3 and {9, 4} <= clases  # solo el acorde de la tónica: La, Mi y su tercera


# ── Stabs y plucks (motor-detalles.md) ────────────────────────────────
from gf_motor.detalles import CICLO, PATRONES_PLUCK, PATRONES_STAB, avisos as avisos_detalles, clip_detalle, huecos_lead  # noqa: E402

_LEAD_A = _compases((0, 2, 4, 6), compases=16, pitch=69)  # el lead toca la primera mitad de cada compás


def test_huecos_del_lead():
    libres = huecos_lead(_LEAD_A)
    assert 8 in libres and 14 in libres and 30 in libres
    assert not {0, 2, 4, 6, 16, 18} & set(libres)
    assert huecos_lead([]) == list(range(CICLO))


@pytest.mark.parametrize("prog", [None, *PROGRESIONES])
def test_stabs_y_plucks_en_escala(prog):
    casa = {(9 + g) % 12 for g in _LD}
    libres = huecos_lead(_LEAD_A)
    for parte, patrones in (("stab", PATRONES_STAB), ("pluck", PATRONES_PLUCK)):
        for nombre in patrones:
            c = clip_detalle(parte, nombre, prog, "A", _LD, libres, [0, 4, 8, 12], _LEAD_A)
            assert c["notas"], nombre
            for n in c["notas"]:
                assert n["pitch"] % 12 in casa and 0 <= n["start_beat"] < 64, (nombre, n)


def test_stabs_en_huecos_y_fuera_del_kick():
    libres = huecos_lead(_LEAD_A)
    for nombre in ("GANCHO", "RESPUESTA"):
        c = clip_detalle("stab", nombre, "I-II", "A", _LD, libres, [0, 4, 8, 12], _LEAD_A)
        pasos = {round(n["start_beat"] * 4) for n in c["notas"]}
        assert all(p % CICLO in libres and p % 16 not in (0, 4, 8, 12) for p in pasos), nombre
    g = clip_detalle("stab", "GANCHO", "I-II", "A", _LD, libres, [0, 4, 8, 12], _LEAD_A)["notas"]
    golpes = sorted({round(n["start_beat"] * 4) % CICLO for n in g})
    assert 1 <= len(golpes) <= 3  # la misma figura cada 2 compases


def test_riff_solo_en_huecos_y_con_semilla():
    libres = huecos_lead(_LEAD_A)
    a = clip_detalle("pluck", "RIFF EN HUECOS", "I-II", "A", _LD, libres, [], _LEAD_A, tanda=1)["notas"]
    assert a == clip_detalle("pluck", "RIFF EN HUECOS", "I-II", "A", _LD, libres, [], _LEAD_A, tanda=1)["notas"]
    assert a != clip_detalle("pluck", "RIFF EN HUECOS", "I-II", "A", _LD, libres, [], _LEAD_A, tanda=2)["notas"]
    assert all(round(n["start_beat"] * 4) % CICLO in libres for n in a)


def test_avisos_cuando_el_papel_ya_esta_cubierto():
    punteado = _compases((0, 3, 6, 8, 11, 14), pitch=57)
    largo = [{"pitch": 57, "start_beat": c * 4.0, "duration_beats": 3.9, "velocity": 80} for c in range(16)]
    a = avisos_detalles({"CHORDS": punteado, "CHORDS 2": largo}, _LEAD_A, huecos_lead(_LEAD_A))
    assert "CHORDS" in a["pluck"] and "golpes" in a["stab"] and a["riff"] is None
    b = avisos_detalles({"CHORDS 2": largo}, [], list(range(CICLO)))
    assert b == {"stab": None, "pluck": None, "riff": None}
    denso = _compases(tuple(range(0, 16, 1)), pitch=69)
    assert avisos_detalles({}, denso, huecos_lead(denso))["riff"]


def test_proponer_y_poner_detalles(entorno):
    live, motor = entorno
    _set_acordes(live, motor)
    live.pistas.append({"name": "CHORDS", "clips": [{"start": 0.0, "length": 64.0, "notes": _compases((0, 3, 6, 8, 11, 14), pitch=57), "name": "c", "color": None}]})
    motor.proponer_acordes()
    antes = copy.deepcopy(live.pistas)
    d = motor.proponer_detalles()["detalles"]
    assert live.pistas == antes and d["lead"] == "MELODY"
    assert d["pluck"]["aviso"] and "CHORDS" in d["pluck"]["aviso"]
    assert [o["id"] for o in d["stab"]["opciones"]] == list(PATRONES_STAB)
    motor.poner_detalle("stab", "GANCHO")
    motor.poner_detalle("pluck", "ARPEGIO 3-3-2")
    stabs = next(x for x in live.pistas if x["name"] == "STABS")
    assert stabs["clips"][0]["name"] == "STAB GANCHO" and any(x["name"] == "PLUCKS" for x in live.pistas)
    stabs["clips"][0]["notes"] = stabs["clips"][0]["notes"][1:]
    assert motor.poner_detalle("stab", "ANTICIPO")["detalles"]["stab"]["pista"] == "STABS propuesta"
    with pytest.raises(ValueError):
        motor.poner_detalle("otra", "GANCHO")


# ── Transiciones (motor-transiciones.md) ──────────────────────────────
from gf_motor.transiciones import ELEMENTOS, clips_transicion, secciones  # noqa: E402

_ARREGLO = [  # como el de Juan: compases 1, 49, 65, 97, 129 (+ un clip sin nombre), 145, 193
    {"start": 0.0, "length": 128.0, "name": "INTRO"}, {"start": 192.0, "length": 64.0, "name": "BUILD"},
    {"start": 256.0, "length": 192.0, "name": "DROP"}, {"start": 384.0, "length": 128.0, "name": "BREAK"},
    {"start": 512.0, "length": 64.0, "name": "BUILD"}, {"start": 560.0, "length": 16.0, "name": ""},
    {"start": 576.0, "length": 192.0, "name": "DROP"}, {"start": 768.0, "length": 80.0, "name": "OUTRO"},
]


def test_secciones_terminan_donde_empieza_la_siguiente():
    s = secciones(_ARREGLO)
    assert [x["nombre"] for x in s] == ["INTRO", "BUILD", "DROP", "BREAK", "BUILD", "DROP", "OUTRO"]
    assert s[2]["fin"] == 384.0 and s[4]["fin"] == 576.0  # el clip sin nombre no corta


def test_roll_termina_en_el_drop_y_crece():
    s = secciones(_ARREGLO)
    clips = clips_transicion("roll", "ROLL 4", s, "A", _LD)
    assert [c["inicio"] for c in clips] == [240.0, 560.0] and all(c["longitud"] == 16 for c in clips)
    notas = clips[0]["notas"]
    assert len(notas) == 4 + 8 + 16 + 32  # un compás de negras, corcheas, semicorcheas y fusas
    assert notas[0]["velocity"] < notas[-1]["velocity"] and notas[-1]["start_beat"] + 0.125 == 16
    assert [len(c["notas"]) for c in clips_transicion("roll", "ROLL 2", s, "A", _LD)] == [30, 30]  # medio compás por subdivisión: 2 + 4 + 8 + 16


def test_riser_fill_y_ear_candy_en_su_sitio():
    s = secciones(_ARREGLO)
    r = clips_transicion("riser", "RISER ESCALERA", s, "A", _LD)
    assert [c["inicio"] for c in r] == [192.0, 512.0] and r[0]["inicio"] + r[0]["longitud"] == 256.0
    assert {n["pitch"] % 12 for c in r for n in c["notas"]} <= {9, 1, 4}  # acorde de la tónica: La, Do#, Mi
    f = clips_transicion("fill", "ALEGRE QUE LLAMA", s, "A", _LD)
    fines = [c["inicio"] + c["longitud"] for c in f]
    # Frases de 8 compases (32 tiempos) en INTRO 0-192, DROP 256-384, BREAK 384-512, DROP 576-768 y OUTRO 768-848; sin builds.
    intro, drop1, brk = [32.0 * k for k in range(1, 7)], [288.0, 320.0, 352.0, 384.0], [416.0, 448.0, 480.0, 512.0]
    drop2, outro = [576.0 + 32.0 * k for k in range(1, 7)], [800.0, 832.0]
    assert fines == intro + drop1 + brk + drop2 + outro
    assert 256.0 not in fines and 576.0 not in fines  # la llegada al drop es del roll
    imp = clips_transicion("subdrop", "SUBDROP EN DROP", s, "A", _LD)
    assert [c["inicio"] for c in imp] == [256.0, 576.0] and imp[0]["notas"][0]["pitch"] == 36  # pad del rack SUBDROP
    gota = clips_transicion("ear", "GOTA EN EL BREAK", s, "A", _LD)
    assert [c["inicio"] for c in gota] == [384.0, 448.0]
    assert clips_transicion("roll", "ROLL 4", secciones([{"start": 0.0, "length": 64.0, "name": "INTRO"}]), "A", _LD) == []


def _con_arreglo(live, motor):
    _set_acordes(live, motor)
    live.pistas.append({"name": "ARRANGE", "clips": [{**c, "notes": [], "color": None} for c in _ARREGLO]})


def test_poner_transiciones_varios_clips_y_respeta_lo_editado(entorno):
    live, motor = entorno
    _con_arreglo(live, motor)
    antes = copy.deepcopy(live.pistas)
    t = motor.proponer_transiciones()["transiciones"]
    assert live.pistas == antes and len(t["secciones"]) == 7
    assert {o["id"]: o["clips"] for o in t["roll"]["opciones"]}["ROLL 4"] == 2
    motor.poner_transicion("roll", "ROLL 4")
    roll = next(x for x in live.pistas if x["name"] == "ROLL")
    assert sorted(c["start"] for c in roll["clips"]) == [240.0, 560.0]
    motor.poner_transicion("roll", "ROLL 2")  # más corto: se estira para tapar el anterior
    assert sorted((c["start"], c["length"]) for c in roll["clips"]) == [(240.0, 16.0), (560.0, 16.0)]
    assert min(n["start_beat"] for n in roll["clips"][0]["notes"]) == 8.0
    motor.poner_transicion("ear", "STAB ANTES DEL DROP")
    motor.poner_transicion("ear", "GOTA EN EL BREAK")  # el stab queda tapado con un clip vacío
    ear = next(x for x in live.pistas if x["name"] == "EAR CANDY")
    vacios = [c for c in ear["clips"] if not c["notes"]]
    assert sorted(c["start"] for c in vacios) == [252.0, 572.0]
    roll["clips"][0]["notes"] = roll["clips"][0]["notes"][1:]
    copia = copy.deepcopy(roll)
    assert motor.poner_transicion("roll", "ROLL 8")["transiciones"]["roll"]["pista"] == "ROLL propuesta" and roll == copia
    with pytest.raises(ValueError):
        motor.poner_transicion("nada", "ROLL 4")


def test_transiciones_sin_arrange(entorno):
    live, motor = entorno
    _set_acordes(live, motor)
    with pytest.raises(ValueError, match="ARRANGE"):
        motor.proponer_transiciones()


def test_progresion_desde_el_nombre_del_clip_si_la_sesion_no_la_tiene(entorno):
    live, motor = entorno
    _groove_para_motivo(live, motor)
    live.pistas.append({"name": "CHORDS 2", "clips": [{"start": 0.0, "length": 64.0, "notes": [], "name": "I-II SEPTIMAS LARGAS", "color": None}]})
    assert motor.proponer_pads()["pads"]["progresion"] == "I-II"
    assert motor.proponer_detalles()["detalles"]["progresion"] == "I-II"


def test_recomendacion_de_stabs_y_plucks():
    from gf_motor.detalles import recomendar

    punteado = _compases((0, 3, 6, 8, 11, 14), pitch=57)
    largo = [{"pitch": 57, "start_beat": c * 4.0, "duration_beats": 3.9, "velocity": 80} for c in range(16)]
    r = {(x["parte"], x["patron"]): x["anadir"] for x in recomendar({"CHORDS": punteado}, _LEAD_A, huecos_lead(_LEAD_A))}
    assert r == {("stab", "RESPUESTA"): True, ("pluck", "ARPEGIO 3-3-2"): False, ("pluck", "RIFF EN HUECOS"): True}
    r = {(x["parte"], x["patron"]): x["anadir"] for x in recomendar({"CHORDS 2": largo}, [], list(range(CICLO)))}
    assert r == {("stab", "GANCHO"): True, ("pluck", "ARPEGIO 3-3-2"): True}


def test_rolls_nuevos():
    s = secciones(_ARREGLO)
    tres = clips_transicion("roll", "ROLL TRESILLO", s, "A", _LD)[0]["notas"]
    assert [round(n["start_beat"] * 4) for n in tres[:6]] == [0, 3, 6, 8, 11, 14]  # el 3-3-2 abre el roll
    assert len(tres) == 6 + 12 + 24 + 32
    sil = clips_transicion("roll", "ROLL CON SILENCIO", s, "A", _LD)[0]["notas"]
    assert max(n["start_beat"] for n in sil) < 15  # el último tiempo antes del drop queda vacío
    flam = clips_transicion("roll", "ROLL FLAM", s, "A", _LD)[0]["notas"]
    base = clips_transicion("roll", "ROLL 4", s, "A", _LD)[0]["notas"]
    assert len(flam) == len(base) + 4 + 8 + 16  # flam en negras, corcheas y semicorcheas, no en fusas


def test_fills_de_tribe_y_fx():
    from gf_motor.transiciones import TRIBE

    s = secciones(_ARREGLO)
    kit = Path.home() / "GROOVEFORGE/GrooveForge_Dataset_v1/kits/tribe_colombia.json"
    if kit.exists():  # las notas coinciden con el mapa del kit de Tribe
        teclado = json.loads(kit.read_text())["teclado"]
        assert TRIBE["llamador_abierto"] == teclado["Llamador Abierto"] and TRIBE["tambora_open_r"] == teclado["Tambora Open R"]
        assert TRIBE["caja_latigazo"] == teclado["Caja Latigazo"] and TRIBE["tambora_damped_r"] == teclado["Tambora Damped R"]
    ll = clips_transicion("tribe", "LLAMADOR", s, "A", _LD)
    assert [c["inicio"] for c in ll] == [240.0, 560.0] and {n["pitch"] for n in ll[0]["notas"]} == {48, 50}
    tambora = clips_transicion("fill", "TAMBORA", s, "A", _LD)[0]["notas"]
    assert {41, 43, 44, 46} == {n["pitch"] for n in tambora} and max(n["start_beat"] for n in tambora) < 8
    caja = clips_transicion("fill", "CAJA", s, "A", _LD)[0]["notas"]
    assert caja[-1]["pitch"] == TRIBE["caja_latigazo"]
    # Todo lo de Tribe cae en teclas del kit; G#1 y A#1 (cáscara) son los únicos sostenidos (Juan, 2026-11-08).
    validas = set(TRIBE.values())
    for el, pats in (("fill", ("ALEGRE QUE LLAMA", "TAMBORA", "CAJA")), ("tribe", ("LLAMADOR", "GUACHE RAIN"))):
        for pat in pats:
            notas = {n["pitch"] for c in clips_transicion(el, pat, s, "A", _LD) for n in c["notas"]}
            assert notas <= validas and not {p for p in notas if p % 12 in (1, 3, 6, 8, 10)} - {44, 46}, pat
    caida = clips_transicion("fx", "CAIDA", s, "A", _LD)
    assert [c["inicio"] for c in caida] == [256.0, 576.0] and caida[0]["notas"][0]["pitch"] == 57
    swell = clips_transicion("fx", "SWELL INVERSO", s, "A", _LD)[0]
    assert swell["inicio"] == 252.0 and swell["notas"][0]["duration_beats"] == 4.0
    st = clips_transicion("fx", "STUTTER", s, "A", _LD)[0]["notas"]
    assert len(st) == 16 and st[-1]["duration_beats"] < st[0]["duration_beats"]
    nat = clips_transicion("fx", "NATURALEZA", s, "A", _LD)
    assert [c["inicio"] for c in nat] == [0.0, 384.0] and nat[0]["notas"][0]["pitch"] % 12 == 9


def test_sonido_roll_crea_rack_y_lo_carga(entorno):
    live, motor = entorno
    _con_arreglo(live, motor)
    t = motor.sonido_roll("PIEL")["transiciones"]
    r = t["rack_roll"]
    assert r["nombre"].startswith("DRUMS roll-piel A lidia_dominante ") and r["nombre"].endswith(" R1")
    assert r["cargado"] == ["ROLL"] and live.cargas[-1][1] == "ROLL"
    assert re.match(r"^(KICK|DRUMS) \S+ \S+ \S+ [0-9a-f]{4} R\d+$", r["nombre"])  # GFMotor quita el anterior
    assert motor.sonido_roll("808")["transiciones"]["rack_roll"]["numero"] == 2
    with pytest.raises(ValueError):
        motor.sonido_roll("METAL")


def test_inter_recibe_la_subida_y_el_subdrop():
    # Como el set de Juan: BUILD 129-141, INTER 141-145 (interludio con el subdrop), DROP 145.
    arreglo = [{"start": 0.0, "length": 192.0, "name": "INTRO"}, {"start": 192.0, "length": 64.0, "name": "DROP"},
               {"start": 512.0, "length": 48.0, "name": "BUILD"}, {"start": 560.0, "length": 16.0, "name": "INTER"},
               {"start": 576.0, "length": 160.0, "name": "DROP"}]
    s = secciones(arreglo)
    roll = clips_transicion("roll", "ROLL 4", s, "A", _LD)
    assert [c["inicio"] + c["longitud"] for c in roll] == [192.0, 560.0]  # la subida llega al INTER, no al drop
    riser = clips_transicion("riser", "RISER NOTA", s, "A", _LD)
    assert riser[-1]["inicio"] == 512.0 and riser[-1]["inicio"] + riser[-1]["longitud"] == 560.0
    sub = clips_transicion("subdrop", "SUBDROP INTER", s, "A", _LD)
    assert [c["inicio"] for c in sub] == [560.0] and sub[0]["notas"][0]["pitch"] == 36
    assert [c["inicio"] for c in clips_transicion("subdrop", "SUBDROP EN DROP", s, "A", _LD)] == [192.0, 576.0]
    assert 560.0 in [c["inicio"] for c in clips_transicion("fx", "NATURALEZA", s, "A", _LD)]
    swell = clips_transicion("fx", "SWELL INVERSO", s, "A", _LD)
    assert swell[-1]["inicio"] + 4 == 576.0  # el swell y el stutter sí van al drop, al final del INTER
    assert all(c["inicio"] + c["longitud"] not in (560.0, 576.0) for c in clips_transicion("fill", "CAJA", s, "A", _LD))


def test_superficie_carga_plugin_solo_si_no_hay_instrumento():
    c = _comandos()
    s = _Cancion("CHORDS", "FILL")

    class I:
        def __init__(self, name, children=(), is_loadable=False):
            self.name, self.children, self.is_loadable = name, children, is_loadable

    class Navegador:
        cargado = None
        plugins = I("Plugins", [
            I("Audio Units", [I("Tribe", [I("TRIBE Player 2", is_loadable=True)])]),
            I("VST3", [I("Tribe Instruments", [I("TRIBE Player 2", is_loadable=True)]), I("GrooveForge", [I("GrooveForge Brain Prototype", is_loadable=True)])]),
        ])

        def load_item(self, item):
            self.cargado = item

    n = Navegador()
    assert c.cargar_plugin(s, n, {"plugin": "TRIBE Player 2", "pista": "FILL"}) == {"cargado": True}
    assert s.view.selected_track.name == "FILL" and n.cargado is Navegador.plugins.children[1].children[0].children[0]  # VST3 primero

    class Instrumento:
        type = 1
        name = "Mi sonido"

    s.tracks[0].devices.append(Instrumento())
    n.cargado = None
    assert c.cargar_plugin(s, n, {"plugin": "GrooveForge Brain Prototype", "pista": "CHORDS"})["cargado"] is False and n.cargado is None
    with pytest.raises(c.ErrorComando):
        c.cargar_plugin(s, n, {"plugin": "No Existe", "pista": "FILL"})


def test_el_motor_pone_el_instrumento_en_las_pistas_que_crea(entorno):
    live, motor = entorno
    _con_arreglo(live, motor)
    motor.proponer_acordes()
    motor.poner_acordes("I-II|SEPTIMAS LARGAS")
    motor.proponer_transiciones()
    motor.poner_transicion("fill", "CAJA")
    motor.poner_transicion("roll", "ROLL 4")
    assert ("GrooveForge Brain Prototype", "CHORDS") in live.plugins and ("TRIBE Player 2", "FILL") in live.plugins
    assert not any(p == "ROLL" for _, p in live.plugins)  # ROLL lleva el rack de la drum machine
    motor.poner_acordes("I-II|SEPTIMAS CLAVE")
    assert [p for _, p in live.plugins].count("CHORDS") == 1  # no lo vuelve a poner
    assert motor.estado["instrumentos"]["CHORDS"] == "ya tiene instrumento"


def test_subdrop_crea_su_rack_si_no_hay_y_va_a_su_pista(entorno):
    live, motor = entorno
    _con_arreglo(live, motor)
    motor.proponer_transiciones()
    t = motor.poner_transicion("subdrop", "SUBDROP EN DROP")["transiciones"]
    assert t["rack_subdrop"]["variante"] == "CORTA" and live.cargas[-1][1] == "SUBDROP"
    assert re.match(r"^(KICK|DRUMS) \S+ \S+ \S+ [0-9a-f]{4} R\d+$", t["rack_subdrop"]["nombre"])
    sub = next(x for x in live.pistas if x["name"] == "SUBDROP")
    assert sorted(c["start"] for c in sub["clips"]) == [256.0, 576.0] and not any(p == "SUBDROP" for _, p in live.plugins)
    assert motor.sonido_subdrop("LARGA")["transiciones"]["rack_subdrop"]["numero"] == 2


# ── Armado (motor-armado.md) ──────────────────────────────────────────
from gf_motor.armado import clips_de_tramo, material_de, presencia_inicial, tramo  # noqa: E402

_ARREGLO_INTER = [
    {"start": 0.0, "length": 192.0, "name": "INTRO"}, {"start": 192.0, "length": 64.0, "name": "BUILD"},
    {"start": 256.0, "length": 128.0, "name": "DROP"}, {"start": 384.0, "length": 64.0, "name": "BREAK"},
    {"start": 448.0, "length": 60.0, "name": "BUILD"}, {"start": 508.0, "length": 4.0, "name": "INTER"},
    {"start": 512.0, "length": 128.0, "name": "DROP"}, {"start": 640.0, "length": 144.0, "name": "OUTRO"},
]


def test_presencia_inicial_y_segundo_drop():
    s = secciones(_ARREGLO_INTER)
    p = presencia_inicial(["KICK", "BASS", "STABS", "ATMOS", "OTRA"], s)
    assert p["KICK"] == ["parcial", "todo", "todo", "nada", "todo", "nada", "todo", "parcial"]
    assert p["BASS"] == ["nada", "nada", "todo", "nada", "nada", "nada", "todo", "parcial"]
    assert p["STABS"] == ["nada"] * 6 + ["todo", "nada"]  # solo desde el segundo drop
    assert p["ATMOS"][5] == "todo" and p["OTRA"][2] == "todo" and p["OTRA"][0] == "nada"


def test_tramos_escalonados():
    s = secciones(_ARREGLO_INTER)
    assert tramo("KICK", s[0], "parcial") == (32.0, 192.0)  # entra a los 8 compases
    assert tramo("TOP", s[0], "parcial") == (128.0, 192.0)
    assert tramo("TOP", s[7], "parcial") == (640.0, 672.0)  # sale primero
    assert tramo("KICK", s[7], "parcial") == (640.0, 768.0)
    assert tramo("BASS", s[2], "parcial") == (256.0, 320.0)
    assert tramo("KICK", s[3], "nada") is None and tramo("KICK", s[2], "todo") == (256.0, 384.0)


def test_clips_del_largo_del_material_y_corte_final():
    mat = {"largo": 64.0, "notas": [{"pitch": 36, "start_beat": b, "duration_beats": 0.5, "velocity": 100} for b in (0.0, 30.0, 63.8)]}
    c = clips_de_tramo(mat, 256.0, 384.0 + 8, "KICK · DROP")
    assert [(x["inicio"], x["longitud"]) for x in c] == [(256.0, 64.0), (320.0, 64.0), (384.0, 8.0)]
    assert [n["start_beat"] for n in c[-1]["notas"]] == [0.0]
    assert c[0]["notas"][-1]["duration_beats"] == pytest.approx(0.2)  # cortada al final del clip
    m = material_de([{"start": 32.0, "length": 64.0, "name": "a"}], [{"pitch": 60, "start_beat": 40.0, "duration_beats": 1, "velocity": 90}])
    assert m["desde"] == 32.0 and m["notas"][0]["start_beat"] == 8.0


def _set_para_armar(live, motor):
    _set_acordes(live, motor)
    live.pistas.append({"name": "ARRANGE", "clips": [{**c, "notes": [], "color": None} for c in _ARREGLO_INTER]})
    kick = next(p for p in live.pistas if p["name"] == "KICK")
    kick["clips"] = [{"start": 0.0, "length": 64.0, "notes": _compases((0, 4, 8, 12), pitch=36), "name": "KICK", "color": None}]
    live.pistas.append({"name": "STABS", "clips": [{"start": 0.0, "length": 64.0, "notes": _compases((14,), pitch=61), "name": "S", "color": None}]})
    live.pistas.append({"name": "ROLL", "clips": [{"start": 240.0, "length": 16.0, "notes": _compases((0,), compases=4, pitch=48), "name": "R", "color": None}]})


def test_llenar_el_arreglo(entorno):
    live, motor = entorno
    _set_para_armar(live, motor)
    a = motor.proponer_armado()["armado"]
    assert "KICK" in a["roles"] and "STABS" in a["roles"] and "ROLL" not in a["roles"] and "ARRANGE" not in a["roles"]
    roll_antes = copy.deepcopy(next(p for p in live.pistas if p["name"] == "ROLL"))
    a = motor.llenar_armado()["armado"]
    assert a["registrado"] and len(motor.estado["registros"]) == 1  # registró el set antes de tocarlo
    kick = next(p for p in live.pistas if p["name"] == "KICK")
    inicios = sorted(c["start"] for c in kick["clips"])
    assert 0.0 not in inicios and inicios[0] == 32.0  # el kick entra a los 8 compases
    assert not any(384.0 <= c["start"] < 448.0 for c in kick["clips"])  # fuera del break
    stabs = next(p for p in live.pistas if p["name"] == "STABS")
    assert sorted(c["start"] for c in stabs["clips"]) == [512.0, 576.0]  # solo el segundo drop
    assert next(p for p in live.pistas if p["name"] == "ROLL") == roll_antes  # las transiciones no se tocan
    # Juan pone un clip suyo en el break: volver a llenar no lo toca y salta ese tramo.
    motor.cambiar_presencia("KICK", 3, "todo")
    kick["clips"].append({"start": 400.0, "length": 4.0, "notes": [], "name": "mío", "color": None})
    a = motor.llenar_armado()["armado"]
    assert any(c["name"] == "mío" for c in kick["clips"]) and a["saltados"] == ["KICK compás 97"]
    assert len(motor.estado["registros"]) == 1
    # Releer usa el material guardado aunque el clip del compás 1 ya no exista.
    assert motor.proponer_armado()["armado"]["materiales"]["KICK"]["largo"] == 64.0
    with pytest.raises(ValueError):
        motor.cambiar_presencia("KICK", 99, "todo")


def test_superficie_borra_clip_del_arreglo():
    c = _comandos()
    s = _Cancion("KICK")
    c.escribir_clip(s, {"track_index": 0, "start_beats": 8.0, "length_beats": 4.0, "clip_name": "A", "notes": []}, _Nota)
    pista = s.tracks[0]
    pista.delete_clip = lambda clip: pista.arrangement_clips.remove(clip)
    s.pasos.clear()
    assert c.borrar_clip(s, {"track_index": 0, "start_beats": 0.0}) == {"borrado": False}
    assert c.borrar_clip(s, {"track_index": 0, "start_beats": 8.0}) == {"borrado": True}
    assert not pista.arrangement_clips and s.pasos == ["inicio", "fin"]


def test_presencia_por_nombre_parecido():
    s = secciones(_ARREGLO_INTER)
    p = presencia_inicial(["CHORDS SUS", "PAD 2", "PERC 2"], s)
    assert p["CHORDS SUS"][0] == "todo" and p["CHORDS SUS"][2] == "nada"
    assert p["PAD 2"] == p["CHORDS SUS"] and p["PERC 2"][3] == "todo"


def test_llenar_pide_reiniciar_live_si_gfmotor_es_viejo(entorno):
    live, motor = entorno
    _set_para_armar(live, motor)
    motor.proponer_armado()
    live.version = "2"
    antes = copy.deepcopy(live.pistas)
    with pytest.raises(ValueError, match="cierra y abre Live"):
        motor.llenar_armado()
    assert live.pistas == antes and not motor.estado["registros"]


def test_presencia_con_las_convenciones():
    s = secciones(_ARREGLO_INTER)
    p = presencia_inicial(["BASS SUB", "BASS MID", "CHORDS PLK", "LEAD AUX", "LEAD MAIN"], s)
    assert p["BASS SUB"] == ["todo"] * 5 + ["nada", "todo", "todo"]  # todo el track menos el INTER
    assert p["BASS MID"][0] == "nada" and p["CHORDS PLK"][1] == "todo"
    assert p["LEAD AUX"] == ["nada"] * 6 + ["todo", "nada"] and p["LEAD MAIN"][2] == "todo"


# ── Mezcla de partida (motor-mezcla.md) ───────────────────────────────
from gf_motor.mezcla import CADENAS_DRUMS, TABLA, plan as plan_mezcla, rol_de  # noqa: E402

_RET = ["A Office", "B Plate", "C Hall", "D Rumble", "E Delay", "F Echo"]


def _p(nombre, vol=0.0, pan=0.0, sends=None):
    return {"name": nombre, "volume_db": vol, "pan": pan, "sends": sends or [float("-inf")] * 6}


def test_rol_por_nombre():
    assert rol_de("CHORDS SUS") == "CHORDS SUS" and rol_de("PERC CONGA") == "PERC" and rol_de("PAD propuesta") == "PAD"
    assert rol_de("SC GHOST") is None and rol_de("ARRANGE") is None and rol_de("BRAIN") is None and rol_de("RARO") is None
    assert TABLA["KICK"][2] == {3: -18.0}  # Rumble, template-envios-valhalla §5


def test_plan_respeta_lo_movido_y_retornos_faltantes():
    p = {x["pista"]: x for x in plan_mezcla([_p("KICK"), _p("BASS", vol=-4.0), _p("PAD", sends=[float("-inf"), float("-inf"), -10.0] + [float("-inf")] * 3)], _RET, {})}
    assert p["KICK"]["volume_db"] == -3.0 and p["KICK"]["sends"] == {3: -18.0}
    assert p["BASS"]["volume_db"] is None and p["BASS"]["saltado"]  # Juan ya movió el fader
    assert p["PAD"]["sends"] == {} and "envío C (lo moviste)" in p["PAD"]["saltados_envios"]
    corto = plan_mezcla([_p("KICK", sends=[float("-inf")] * 2)], _RET[:2], {})[0]
    assert corto["sends"] == {} and corto["saltados_envios"] == ["envío D (no hay ese retorno)"]
    # Lo que escribió el motor se puede volver a escribir; si Juan lo movió después, no.
    escrito = {"KICK": {"volume_db": -3.0, "pan": 0.0, "sends": {"3": -18.0}}}
    assert plan_mezcla([_p("KICK", vol=-3.0, sends=[float("-inf")] * 3 + [-18.0, float("-inf"), float("-inf")])], _RET, escrito)[0]["volume_db"] == -3.0
    assert plan_mezcla([_p("KICK", vol=-1.0)], _RET, escrito)[0]["volume_db"] is None


def test_aplicar_mezcla(entorno):
    live, motor = entorno
    _set_para_armar(live, motor)
    live.pistas.append({"name": "PERC CONGA", "clips": [], "volume_db": 0.0})
    m = motor.aplicar_mezcla()["mezcla"]
    kick = next(p for p in live.pistas if p["name"] == "KICK")
    assert kick["volume_db"] == -3.0 and kick["sends"][3] == -18.0
    conga = next(p for p in live.pistas if p["name"] == "PERC CONGA")
    assert conga["pan"] == -0.25 and conga["sends"][0] == -16.0
    sc = next(p for p in live.pistas if p["name"] == "SC GHOST")
    assert "volume_db" not in sc  # SC GHOST no se toca
    kick["volume_db"] = -1.5  # Juan mueve el fader
    motor.aplicar_mezcla()
    assert kick["volume_db"] == -1.5
    live.version = "3"
    with pytest.raises(ValueError, match="cierra y abre Live"):
        motor.proponer_mezcla()


def test_rack_con_volumen_de_cadena(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_RACK_PLANTILLA", str(_plantilla(tmp_path / "molde.adg")))
    w = tmp_path / "clap.wav"
    w.write_bytes(b"x")
    ruta = tmp_path / "r.adg"
    crear_rack([w], "R", ruta, notas=[49], volumenes=CADENAS_DRUMS)
    xml = gzip.decompress(ruta.read_bytes()).decode()
    assert 'Value="0.794328"' in xml or "MixerPreset" not in xml  # −2 dB; el molde de prueba puede no traer mezclador


@pytest.mark.skipif(not (Path.home() / "Music/Ableton/User Library/Presets/Instruments/Drum Rack/808 Core Kit.adg").exists(), reason="sin la plantilla de Juan")
def test_volumen_de_cadena_con_la_plantilla_real(tmp_path):
    real = Path.home() / "Music/Ableton/User Library/Presets/Instruments/Drum Rack/808 Core Kit.adg"
    wavs = []
    for n in ("clave", "hat"):
        w = tmp_path / f"{n}.wav"
        w.write_bytes(b"x")
        wavs.append(w)
    crear_rack(wavs, "R", tmp_path / "r.adg", plantilla=real, notas=[47, 52], volumenes=CADENAS_DRUMS)
    xml = gzip.decompress((tmp_path / "r.adg").read_bytes()).decode()
    assert xml.count('Value="0.630957"') >= 1 and xml.count('Value="0.316228"') >= 1  # −4 y −10 dB

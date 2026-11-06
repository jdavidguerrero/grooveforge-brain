from __future__ import annotations

import copy
import json
import sys
from pathlib import Path
import urllib.error
import urllib.request
import threading

import pytest
import soundfile as sf

from gf_motor.bajo import COMPASES, FAMILIAS, PASOS_KICK, fundamental, patron
from gf_motor.borrador import COLORES, PLANTILLAS, clip_kick, clips_guia
from gf_motor.edicion import diferencia
from gf_motor.brain import cc_de_capa
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
    assert c.listar_clips(s, {"track_index": 0})["clips"] == [{"start": 8.0, "length": 4.0, "name": "INTRO"}]
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

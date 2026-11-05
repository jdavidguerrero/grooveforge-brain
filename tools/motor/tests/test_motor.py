from __future__ import annotations

import copy
import json
import urllib.error
import urllib.request
import threading

import pytest
import soundfile as sf

from gf_motor.borrador import PLANTILLAS, clips_guia, clips_kick
from gf_motor.edicion import diferencia
from gf_motor.live import ClienteLive, ErrorLive
from gf_motor.motor import Motor, SetProtegido, gf_log
from gf_motor.servidor import crear_servidor
from live_simulado import LiveSimulado


@pytest.fixture
def entorno(tmp_path, monkeypatch):
    monkeypatch.setenv("GF_DATASET", str(tmp_path / "dataset"))
    live = LiveSimulado()
    motor = Motor(ClienteLive(puerto=live.puerto), tmp_path / "samples")
    yield live, motor
    live.cerrar()


def _eventos(tipo):
    return [e for e in gf_log.load_events() if e["type"] == tipo]


def nota(inicio, pitch=60, vel=110, dur=0.25):
    return {"pitch": pitch, "start_beat": inicio, "duration_beats": dur, "velocity": vel}


# ── Plantilla ──
def test_plantilla_organic_dura_lo_del_clip_fantasma():
    assert sum(c for _, c, _ in PLANTILLAS["organic"]) == 212


def test_bloques_contiguos_y_kick_solo_en_sus_tramos():
    g = clips_guia("organic")
    assert all(a["inicio"] + a["longitud"] == b["inicio"] for a, b in zip(g, g[1:]))
    k = clips_kick("organic")
    assert "KICK Break" not in [c["nombre"] for c in k]
    assert all(len(c["notas"]) == c["longitud"] for c in k)


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
    with pytest.raises(ErrorLive, match="Sin conexión"):
        ClienteLive(puerto=1, timeout=0.2).sesion()


def test_dos_pistas_iguales_detienen(entorno):
    live, motor = entorno
    live.pistas.append({"name": "KICK", "clips": []})
    motor.partida("organic", "A", "menor", 122)
    with pytest.raises(ErrorLive, match="2 pistas"):
        motor.borrador_kick()
    assert "write_arrangement_clip" not in live.comandos


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
    assert [c["name"] for c in guia["clips"]] == ["Intro", "Build", "Drop", "Break", "Build", "Drop", "Outro"]


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
    live.pistas[1]["clips"][0]["notes"] = [nota(0, vel=64)]
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
    clips = live.pistas[1]["clips"]
    clips[0]["notes"] = clips[0]["notes"][1:]
    clips[1]["start"] = 999.0
    live.comandos.clear()
    resultado = motor.revisar_edicion()["ultima_revision"]
    assert resultado[0]["estado"] == "leido" and len(resultado[0]["quitadas"]) == 1
    assert resultado[1]["estado"] == "movido_o_borrado"
    assert resultado[2]["sin_cambios"]
    assert set(live.comandos) <= {"get_session", "read_midi_clip"}
    assert _eventos("motor_edicion")[0]["contexto"]["bpm"] == 122


def test_kicks_y_eleccion(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    ops = motor.generar_kicks()["opciones"][-1]
    assert [o["letra"] for o in ops] == ["A", "B", "C"]
    audios = [sf.read(o["ruta"])[0] for o in ops]
    assert all(abs(a).max() > 0.1 for a in audios)
    assert not (len(audios[0]) == len(audios[1]) and (audios[0] == audios[1]).all())
    assert not list(motor.carpeta_live.rglob("*.wav"))
    motor.elegir_kick("B")
    copiados = list(motor.carpeta_live.rglob("*.wav"))
    assert len(copiados) == 1 and copiados[0].name.endswith("1B.wav")
    assert _eventos("motor_eleccion")[0]["elegido"] == "B"
    assert _eventos("motor_eleccion")[0]["escuchadas"] == "unknown"


def test_borradores_guardados_sobreviven_a_cerrar_el_motor(entorno):
    _, motor = entorno
    motor.partida("organic", "A", "menor", 122)
    motor.borrador_kick()
    guardado = json.loads((motor._carpeta() / "estado.json").read_text())
    assert guardado["borradores"][0]["clips"][0]["notas"][0]["pitch"] == 60


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

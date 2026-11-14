import json

import numpy as np
import soundfile as sf

import muestreo_tribe as mt


def test_plan_cubre_el_mapa_con_espacio_para_los_largos():
    plan = mt.plan_de_muestreo()
    assert len(plan) == len(mt.TRIBE) * len(mt.VELOCITIES) * mt.REPETICIONES
    assert {g["articulacion"] for g in plan} == set(mt.TRIBE)
    inicios = [g["inicio"] for g in plan]
    assert inicios == sorted(inicios)
    largos = [g for g in plan if g["articulacion"] in mt.LARGOS]
    assert largos and all(g["espacio"] == mt.ESPACIO_LARGO for g in largos)


def test_midi_tiene_cada_golpe(tmp_path):
    import mido

    plan = mt.plan_de_muestreo()
    mt.escribir_midi(plan, tmp_path / "m.mid")
    ons = [m for m in mido.MidiFile(tmp_path / "m.mid") if m.type == "note_on" and m.velocity > 0]
    assert len(ons) == len(plan)
    assert [m.note for m in ons] == [g["nota"] for g in plan]


def test_cortar_alinea_y_conserva_niveles(tmp_path):
    sr = 48_000
    plan = mt.plan_de_muestreo()[:16]  # dos articulaciones
    corrimiento = 0.137  # el export de Ableton nunca empieza exacto
    dur = plan[-1]["inicio"] + plan[-1]["espacio"] + 1
    x = np.zeros((int(dur * sr), 2), dtype=np.float32)
    t = np.arange(int(0.3 * sr)) / sr
    for g in plan:
        a = int((g["inicio"] + corrimiento) * sr)
        golpe = (g["velocity"] / 127) * 0.8 * np.sin(2 * np.pi * 200 * t) * np.exp(-t * 20)
        x[a:a + len(golpe), 0] += golpe
        x[a:a + len(golpe), 1] += golpe
    sf.write(tmp_path / "export.wav", x, sr, subtype="FLOAT")
    (tmp_path / "plan.json").write_text(json.dumps({"golpes": plan}))
    r = mt.cortar(tmp_path / "export.wav", tmp_path / "plan.json", tmp_path / "kit")
    assert len(r["golpes"]) == len(plan) and not r["vacios"]
    assert abs(r["desfase_muestras"] - int(corrimiento * sr)) < sr * 0.003
    picos = {g["velocity"]: g["pico_db"] for g in r["golpes"] if g["rr"] == 1 and g["articulacion"] == plan[0]["articulacion"]}
    assert picos[40] < picos[70] < picos[100] < picos[127]  # las capas de velocity no se normalizan
    assert all((tmp_path / "kit" / g["archivo"]).exists() for g in r["golpes"])
    assert "uso personal" in r["licencia"]


def test_normalizacion_global_sube_todo_igual(tmp_path):
    sr = 48_000
    plan = mt.plan_de_muestreo()[:8]
    t = np.arange(int(0.3 * sr)) / sr
    x = np.zeros((int((plan[-1]["inicio"] + 2) * sr), 2), dtype=np.float32)
    for g in plan:
        a = int(g["inicio"] * sr)
        golpe = 0.01 * (g["velocity"] / 127) * np.sin(2 * np.pi * 200 * t) * np.exp(-t * 20)  # export ~ -40 dB
        x[a:a + len(golpe)] += golpe[:, None]
    sf.write(tmp_path / "e.wav", x, sr, subtype="FLOAT")
    (tmp_path / "p.json").write_text(json.dumps({"golpes": plan}))
    r = mt.cortar(tmp_path / "e.wav", tmp_path / "p.json", tmp_path / "k")
    picos = {g["velocity"]: g["pico_db"] for g in r["golpes"] if g["rr"] == 1}
    assert abs(max(picos.values()) + 1.0) < 0.2  # el más fuerte queda en -1 dBFS
    assert picos[127] - picos[40] > 9  # la diferencia entre velocities se conserva (~10 dB)
    assert r["ganancia_db"] > 30

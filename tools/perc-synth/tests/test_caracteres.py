from __future__ import annotations

import numpy as np
import pytest

from gf_perc.caracteres import GRUPOS_DRUMS, MATERIALES_DRUMS, sintetizar_drum, CARACTERES_KICK, COLAS_KICK, huella_kick, mas_lejanas, nota_grave, rasgos_kick, sintetizar_caracter, sintetizar_cola
from gf_perc.kit import sintetizar_voz
from gf_perc.tonalidad import clase_de


def _rasgos(caracter, semillas=(1, 2, 3, 4)):
    filas = [rasgos_kick(sintetizar_caracter(caracter, "organic", "A", "menor", s)[0]) for s in semillas]
    return {k: float(np.mean([f[k] for f in filas])) for k in filas[0]}


@pytest.fixture(scope="module")
def r():
    return {c: _rasgos(c) for c in CARACTERES_KICK}


def test_duraciones_se_oyen_distintas(r):
    assert r["GOLPE MADERA"]["cola_ms"] < 0.7 * r["GOLPE PIEL"]["cola_ms"]
    assert r["GOLPE PIEL"]["cola_ms"] < 0.75 * r["CUERPO"]["cola_ms"]
    assert r["CUERPO"]["cola_ms"] < 0.75 * r["SUB"]["cola_ms"]


def test_cuerpo_tiene_mas_cuerpo_que_sub(r):
    assert r["CUERPO"]["cuerpo_db"] > r["SUB"]["cuerpo_db"] + 2.0


def test_acustico_pega_mas_y_no_tiene_cola_de_808(r):
    assert r["ACUSTICO"]["pegada_db"] > r["CUERPO"]["pegada_db"] + 3.0
    assert 100.0 < r["ACUSTICO"]["cola_ms"] < r["CUERPO"]["cola_ms"]


def test_todos_los_caracteres_lejos_entre_si():
    medias = {c: np.mean([huella_kick(sintetizar_caracter(c, "organic", "A", "menor", s)[0]) for s in (1, 2, 3)], axis=0) for c in CARACTERES_KICK}
    nombres = list(medias)
    for i, a in enumerate(nombres):
        for b in nombres[i + 1 :]:
            assert np.linalg.norm(medias[a] - medias[b]) > 2.0, (a, b)


@pytest.mark.parametrize("caracter", list(CARACTERES_KICK))
@pytest.mark.parametrize("raiz,modo,hz", [("A", "menor", 55.0), ("F", "menor", 43.65)])
def test_siempre_en_la_nota_y_sin_clipping(caracter, raiz, modo, hz):
    y, m = sintetizar_caracter(caracter, "organic", raiz, modo, 1)
    assert np.isfinite(y).all() and np.abs(y).max() <= 1.0
    inicio = int(0.05 * 48000)
    espectro = np.abs(np.fft.rfft(y[inicio:] * np.hanning(len(y) - inicio), n=48000))
    assert abs(espectro[25:150].argmax() + 25 - hz) <= hz * 0.09
    assert m["caracter"] == caracter


def test_acustico_es_otro_modelo_y_el_808_no_cambia():
    assert not np.allclose(sintetizar_voz("kick", "piel", modelo="acustico", room=0.0)[0][:4800], sintetizar_voz("kick", "piel", room=0.0)[0][:4800])
    assert np.array_equal(sintetizar_voz("kick", "piel")[0], sintetizar_voz("kick", "piel", modelo="808")[0])


def test_mas_lejanas_evita_lo_ya_presentado():
    candidatas = [np.array([0.0]), np.array([1.0]), np.array([10.0])]
    assert mas_lejanas(candidatas, [np.array([0.2])], 1) == [2]
    assert mas_lejanas(candidatas, [np.array([10.0])], 1) == [0]
    assert mas_lejanas(candidatas, [], 2) == [0, 2]


def _nivel(y, t0, t1):
    return float(np.sqrt(np.mean(y[int(t0 * 48000) : int(t1 * 48000)] ** 2)) / np.abs(y).max())


@pytest.fixture(scope="module")
def colas():
    return {c: sintetizar_cola(c, "organic", "A", "menor")[0] for c in COLAS_KICK}


def test_tanda_tiene_las_dos_familias():
    assert sum(c.startswith("CUERPO") for c in COLAS_KICK) == 3
    assert sum(c.startswith("INICIAL") for c in COLAS_KICK) == 5


@pytest.mark.parametrize("raiz,nota", [("A", 33), ("F", 29), ("D", 26), ("C", 36), ("C#", 25), ("G#", 32)])
def test_todo_kick_en_la_fundamental(raiz, nota):
    for c in COLAS_KICK:
        assert sintetizar_cola(c, "organic", raiz, "menor")[1]["nota_midi"] == nota


def test_familia_cuerpo_sostiene_mas_que_la_inicial(colas):
    # Se separan al final del golpe: la inicial ya casi se apagó y la de cuerpo sigue sonando.
    assert _nivel(colas["CUERPO PIEL CORTA"], 0.15, 0.18) > 1.5 * _nivel(colas["INICIAL PIEL CORTA"], 0.15, 0.18)


def test_ligera_tiene_menos_cuerpo(colas):
    assert _nivel(colas["CUERPO PIEL LIGERA"], 0.06, 0.15) < 0.85 * _nivel(colas["CUERPO PIEL CORTA"], 0.06, 0.15)


def test_inicial_comparte_el_golpe(colas):
    base = colas["INICIAL MADERA CORTA"][:864]
    for c, y in colas.items():
        if c.startswith("INICIAL"):
            assert np.corrcoef(base, y[:864])[0, 1] > 0.98


def test_otra_tanda_cambia_el_golpe(colas):
    otra = sintetizar_cola("CUERPO PIEL CORTA", "organic", "A", "menor", tanda=2)[0]
    n = min(len(otra), len(colas["CUERPO PIEL CORTA"]), int(0.15 * 48000))
    assert np.corrcoef(otra[:n], colas["CUERPO PIEL CORTA"][:n])[0, 1] < 0.97
    assert np.array_equal(colas["CUERPO PIEL CORTA"], sintetizar_cola("CUERPO PIEL CORTA", "organic", "A", "menor")[0])


def test_secas_mas_cortas_que_las_cortas(colas):
    assert len(colas["INICIAL PIEL SECA"]) < 0.8 * len(colas["INICIAL PIEL CORTA"])
    assert len(colas["CUERPO PIEL LIGERA"]) < 0.8 * len(colas["CUERPO PIEL CORTA"])


def test_nota_grave_es_siempre_la_tonica():
    nombres = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    for i, raiz in enumerate(nombres):
        n = nota_grave(raiz, "menor")
        assert 25 <= n <= 36 and n % 12 == i
    with pytest.raises(ValueError):
        nota_grave("A", "inventado")


@pytest.mark.parametrize("cola", list(COLAS_KICK))
def test_cola_sana(cola):
    y, m = sintetizar_cola(cola, "organic", "F", "menor")
    assert np.isfinite(y).all() and 0.5 < np.abs(y).max() <= 1.0 and m["cola"] == cola


@pytest.mark.parametrize("grupo", list(GRUPOS_DRUMS))
def test_materiales_de_drums_se_distinguen(grupo):
    voz = GRUPOS_DRUMS[grupo][0]
    h = {m: huella_kick(sintetizar_drum(voz, m, "organic", "A", "menor")[0]) for m in MATERIALES_DRUMS}
    nombres = list(h)
    for i, a in enumerate(nombres):
        for b in nombres[i + 1 :]:
            assert np.linalg.norm(h[a] - h[b]) > 1.0, (grupo, a, b)


@pytest.mark.parametrize("raiz,clase", [("A", 9), ("D", 2), ("F#", 6)])
@pytest.mark.parametrize("material", list(MATERIALES_DRUMS))
def test_clave_y_snare_en_la_fundamental(material, raiz, clase):
    for voz in ("clave", "snare"):
        for tanda in (1, 2):
            m = sintetizar_drum(voz, material, "organic", raiz, "lidia_dominante", tanda)[1]
            assert m["nota_midi"] % 12 == clase and m["afinado"] == "nota"


@pytest.mark.parametrize("voz", ["clap", "hat_cerrado", "hat_abierto"])
@pytest.mark.parametrize("material", list(MATERIALES_DRUMS))
def test_ruido_se_alinea_a_la_fundamental_sin_perder_caracter(voz, material):
    for raiz in ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"):
        m = sintetizar_drum(voz, material, "organic", raiz, "menor")[1]
        assert m["afinado"] == "resonancia" and m["nota_midi"] is None
        nota = 69 + 12 * np.log2(m["resonancia_hz"] / 440.0)
        grado = round(nota - clase_de(raiz)) % 12
        # El tercer armónico puro queda 2 cents sobre la quinta temperada.
        assert abs(nota - round(nota)) < 0.03 and grado in (0, 7)
        assert 0.75 < m["ajuste"]["afinacion"] < 1.25


@pytest.mark.parametrize("material", list(MATERIALES_DRUMS))
def test_clave_suena_en_la_tonica(material):
    y, m = sintetizar_drum("clave", material, "organic", "A", "menor")
    espectro = np.abs(np.fft.rfft(y, n=48000))
    pico = int(espectro[200:6000].argmax()) + 200
    assert abs(1200 * np.log2(pico / (440.0 * 2 ** ((m["nota_midi"] - 69) / 12)))) < 60


def test_hats_comparten_afinacion():
    for material in MATERIALES_DRUMS:
        a = sintetizar_drum("hat_cerrado", material, "organic", "A", "menor")[1]
        b = sintetizar_drum("hat_abierto", material, "organic", "A", "menor")[1]
        assert a["resonancia_hz"] == b["resonancia_hz"]


def test_hat_abierto_dura_mas_y_tanda_cambia_el_sonido():
    for m in MATERIALES_DRUMS:
        c = sintetizar_drum("hat_cerrado", m, "organic", "A", "menor")[0]
        a = sintetizar_drum("hat_abierto", m, "organic", "A", "menor")[0]
        assert len(a) > 1.5 * len(c)
    x, y = (sintetizar_drum("snare", "PIEL", "organic", "A", "menor", t)[0] for t in (1, 2))
    assert len(x) != len(y) or not np.array_equal(x, y)

from __future__ import annotations

import numpy as np
import pytest

from gf_perc.caracteres import CARACTERES_KICK, COLAS_KICK, huella_kick, mas_lejanas, nota_grave, rasgos_kick, sintetizar_caracter, sintetizar_cola
from gf_perc.kit import sintetizar_voz


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


def _env(y, t):
    w = 480
    e = np.sqrt(np.convolve(y**2, np.ones(w) / w, "same"))
    i = int(t * 48000)
    return 20.0 * np.log10((e[i] if i < len(e) else 0.0) / e.max() + 1e-6)


def _banda(y, a, b, t0=0.09, t1=0.3):
    s = y[int(t0 * 48000) : int(t1 * 48000)]
    return 10.0 * np.log10(float((np.abs(np.fft.rfft(s, n=48000)) ** 2)[a:b].sum()) + 1e-12)


@pytest.fixture(scope="module")
def colas():
    return {c: sintetizar_cola(c, "organic", "A", "menor", 1)[0] for c in COLAS_KICK}


def test_todas_las_colas_comparten_el_golpe(colas):
    base = colas["MADERA CORTA"][:864]
    for y in colas.values():
        assert np.corrcoef(base, y[:864])[0, 1] > 0.98


def test_otra_semilla_cambia_el_golpe(colas):
    # Otra tanda debe cambiar de verdad el golpe, no solo la semilla.
    medidas = [sintetizar_cola("PIEL SECA", "organic", "A", "menor", s)[0] for s in (1000, 2000, 3000, 4000)]
    assert len({len(y) for y in medidas}) > 1
    for a, b in zip(medidas, medidas[1:]):
        n = min(len(a), len(b), int(0.15 * 48000))
        assert np.corrcoef(a[:n], b[:n])[0, 1] < 0.97


def test_las_secas_son_las_mas_cortas_y_conservan_el_golpe(colas):
    normales = [c for c in COLAS_KICK if "SECA" not in c]
    secas = [c for c in COLAS_KICK if "SECA" in c]
    assert len(secas) == 5
    for s in secas:
        assert len(colas[s]) < 0.7 * min(len(colas[n]) for n in normales)
        assert np.abs(colas[s]).max() > 0.5


def test_la_base_sostiene_el_cuerpo(colas):
    # Segunda referencia de Juan: a los 150 ms sigue por encima de la mitad del máximo y corta cerca de 190 ms.
    for nombre in ("MADERA SECA", "PIEL SECA"):
        y = colas[nombre]
        w = 960
        e = np.sqrt(np.convolve(y**2, np.ones(w) / w, "same"))
        assert e[int(0.15 * 48000)] > 0.4 * e[: int(0.05 * 48000)].max()
        assert rasgos_kick(y)["pegada_db"] < 7.0
        assert 0.18 < len(y) / 48000 < 0.35


@pytest.mark.parametrize("llena,ligera", [("MADERA SECA", "MADERA SECA LIGERA"), ("PIEL SECA", "PIEL SECA LIGERA"), ("MADERA CORTA", "MADERA CORTA LIGERA"), ("PIEL CORTA", "PIEL CORTA LIGERA")])
def test_las_ligeras_tienen_menos_cuerpo_y_el_mismo_inicio(colas, llena, ligera):
    def nivel(y):
        return float(np.sqrt(np.mean(y[int(0.06 * 48000) : int(0.15 * 48000)] ** 2)) / np.abs(y).max())

    assert nivel(colas[ligera]) < 0.85 * nivel(colas[llena])
    assert np.corrcoef(colas[llena][:480], colas[ligera][:480])[0, 1] > 0.98


def test_nota_grave_tonica_o_quinta():
    assert nota_grave("A", "menor") == 28  # E1, la quinta de La
    assert nota_grave("F", "menor") == 29  # F1, la tónica
    assert nota_grave("G", "menor") == 31
    assert nota_grave("D", "frigio") == 26
    for raiz in ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"):
        assert 25 <= nota_grave(raiz, "menor") <= 32


@pytest.mark.parametrize("material", ["MADERA", "PIEL"])
def test_la_cola_larga_dura_mas(colas, material):
    assert len(colas[f"{material} LARGA"]) > 1.5 * len(colas[f"{material} CORTA"])
    # Después de 0,2 s el golpe ya no pesa: lo que se mide es la cola.
    assert _banda(colas[f"{material} LARGA"], 60, 1000, 0.2, 0.4) > _banda(colas[f"{material} CORTA"], 60, 1000, 0.2, 0.4) + 5.0


def test_madera_y_piel_se_separan_en_la_cola(colas):
    # Madera: una octava arriba y parciales lejanos. Piel: modos juntos, cerca de la nota.
    assert _banda(colas["MADERA LARGA"], 150, 1000) > _banda(colas["PIEL LARGA"], 150, 1000) + 5.0
    assert _env(colas["MADERA LARGA"], 0.3) > _env(colas["PIEL LARGA"], 0.3) + 2.0


@pytest.mark.parametrize("cola", list(COLAS_KICK))
def test_cola_sana_y_en_tonalidad(cola):
    y, m = sintetizar_cola(cola, "organic", "F", "menor", 1)
    assert np.isfinite(y).all() and np.abs(y).max() <= 1.0 and m["nota_midi"] == 29
    assert sintetizar_cola(cola, "organic", "A", "menor", 1, grave=False)[1]["nota_midi"] == 33

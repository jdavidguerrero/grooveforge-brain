"""Valida lo medible. Si suena orgánico lo decide la escucha (teoría §11)."""

import numpy as np
import pytest

from gf_perc.core import SR, banco_modal, tasa_decay
from gf_perc.instrumentos import INSTRUMENTOS, MEMBRANA, articulaciones_de, sintetizar

TODOS = [(i, a) for i in INSTRUMENTOS for a in articulaciones_de(i)]


def t60(y: np.ndarray) -> float:
    """Tiempo hasta caer 60 dB desde el pico, sobre la envolvente de energía."""
    ventana = int(0.002 * SR)
    env = np.sqrt(np.convolve(y**2, np.ones(ventana) / ventana, mode="same"))
    pico = int(np.argmax(env))
    debajo = np.nonzero(env[pico:] < env[pico] * 1e-3)[0]
    return (debajo[0] if debajo.size else len(y) - pico) / SR


def centroide(y: np.ndarray) -> float:
    espectro = np.abs(np.fft.rfft(y, n=SR))
    return float((np.arange(espectro.size) * espectro).sum() / espectro.sum())


def energia_en(y: np.ndarray, hz: float, ancho: float = 12.0) -> float:
    espectro = np.abs(np.fft.rfft(y, n=SR * 2)) / 2
    f = np.arange(espectro.size) / 2
    return float(np.sum(espectro[(f > hz - ancho) & (f < hz + ancho)] ** 2))


@pytest.mark.parametrize("instrumento,articulacion", TODOS)
def test_audio_valido(instrumento, articulacion):
    y, _ = sintetizar(instrumento, articulacion)
    assert np.all(np.isfinite(y))
    assert 0.2 < np.max(np.abs(y)) <= 1.0
    assert abs(float(y[-1])) < 1e-3
    assert len(y) > 0.02 * SR


@pytest.mark.parametrize("instrumento,articulacion", TODOS)
def test_determinista(instrumento, articulacion):
    a, ma = sintetizar(instrumento, articulacion, seed_timbre=7, seed_golpe=3)
    b, mb = sintetizar(instrumento, articulacion, seed_timbre=7, seed_golpe=3)
    assert np.array_equal(a, b)
    assert ma == mb


@pytest.mark.parametrize("instrumento,articulacion", TODOS)
def test_golpes_distintos_no_son_identicos(instrumento, articulacion):
    a, _ = sintetizar(instrumento, articulacion, seed_golpe=1)
    b, _ = sintetizar(instrumento, articulacion, seed_golpe=2)
    n = min(len(a), len(b))
    assert not np.array_equal(a[:n], b[:n])


def test_seed_timbre_cambia_el_instrumento_solo_con_variacion():
    _, a = sintetizar("llamador", "abierto", seed_timbre=1, seed_golpe=1, variacion=1.0)
    _, b = sintetizar("llamador", "abierto", seed_timbre=2, seed_golpe=1, variacion=1.0)
    _, c = sintetizar("llamador", "abierto", seed_timbre=2, seed_golpe=9, variacion=1.0)
    assert a["golpe"]["f11_hz"] != b["golpe"]["f11_hz"]
    assert b["golpe"]["f11_hz"] == c["golpe"]["f11_hz"]


def test_banco_modal_produce_la_frecuencia_pedida():
    y = banco_modal(np.array([440.0]), np.array([1.0]), np.array([3.0]), 1.0)
    espectro = np.abs(np.fft.rfft(y))
    assert abs(np.argmax(espectro) - 440) <= 1


def test_ceros_de_bessel_son_de_la_membrana():
    from scipy.special import jv

    for m, _n, j in MEMBRANA:
        assert abs(jv(m, j)) < 1e-3


def test_golpe_central_excita_mas_el_modo_01_que_el_11():
    """Con r pequeño, J_1 es casi cero: es la base física de la articulación `bajo`."""
    f11 = INSTRUMENTOS["llamador"].f11
    comp = INSTRUMENTOS["llamador"].compresion
    f01 = f11 * (2.4048 / 3.8317) ** comp
    bajo, _ = sintetizar("llamador", "bajo", room=0.0)
    abierto, _ = sintetizar("llamador", "abierto", room=0.0)
    razon_bajo = energia_en(bajo, f11) / energia_en(bajo, f01)
    razon_abierto = energia_en(abierto, f11) / energia_en(abierto, f01)
    assert razon_bajo < razon_abierto


def test_madera_decae_antes_que_piel_abierta():
    clave, _ = sintetizar("clave", room=0.0)
    alegre, _ = sintetizar("alegre", "abierto", room=0.0)
    assert t60(clave) < t60(alegre)


def test_apagado_decae_antes_que_abierto():
    abierto, _ = sintetizar("llamador", "abierto", room=0.0)
    apagado, _ = sintetizar("llamador", "apagado", room=0.0)
    assert t60(apagado) < t60(abierto)


def test_slap_es_mas_brillante_que_abierto_y_abierto_que_bajo():
    bajo, _ = sintetizar("alegre", "bajo", room=0.0)
    abierto, _ = sintetizar("alegre", "abierto", room=0.0)
    slap, _ = sintetizar("alegre", "slap", room=0.0)
    assert centroide(bajo) < centroide(abierto) < centroide(slap)


@pytest.mark.parametrize("instrumento,articulacion", [("alegre", "abierto"), ("clave", "golpe"), ("llamador", "slap")])
def test_mas_velocity_es_mas_brillante(instrumento, articulacion):
    suave = [centroide(sintetizar(instrumento, articulacion, 0.25, seed_golpe=s, room=0.0)[0]) for s in range(1, 9)]
    fuerte = [centroide(sintetizar(instrumento, articulacion, 1.0, seed_golpe=s, room=0.0)[0]) for s in range(1, 9)]
    assert np.mean(fuerte) > np.mean(suave)


def test_mas_grande_es_mas_grave():
    assert centroide(sintetizar("bombo", "abierto", room=0.0)[0]) < centroide(sintetizar("alegre", "abierto", room=0.0)[0])


def test_guache_es_mas_brillante_que_maracon():
    assert centroide(sintetizar("guache", "corto", room=0.0)[0]) > centroide(sintetizar("maracon", "corto", room=0.0)[0])


def test_tasa_decay_sube_con_la_frecuencia():
    assert tasa_decay(np.array([4000.0]), 10, 5, 2)[0] > tasa_decay(np.array([400.0]), 10, 5, 2)[0]


def test_articulacion_invalida_falla():
    with pytest.raises(ValueError):
        sintetizar("clave", "slap")

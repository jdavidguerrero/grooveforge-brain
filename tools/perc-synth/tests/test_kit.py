"""Valida lo medible de los kits. Teoría: percusion-kits-808-organica.md §7."""

import numpy as np
import pytest

from gf_perc.core import SR
from gf_perc.kit import KITS, MATERIALES, NOTAS, VOCES, sintetizar_voz

TODO = [(v, m) for v in VOCES for m in MATERIALES]


def centroide(y):
    e = np.abs(np.fft.rfft(y, n=SR))
    return float((np.arange(e.size) * e).sum() / e.sum())


def proporcion_grave(y, hz=150):
    e = np.abs(np.fft.rfft(y, n=SR)) ** 2
    return float(e[:hz].sum() / e.sum())


@pytest.mark.parametrize("voz,material", TODO)
def test_audio_valido(voz, material):
    y, _ = sintetizar_voz(voz, material)
    assert np.all(np.isfinite(y))
    assert 0.2 < np.max(np.abs(y)) <= 1.0
    assert abs(float(y[-1])) < 1e-3
    assert len(y) < 3 * SR


@pytest.mark.parametrize("voz,material", TODO)
def test_determinista(voz, material):
    a, _ = sintetizar_voz(voz, material, seed_timbre=4, seed_golpe=2)
    b, _ = sintetizar_voz(voz, material, seed_timbre=4, seed_golpe=2)
    assert np.array_equal(a, b)


@pytest.mark.parametrize("material", MATERIALES)
def test_kick_conserva_el_grave(material):
    y, _ = sintetizar_voz("kick", material)
    assert proporcion_grave(y) > 0.5


@pytest.mark.parametrize("material", MATERIALES)
def test_hat_abierto_dura_mas_que_cerrado(material):
    assert len(sintetizar_voz("hat_abierto", material)[0]) > 2 * len(sintetizar_voz("hat_cerrado", material)[0])


@pytest.mark.parametrize("material", MATERIALES)
def test_hats_son_mas_brillantes_que_congas(material):
    assert centroide(sintetizar_voz("hat_cerrado", material)[0]) > centroide(sintetizar_voz("conga_alta", material)[0])


@pytest.mark.parametrize("material", MATERIALES)
def test_congas_ordenadas_por_afinacion(material):
    picos = []
    for voz in ("conga_baja", "conga_media", "conga_alta"):
        y, _ = sintetizar_voz(voz, material, room=0.0)
        picos.append(int(np.argmax(np.abs(np.fft.rfft(y, n=SR))[:800])))
    assert picos[0] < picos[1] < picos[2]


@pytest.mark.parametrize("material", MATERIALES)
@pytest.mark.parametrize(
    "voz,f0", [("conga_baja", 178), ("conga_media", 245), ("conga_alta", 345), ("bongo_bajo", 410), ("bongo_alto", 560)]
)
def test_voz_afinada_conserva_su_nota(voz, f0, material):
    """Regresión: un contacto largo anulaba la fundamental y solo dejaba un golpe sordo."""
    y, _ = sintetizar_voz(voz, material, room=0.0)
    pico = int(np.argmax(np.abs(np.fft.rfft(y, n=SR))))
    assert abs(pico - f0) / f0 < 0.2


@pytest.mark.parametrize("material", ["madera", "piel"])
@pytest.mark.parametrize("voz", ["clave", "rim"])
def test_voces_agudas_no_retumban(voz, material):
    y, _ = sintetizar_voz(voz, material, room=0.0)
    assert proporcion_grave(y) < 0.05


@pytest.mark.parametrize("material", MATERIALES)
def test_clap_tiene_varias_rafagas(material):
    y, _ = sintetizar_voz("clap", material, room=0.0)
    ventana = int(0.0012 * SR)
    env = np.sqrt(np.convolve(y[: int(0.04 * SR)] ** 2, np.ones(ventana) / ventana, mode="same"))
    subidas = np.sum((env[1:-1] > env[:-2]) & (env[1:-1] >= env[2:]) & (env[1:-1] > 0.25 * env.max()))
    assert subidas >= 2


@pytest.mark.parametrize("voz", ["kick", "conga_media", "clave", "snare", "hat_cerrado"])
def test_cada_material_suena_distinto(voz):
    a, b, c = (sintetizar_voz(voz, m, room=0.0)[0] for m in MATERIALES)
    n = min(len(a), len(b), len(c))
    assert not np.allclose(a[:n], b[:n], atol=0.01)
    assert not np.allclose(b[:n], c[:n], atol=0.01)


def test_material_organico_varia_entre_golpes():
    a, _ = sintetizar_voz("conga_media", "piel", seed_golpe=1)
    b, _ = sintetizar_voz("conga_media", "piel", seed_golpe=2)
    n = min(len(a), len(b))
    assert not np.allclose(a[:n], b[:n], atol=0.01)


def test_sin_variacion_no_cambia_la_afinacion_del_kit():
    _, meta = sintetizar_voz("hat_cerrado", "clasico")
    assert meta["ajuste"]["afinacion"] == 1.0


def test_kits_y_notas_cubren_todas_las_voces():
    assert set(NOTAS) == set(VOCES)
    assert len(set(NOTAS.values())) == len(VOCES)
    for materiales in KITS.values():
        assert set(materiales) == set(VOCES)


def test_voz_y_material_invalidos_fallan():
    with pytest.raises(ValueError):
        sintetizar_voz("tuba", "piel")
    with pytest.raises(ValueError):
        sintetizar_voz("kick", "plastico")


def banda(y, lo, hi):
    e = np.abs(np.fft.rfft(y, n=SR)) ** 2
    return float(e[lo:hi].sum())


def rms(y):
    return float(np.sqrt(np.mean(y**2)))


@pytest.mark.parametrize("material", MATERIALES)
def test_kick_gordo_tiene_mas_nivel_y_cuerpo(material):
    delgado, _ = sintetizar_voz("kick", material, gordura=0.0, room=0.0)
    gordo, _ = sintetizar_voz("kick", material, gordura=1.0, room=0.0)
    # Se mide el golpe, no la cola: el RMS total bajaría solo por ser más largo.
    n = int(0.15 * SR)
    assert rms(gordo[:n]) > 1.25 * rms(delgado[:n])
    assert banda(gordo, 60, 250) > 1.5 * banda(delgado, 60, 250)


@pytest.mark.parametrize("voz,material", TODO)
def test_variantes_validas_y_distintas(voz, material):
    a, _ = sintetizar_voz(voz, material, seed_timbre=11, variacion=1.0)
    b, _ = sintetizar_voz(voz, material, seed_timbre=12, variacion=1.0)
    assert np.all(np.isfinite(a)) and np.max(np.abs(a)) <= 1.0
    n = min(len(a), len(b))
    assert not np.allclose(a[:n], b[:n], atol=0.01)


def test_variante_de_kick_conserva_el_grave():
    for semilla in range(20):
        y, _ = sintetizar_voz("kick", "madera", seed_timbre=semilla, variacion=1.0)
        assert proporcion_grave(y) > 0.5

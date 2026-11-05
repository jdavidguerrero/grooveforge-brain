"""Valida afinación, perfiles y pareja de hats. Teoría: percusion-generos-tonalidad.md §6."""

import numpy as np
import pytest

from gf_perc.core import SR
from gf_perc.generos import PATRONES, PERFILES, aplicar_choke, notas_de, sintetizar_en_genero
from gf_perc.kit import VOCES, Ajuste, base_hz, sintetizar_voz
from gf_perc.tonalidad import KICK_MAX, KICK_MIN, MODOS, clase_de, hz_de, midi_de, nota_kick

RAICES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]


def cents_a_objetivo(y, objetivo):
    """Pico cerca de la nota, saltando el ataque donde aún cae la afinación."""
    inicio = min(int(0.04 * SR), len(y) // 4)
    tramo = y[inicio:] * np.hanning(len(y) - inicio)
    espectro = np.abs(np.fft.rfft(tramo, n=SR * 8))
    f = np.arange(espectro.size) / 8
    ventana = (f > objetivo * 0.8) & (f < objetivo * 1.25)
    pico = f[ventana][np.argmax(espectro[ventana])]
    return abs(1200 * np.log2(pico / objetivo))


@pytest.mark.parametrize("texto,clase", [("A", 9), ("La", 9), ("do#", 1), ("Bb", 10), ("si", 11), ("Sol", 7), ("Eb", 3)])
def test_lee_tonalidades(texto, clase):
    assert clase_de(texto) == clase


def test_tonalidad_invalida_falla():
    with pytest.raises(ValueError):
        clase_de("H")


@pytest.mark.parametrize("material", ["clasico", "madera", "piel"])
@pytest.mark.parametrize("raiz", RAICES)
def test_fundamental_cae_en_su_nota(raiz, material):
    """Sin capas ni ataque se mide solo la nota: es lo que controla `afinar_hz`."""
    notas = notas_de("organic", raiz, "menor")
    for voz in ("kick", "conga_baja", "conga_media", "conga_alta", "bongo_bajo", "bongo_alto", "clave"):
        objetivo = hz_de(notas[voz])
        y, _ = sintetizar_voz(voz, material, afinar_hz=objetivo, room=0.0, centro=Ajuste(capa=0.0, ataque=0.0))
        assert cents_a_objetivo(y, objetivo) < 25, voz


@pytest.mark.parametrize("genero", sorted(PERFILES))
@pytest.mark.parametrize("raiz", RAICES)
def test_voz_completa_queda_cerca_de_su_nota(genero, raiz):
    """Las capas y la caída de afinación mueven el pico medido; se acepta una tolerancia mayor."""
    notas = notas_de(genero, raiz, "menor")
    for voz in ("kick", "conga_baja", "conga_media", "conga_alta", "bongo_bajo", "bongo_alto"):
        y, _ = sintetizar_en_genero(voz, genero, raiz, "menor")
        assert cents_a_objetivo(y, hz_de(notas[voz])) < 80, voz


@pytest.mark.parametrize("voz", ["snare", "rim"])
def test_voces_con_mucho_ruido_reciben_la_nota_pedida(voz):
    _, meta = sintetizar_voz(voz, "piel", afinar_hz=220.0)
    assert meta["afinar_hz"] == 220.0
    assert meta["ajuste"]["afinacion"] * base_hz(voz, "piel") == pytest.approx(220.0, abs=0.1)


@pytest.mark.parametrize("modo", sorted(MODOS))
@pytest.mark.parametrize("raiz", RAICES)
def test_notas_en_escala_rango_y_orden(raiz, modo):
    clase = clase_de(raiz)
    escala = {(clase + g) % 12 for g in MODOS[modo]}
    notas = notas_de("organic", raiz, modo)
    assert all(n % 12 in escala for n in notas.values())
    assert KICK_MIN <= notas["kick"] <= KICK_MAX
    orden = [notas[v] for v in ("conga_baja", "conga_media", "conga_alta", "bongo_bajo", "bongo_alto")]
    assert orden == sorted(set(orden))


def test_kick_usa_quinta_si_la_tonica_no_cae_en_rango():
    assert nota_kick(clase_de("A"), "menor") == (33, "tonica")
    midi, grado = nota_kick(clase_de("C#"), "menor")
    assert grado == "quinta" and midi == 32


def test_ida_y_vuelta_entre_hz_y_midi():
    assert midi_de(hz_de(45)) == pytest.approx(45)


def test_perfiles_y_patrones_completos():
    for genero, perfil in PERFILES.items():
        assert set(perfil.materiales) == set(VOCES)
        assert {"hat_cerrado", "hat_abierto", "kick"} <= set(PATRONES[genero])
        for voz, pasos in PATRONES[genero].items():
            assert voz in VOCES and all(0 <= p < 16 and 0 < v <= 1 for p, v in pasos.items())


@pytest.mark.parametrize("genero", sorted(PERFILES))
def test_hats_de_cada_genero_abierto_mas_largo(genero):
    a, _ = sintetizar_en_genero("hat_abierto", genero, "A", "menor")
    c, _ = sintetizar_en_genero("hat_cerrado", genero, "A", "menor")
    assert len(a) > 2 * len(c)


def test_pareja_de_hats_comparte_timbre():
    _, cerrado = sintetizar_voz("hat_cerrado", "clasico", seed_timbre=9, variacion=1.0)
    _, abierto = sintetizar_voz("hat_abierto", "clasico", seed_timbre=9, variacion=1.0)
    assert cerrado["ajuste"] == abierto["ajuste"]


def test_choke_corta_el_abierto_al_entrar_el_cerrado():
    golpe = np.ones(1000)
    pista = np.zeros(2000)
    aplicar_choke(pista, [0], [300], [golpe])
    assert pista[299] == 1.0
    assert pista[320] < 1.0
    assert np.all(pista[300 + 241:] == 0.0)


def test_choke_ignora_cerrado_anterior_y_lejano():
    pista = np.zeros(2000)
    aplicar_choke(pista, [500], [100, 1800], [np.ones(1000)])
    assert np.all(pista[500:1500] == 1.0)


def test_los_generos_suenan_distinto():
    kick = [sintetizar_en_genero("kick", g, "A", "menor")[0] for g in PERFILES]
    assert len({round(len(y) / SR, 3) for y in kick}) >= 3

"""Tests del reproductor de E2 (apps/docs/consola/04-reproductor-e2.md): programación exacta y mezcla, sin audio."""
import numpy as np
import pytest

import reproductor as r


def _sonidos(largo=100):
    return {k: np.ones(largo, dtype=np.float32) for k in r.GANANCIA}


def test_eventos_en_su_muestra():
    lista, ciclo = r.eventos(120.0)
    paso = r.SR * 60 / 120 / 4  # 6000 muestras a 120 BPM
    assert ciclo == round(16 * 16 * paso)
    kicks = [t for t, v in lista if v == "kick"]
    assert kicks[:4] == [0, 24000, 48000, 72000]
    abiertos = [t for t, v in lista if v == "hat_abierto"]
    assert abiertos == [round((c * 16 + 14) * paso) for c in (3, 7, 11, 15)]  # cierre de cada 4 compases
    assert lista == sorted(lista)


@pytest.mark.parametrize("bloque", [128, 256, 1000])
def test_cada_golpe_cae_en_su_muestra_con_cualquier_bloque(bloque):
    s = {k: np.zeros(10, dtype=np.float32) for k in r.GANANCIA}
    s["kick"] = np.array([1.0] + [0.0] * 9, dtype=np.float32)  # un impulso por kick
    seq = r.Secuenciador(s, 120.0)
    total = seq.ciclo * 2 + 5000
    audio = np.concatenate([seq.bloque(bloque) for _ in range(total // bloque)])
    impulsos = np.nonzero(audio > 0.5)[0]
    esperados = [t for vuelta in (0, 1, 2) for t, v in seq.lista if v == "kick" for t in [t + vuelta * seq.ciclo] if t < len(audio)]
    assert list(impulsos) == esperados  # mismas muestras con bloques de 128, 256 o 1000, y al dar la vuelta


def test_voces_largas_siguen_entre_bloques_y_el_choke_corta():
    s = _sonidos(largo=50_000)
    seq = r.Secuenciador(s, 120.0)
    seq.bloque(256)
    assert any(v.nombre == "kick" for v in seq.activas)
    seq._disparar("hat_cerrado", 0, np.zeros(10, dtype=np.float32))
    seq._disparar("hat_abierto", 0, np.zeros(10, dtype=np.float32))
    assert not any(v.nombre == "hat_cerrado" for v in seq.activas)  # el abierto corta al cerrado


def test_salida_sin_saturar():
    seq = r.Secuenciador(_sonidos(largo=5000), 122.0)
    audio = np.concatenate([seq.bloque(256) for _ in range(400)])
    assert audio.dtype == np.float32 and np.all(np.abs(audio) <= 1.0) and np.all(np.isfinite(audio))


def test_sonidos_de_la_drum_machine_a_48k():
    s = r.sonidos_drum_machine()
    assert set(s) >= set(r.GANANCIA) and all(v.dtype == np.float32 and len(v) > 1000 for v in s.values())

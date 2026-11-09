import numpy as np
import pytest

from gf_perc.core import SR
from gf_perc.subdrop import SUBDROPS, sintetizar_subdrop


def _hz_final(y):
    cola = y[-int(0.6 * SR):-int(0.1 * SR)]
    cruces = np.where(np.diff(np.signbit(cola)))[0]
    return SR / np.mean(np.diff(cruces[::2]))


@pytest.mark.parametrize("variante", list(SUBDROPS))
def test_subdrop_cae_a_la_tonica_sin_clics(variante):
    y, meta = sintetizar_subdrop(variante, "A", "lidia_dominante")
    assert np.all(np.isfinite(y)) and abs(np.max(np.abs(y)) - 10 ** (-1 / 20)) < 1e-3
    assert abs(y[0]) < 0.05 and abs(y[-1]) < 1e-3
    hz = 440.0 * 2 ** ((meta["nota_midi"] - 69) / 12)
    assert meta["nota_midi"] % 12 == 9 and abs(_hz_final(y) - hz) / hz < 0.03  # termina en La
    inicio = y[: int(0.02 * SR)]
    cruces = np.where(np.diff(np.signbit(inicio)))[0]
    assert len(cruces) >= 2  # empieza más agudo: hay ciclos en los primeros 20 ms

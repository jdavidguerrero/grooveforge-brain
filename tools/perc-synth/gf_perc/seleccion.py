"""Selección de variantes distintas entre sí. Teoría: percusion-kits-808-organica.md §10."""

from __future__ import annotations

import numpy as np

from .core import SR

_BANDAS = np.geomspace(40.0, 16000.0, 13)


def huella(y: np.ndarray) -> np.ndarray:
    """Energía relativa por bandas logarítmicas y duración, en escala logarítmica."""
    espectro = np.abs(np.fft.rfft(y, n=SR)) ** 2
    bandas = np.array([espectro[int(a) : int(b)].sum() for a, b in zip(_BANDAS[:-1], _BANDAS[1:])])
    bandas = 10.0 * np.log10(bandas / max(bandas.sum(), 1e-12) + 1e-6)
    # La duración en log pesa como unos dB: 6 unidades equivalen a duplicarla.
    return np.concatenate([bandas, [20.0 * np.log10(max(len(y), 1) / SR)]])


def mas_distintas(huellas: list[np.ndarray], n: int) -> list[int]:
    """Farthest-first desde el índice 0: cada elección es la más lejana a las ya elegidas."""
    m = np.asarray(huellas)
    elegidos = [0]
    distancia = np.linalg.norm(m - m[0], axis=1)
    while len(elegidos) < min(n, len(m)):
        siguiente = int(np.argmax(distancia))
        elegidos.append(siguiente)
        distancia = np.minimum(distancia, np.linalg.norm(m - m[siguiente], axis=1))
    return elegidos

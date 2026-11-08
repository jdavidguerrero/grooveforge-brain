"""Subdrop: seno grave que cae de tono hasta la tónica (apps/docs/produccion/motor-transiciones.md §3.3).

La caída de tono sigue una exponencial: arranca más agudo y se asienta en la tónica, el gesto del subdrop de un
808 largo. Un poco de saturación suma armónicos para que se oiga también
en parlantes que no llegan a 55 Hz; la fundamental sigue mandando.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .caracteres import nota_grave
from .core import SR


@dataclass(frozen=True)
class Subdrop:
    octavas: float   # cuánto arriba empieza
    caida_s: float   # constante de tiempo de la caída de tono
    largo_s: float   # duración total
    decay_s: float   # constante de tiempo del volumen
    drive: float     # saturación (0 = seno puro)


SUBDROPS = {
    "LARGA": Subdrop(1.0, 0.35, 3.5, 1.4, 1.5),
    "CORTA": Subdrop(1.0, 0.12, 1.8, 0.6, 1.8),
    "808": Subdrop(0.25, 0.05, 2.5, 1.0, 2.5),
}


def sintetizar_subdrop(variante: str, raiz: str, modo: str) -> tuple[np.ndarray, dict]:
    s = SUBDROPS[variante]
    nota = nota_grave(raiz, modo)
    f0 = 440.0 * 2 ** ((nota - 69) / 12)
    t = np.arange(int(s.largo_s * SR)) / SR
    f = f0 * 2 ** (s.octavas * np.exp(-t / s.caida_s))
    fase = 2 * np.pi * np.cumsum(f) / SR
    amp = np.exp(-t / s.decay_s) * np.minimum(1.0, t / 0.003)  # 3 ms de entrada: sin clic
    amp[-int(0.05 * SR):] *= np.linspace(1.0, 0.0, int(0.05 * SR))  # y sin clic al final
    y = np.tanh(s.drive * np.sin(fase)) / np.tanh(s.drive) * amp
    y *= 10 ** (-1 / 20) / np.max(np.abs(y))  # pico a −1 dBFS
    return y.astype(np.float32), {"variante": variante, "raiz": raiz, "modo": modo, "nota_midi": nota, "f_final_hz": round(float(f[-1]), 2)}

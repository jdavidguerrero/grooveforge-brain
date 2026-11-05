"""Patrones de bajo por sensación con el kick. Diseño: apps/docs/produccion/motor-bajo.md §3."""

from __future__ import annotations

import random

COMPASES = 16
PASOS_POR_TIEMPO = 4
PASOS = 16
PASOS_KICK = {0, 4, 8, 12}
_NOTAS = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}

# (paso, largo en pasos, intervalo, velocity base). Cada familia es un compás fuera de los golpes del kick.
FAMILIAS = {
    "ROLLING": [(p, 1, 0, 92 if p % 4 == 2 else 60) for p in range(16) if p % 4],
    "ROLLING AIRE": [(p, 1, 0, 94 if p % 4 == 2 else 66) for p in range(16) if p % 4 >= 2],
    "CONTRATIEMPO": [(2, 2, 0, 100), (6, 2, 0, 96), (10, 2, 0, 100), (14, 2, 0, 96)],
    "TRESILLO": [(3, 2, 0, 98), (6, 2, 12, 84), (11, 2, 0, 96), (14, 2, 0, 88)],
    "TAMBORA": [(2, 2, 0, 96), (5, 1, 0, 50), (6, 2, 12, 84), (10, 2, 0, 98), (13, 1, 0, 48), (14, 2, 12, 82)],
    "OCTAVAS": [(2, 2, 0, 96), (6, 2, 12, 84), (10, 2, 0, 94), (14, 2, 12, 86)],
}


def fundamental(raiz: str) -> int:
    """La nota de la tónica dentro de Mi1–Do2 (28–36) o, si no cabe, la más cercana."""
    clase = _NOTAS[raiz]
    return min((n for n in range(24, 48) if n % 12 == clase), key=lambda n: (0 if 28 <= n <= 36 else 1, abs(n - 33), -n))


def _compas(base: list[tuple], rnd: random.Random) -> list[tuple]:
    """Variación ligera: un ghost de más o de menos y acentos que se mueven; nunca sobre el kick."""
    filas = [(p, d, i, max(1, min(127, v + rnd.randint(-6, 6)))) for p, d, i, v in base]
    ocupados = {x for p, d, _, _ in filas for x in range(p, p + d)}
    libres = [p for p in range(16) if p not in ocupados and p not in PASOS_KICK]
    if libres and rnd.random() < 0.6:
        filas.append((rnd.choice(libres), 1, 0, rnd.randint(42, 56)))
    return sorted(filas)


def _remate(filas: list[tuple], rnd: random.Random) -> list[tuple]:
    """Último compás de la frase: una nota que no es la fundamental empuja hacia el siguiente."""
    salida = list(filas)
    ultimo = max(range(len(salida)), key=lambda i: salida[i][0])
    p, d, _, v = salida[ultimo]
    salida[ultimo] = (p, d, rnd.choice((12, 7, -1)), max(v, 88))
    return salida


def patron(familia: str, raiz: str, tanda: int) -> dict:
    rnd = random.Random(f"{familia}:{tanda}")
    tonica = fundamental(raiz)
    frase = [_compas(FAMILIAS[familia], rnd) for _ in range(3)]
    frase.append(_remate(_compas(FAMILIAS[familia], rnd), rnd))
    notas = []
    for c in range(COMPASES):
        for p, d, i, v in frase[c % len(frase)]:
            notas.append(
                {
                    "pitch": tonica + i,
                    "start_beat": (c * PASOS + p) / PASOS_POR_TIEMPO,
                    "duration_beats": round(d * 0.9 / PASOS_POR_TIEMPO, 4),
                    "velocity": v,
                }
            )
    return {"nombre": familia, "inicio": 0.0, "longitud": float(COMPASES * PASOS / PASOS_POR_TIEMPO), "notas": notas}

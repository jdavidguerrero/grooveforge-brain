"""Notas de cada voz según tonalidad. Teoría: percusion-generos-tonalidad.md §2."""

from __future__ import annotations

import numpy as np

_CLASES = {
    "C": 0, "DO": 0, "D": 2, "RE": 2, "E": 4, "MI": 4, "F": 5, "FA": 5,
    "G": 7, "SOL": 7, "A": 9, "LA": 9, "B": 11, "SI": 11,
}
MODOS = {
    "menor": (0, 2, 3, 5, 7, 8, 10),
    "mayor": (0, 2, 4, 5, 7, 9, 11),
    "dorico": (0, 2, 3, 5, 7, 9, 10),
    "frigio": (0, 1, 3, 5, 7, 8, 10),
    "lidia_dominante": (0, 2, 4, 6, 7, 9, 10),
}
KICK_MIN, KICK_MAX = 28, 36  # 41,2 a 65,4 Hz
# Grados de la escala (0 = tónica) permitidos por voz.
_GRADOS = {
    "snare": (0, 4),
    "rim": (0, 4),
    "clave": (0, 4),
    "conga_baja": (0, 4),
    "conga_media": (2, 4, 0),
    "conga_alta": (0, 4, 6),
    "bongo_bajo": (0, 2, 4),
    "bongo_alto": (0, 4),
}
_ASCENDENTES = ("conga_baja", "conga_media", "conga_alta", "bongo_bajo", "bongo_alto")


def clase_de(raiz: str) -> int:
    texto = raiz.strip().upper().replace("♯", "#").replace("♭", "B")
    alteracion = 0
    if len(texto) > 1 and texto[-1] == "#":
        alteracion, texto = 1, texto[:-1]
    elif len(texto) > 1 and texto[-1] == "B" and texto[:-1] in _CLASES:
        alteracion, texto = -1, texto[:-1]
    if texto not in _CLASES:
        raise ValueError(f"tonalidad desconocida: {raiz!r}")
    return (_CLASES[texto] + alteracion) % 12


def hz_de(midi: float) -> float:
    return 440.0 * 2.0 ** ((midi - 69) / 12)


def midi_de(hz: float) -> float:
    return 69.0 + 12.0 * float(np.log2(hz / 440.0))


def nota_kick(clase: int, modo: str) -> tuple[int, str]:
    """Tónica si cae en el rango de peso del kick; si no, la quinta."""
    quinta = (clase + MODOS[modo][4]) % 12
    for grado, pc in (("tonica", clase), ("quinta", quinta)):
        for midi in range(KICK_MIN, KICK_MAX + 1):
            if midi % 12 == pc:
                return midi, grado
    raise ValueError("ninguna nota en el rango del kick")


def _mas_cercana(hz: float, clases: set[int], minimo: int = 0) -> int:
    objetivo = midi_de(hz)
    candidatas = [m for m in range(max(minimo, 12), 110) if m % 12 in clases]
    return min(candidatas, key=lambda m: abs(m - objetivo))


def asignar_notas(raiz: str, modo: str, bases_hz: dict[str, float]) -> dict[str, int]:
    """Nota MIDI por voz. `bases_hz` da el registro natural de cada voz con sus materiales."""
    if modo not in MODOS:
        raise ValueError(f"modo desconocido: {modo!r}")
    clase = clase_de(raiz)
    escala = MODOS[modo]
    notas = {"kick": nota_kick(clase, modo)[0]}
    previa = 0
    for voz in _ASCENDENTES:
        clases = {(clase + escala[g]) % 12 for g in _GRADOS[voz]}
        notas[voz] = _mas_cercana(bases_hz[voz], clases, previa + 1)
        previa = notas[voz]
    for voz in ("snare", "rim", "clave"):
        clases = {(clase + escala[g]) % 12 for g in _GRADOS[voz]}
        notas[voz] = _mas_cercana(bases_hz[voz], clases)
    return notas

"""Patrones de batería a partir del kick y el bajo reales. Diseño: apps/docs/produccion/motor-drums.md §7."""

from __future__ import annotations

import random
from collections import Counter

PASOS = 16
COMPASES = 16
CLAVE, SNARE, CLAP, HAT_CERRADO, HAT_ABIERTO = 47, 48, 49, 52, 53
_SON_32 = (0, 3, 6, 10, 12)
_NEGRAS = {0, 4, 8, 12}


def _conteo(notas: list[dict]) -> tuple[Counter, int]:
    """Cuántos compases tienen un golpe en cada paso. Varias notas en el mismo paso y compás cuentan una vez."""
    golpes = {(int(n["start_beat"] // 4), round(n["start_beat"] * 4) % PASOS) for n in notas}
    compases = max((c for c, _ in golpes), default=-1) + 1
    return Counter(p for _, p in golpes), compases


def _fuertes_y_ghosts(notas: list[dict]) -> tuple[set[int], set[int]]:
    conteo, compases = _conteo(notas)
    umbral = max(1, compases / 2)
    fuertes = {p for p, n in conteo.items() if n >= umbral}
    # Un golpe suelto, como un remate en el último compás, no ocupa el paso en toda la frase.
    ghosts = {p for p, n in conteo.items() if p not in fuertes and n >= max(2, compases / 8)}
    return fuertes, ghosts


def analizar(notas_kick: list[dict], notas_bajo: list[dict]) -> dict:
    kick, kick_ghost = _fuertes_y_ghosts(notas_kick)
    kick = kick or set(_NEGRAS)  # Sin clip de kick se asume el cuatro por cuatro del borrador.
    bajo, bajo_ghost = _fuertes_y_ghosts(notas_bajo)
    ocupados = kick | kick_ghost | bajo | bajo_ghost
    return {"kick": sorted(kick), "bajo": sorted(bajo), "ghosts": sorted((kick_ghost | bajo_ghost) - kick - bajo), "libres": [p for p in range(PASOS) if p not in ocupados]}


def _respuesta(a: dict) -> list[int]:
    salida: list[int] = []
    for p in a["libres"]:
        if len(salida) < 4 and (not salida or p - salida[-1] >= 2):
            salida.append(p)
    return salida


def _contra(a: dict) -> list[int]:
    libres = set(a["libres"])
    salida = set()
    for b in a["bajo"]:
        siguiente = next((b + d for d in (1, 2) if b + d in libres), None)
        if siguiente is not None:
            salida.add(siguiente)
    return sorted(salida)


def opciones(a: dict) -> list[dict]:
    """Las tres relaciones con kick y bajo; se descarta la que no tenga con qué sonar."""
    salida = [
        {"nombre": "RESPUESTA", "clave": _respuesta(a)},
        {"nombre": "SON 3-2", "clave": list(_SON_32)},
        {"nombre": "CONTRA", "clave": _contra(a)},
    ]
    return [o for o in salida if o["clave"]]


def _nota(compas: int, paso: int, pitch: int, velocity: int, largo: float = 0.2) -> dict:
    return {"pitch": pitch, "start_beat": (compas * PASOS + paso) / 4, "duration_beats": largo, "velocity": max(1, min(127, velocity))}


def clip_drums(opcion: dict, semilla: int = 1) -> dict:
    rnd = random.Random(f"{opcion['nombre']}:{semilla}")
    notas = []
    for c in range(COMPASES):
        for i, p in enumerate(opcion["clave"]):
            notas.append(_nota(c, p, CLAVE, (104 if i == 0 else 88) + rnd.randint(-6, 6)))
        for p in (4, 12):
            notas.append(_nota(c, p, CLAP, 100 + rnd.randint(-5, 5)))
        for p in sorted(_NEGRAS):
            notas.append(_nota(c, p, HAT_CERRADO, 64 + rnd.randint(-6, 6)))
        for p in (2, 6, 10, 14):
            notas.append(_nota(c, p, HAT_ABIERTO, 80 + rnd.randint(-6, 6)))
        if c % 4 == 3:
            notas.append(_nota(c, 15, SNARE, 76 + rnd.randint(-6, 6)))
    return {"nombre": f"DRUMS {opcion['nombre']}", "inicio": 0.0, "longitud": float(COMPASES * 4), "notas": notas}

"""Qué cambió Juan en un borrador. Teoría: apps/docs/theory/motor-primera-entrega.md §4."""

from __future__ import annotations

_TOL = 1e-3
_MAX_MOVIDA = 0.5


def diferencia(antes: list[dict], despues: list[dict]) -> dict:
    a, d = list(antes), list(despues)
    velocidad, duracion, movidas = [], [], []

    def anotar_cambios(x: dict, y: dict) -> None:
        if x["velocity"] != y["velocity"]:
            velocidad.append({"inicio": y["start_beat"], "pitch": y["pitch"], "de": x["velocity"], "a": y["velocity"]})
        if abs(x["duration_beats"] - y["duration_beats"]) > _TOL:
            duracion.append({"inicio": y["start_beat"], "pitch": y["pitch"], "de": x["duration_beats"], "a": y["duration_beats"]})

    for x in list(a):
        igual = next((y for y in d if y["pitch"] == x["pitch"] and abs(y["start_beat"] - x["start_beat"]) <= _TOL), None)
        if igual is not None:
            anotar_cambios(x, igual)
            a.remove(x)
            d.remove(igual)

    pares = sorted(
        (
            (abs(x["start_beat"] - y["start_beat"]), i, j)
            for i, x in enumerate(a)
            for j, y in enumerate(d)
            if x["pitch"] == y["pitch"] and abs(x["start_beat"] - y["start_beat"]) < _MAX_MOVIDA
        )
    )
    usadas_a, usadas_d = set(), set()
    for _, i, j in pares:
        if i not in usadas_a and j not in usadas_d:
            usadas_a.add(i)
            usadas_d.add(j)
            movidas.append({"pitch": a[i]["pitch"], "de": a[i]["start_beat"], "a": d[j]["start_beat"]})
            anotar_cambios(a[i], d[j])

    quitadas = [n for i, n in enumerate(a) if i not in usadas_a]
    anadidas = [n for j, n in enumerate(d) if j not in usadas_d]
    return {
        "sin_cambios": not (velocidad or duracion or movidas or quitadas or anadidas),
        "movidas": movidas,
        "quitadas": quitadas,
        "anadidas": anadidas,
        "velocidad": velocidad,
        "duracion": duracion,
    }

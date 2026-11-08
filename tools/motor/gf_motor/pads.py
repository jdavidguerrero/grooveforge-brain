"""Pads y atmósfera sobre la progresión elegida. Diseño: apps/docs/produccion/motor-pads.md."""

from __future__ import annotations

import random

from .acordes import PROGRESIONES, clases_acorde
from .bajo import _NOTAS

COMPASES = 16
PATRONES_PAD = ("DRON", "LINEA GUIA", "ABIERTO ALTO")
PATRONES_ATMOS = ("NUBE", "SWELL", "RAIZ GRAVE")
DRON_BASE = 45  # La2: por encima del bajo (Mi1–Do2) y por debajo de los acordes.
ALTO_MIN, ALTO_MAX = 64, 88  # Por encima de CHORDS (48–64).
GUIA_MIN = 65
NUBE_MIN, NUBE_MAX = 69, 88


def _en_rango(clase: int, minimo: int) -> int:
    return minimo + (clase - minimo) % 12


def _grados(progresion: str | None) -> tuple[int, ...]:
    return PROGRESIONES.get(progresion or "", (0,))


def _nota(pitch: int, inicio: float, duracion: float, velocity: int) -> dict:
    return {"pitch": pitch, "start_beat": round(inicio, 4), "duration_beats": round(duracion, 4), "velocity": velocity}


def _dron(tonica: int, escala, grados) -> list[dict]:
    raiz = _en_rango(tonica, DRON_BASE)
    return [_nota(raiz, 0, COMPASES * 4 - 0.5, 70), _nota(raiz + 7, 0, COMPASES * 4 - 0.5, 64)]


def _linea_guia(tonica: int, escala, grados) -> list[dict]:
    """3.ª y 7.ª de cada acorde; cada voz toma la octava más cercana a la anterior y se liga si no cambia."""
    voces: list[list[dict]] = [[], []]
    previas = [None, None]
    for c in range(COMPASES):
        clases = clases_acorde(tonica, escala, grados[c % len(grados)], True)
        for v, clase in enumerate((clases[1], clases[3])):
            base = _en_rango(clase, GUIA_MIN + 7 * v)
            candidatos = (base - 12, base, base + 12) if previas[v] else (base,)
            pitch = min((p for p in candidatos if p >= GUIA_MIN), key=lambda p: abs(p - (previas[v] or p)))
            if voces[v] and voces[v][-1]["pitch"] == pitch:
                voces[v][-1]["duration_beats"] = round(voces[v][-1]["duration_beats"] + 4, 4)
            else:
                voces[v].append(_nota(pitch, c * 4, 3.9, 66))
            previas[v] = pitch
    return voces[0] + voces[1]


def _abierto_alto(tonica: int, escala, grados) -> list[dict]:
    notas = []
    for c in range(COMPASES):
        clases = clases_acorde(tonica, escala, grados[c % len(grados)])
        raiz = _en_rango(clases[0], ALTO_MIN)
        for p in (raiz, raiz + (clases[2] - clases[0]) % 12, raiz + 12 + (clases[1] - clases[0]) % 12):
            notas.append(_nota(p, c * 4, 3.9, 62))
    return notas


def _nube(tonica: int, escala, grados, tanda: int) -> list[dict]:
    """Notas sueltas y largas de color (fundamental, quinta, 4.ª), fuera del pulso; la tanda es la semilla."""
    azar = random.Random(f"nube-{tonica}-{escala}-{tanda}")
    clases = [(tonica + escala[g]) % 12 for g in (0, 4, 3)]
    alturas = sorted(p for p in range(NUBE_MIN, NUBE_MAX + 1) if p % 12 in clases)
    notas = []
    for bloque in range(COMPASES // 2):
        for _ in range(azar.choice((1, 2))):
            inicio = bloque * 8 + azar.randrange(0, 4) + azar.choice((0.25, 0.5, 0.75))  # deja al menos 4 tiempos al final
            duracion = min(azar.uniform(6, 12), COMPASES * 4 - inicio)
            notas.append(_nota(azar.choice(alturas), inicio, duracion, azar.randrange(40, 66)))
    return sorted(notas, key=lambda n: n["start_beat"])


def _swell(tonica: int, escala, grados) -> list[dict]:
    """Cada 4 compases, las notas del acorde entran una por compás y se sostienen hasta el final del bloque."""
    notas = []
    for bloque in range(COMPASES // 4):
        clases = clases_acorde(tonica, escala, grados[(bloque * 4) % len(grados)])
        raiz = _en_rango(clases[0], 57)
        voces = (raiz, raiz + (clases[2] - clases[0]) % 12, raiz + 12 + (clases[1] - clases[0]) % 12, raiz + 12)
        fin = (bloque + 1) * 16 - 0.25
        for k, p in enumerate(voces):
            inicio = bloque * 16 + k * 4
            notas.append(_nota(p, inicio, fin - inicio, 50 + 8 * k))
    return notas


def _raiz_grave(tonica: int, escala, grados) -> list[dict]:
    raiz = _en_rango(tonica, DRON_BASE)
    return [_nota(raiz, 0, 31.5, 72), _nota(raiz, 32, 31.5, 72)]


def clip_pad(parte: str, patron: str, progresion: str | None, raiz: str, escala: tuple[int, ...], tanda: int = 1) -> dict:
    tonica, grados = _NOTAS[raiz], _grados(progresion)
    if patron == "NUBE":
        notas = _nube(tonica, escala, grados, tanda)
    else:
        fn = {"DRON": _dron, "LINEA GUIA": _linea_guia, "ABIERTO ALTO": _abierto_alto, "SWELL": _swell, "RAIZ GRAVE": _raiz_grave}[patron]
        notas = fn(tonica, escala, grados)
    prefijo = "PAD" if parte == "pad" else "ATMOS"
    return {"nombre": f"{prefijo} {patron}", "inicio": 0.0, "longitud": float(COMPASES * 4), "notas": notas}

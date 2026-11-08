"""Stabs y plucks en los huecos del lead, con avisos si otra pista ya cumple ese papel. Diseño: apps/docs/produccion/motor-detalles.md."""

from __future__ import annotations

import random

from .acordes import PROGRESIONES, clases_acorde
from .bajo import _NOTAS

COMPASES = 16
CICLO = 32  # 2 compases en semicorcheas
PATRONES_STAB = ("GANCHO", "RESPUESTA", "ANTICIPO")
PATRONES_PLUCK = ("ARPEGIO 16", "ARPEGIO 3-3-2", "RIFF EN HUECOS")
STAB_MIN = 60  # Do4: la tríada cabe hasta Do5
PLUCK_MIN, PLUCK_MAX = 64, 84
_PREFERIDOS = (14, 6, 10, 3, 11, 30, 22, 26, 19, 27, 7, 15, 23, 31, 2, 18)  # «y» y síncopas primero
_TRESILLO = (0, 3, 6, 8, 11, 14)


def _paso(beat: float) -> int:
    return round(beat * 4)


def huecos_lead(lead: list[dict]) -> list[int]:
    """Pasos del ciclo de 2 compases donde el lead calla en al menos 3 de cada 4 vueltas en que suena."""
    if not lead:
        return list(range(CICLO))
    ocupado: dict[int, set[int]] = {}
    for n in lead:
        for s in range(_paso(n["start_beat"]), max(_paso(n["start_beat"] + n["duration_beats"]), _paso(n["start_beat"]) + 1)):
            ocupado.setdefault(s // CICLO, set()).add(s % CICLO)
    vueltas = len(ocupado)
    return [p for p in range(CICLO) if sum(p in v for v in ocupado.values()) <= vueltas / 4]


def avisos(acordes: dict[str, list[dict]], lead: list[dict], libres: list[int]) -> dict[str, str | None]:
    """Qué papel ya cubre otra pista. Informa; no quita opciones."""
    stab = pluck = riff = None
    for nombre, notas in acordes.items():
        if not notas:
            continue
        duracion = sum(n["duration_beats"] for n in notas) / len(notas)
        compases = {int(n["start_beat"] // 4) for n in notas}
        golpes = len({round(n["start_beat"], 3) for n in notas}) / max(len(compases), 1)
        if duracion <= 1.0:
            pluck = f"Tu {nombre} ya es punteado: hace el papel de pluck, y un arpegio lo duplicaría."
            if golpes >= 4:
                stab = f"Tu {nombre} ya marca el ritmo con {golpes:.0f} golpes por compás: un stab solo suma si es escaso."
    if lead and len(libres) < 0.3 * CICLO:
        riff = "Tu lead deja pocos huecos para un riff: sonaría encima de él."
    return {"stab": stab, "pluck": pluck, "riff": riff}


def _grados(progresion: str | None) -> tuple[int, ...]:
    return PROGRESIONES.get(progresion or "", (0,))


def _triada(tonica: int, escala, grado: int, minimo: int) -> list[int]:
    clases = clases_acorde(tonica, escala, grado)
    raiz = minimo + (clases[0] - minimo) % 12
    return [raiz, raiz + (clases[1] - clases[0]) % 12, raiz + (clases[2] - clases[0]) % 12]


def _nota(pitch: int, paso: int, duracion: float, velocity: int) -> dict:
    return {"pitch": pitch, "start_beat": paso / 4, "duration_beats": duracion, "velocity": velocity}


def _stab(tonica, escala, grados, paso_abs: int, velocity: int, siguiente: bool = False) -> list[dict]:
    c = paso_abs // 16 + (1 if siguiente else 0)
    return [_nota(p, paso_abs, 0.3, velocity) for p in _triada(tonica, escala, grados[c % len(grados)], STAB_MIN)]


def _gancho(tonica, escala, grados, libres, kick, lead, tanda) -> list[dict]:
    pasos = [p for p in _PREFERIDOS if p in libres and p % 16 not in kick][:3] or [14, 30]
    notas = []
    for ciclo in range(COMPASES // 2):
        for i, p in enumerate(sorted(pasos)):
            notas += _stab(tonica, escala, grados, ciclo * CICLO + p, 104 if i == 0 else 92)
    return notas


def _respuesta(tonica, escala, grados, libres, kick, lead, tanda) -> list[dict]:
    """Un golpe en el primer hueco tras el final de cada frase del lead (una nota seguida de 1 tiempo de silencio)."""
    if not lead:
        return _anticipo(tonica, escala, grados, libres, kick, lead, tanda)
    inicios = sorted(_paso(n["start_beat"]) for n in lead)
    finales = sorted(_paso(n["start_beat"] + n["duration_beats"]) for n in lead)
    notas, usados = [], set()
    for f in finales:
        siguiente = next((i for i in inicios if i >= f), None)
        if siguiente is not None and siguiente - f < 4:
            continue
        paso = next((s for s in range(f, f + 8) if s % CICLO in libres and s % 16 not in kick), None)
        if paso is None or paso >= COMPASES * 16 or paso // CICLO in usados:
            continue
        usados.add(paso // CICLO)
        notas += _stab(tonica, escala, grados, paso, 100)
    return notas


def _anticipo(tonica, escala, grados, libres, kick, lead, tanda) -> list[dict]:
    notas = []
    for c in range(1, COMPASES, 2):
        notas += _stab(tonica, escala, grados, c * 16 + 14, 100, siguiente=True)
    return notas


def _tonos_pluck(tonica, escala, grado) -> list[int]:
    raiz, tercera, quinta = _triada(tonica, escala, grado, PLUCK_MIN)
    return [raiz, tercera, quinta, raiz + 12]


def _arpegio16(tonica, escala, grados, libres, kick, lead, tanda) -> list[dict]:
    orden = (0, 1, 2, 3, 2, 1)
    notas = []
    for c in range(COMPASES):
        tonos = _tonos_pluck(tonica, escala, grados[c % len(grados)])
        for s in range(16):
            notas.append(_nota(tonos[orden[s % len(orden)]], c * 16 + s, 0.22, 88 if s in _TRESILLO else 66))
    return notas


def _arpegio332(tonica, escala, grados, libres, kick, lead, tanda) -> list[dict]:
    notas = []
    for c in range(COMPASES):
        tonos = _tonos_pluck(tonica, escala, grados[c % len(grados)])
        for i, s in enumerate(_TRESILLO):
            notas.append(_nota(tonos[i % 4], c * 16 + s, 0.4, 96 if i % 3 == 0 else 80))
    return notas


def _riff(tonica, escala, grados, libres, kick, lead, tanda) -> list[dict]:
    """Frase fija de 2 compases en los huecos del lead; la altura sigue el acorde del compás. La tanda es la semilla."""
    azar = random.Random(f"riff-{tonica}-{escala}-{tanda}")
    candidatos = [p for p in libres if p % 2 == 0 or p % 4 == 3] or list(libres)
    pasos = sorted(azar.sample(candidatos, min(6, len(candidatos))))
    figura = [(p, azar.randrange(4), azar.choice((0.22, 0.45))) for p in pasos]
    notas = []
    for ciclo in range(COMPASES // 2):
        for p, k, d in figura:
            paso = ciclo * CICLO + p
            tonos = _tonos_pluck(tonica, escala, grados[(paso // 16) % len(grados)])
            notas.append(_nota(tonos[k], paso, d, 90))
    return notas


_FUNCIONES = {
    "GANCHO": _gancho, "RESPUESTA": _respuesta, "ANTICIPO": _anticipo,
    "ARPEGIO 16": _arpegio16, "ARPEGIO 3-3-2": _arpegio332, "RIFF EN HUECOS": _riff,
}


def clip_detalle(parte: str, patron: str, progresion: str | None, raiz: str, escala: tuple[int, ...], libres: list[int], kick: list[int], lead: list[dict], tanda: int = 1) -> dict:
    notas = _FUNCIONES[patron](_NOTAS[raiz], escala, _grados(progresion), libres, set(kick), lead, tanda)
    notas = [n for n in notas if n["start_beat"] < COMPASES * 4]
    prefijo = "STAB" if parte == "stab" else "PLUCK"
    return {"nombre": f"{prefijo} {patron}", "inicio": 0.0, "longitud": float(COMPASES * 4), "notas": sorted(notas, key=lambda n: (n["start_beat"], n["pitch"]))}

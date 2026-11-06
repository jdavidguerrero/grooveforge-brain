"""Motivos melódicos sobre el groove. Diseño: apps/docs/produccion/motor-motivo.md."""

from __future__ import annotations

import random

from .bajo import _NOTAS

COMPASES = 16
PASOS_MOTIVO = 32
TOPE_OCUPACION = 0.8
_NOTA_LA3 = 57
_MAXIMO = 76  # Mi5.
# Grado (índice en la escala) que define cada modo.
COLOR = {"lidia_dominante": 3, "menor": 5, "dorico": 5, "frigio": 1, "mayor": 6}
# Contornos en grados. "C" es la nota de color, que siempre aparece temprano.
_CONTORNOS = ((0, "C", 4, 2), (4, "C", 2, 0), (2, 4, "C", 4), (0, 2, "C", 1), (4, 2, "C", 2), ("C", 4, 2, 0))

# (paso de 0 a 31, largo en pasos). Ninguna empieza en un golpe del kick a negras.
RITMOS_FIJOS = {
    "CORTO": ((2, 1), (3, 1), (6, 2), (14, 1), (15, 1), (18, 1), (22, 2), (27, 2)),
    "MIXTO": ((2, 6), (10, 1), (11, 1), (14, 2), (18, 8), (27, 1), (30, 2)),
    "LARGO": ((2, 10), (14, 2), (18, 8), (27, 3)),
}


def _ritmo_en_huecos(libres: list[int]) -> tuple[tuple[int, int], ...]:
    salida = []
    for compas in (0, 16):
        elegidos = [p for p in libres if p % 4 != 0][:5]
        for i, p in enumerate(elegidos):
            siguiente = elegidos[i + 1] if i + 1 < len(elegidos) else 16
            salida.append((compas + p, min(2, siguiente - p)))
    return tuple(salida)


def _ritmo_con_clave(clave: list[int]) -> tuple[tuple[int, int], ...]:
    pasos = [p for p in clave if p % 4 != 0] or list(clave)
    return tuple((c + p, 2) for c in (0, 16) for p in pasos)


def opciones(libres: list[int], clave: list[int]) -> dict[str, tuple[tuple[int, int], ...]]:
    salida = dict(RITMOS_FIJOS)
    if len(libres) >= 2:
        salida["RESPUESTA"] = _ritmo_en_huecos(libres)
    if clave:
        salida["CON CLAVE"] = _ritmo_con_clave(clave)
    return salida


def _altura(tonica: int, escala: tuple[int, ...], grado: int) -> int:
    return tonica + escala[grado % 7] + 12 * (grado // 7)


def motivo(nombre: str, ritmo: tuple[tuple[int, int], ...], raiz: str, modo: str, escala: tuple[int, ...], tanda: int = 1) -> dict:
    rnd = random.Random(f"{nombre}:{raiz}:{modo}:{tanda}")
    tonica = _NOTA_LA3 + (_NOTAS[raiz] - 9) % 12
    if tonica > 62:
        tonica -= 12  # Mantiene el motivo en registro medio aunque cambie la tonalidad.
    contorno = rnd.choice(_CONTORNOS)
    color = COLOR.get(modo, 5)
    grados = [color if g == "C" else g for g in contorno]
    frase_a = [(p, d, grados[i % len(grados)]) for i, (p, d) in enumerate(ritmo)]
    # Termina en la tónica o la quinta: la frase descansa aunque el color haya sonado antes.
    frase_b = list(frase_a)
    p, d, final_a = frase_b[-1]
    # Debe diferir del final de A: si no, B sería igual y no habría remate.
    frase_b[-1] = (p, d, 0 if final_a != 0 else 4)
    if len(frase_b) > 1:
        p2, d2, _ = frase_b[-2]
        frase_b[-2] = (p2, d2, 2)
    notas = []
    for bloque in range(COMPASES // 2):
        frase = frase_b if bloque % 4 == 3 else frase_a
        for i, (p, d, g) in enumerate(frase):
            pitch = _altura(tonica, escala, g)
            while pitch > _MAXIMO:
                pitch -= 12
            velocity = (98 if i == 0 else 84 if d >= 2 else 72) + rnd.randint(-5, 5)
            notas.append({"pitch": pitch, "start_beat": (bloque * PASOS_MOTIVO + p) / 4, "duration_beats": round(d * 0.92 / 4, 4), "velocity": velocity})
    return {"nombre": f"MOTIVO {nombre}", "inicio": 0.0, "longitud": float(COMPASES * 4), "notas": notas}


def ocupacion(clip: dict) -> float:
    return sum(n["duration_beats"] for n in clip["notas"]) / clip["longitud"]

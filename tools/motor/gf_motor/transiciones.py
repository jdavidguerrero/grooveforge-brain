"""Roll, riser, fill y ear candy en los puntos del arreglo. Diseño: apps/docs/produccion/motor-transiciones.md."""

from __future__ import annotations

from .acordes import clases_acorde
from .bajo import _NOTAS
from .drums import CLAP, SNARE

ELEMENTOS = {
    "roll": ("ROLL", ("ROLL 4", "ROLL 8", "ROLL 2")),
    "riser": ("RISER", ("RISER NOTA", "RISER ESCALERA", "RISER OCTAVAS")),
    "fill": ("FILL", ("FILL TRESILLO", "FILL ULTIMO TIEMPO")),
    "ear": ("EAR CANDY", ("IMPACTO", "STAB ANTES DEL DROP", "GOTA EN EL BREAK")),
}
_SUBDIVISIONES = (1.0, 0.5, 0.25, 0.125)  # negras, corcheas, semicorcheas, fusas (en tiempos)
RISER_MAX = 64.0  # 16 compases


def secciones(clips: list[dict]) -> list[dict]:
    """Bloques con nombre de ARRANGE; cada uno termina donde empieza el siguiente con nombre."""
    con_nombre = sorted((c for c in clips if (c.get("name") or "").strip()), key=lambda c: c["start"])
    salida = []
    for i, c in enumerate(con_nombre):
        fin = con_nombre[i + 1]["start"] if i + 1 < len(con_nombre) else c["start"] + c["length"]
        salida.append({"nombre": c["name"].strip().upper(), "inicio": c["start"], "fin": fin})
    return salida


def _drops(secs: list[dict]) -> list[tuple[dict | None, dict]]:
    """(sección anterior, drop) para cada DROP; la anterior es la que sube."""
    return [(secs[i - 1] if i else None, s) for i, s in enumerate(secs) if s["nombre"].startswith("DROP")]


def _nota(pitch: int, inicio: float, duracion: float, velocity: int) -> dict:
    return {"pitch": pitch, "start_beat": round(inicio, 4), "duration_beats": round(duracion, 4), "velocity": max(1, min(127, round(velocity)))}


def _roll(llegada: float, compases: float) -> list[dict]:
    """Cuatro tramos iguales de negras a fusas; la fuerza sube de 50 a 120 a lo largo de todo el roll."""
    largo = compases * 4
    inicio = llegada - largo
    tramo = largo / len(_SUBDIVISIONES)
    notas = []
    for k, paso in enumerate(_SUBDIVISIONES):
        t = inicio + k * tramo
        while t < inicio + (k + 1) * tramo - 1e-6:
            notas.append(_nota(SNARE, t, paso * 0.8, 50 + 70 * (t - inicio) / largo))
            t += paso
    return notas


def _riser(patron: str, tonica: int, escala, desde: float, llegada: float) -> list[dict]:
    inicio = max(desde, llegada - RISER_MAX)
    largo = llegada - inicio
    raiz = 57 + (tonica - 57) % 12  # La3
    if patron == "RISER NOTA":
        return [_nota(raiz, inicio, largo - 0.25, 90)]
    if patron == "RISER OCTAVAS":
        cuarto = largo / 4
        return [_nota(raiz - 12 + 12 * k, inicio + k * cuarto, cuarto - 0.25, 80 + 10 * k) for k in range(4)]
    clases = clases_acorde(tonica, escala, 0)
    voces = [raiz, raiz + (clases[1] - clases[0]) % 12, raiz + (clases[2] - clases[0]) % 12, raiz + 12]
    paso = largo / len(voces)
    return [_nota(p, inicio + k * paso, llegada - (inicio + k * paso) - 0.25, 70 + 12 * k) for k, p in enumerate(voces)]


def _fill(patron: str, llegada: float) -> list[dict]:
    compas = llegada - 4
    if patron == "FILL TRESILLO":
        return [_nota(CLAP, compas + s / 4, 0.2, 70 + 8 * i) for i, s in enumerate((0, 3, 6, 8, 11, 14))]
    notas = [_nota(SNARE, compas + s / 4, 0.2, 80 + 10 * k) for k, s in enumerate((12, 13, 14, 15))]
    return notas + [_nota(CLAP, compas + 15 / 4, 0.2, 115)]


def _ear(patron: str, tonica: int, escala, secs: list[dict]) -> list[dict]:
    notas = []
    if patron == "IMPACTO":
        for _, drop in _drops(secs):
            notas.append(_nota(33 + (tonica - 33) % 12, drop["inicio"], 7.75, 115))  # La1: el sub drop
    elif patron == "STAB ANTES DEL DROP":
        clases = clases_acorde(tonica, escala, 0)
        raiz = 60 + (clases[0] - 60) % 12
        for _, drop in _drops(secs):
            for p in (raiz, raiz + (clases[1] - clases[0]) % 12, raiz + (clases[2] - clases[0]) % 12):
                notas.append(_nota(p, drop["inicio"] - 4 + 2.5, 0.3, 105))
    else:
        alta = 81 + (tonica - 81) % 12
        for s in secs:
            if s["nombre"].startswith("BREAK"):
                for t in (s["inicio"], s["inicio"] + (s["fin"] - s["inicio"]) / 2):
                    notas += [_nota(alta, t, 6.0, 80), _nota(alta + 7, t, 6.0, 70)]
    return notas



def clips_transicion(elemento: str, patron: str, secs: list[dict], raiz: str, escala: tuple[int, ...]) -> list[dict]:
    """Un clip por punto del arreglo, o ninguno si el arreglo no tiene dónde ponerlo."""
    tonica = _NOTAS[raiz]
    clips = []
    if elemento == "roll":
        compases = {"ROLL 4": 4, "ROLL 8": 8, "ROLL 2": 2}[patron]
        for antes, drop in _drops(secs):
            largo = min(compases * 4, drop["inicio"] - (antes["inicio"] if antes else 0.0))
            if largo >= 4:
                clips.append((drop["inicio"] - largo, largo, _roll(drop["inicio"], largo / 4)))
    elif elemento == "riser":
        for antes, drop in _drops(secs):
            desde = antes["inicio"] if antes else 0.0
            if drop["inicio"] - desde >= 4:
                notas = _riser(patron, tonica, escala, desde, drop["inicio"])
                clips.append((notas[0]["start_beat"], drop["inicio"] - notas[0]["start_beat"], notas))
    elif elemento == "fill":
        for s in secs[1:]:
            if s["inicio"] >= 4:
                clips.append((s["inicio"] - 4, 4.0, _fill(patron, s["inicio"])))
    else:
        notas = _ear(patron, tonica, escala, secs)
        for n in notas:
            inicio = n["start_beat"] - n["start_beat"] % 4
            existente = next((c for c in clips if c[0] == inicio), None)
            if existente:
                existente[2].append(n)
            else:
                clips.append((inicio, 8.0, [n]))
    return [
        {"nombre": f"{patron}", "inicio": float(inicio), "longitud": float(largo), "notas": [{**n, "start_beat": round(n["start_beat"] - inicio, 4)} for n in notas]}
        for inicio, largo, notas in sorted(clips, key=lambda c: c[0])
    ]

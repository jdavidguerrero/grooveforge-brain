"""Reparto de los materiales por los bloques de ARRANGE. Diseño: apps/docs/produccion/motor-armado.md."""

from __future__ import annotations

TODO, NADA, PARCIAL = "todo", "nada", "parcial"
TURNO = 32.0  # 8 compases por turno de entrada o salida
MATERIAL_MAX = 64.0  # 16 compases
PISTAS_FIJAS = {"ARRANGE", "ROLL", "ROLL TRIBE", "RISER", "FILL", "EAR CANDY", "FX", "SUBDROP", "GUIA"}

# (INTRO, BUILD, DROP, BREAK, INTER, OUTRO); el turno de entrada y salida va aparte.
_KINDS = ("INTRO", "BUILD", "DROP", "BREAK", "INTER", "OUTRO")
_PRESENCIA = {
    "KICK": (PARCIAL, TODO, TODO, NADA, NADA, PARCIAL),
    "SC GHOST": (PARCIAL, TODO, TODO, NADA, NADA, PARCIAL),
    "PERC": (PARCIAL, TODO, TODO, TODO, NADA, PARCIAL),
    "DRUMS": (PARCIAL, TODO, TODO, NADA, NADA, PARCIAL),
    "TOP": (PARCIAL, TODO, TODO, NADA, NADA, PARCIAL),
    "BASS": (NADA, NADA, TODO, NADA, NADA, PARCIAL),
    "LEAD": (NADA, NADA, TODO, TODO, NADA, NADA),
    "CHORDS": (NADA, TODO, TODO, NADA, NADA, NADA),
    "CHORDS 2": (TODO, NADA, NADA, TODO, NADA, TODO),
    "CHORDS SUS": (TODO, NADA, NADA, TODO, NADA, TODO),
    "PAD": (TODO, NADA, NADA, TODO, NADA, TODO),
    "ATMOS": (TODO, NADA, NADA, TODO, TODO, TODO),
}
_SEGUNDO_DROP = {"AUX LEAD", "STABS", "PLUCKS"}
# Turno de entrada en la intro (1 = entra a los 8 compases) y de salida en el outro (1 = sale primero).
_ENTRA = {"KICK": 1, "SC GHOST": 1, "PERC": 2, "DRUMS": 3, "TOP": 4}
_SALE = {"KICK": 4, "SC GHOST": 4, "PERC": 3, "DRUMS": 2, "TOP": 1}


def _kind(nombre: str) -> str | None:
    return next((k for k in _KINDS if nombre.startswith(k)), None)


def _fila(rol: str) -> tuple[str, ...]:
    """Por nombre exacto; si no, por el prefijo más largo («PAD 2» se comporta como PAD); si no, solo en los drops."""
    if rol in _PRESENCIA:
        return _PRESENCIA[rol]
    previos = [k for k in _PRESENCIA if rol.startswith(k + " ")]
    return _PRESENCIA[max(previos, key=len)] if previos else (NADA, NADA, TODO, NADA, NADA, NADA)


def presencia_inicial(roles: list[str], secs: list[dict]) -> dict[str, list[str]]:
    salida = {}
    for rol in roles:
        fila, drops = [], 0
        for s in secs:
            kind = _kind(s["nombre"])
            if kind == "DROP":
                drops += 1
            if rol in _SEGUNDO_DROP:
                fila.append(TODO if kind == "DROP" and drops >= 2 else NADA)
            elif kind is None:
                fila.append(NADA)
            else:
                fila.append(_fila(rol)[_KINDS.index(kind)])
        salida[rol] = fila
    return salida


def tramo(rol: str, s: dict, valor: str) -> tuple[float, float] | None:
    """Dónde suena el elemento dentro de la sección, o None."""
    a, b = s["inicio"], s["fin"]
    if valor == NADA or b <= a:
        return None
    if valor == TODO:
        return a, b
    kind = _kind(s["nombre"])
    if kind == "INTRO":
        inicio = min(a + TURNO * _ENTRA.get(rol, 2), b - 16.0)  # siempre suena al menos 4 compases
        return (max(a, inicio), b)
    if kind == "OUTRO":
        return a, min(b, max(a + 16.0, a + TURNO * _SALE.get(rol, 2)))
    return a, a + (b - a) / 2


def clips_de_tramo(material: dict, a: float, b: float, nombre: str) -> list[dict]:
    """El material repetido desde `a`; el último clip se corta en `b`."""
    largo = material["largo"]
    clips, t = [], a
    while t < b - 1e-6:
        fin = min(t + largo, b)
        notas = [
            {**n, "duration_beats": round(min(n["duration_beats"], fin - t - n["start_beat"]), 4)}
            for n in material["notas"] if n["start_beat"] < fin - t - 1e-6
        ]
        clips.append({"inicio": t, "longitud": fin - t, "notas": notas, "nombre": nombre})
        t = fin
    return clips


def material_de(clips: list[dict], notas_abs: list[dict]) -> dict | None:
    """Hasta 16 compases desde el primer clip de la pista (normalmente el compás 1), relativos a su inicio."""
    if not clips:
        return None
    desde = min(c["start"] for c in clips)
    del_material = [c for c in clips if c["start"] < desde + MATERIAL_MAX - 1e-6]
    fin = max(c.get("end", c["start"] + c["length"]) for c in del_material)
    largo = min(MATERIAL_MAX, fin - desde)
    notas = [{**n, "start_beat": round(n["start_beat"] - desde, 4)} for n in notas_abs if desde <= n["start_beat"] < desde + largo - 1e-6]
    if not notas:
        return None
    return {"largo": largo, "desde": desde, "notas": sorted(notas, key=lambda n: (n["start_beat"], n["pitch"]))}

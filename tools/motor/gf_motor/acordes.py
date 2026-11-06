"""Acordes apilados desde la escala, con voicings y ritmos. Diseño: apps/docs/produccion/motor-acordes.md."""

from __future__ import annotations

from collections import Counter

from .bajo import _NOTAS

COMPASES = 16
REG_MIN, REG_MAX = 48, 64  # Do3–Mi4: la melodía va por encima y el bajo por debajo.
NOMBRES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
PROGRESIONES = {"I-II-VI-II": (0, 1, 5, 1), "I-II": (0, 1), "I-II-V-I": (0, 1, 4, 0)}
_TRESILLO = (0, 3, 6, 8, 11, 14)
_CONTRATIEMPO = (2, 6, 10, 14)
_CORCHEAS = (0, 2, 4, 6, 8, 10, 12, 14)
_QUIEBRE_ARCO = (0, 1, 2, 3, 2, 1)
_FIJOS = {
    "BLOQUE TRESILLO": ("triada", _TRESILLO),
    "DECIMAS TRESILLO": ("decimas", _TRESILLO),
    "1-5-8 TRESILLO": ("158", _TRESILLO),
    "SEPTIMAS CONTRATIEMPO": ("septima", _CONTRATIEMPO),
    "ARCO": ("arco", _CORCHEAS),
}


def patrones(libres: list[int], clave: list[int]) -> dict[str, tuple[str, tuple[int, ...]]]:
    salida = dict(_FIJOS)
    huecos = [p for p in libres][:4]
    if len(huecos) >= 2:
        salida["SEPTIMAS HUECOS"] = ("septima", tuple(huecos))
    if clave:
        salida["SEPTIMAS CLAVE"] = ("septima", tuple(clave))
    return salida


def clases_acorde(tonica: int, escala: tuple[int, ...], grado: int, septima: bool = False) -> list[int]:
    grados = (grado, grado + 2, grado + 4, grado + 6) if septima else (grado, grado + 2, grado + 4)
    return [(tonica + escala[g % 7]) % 12 for g in grados]


def nombre_acorde(clases: list[int]) -> str:
    raiz = clases[0]
    intervalos = [(c - raiz) % 12 for c in clases[1:]]
    tercera, quinta = intervalos[0], intervalos[1]
    calidad = {(4, 7): "", (3, 7): "m", (3, 6): "dim", (4, 8): "aug", (4, 6): "(b5)", (3, 8): "m(#5)", (2, 7): "sus2", (5, 7): "sus4"}.get((tercera, quinta), "?")
    if len(intervalos) == 3:
        calidad += {10: "7", 11: "maj7" if calidad == "" else "(maj7)", 9: "6"}.get(intervalos[2], "")
    return NOMBRES[raiz] + calidad


def _colocar(clases: list[int], previo: list[int] | None) -> list[int]:
    """Inversión en registro más cercana al acorde anterior; sin anterior, la más centrada."""
    opciones = []
    for i in range(len(clases)):
        orden = clases[i:] + clases[:i]
        for base in range(REG_MIN, REG_MIN + 12):
            if base % 12 != orden[0]:
                continue
            notas, actual = [base], base
            for c in orden[1:]:
                actual += (c - actual) % 12
                notas.append(actual)
            if notas[-1] <= REG_MAX:
                opciones.append(notas)
    if not opciones:  # Una cuatriada puede no caber abierta: se baja la nota más alta una octava.
        base = REG_MIN + (clases[0] - REG_MIN) % 12
        notas = sorted((base + (c - base) % 12) for c in clases)
        opciones = [notas]
    objetivo = previo if previo else [56] * len(clases)
    return min(opciones, key=lambda v: (sum(abs(a - b) for a, b in zip(sorted(v), sorted(objetivo))), v))


def _voicing(tipo: str, tonica: int, escala: tuple[int, ...], grado: int, previo: list[int] | None) -> list[int]:
    if tipo == "septima":
        return _colocar(clases_acorde(tonica, escala, grado, True), previo)
    clases = clases_acorde(tonica, escala, grado)
    if tipo == "triada":
        return _colocar(clases, previo)
    raiz = REG_MIN + (clases[0] - REG_MIN) % 12
    if tipo == "decimas":
        return [raiz, raiz + 12 + (clases[1] - clases[0]) % 12]
    if tipo == "158":
        return [raiz, raiz + (clases[2] - clases[0]) % 12, raiz + 12]
    colocado = _colocar(clases, previo)  # arco
    return [*sorted(colocado), sorted(colocado)[0] + 12]


def clip_acordes(progresion: str, patron: str, patron_def: tuple[str, tuple[int, ...]], raiz: str, escala: tuple[int, ...]) -> dict:
    tipo, pasos = patron_def
    tonica = _NOTAS[raiz]
    grados = PROGRESIONES[progresion]
    voicings, previo = {}, None
    for g in dict.fromkeys(grados):
        previo = _voicing(tipo, tonica, escala, g, previo)
        voicings[g] = previo
    notas = []
    for c in range(COMPASES):
        actual = grados[c % len(grados)]
        siguiente = grados[(c + 1) % len(grados)]
        for i, p in enumerate(pasos):
            hasta = pasos[i + 1] if i + 1 < len(pasos) else 16
            ultimo = i == len(pasos) - 1
            # Cierre de la frase: el último golpe adelanta el acorde que viene.
            v = voicings[siguiente] if c % 4 == 3 and ultimo else voicings[actual]
            duracion = round(max(hasta - p, 1) * 0.9 / 4, 4)
            inicio = (c * 16 + p) / 4
            velocity = 96 if i == 0 else 84
            if tipo == "arco":
                notas.append({"pitch": v[_QUIEBRE_ARCO[i % len(_QUIEBRE_ARCO)]], "start_beat": inicio, "duration_beats": duracion, "velocity": velocity - 10})
            else:
                notas += [{"pitch": n, "start_beat": inicio, "duration_beats": duracion, "velocity": velocity} for n in v]
    return {"nombre": f"{progresion} {patron}", "inicio": 0.0, "longitud": float(COMPASES * 4), "notas": notas}


def choques(melodia: list[dict], progresion: str, septima: bool, raiz: str, escala: tuple[int, ...]) -> dict:
    """Parte de la melodía, por duración, a un semitono de un acorde sin pertenecerle. Información, no un filtro."""
    if not melodia:
        return {"porcentaje": None, "frecuentes": []}
    tonica = _NOTAS[raiz]
    grados = PROGRESIONES[progresion]
    total = chocan = 0.0
    frecuentes: Counter = Counter()
    for n in melodia:
        c = int(n["start_beat"] // 4)
        clases = clases_acorde(tonica, escala, grados[c % len(grados)], septima)
        pc = n["pitch"] % 12
        total += n["duration_beats"]
        if pc not in clases and any((pc - k) % 12 in (1, 11) for k in clases):
            chocan += n["duration_beats"]
            frecuentes[f"{NOMBRES[pc]} sobre {nombre_acorde(clases)}"] += n["duration_beats"]
    return {"porcentaje": round(100 * chocan / total) if total else 0, "frecuentes": [k for k, _ in frecuentes.most_common(2)]}


def nombres_progresion(progresion: str, raiz: str, escala: tuple[int, ...]) -> str:
    return " – ".join(nombre_acorde(clases_acorde(_NOTAS[raiz], escala, g)) for g in PROGRESIONES[progresion])

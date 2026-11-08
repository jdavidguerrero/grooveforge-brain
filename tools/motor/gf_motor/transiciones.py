"""Roll, riser, fill, fills de Tribe, ear candy y FX en los puntos del arreglo. Diseño: apps/docs/produccion/motor-transiciones.md."""

from __future__ import annotations

from .acordes import clases_acorde
from .bajo import _NOTAS
from .drums import SNARE

ELEMENTOS = {
    "roll": ("ROLL", ("ROLL 4", "ROLL 8", "ROLL 2", "ROLL TRESILLO", "ROLL CON SILENCIO", "ROLL FLAM")),
    "riser": ("RISER", ("RISER NOTA", "RISER ESCALERA", "RISER OCTAVAS")),
    "fill": ("FILL", ("ALEGRE QUE LLAMA", "TAMBORA", "CAJA")),
    "ear": ("EAR CANDY", ("IMPACTO", "SUBDROP INTER", "STAB ANTES DEL DROP", "GOTA EN EL BREAK")),
    "tribe": ("ROLL TRIBE", ("LLAMADOR", "GUACHE RAIN")),
    "fx": ("FX", ("CAIDA", "SWELL INVERSO", "STUTTER", "NATURALEZA")),
}
# Teclado de Tribe · Colombian Percussion (GrooveForge_Dataset_v1/kits/tribe_colombia.json, octava 0).
TRIBE = {
    "tambora_open_l": 41, "tambora_open_r": 43, "tambora_cascara_l": 44, "tambora_damped_l": 45,
    "tambora_cascara_r": 46, "tambora_damped_r": 47,  # G#1 y A#1: los únicos sostenidos de Tribe (Juan, 2026-11-08)
    "llamador_bajoneo": 48, "llamador_abierto": 50, "alegre_bajoneo": 52, "alegre_abierto": 53,
    "alegre_quemao": 55, "alegre_latigazo": 57, "guache_rain": 62, "guache_tremolo": 64,
    "caja_fondo_l": 71, "caja_fondo_r": 72, "caja_quemao_l": 74, "caja_quemao_r": 76, "caja_latigazo": 77,
}
FRASE = 32.0  # 8 compases: los fills van al final de cada frase, dentro de las secciones
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


def subidas(secs: list[dict]) -> list[tuple[float, float]]:
    """(desde, llegada) de cada subida a un drop. Si el drop viene después de un INTER (interludio con el subdrop),
    la subida llega al INTER: ahí se corta la tensión, y el INTER lleva al drop."""
    salida = []
    for i, s in enumerate(secs):
        if not s["nombre"].startswith("DROP") or i == 0:
            continue
        j = i - 1
        if secs[j]["nombre"].startswith("INTER") and j > 0:
            j -= 1
        salida.append((secs[j]["inicio"], secs[j]["fin"]))
    return salida


def fines_de_frase(secs: list[dict]) -> list[float]:
    """Final de cada frase de 8 compases dentro de las secciones. Fuera: builds e interludios (ahí van riser, roll y
    subdrop) y las llegadas de las subidas (son del roll)."""
    llegadas = {d["inicio"] for _, d in _drops(secs)} | {fin for _, fin in subidas(secs)}

    salida = []
    for s in secs:
        if s["nombre"].startswith(("BUILD", "INTER")):
            continue
        fin = s["inicio"] + FRASE
        while fin <= s["fin"] + 1e-6:
            if fin not in llegadas:
                salida.append(fin)
            fin += FRASE
    return sorted(set(salida))


def _drops(secs: list[dict]) -> list[tuple[dict | None, dict]]:
    """(sección anterior, drop) para cada DROP; la anterior es la que sube."""
    return [(secs[i - 1] if i else None, s) for i, s in enumerate(secs) if s["nombre"].startswith("DROP")]


def _nota(pitch: int, inicio: float, duracion: float, velocity: int) -> dict:
    return {"pitch": pitch, "start_beat": round(inicio, 4), "duration_beats": round(duracion, 4), "velocity": max(1, min(127, round(velocity)))}


def _roll(llegada: float, compases: float, subdivisiones=_SUBDIVISIONES, pitch: int = SNARE) -> list[dict]:
    """Tramos iguales, uno por subdivisión; la fuerza sube de 50 a 120 a lo largo de todo el roll."""
    largo = compases * 4
    inicio = llegada - largo
    tramo = largo / len(subdivisiones)
    notas = []
    for k, paso in enumerate(subdivisiones):
        t = inicio + k * tramo
        while t < inicio + (k + 1) * tramo - 1e-6:
            notas.append(_nota(pitch(len(notas)) if callable(pitch) else pitch, t, paso * 0.8, 50 + 70 * (t - inicio) / largo))
            t += paso
    return notas


def _roll_patron(patron: str, llegada: float, compases: float) -> list[dict]:
    if patron == "ROLL TRESILLO":
        # 3-3-2 en semicorcheas, luego tresillos de corchea, sextillos y fusas: el tresillo de la base, acelerando.
        primero = [_nota(SNARE, llegada - compases * 4 + s / 4, 0.2, 50 + 4 * i) for i, s in enumerate((0, 3, 6, 8, 11, 14))]
        resto = _roll(llegada, compases * 3 / 4, (1 / 3, 1 / 6, 0.125))
        for n in resto:
            n["velocity"] = max(n["velocity"], 75)
        return primero + resto
    if patron == "ROLL CON SILENCIO":
        return [n for n in _roll(llegada, compases) if n["start_beat"] < llegada - 1 - 1e-6]  # el hueco antes del drop
    notas = _roll(llegada, compases)
    if patron == "ROLL FLAM":
        flams = [_nota(SNARE, n["start_beat"] - 0.0625, 0.05, n["velocity"] * 0.55) for n in notas if n["duration_beats"] >= 0.2 - 1e-6]
        return sorted(notas + flams, key=lambda n: n["start_beat"])
    return notas


def _tribe(patron: str, llegada: float) -> list[dict]:
    """Fills con Tribe: la llamada del tamborero antes del cambio (cumbia y bullerengue: el alegre llama, la tambora remata)."""
    if patron == "LLAMADOR":
        sonido = lambda i: TRIBE["llamador_abierto"] if i % 2 == 0 else TRIBE["llamador_bajoneo"]  # noqa: E731
        return _roll(llegada, 4, (1.0, 0.5, 1 / 3, 0.25), pitch=sonido)
    if patron == "GUACHE RAIN":
        lluvia = [_nota(TRIBE["guache_rain"], llegada - 16 + 4 * k, 3.9, 60 + 10 * k) for k in range(2)]
        tremolo = [_nota(TRIBE["guache_tremolo"], llegada - 8 + s / 4, 0.25, 40 + 70 * s / 31) for s in range(32)]
        return lluvia + tremolo
    compas = llegada - 4
    if patron == "ALEGRE QUE LLAMA":
        frase = ((0, "abierto"), (3, "quemao"), (6, "abierto"), (8, "latigazo"), (10, "quemao"), (11, "quemao"),
                 (12, "abierto"), (13, "abierto"), (14, "latigazo"), (15, "quemao"))
        return [_nota(TRIBE[f"alegre_{s}"], compas + p / 4, 0.25, 70 + 4 * i) for i, (p, s) in enumerate(frase)]
    if patron == "CAJA":
        fondo = [_nota(TRIBE["caja_fondo_l" if s % 2 == 0 else "caja_fondo_r"], compas + s / 4, 0.2, 70 + 2 * s) for s in range(8, 14)]
        return fondo + [_nota(TRIBE["caja_quemao_l"], compas + 14 / 4, 0.2, 100), _nota(TRIBE["caja_latigazo"], compas + 15 / 4, 0.25, 115)]
    # TAMBORA: cáscara en corcheas, luego en semicorcheas, y remate en abiertos
    cascara = [_nota(TRIBE["tambora_cascara_l" if s % 4 == 0 else "tambora_cascara_r"], compas - 4 + s / 4, 0.25, 64 + s) for s in range(0, 16, 2)]
    cascara += [_nota(TRIBE["tambora_cascara_l" if s % 2 == 0 else "tambora_cascara_r"], compas + s / 4, 0.2, 80 + 2 * s) for s in range(8)]
    abiertos = [_nota(TRIBE["tambora_open_l" if k % 2 == 0 else "tambora_open_r"], compas + s / 4, 0.25, 96 + 4 * k) for k, s in enumerate((8, 10, 12, 13, 14, 15))]
    return cascara + abiertos


def _fx(patron: str, tonica: int, llegada: float) -> list[dict]:
    raiz = 57 + (tonica - 57) % 12
    if patron == "CAIDA":
        return [_nota(raiz, llegada, 7.75, 110)]  # el preset cierra el filtro: barre la tensión del build
    if patron == "SWELL INVERSO":
        return [_nota(raiz, llegada - 4, 4.0, 100)]  # ataque de un compás que termina justo en el drop
    alta = raiz + 12  # STUTTER: corcheas, semicorcheas y fusas en el último compás, cada vez más cortas
    notas = [_nota(alta, llegada - 4 + 0.5 * k, 0.25, 96) for k in range(4)]
    notas += [_nota(alta, llegada - 2 + 0.25 * k, 0.12, 100) for k in range(4)]
    return notas + [_nota(alta, llegada - 1 + 0.125 * k, 0.06, 104) for k in range(8)]


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




def _ear(patron: str, tonica: int, escala, secs: list[dict]) -> list[dict]:
    notas = []
    if patron == "IMPACTO":
        for _, drop in _drops(secs):
            notas.append(_nota(33 + (tonica - 33) % 12, drop["inicio"], 7.75, 115))  # La1: el sub drop
    elif patron == "SUBDROP INTER":
        for s in secs:
            if s["nombre"].startswith("INTER"):
                notas.append(_nota(33 + (tonica - 33) % 12, s["inicio"], min(s["fin"] - s["inicio"], 8.0) - 0.25, 118))
    elif patron == "STAB ANTES DEL DROP":
        clases = clases_acorde(tonica, escala, 0)
        raiz = 60 + (clases[0] - 60) % 12
        for _, drop in _drops(secs):
            for p in (raiz, raiz + (clases[1] - clases[0]) % 12, raiz + (clases[2] - clases[0]) % 12):
                notas.append(_nota(p, drop["inicio"] - 4 + 2.5, 0.3, 105))
    else:
        alta = 81 + (tonica - 81) % 12
        for s in secs:
            if s["nombre"].startswith(("BREAK", "INTER")):
                for t in sorted({s["inicio"], s["inicio"] + 32 * ((s["fin"] - s["inicio"]) // 64)}):
                    notas += [_nota(alta, t, 6.0, 80), _nota(alta + 7, t, 6.0, 70)]
    return notas



def clips_transicion(elemento: str, patron: str, secs: list[dict], raiz: str, escala: tuple[int, ...]) -> list[dict]:
    """Un clip por punto del arreglo, o ninguno si el arreglo no tiene dónde ponerlo."""
    tonica = _NOTAS[raiz]
    clips = []
    if elemento == "roll":
        compases = {"ROLL 8": 8, "ROLL 2": 2}.get(patron, 4)
        for desde, llegada in subidas(secs):
            largo = min(compases * 4, llegada - desde)
            if largo >= 4:
                clips.append((llegada - largo, largo, _roll_patron(patron, llegada, largo / 4)))
    elif elemento == "riser":
        for desde, llegada in subidas(secs):
            if llegada - desde >= 4:
                notas = _riser(patron, tonica, escala, desde, llegada)
                clips.append((notas[0]["start_beat"], llegada - notas[0]["start_beat"], notas))
    elif elemento == "fill":
        largo = 8.0 if patron == "TAMBORA" else 4.0
        for fin in fines_de_frase(secs):
            if fin >= largo:
                clips.append((fin - largo, largo, _tribe(patron, fin)))
    elif elemento == "tribe":
        for desde, llegada in subidas(secs):
            if llegada - desde >= 16:
                clips.append((llegada - 16.0, 16.0, _tribe(patron, llegada)))
    elif elemento == "fx" and patron == "NATURALEZA":
        for s in secs:
            if s["nombre"].startswith(("INTRO", "BREAK", "INTER")):
                largo = min(s["fin"] - s["inicio"], 32.0)
                clips.append((s["inicio"], largo, [_nota(45 + (tonica - 45) % 12, s["inicio"], largo - 0.25, 80)]))
    elif elemento == "fx":
        for _, drop in _drops(secs):
            notas = _fx(patron, tonica, drop["inicio"])
            inicio = min(n["start_beat"] for n in notas)
            clips.append((inicio, max(n["start_beat"] + n["duration_beats"] for n in notas) - inicio + 0.25, notas))
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

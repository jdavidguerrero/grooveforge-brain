"""Mezcla de partida por rol: fader, paneo y envíos. Diseño: apps/docs/produccion/motor-mezcla.md."""

from __future__ import annotations

import math

A, B, C, D, E, F = range(6)  # retornos por orden: Office, Plate, Hall, Rumble, Delay, Echo
NO_TOCAR = {"ARRANGE", "BRAIN", "SC GHOST", "GUIA"}
_LEAD = (-9.0, 0.0, {B: -18.0, E: -24.0, F: -28.0})
_PLK = (-14.0, -0.10, {B: -20.0, E: -20.0})
_SUS = (-15.0, 0.0, {C: -20.0})

# rol: (fader dB, paneo -1..1, {retorno: dB})
TABLA = {
    "KICK": (-3.0, 0.0, {D: -18.0}),
    "BASS SUB": (-6.0, 0.0, {}),
    "BASS": (-8.0, 0.0, {}),
    "BASS MID": (-8.0, 0.0, {}),
    "DRUMS": (-10.0, 0.0, {A: -20.0, B: -24.0}),
    "PERC": (-12.0, -0.25, {A: -16.0, C: -28.0, E: -28.0}),
    "TOP": (-16.0, 0.30, {A: -18.0}),
    "LEAD": _LEAD, "LEAD MAIN": _LEAD, "MELODY": _LEAD,
    "LEAD AUX": (-13.0, 0.15, _LEAD[2]), "AUX LEAD": (-13.0, 0.15, _LEAD[2]),
    "CHORDS": _PLK, "CHORDS PLK": _PLK,
    "CHORDS 2": _SUS, "CHORDS SUS": _SUS,
    "STABS": (-14.0, -0.20, {B: -20.0, E: -20.0}),
    "PLUCKS": (-15.0, 0.20, {B: -20.0, E: -20.0}),
    "PAD": (-18.0, 0.0, {C: -16.0}),
    "ATMOS": (-20.0, 0.0, {C: -16.0}),
    "RISER": (-16.0, 0.0, {C: -20.0}),
    "FX": (-16.0, 0.0, {C: -20.0}),
    "EAR CANDY": (-16.0, 0.0, {F: -24.0}),
    "ROLL": (-12.0, 0.0, {A: -20.0}),
    "ROLL TRIBE": (-14.0, -0.20, {A: -20.0}),
    "FILL": (-14.0, 0.20, {A: -20.0}),
    "SUBDROP": (-8.0, 0.0, {}),
}

# Capa 2: volumen de cadena (dB) por nota de los racks del motor.
CADENAS_DRUMS = {47: -4.0, 48: -3.0, 49: -2.0, 52: -10.0, 53: -12.0}  # clave, snare, clap, hat cerrado, hat abierto
CADENAS_ROLL = {48: 0.0, 49: -3.0}


def rol_de(nombre: str) -> str | None:
    """Exacto; si no, el prefijo más largo («PERC CONGA» → PERC, «CHORDS SUS 2» → CHORDS SUS)."""
    nombre = nombre.strip().upper()
    if nombre in NO_TOCAR:
        return None
    if nombre in TABLA:
        return nombre
    base = nombre.removesuffix(" PROPUESTA")
    if base in TABLA:
        return base
    previos = [k for k in TABLA if base.startswith(k + " ")]
    return max(previos, key=len) if previos else None


def _igual(a: float, b: float, tol: float) -> bool:
    if math.isinf(a) or math.isinf(b):
        return math.isinf(a) and math.isinf(b)
    return abs(a - b) <= tol


def plan(pistas: list[dict], retornos: list[str], escrito: dict[str, dict], forzar: bool = False) -> list[dict]:
    """Qué se pondría en cada pista y qué se salta por la regla 6. No escribe nada.
    `forzar`: Juan pidió reiniciar todo a la partida, incluido lo que movió."""
    salida = []
    for p in pistas:
        nombre = p["name"]
        rol = rol_de(nombre)
        if rol is None or nombre.upper() in NO_TOCAR:
            continue
        fader, pan, envios = TABLA[rol]
        previo = escrito.get(nombre)
        propio = previo is not None and previo.get("volume_db") is not None
        esperado_fader = previo["volume_db"] if propio else 0.0  # sin registro propio: el valor por defecto de Live
        esperado_pan = previo["pan"] if propio else 0.0
        mover = forzar or (_igual(p["volume_db"], esperado_fader, 0.3) and _igual(p["pan"], esperado_pan, 0.02))
        sends, saltados = {}, []
        for i, db in envios.items():
            if i >= len(retornos) or i >= len(p["sends"]):
                saltados.append(f"envío {'ABCDEF'[i]} (no hay ese retorno)")
                continue
            esperado = (previo or {}).get("sends", {}).get(str(i), float("-inf"))
            if forzar or _igual(p["sends"][i], esperado, 0.3):
                sends[i] = db
            else:
                saltados.append(f"envío {'ABCDEF'[i]} (lo moviste)")
        salida.append({
            "pista": nombre, "rol": rol, "volume_db": fader if mover else None, "pan": pan if mover else None,
            "sends": sends, "saltado": [] if mover else ["fader y paneo (los moviste)"], "saltados_envios": saltados,
        })
    return salida

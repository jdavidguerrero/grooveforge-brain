"""Cadena de efectos por rol desde la plantilla GF ROL.adg de Juan. Diseño: apps/docs/produccion/motor-mezcla.md §4.

La plantilla es un Audio Effect Rack de Live 12 con EQ Eight → Compressor → Saturator → Delay → Utility en sus
valores por defecto. El motor la clona y solo cambia valores y qué dispositivo va encendido.
"""

from __future__ import annotations

import gzip
import os
import xml.etree.ElementTree as ET
from pathlib import Path

# EQ Eight, orden del menú de tipos de filtro de Live (y del valor Mode del .adg)
LOW_CUT_48, LOW_CUT_12, LOW_SHELF, BELL, NOTCH, HIGH_SHELF, HIGH_CUT_12, HIGH_CUT_48 = range(8)
SOFT_SINE = 1  # Saturator: Analog Clip, Soft Sine, …


def plantilla_por_defecto() -> Path:
    return Path(os.environ.get("GF_ROL_PLANTILLA") or Path.home() / "Music/Ableton/User Library/Presets/Audio Effects/Audio Effect Rack/GF ROL.adg")


def _db_a_lineal(db: float) -> float:
    return 10 ** (db / 20)


_LEVE_SAT = {"drive": 2.0, "mezcla": 0.3}
_HAAS = True
# rol: corte grave (Hz, None = sin corte), corte agudo (Hz), compresor (umbral dB, ratio, ataque ms, release ms) o None,
#      saturador o None, Haas, ancho (1 = 100 %), mono en el grave, todo en mono
_ROLES = {
    "KICK": (30, None, None, _LEVE_SAT, False, 1.0, True, False),
    "BASS SUB": (30, 120, (-10, 2, 10, 100), None, False, 1.0, False, True),
    "BASS": (30, None, (-10, 2, 10, 100), _LEVE_SAT, False, 1.0, True, False),
    "BASS MID": (120, None, (-12, 3, 5, 80), {"drive": 4.0, "mezcla": 0.5}, False, 1.0, True, False),
    "DRUMS": (150, None, (-12, 4, 10, 60), _LEVE_SAT, False, 1.0, False, False),
    "PERC": (150, None, None, None, False, 1.0, False, False),
    "TOP": (400, None, None, None, False, 1.0, False, False),
    "LEAD": (180, None, (-10, 2.5, 10, 80), _LEVE_SAT, False, 1.0, False, False),
    "CHORDS PLK": (200, None, None, _LEVE_SAT, _HAAS, 1.0, False, False),
    "STABS": (200, None, None, _LEVE_SAT, _HAAS, 1.0, False, False),
    "PLUCKS": (200, None, None, _LEVE_SAT, _HAAS, 1.0, False, False),
    "CHORDS SUS": (250, None, None, None, False, 1.3, False, False),
    "PAD": (250, 10000, None, None, False, 1.3, False, False),
    "ATMOS": (250, 10000, None, None, False, 1.3, False, False),
    "FX": (200, None, None, None, False, 1.0, False, False),
    "SUBDROP": (25, None, None, None, False, 1.0, False, True),
    "ROLL": (150, None, None, None, False, 1.0, False, False),
}
_ALIAS = {
    "LEAD MAIN": "LEAD", "LEAD AUX": "LEAD", "AUX LEAD": "LEAD", "MELODY": "LEAD", "CHORDS": "CHORDS PLK",
    "CHORDS 2": "CHORDS SUS", "RISER": "FX", "EAR CANDY": "FX", "ROLL TRIBE": "ROLL", "FILL": "ROLL",
}


def ajustes(rol: str) -> dict | None:
    clave = _ALIAS.get(rol, rol)
    if clave not in _ROLES:
        return None
    grave, agudo, comp, sat, haas, ancho, bass_mono, mono = _ROLES[clave]
    return {"grave": grave, "agudo": agudo, "comp": comp, "sat": sat, "haas": haas, "ancho": ancho, "bass_mono": bass_mono, "mono": mono}


def _poner(dispositivo: ET.Element, nombre: str, valor) -> None:
    control = dispositivo.find(f"{nombre}/Manual")
    if control is None:
        raise ValueError(f"La plantilla no tiene {dispositivo.tag}/{nombre}")
    control.set("Value", str(valor).lower() if isinstance(valor, bool) else f"{valor}")


def crear_rack_rol(rol: str, nombre: str, destino: Path, plantilla: Path | None = None) -> dict:
    a = ajustes(rol)
    if a is None:
        raise ValueError(f"Rol sin cadena: {rol!r}")
    fuente = plantilla or plantilla_por_defecto()
    if not fuente.exists():
        raise ValueError(f"No encuentro la plantilla del rack de rol: {fuente}")
    raiz = ET.fromstring(gzip.decompress(fuente.read_bytes()).decode("utf-8"))
    eq, comp, sat, delay, util = (next(raiz.iter(t)) for t in ("Eq8", "Compressor2", "Saturator", "Delay", "StereoGain"))

    banda1, banda8 = eq.find("Bands.0/ParameterA"), eq.find("Bands.7/ParameterA")
    if a["grave"]:
        _poner(banda1, "IsOn", True)
        _poner(banda1, "Mode", LOW_CUT_48)
        _poner(banda1, "Freq", a["grave"])
    else:
        _poner(banda1, "IsOn", False)
    if a["agudo"]:
        _poner(banda8, "IsOn", True)
        _poner(banda8, "Mode", HIGH_CUT_48)
        _poner(banda8, "Freq", a["agudo"])
    else:
        _poner(banda8, "IsOn", False)

    _poner(comp, "On", a["comp"] is not None)
    if a["comp"]:
        umbral, ratio, ataque, release = a["comp"]
        _poner(comp, "Threshold", round(_db_a_lineal(umbral), 6))
        _poner(comp, "Ratio", ratio)
        _poner(comp, "Attack", ataque)
        _poner(comp, "Release", release)

    _poner(sat, "On", a["sat"] is not None)
    if a["sat"]:
        _poner(sat, "Type", SOFT_SINE)
        _poner(sat, "PreDrive", a["sat"]["drive"])
        _poner(sat, "DryWet", a["sat"]["mezcla"])

    # Haas (motor-recomendaciones.md §4): sin sync, L 1 ms, R 15 ms, sin feedback ni filtro, todo húmedo.
    _poner(delay, "On", a["haas"])
    if a["haas"]:
        for k, v in (("DelayLine_Link", False), ("DelayLine_SyncL", False), ("DelayLine_SyncR", False),
                     ("DelayLine_SimpleDelayTimeL", 1), ("DelayLine_SimpleDelayTimeR", 15), ("Feedback", 0),
                     ("Filter_On", False), ("DryWet", 1)):
            _poner(delay, k, v)

    _poner(util, "StereoWidth", a["ancho"])
    _poner(util, "BassMono", a["bass_mono"])
    _poner(util, "BassMonoFrequency", 120)
    _poner(util, "Mono", a["mono"])

    grupo = next(raiz.iter("AudioEffectGroupDevice"))
    nombre_usuario = grupo.find("UserName")
    if nombre_usuario is not None:
        nombre_usuario.set("Value", nombre)
    destino.parent.mkdir(parents=True, exist_ok=True)
    destino.write_bytes(gzip.compress(('<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(raiz, encoding="unicode")).encode("utf-8")))
    return a

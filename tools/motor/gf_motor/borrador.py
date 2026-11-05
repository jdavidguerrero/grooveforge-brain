"""Plantilla provisional y borradores MIDI. Teoría: apps/docs/theory/motor-primera-entrega.md §7."""

from __future__ import annotations

TIEMPOS_POR_COMPAS = 4
COMPASES_KICK = 16
_NOTA_GUIA = {"pitch": 0, "start_beat": 0.0, "duration_beats": 0.25, "velocity": 1}

# Paleta de la cabeza Studio. Cada tipo de sección tiene su color, para reconocerla al ver el arrangement.
COLORES = {"INTRO": 0x1D9E75, "BUILD": 0xE8B84A, "DROP": 0xFF6A35, "BREAK": 0x534AB7, "OUTRO": 0x5DCAA5}

PLANTILLAS = {
    "organic": (("INTRO", 32), ("BUILD", 16), ("DROP", 48), ("BREAK", 32), ("BUILD", 16), ("DROP", 48), ("OUTRO", 20)),
}


def clips_guia(genero: str) -> list[dict]:
    clips, compas = [], 0
    for nombre, compases in PLANTILLAS[genero]:
        clips.append(
            {
                "nombre": nombre,
                "inicio": float(compas * TIEMPOS_POR_COMPAS),
                "longitud": float(compases * TIEMPOS_POR_COMPAS),
                "color": COLORES[nombre],
                "notas": [dict(_NOTA_GUIA)],
            }
        )
        compas += compases
    return clips


def clip_kick(nota: int) -> dict:
    """Un clip de 16 compases a negras; lo demás lo arma Juan al copiarlo en el arrangement."""
    tiempos = COMPASES_KICK * TIEMPOS_POR_COMPAS
    return {
        "nombre": "KICK",
        "inicio": 0.0,
        "longitud": float(tiempos),
        "notas": [{"pitch": nota, "start_beat": float(t), "duration_beats": 0.25, "velocity": 110} for t in range(tiempos)],
    }

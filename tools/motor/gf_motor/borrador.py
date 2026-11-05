"""Plantilla provisional y borradores MIDI. Teoría: apps/docs/theory/motor-primera-entrega.md §7."""

from __future__ import annotations

TIEMPOS_POR_COMPAS = 4
NOTA_KICK = 60  # Un Simpler toca esta nota sin transponer.
_NOTA_GUIA = {"pitch": 0, "start_beat": 0.0, "duration_beats": 0.25, "velocity": 1}

PLANTILLAS = {
    "organic": (
        ("Intro", 32, True),
        ("Build", 16, True),
        ("Drop", 48, True),
        ("Break", 32, False),
        ("Build", 16, False),
        ("Drop", 48, True),
        ("Outro", 20, True),
    ),
}


def clips_guia(genero: str) -> list[dict]:
    clips, compas = [], 0
    for nombre, compases, _ in PLANTILLAS[genero]:
        clips.append(
            {
                "nombre": nombre,
                "inicio": float(compas * TIEMPOS_POR_COMPAS),
                "longitud": float(compases * TIEMPOS_POR_COMPAS),
                "notas": [dict(_NOTA_GUIA)],
            }
        )
        compas += compases
    return clips


def clips_kick(genero: str) -> list[dict]:
    """Un clip por tramo con kick, a negras."""
    clips, compas = [], 0
    for nombre, compases, con_kick in PLANTILLAS[genero]:
        if con_kick:
            tiempos = compases * TIEMPOS_POR_COMPAS
            clips.append(
                {
                    "nombre": f"KICK {nombre}",
                    "inicio": float(compas * TIEMPOS_POR_COMPAS),
                    "longitud": float(tiempos),
                    "notas": [
                        {"pitch": NOTA_KICK, "start_beat": float(t), "duration_beats": 0.25, "velocity": 110} for t in range(tiempos)
                    ],
                }
            )
        compas += compases
    return clips

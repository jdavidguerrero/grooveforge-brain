"""Perfiles de género: hipótesis para corregir de oído. Teoría: percusion-generos-tonalidad.md §3."""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from .core import SR
from .kit import ROOM, VOCES, Ajuste, base_hz, sintetizar_voz
from .tonalidad import asignar_notas, hz_de

_C, _M, _P = "clasico", "madera", "piel"
FADE_CHOKE_MS = 5.0


@dataclass(frozen=True)
class Perfil:
    bpm: float
    materiales: dict[str, str]
    centro: Ajuste
    sala: float
    por_voz: dict[str, Ajuste] = field(default_factory=dict)


def _kit(**grupos: str) -> dict[str, str]:
    salida = {}
    for voz in VOCES:
        familia = voz.split("_")[0]
        salida[voz] = grupos.get(voz) or grupos[familia]
    return salida


PERFILES = {
    "organic": Perfil(
        122.0,
        _kit(kick=_P, snare=_P, clap=_P, rim=_M, clave=_M, conga=_P, bongo=_P, hat=_M, maraca=_M),
        Ajuste(decay=1.15, capa=1.15, ataque=0.75, brillo=0.85, gordura=1.1),
        sala=1.3,
    ),
    "house": Perfil(
        124.0,
        _kit(kick=_C, snare=_C, clap=_P, rim=_M, clave=_M, conga=_P, bongo=_M, hat=_C, maraca=_C),
        Ajuste(decay=0.85, ataque=1.1, brillo=1.05, gordura=1.3),
        sala=0.7,
        por_voz={"clap": Ajuste(decay=1.1, ataque=1.1, brillo=1.05)},
    ),
    "nu_disco": Perfil(
        122.0,
        _kit(kick=_P, snare=_P, clap=_P, rim=_M, clave=_M, conga=_P, bongo=_P, hat=_C, maraca=_P),
        Ajuste(decay=0.95, ataque=1.0, brillo=1.15, gordura=1.15),
        sala=1.1,
        por_voz={"hat_abierto": Ajuste(decay=1.25, brillo=1.15), "snare": Ajuste(decay=1.15, brillo=1.1)},
    ),
    "indie_tech": Perfil(
        124.0,
        _kit(kick=_M, snare=_M, clap=_M, rim=_M, clave=_M, conga=_M, bongo=_M, hat=_C, maraca=_C),
        Ajuste(decay=0.75, ataque=1.25, brillo=0.7, gordura=0.9),
        sala=0.5,
    ),
}

_CORCHEAS_DEBILES = {2: 0.5, 6: 0.46, 10: 0.5, 14: 0.46}
# Patrones solo para oír cada kit en contexto: el ritmo real viene del paso de percusión.
PATRONES: dict[str, dict[str, dict[int, float]]] = {
    "organic": {
        "kick": {0: 0.92, 4: 0.88, 8: 0.92, 12: 0.88},
        "clap": {4: 0.5, 12: 0.55},
        "hat_cerrado": {i: 0.38 for i in (2, 6, 10)},
        "hat_abierto": {14: 0.42},
        "maraca": {i: (0.5 if i % 4 == 2 else 0.28) for i in range(16)},
        "clave": {0: 0.5, 3: 0.48, 10: 0.52},
        "conga_baja": {5: 0.7, 13: 0.74},
        "conga_media": {3: 0.55, 9: 0.62},
        "conga_alta": {2: 0.72, 10: 0.66, 15: 0.5},
        "bongo_bajo": {7: 0.5},
        "bongo_alto": {11: 0.48},
        "rim": {6: 0.35},
    },
    "house": {
        "kick": {0: 1.0, 4: 0.96, 8: 1.0, 12: 0.96},
        "clap": {4: 0.92, 12: 0.95},
        "hat_cerrado": {i: (0.6 if i % 4 == 0 else 0.36) for i in range(16) if i % 4 != 2},
        "hat_abierto": {2: 0.68, 6: 0.64, 10: 0.68, 14: 0.64},
        "rim": {7: 0.5, 15: 0.46},
        "conga_alta": {3: 0.48, 11: 0.5},
    },
    "nu_disco": {
        "kick": {0: 0.98, 4: 0.94, 8: 0.98, 12: 0.94},
        "clap": {4: 0.78, 12: 0.82},
        "snare": {4: 0.62, 12: 0.66, 7: 0.3, 15: 0.34},
        "hat_cerrado": {i: 0.42 for i in (0, 4, 8, 12)},
        "hat_abierto": {2: 0.78, 6: 0.74, 10: 0.78, 14: 0.74},
        "conga_baja": {7: 0.54, 13: 0.58},
        "conga_alta": {3: 0.6, 11: 0.62},
        "bongo_alto": {5: 0.44},
        "maraca": {i: 0.28 for i in range(1, 16, 2)},
    },
    "indie_tech": {
        "kick": {0: 0.96, 4: 0.92, 8: 0.96, 12: 0.92},
        "clap": {12: 0.72},
        "snare": {3: 0.36, 11: 0.4},
        "hat_cerrado": {i: 0.44 for i in (2, 3, 6, 10, 11)},
        "hat_abierto": {14: 0.55},
        "rim": {5: 0.6, 13: 0.55},
        "clave": {7: 0.46},
        "conga_media": {9: 0.5},
        "bongo_alto": {1: 0.32},
    },
}
NIVELES = {
    "kick": 0.9, "clap": 0.42, "snare": 0.3, "hat_cerrado": 0.2, "hat_abierto": 0.17, "maraca": 0.14, "rim": 0.24,
    "clave": 0.18, "conga_baja": 0.42, "conga_media": 0.36, "conga_alta": 0.34, "bongo_bajo": 0.26, "bongo_alto": 0.26,
}


def notas_de(genero: str, raiz: str, modo: str) -> dict[str, int]:
    perfil = PERFILES[genero]
    bases = {v: base_hz(v, perfil.materiales[v]) for v in VOCES if base_hz(v, perfil.materiales[v]) is not None}
    return asignar_notas(raiz, modo, bases)


def sintetizar_en_genero(
    voz: str,
    genero: str,
    raiz: str,
    modo: str,
    velocity: float = 0.85,
    seed_timbre: int = 1,
    seed_golpe: int = 1,
    variacion: float = 0.0,
) -> tuple[np.ndarray, dict]:
    perfil = PERFILES[genero]
    material = perfil.materiales[voz]
    notas = notas_de(genero, raiz, modo)
    afinar = hz_de(notas[voz]) if voz in notas else None
    centro = perfil.por_voz.get(voz, perfil.centro)
    y, meta = sintetizar_voz(
        voz, material, velocity, seed_timbre, seed_golpe, variacion,
        room=ROOM[material] * perfil.sala, centro=centro, afinar_hz=afinar,
    )
    return y, {**meta, "genero": genero, "raiz": raiz, "modo": modo, "nota_midi": notas.get(voz)}


def aplicar_choke(abierto: np.ndarray, inicio_abierto: list[int], inicios_cerrado: list[int], golpes: list[np.ndarray]) -> None:
    """Suma cada hat abierto hasta el siguiente cerrado, con un fade corto (teoría §4). Escribe en `abierto`."""
    fade = int(FADE_CHOKE_MS * 1e-3 * SR)
    cerrados = np.sort(np.asarray(inicios_cerrado, dtype=int))
    for inicio, golpe in zip(inicio_abierto, golpes):
        y = golpe
        siguiente = cerrados[cerrados > inicio]
        if siguiente.size and siguiente[0] - inicio < len(y):
            corte = int(siguiente[0] - inicio)
            y = y[: corte + fade].copy()
            tramo = len(y) - corte
            y[corte:] *= np.linspace(1.0, 0.0, tramo)
        fin = min(inicio + len(y), len(abierto))
        abierto[inicio:fin] += y[: fin - inicio]

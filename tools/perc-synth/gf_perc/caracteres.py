"""Caracteres de kick: opciones que se oyen distintas. Teoría: percusion-kits-808-organica.md §11."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .core import SR
from .generos import notas_de
from .kit import Ajuste, sintetizar_voz
from .tonalidad import MODOS, clase_de

VARIACION = 0.8


@dataclass(frozen=True)
class Caracter:
    material: str
    centro: Ajuste
    room: float
    modelo: str = "808"


CARACTERES_KICK = {
    "GOLPE MADERA": Caracter("madera", Ajuste(decay=0.5, capa=1.1, ataque=1.7, brillo=1.15, gordura=0.85), 0.08),
    "GOLPE PIEL": Caracter("piel", Ajuste(decay=0.7, capa=0.5, ataque=1.1, brillo=0.8, gordura=0.55), 0.14),
    "ACUSTICO": Caracter("piel", Ajuste(), 0.3, "acustico"),
    "CUERPO": Caracter("piel", Ajuste(decay=1.0, capa=0.7, ataque=0.6, brillo=0.8, gordura=2.1), 0.1),
    "SUB": Caracter("piel", Ajuste(decay=1.9, capa=0.25, ataque=0.25, brillo=0.7, gordura=0.75), 0.05),
}


# La variación está en la cola; la base es común (teoría §12-13). Las secas llevan poca cola y nada de sala.
COLAS_KICK = {
    "MADERA CORTA": ("cola_madera", 1.2, False),
    "MADERA LARGA": ("cola_madera", 2.6, False),
    "PIEL CORTA": ("cola_piel", 1.2, False),
    "PIEL LARGA": ("cola_piel", 2.6, False),
    "MADERA + PIEL": ("cola_mixta", 1.8, False),
    "MADERA SECA": ("cola_madera", 0.6, True),
    "PIEL SECA": ("cola_piel", 0.6, True),
    "MADERA + PIEL SECA": ("cola_mixta", 0.6, True),
    "MADERA CORTA LIGERA": ("cola_madera", 1.2, False),
    "PIEL CORTA LIGERA": ("cola_piel", 1.2, False),
    "MADERA SECA LIGERA": ("cola_madera", 0.6, True),
    "PIEL SECA LIGERA": ("cola_piel", 0.6, True),
}
CUERPO_LIGERO = 0.4


# Las dos referencias de Juan terminan entre 35 y 40 Hz (C#1 = 34,6 Hz; G#1 = 51,9 Hz).
_GRAVE_MIN, _GRAVE_MAX = 25, 32


def nota_grave(raiz: str, modo: str) -> int:
    """Tónica o quinta dentro del registro grave. Todas las escalas de MODOS tienen quinta justa."""
    if modo not in MODOS:
        raise ValueError(f"modo desconocido: {modo!r}")
    clase = clase_de(raiz)
    for c in (clase, (clase + 7) % 12):
        for nota in range(_GRAVE_MIN, _GRAVE_MAX + 1):
            if nota % 12 == c:
                return nota
    raise ValueError(f"sin nota grave para {raiz} {modo}")


def sintetizar_cola(cola: str, genero: str, raiz: str, modo: str, seed_timbre: int = 1, velocity: float = 0.85, grave: bool = True) -> tuple[np.ndarray, dict]:
    modelo, decay, seca = COLAS_KICK[cola]
    nota = nota_grave(raiz, modo) if grave else notas_de(genero, raiz, modo)["kick"]
    y, meta = sintetizar_voz(
        "kick", "piel", velocity, seed_timbre, 1, 0.0, room=0.0 if seca else 0.08, centro=Ajuste(decay=decay, capa=2.2 if seca else 1.0, gordura=CUERPO_LIGERO if "LIGERA" in cola else 1.0), afinar_hz=440.0 * 2 ** ((nota - 69) / 12), modelo=modelo
    )
    return y, {**meta, "cola": cola, "seca": seca, "genero": genero, "raiz": raiz, "modo": modo, "nota_midi": nota}


def sintetizar_caracter(caracter: str, genero: str, raiz: str, modo: str, seed_timbre: int = 1, velocity: float = 0.85, variacion: float = VARIACION) -> tuple[np.ndarray, dict]:
    c = CARACTERES_KICK[caracter]
    nota = notas_de(genero, raiz, modo)["kick"]
    y, meta = sintetizar_voz(
        "kick", c.material, velocity, seed_timbre, 1, variacion, room=c.room, centro=c.centro, afinar_hz=440.0 * 2 ** ((nota - 69) / 12), modelo=c.modelo
    )
    return y, {**meta, "caracter": caracter, "genero": genero, "raiz": raiz, "modo": modo, "nota_midi": nota}


def rasgos_kick(y: np.ndarray) -> dict[str, float]:
    """Lo que se oye de un kick. Los dB son relativos a la energía total."""
    espectro = np.abs(np.fft.rfft(y, n=SR)) ** 2
    total = max(float(espectro.sum()), 1e-12)

    def banda(a: int, b: int) -> float:
        return 10.0 * np.log10(float(espectro[a:b].sum()) / total + 1e-9)

    ventana = int(0.01 * SR)
    envolvente = np.sqrt(np.convolve(y**2, np.ones(ventana) / ventana, "same"))
    pico = int(envolvente.argmax())
    debajo = envolvente[pico:] < envolvente[pico] * 0.1
    cola_ms = 1000.0 * (int(debajo.argmax()) if debajo.any() else len(envolvente) - pico) / SR
    rms = float(np.sqrt(np.mean(y[: int(0.15 * SR)] ** 2)))
    return {
        "sub_db": banda(20, 90),
        "cuerpo_db": banda(90, 300),
        "medios_db": banda(300, 2000),
        "cola_ms": cola_ms,
        "pegada_db": 20.0 * np.log10(float(np.abs(y).max()) / max(rms, 1e-9)),
    }


def huella_kick(y: np.ndarray) -> np.ndarray:
    """Unidades comparables: 1 equivale a una diferencia apenas evidente en cada rasgo."""
    r = rasgos_kick(y)
    return np.array(
        [r["sub_db"] / 1.5, r["cuerpo_db"] / 1.5, r["medios_db"] / 4.0, 20.0 * np.log10(max(r["cola_ms"], 1.0)) / 2.0, r["pegada_db"] / 1.5]
    )


def mas_lejanas(candidatas: list[np.ndarray], previas: list[np.ndarray], n: int) -> list[int]:
    """Farthest-first respecto a lo ya presentado, para que pedir otra tanda no repita lo oído."""
    m = np.asarray(candidatas)
    elegidas: list[int] = []
    if previas:
        p = np.asarray(previas)
        distancia = np.min(np.linalg.norm(m[:, None] - p[None], axis=2), axis=1)
    else:
        distancia = None
    while len(elegidas) < min(n, len(m)):
        siguiente = 0 if distancia is None else int(np.argmax(distancia))
        elegidas.append(siguiente)
        nueva = np.linalg.norm(m - m[siguiente], axis=1)
        distancia = nueva if distancia is None else np.minimum(distancia, nueva)
    return elegidas

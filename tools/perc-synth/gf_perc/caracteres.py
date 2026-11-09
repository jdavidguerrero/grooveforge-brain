"""Caracteres de kick: opciones que se oyen distintas. Teoría: percusion-kits-808-organica.md §11."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .core import SR
from .generos import notas_de
from .kit import Ajuste, base_hz, sintetizar_voz
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


@dataclass(frozen=True)
class Cola:
    modelo: str
    decay: float
    seca: bool
    semilla: int
    cuerpo: float = 1.0


# §16-17. Familia CUERPO: recetas de los tres que eligió Juan. Familia INICIAL: la base de §13. Todas en la tónica.
# La madera con la base de §14 sonó rara, así que aquí no se combina.
COLAS_KICK = {
    "CUERPO MIX SECA": Cola("cola_mixta", 0.6, True, 1000),
    "CUERPO PIEL CORTA": Cola("cola_piel", 1.2, False, 2000),
    "CUERPO PIEL LIGERA": Cola("cola_piel", 0.6, True, 2000, 0.4),
    "INICIAL MADERA CORTA": Cola("inicial_madera", 1.2, False, 1000),
    "INICIAL PIEL CORTA": Cola("inicial_piel", 1.2, False, 1000),
    "INICIAL MIX": Cola("inicial_mixta", 1.8, False, 1000),
    "INICIAL MADERA SECA": Cola("inicial_madera", 0.6, True, 1000),
    "INICIAL PIEL SECA": Cola("inicial_piel", 0.6, True, 1000),
}


# Regla de Juan: todo lo generado va a la fundamental. C#1–C2 (34,6–65,4 Hz) contiene una vez cada tónica.
_GRAVE_MIN, _GRAVE_MAX = 25, 36


def nota_grave(raiz: str, modo: str) -> int:
    """La tónica en el registro de kick. Si cae dos veces en el rango, la más grave."""
    if modo not in MODOS:
        raise ValueError(f"modo desconocido: {modo!r}")
    clase = clase_de(raiz)
    return next(n for n in range(_GRAVE_MIN, _GRAVE_MAX + 1) if n % 12 == clase)


def sintetizar_cola(cola: str, genero: str, raiz: str, modo: str, tanda: int = 1, velocity: float = 0.85) -> tuple[np.ndarray, dict]:
    """La tanda 1 de CUERPO reproduce los kicks elegidos; las siguientes cambian el golpe y conservan la receta."""
    c = COLAS_KICK[cola]
    semilla = c.semilla + (tanda - 1) * 1000
    nota = nota_grave(raiz, modo)
    y, meta = sintetizar_voz(
        "kick", "piel", velocity, semilla, 1, 0.0, room=0.0 if c.seca else 0.08,
        centro=Ajuste(decay=c.decay, capa=2.2 if c.seca else 1.0, gordura=c.cuerpo), afinar_hz=440.0 * 2 ** ((nota - 69) / 12), modelo=c.modelo,
    )
    return y, {**meta, "cola": cola, "seca": c.seca, "genero": genero, "raiz": raiz, "modo": modo, "nota_midi": nota}


# §Motor de drums: `apps/docs/produccion/motor-drums.md`.
MATERIALES_DRUMS = {"MADERA": "madera", "PIEL": "piel", "808": "clasico"}
GRUPOS_DRUMS = {"CLAVE": ("clave",), "SNARE": ("snare",), "CLAP": ("clap",), "HATS": ("hat_cerrado", "hat_abierto")}
_VARIACION_DRUMS = 0.6


# Resonancia principal de las voces sin nota clara (kit.py), que se mueve a la fundamental más cercana.
_RESONANCIA_HZ = {
    ("clap", "madera"): 780.0, ("clap", "piel"): 1250.0, ("clap", "clasico"): 1050.0,
    ("hat_cerrado", "madera"): 6100.0, ("hat_cerrado", "piel"): 5200.0, ("hat_cerrado", "clasico"): 7200.0,
    ("hat_abierto", "madera"): 6100.0, ("hat_abierto", "piel"): 5200.0, ("hat_abierto", "clasico"): 7200.0,
}


def _tonica_cercana(hz: float, raiz: str) -> int:
    clase = clase_de(raiz)
    return min((n for n in range(12, 128) if n % 12 == clase), key=lambda n: abs(69 + 12 * np.log2(hz / 440.0) - n))


def _armonico_cercano(hz: float, raiz: str) -> float:
    """Tónica o su tercer armónico: con solo octavas, una resonancia se movería hasta un 41 % y cambiaría de carácter."""
    tonicas = [_hz(n) for n in range(12, 128) if n % 12 == clase_de(raiz)]
    return min((f * k for f in tonicas for k in (1.0, 1.5)), key=lambda f: abs(np.log2(f / hz)))


def _hz(nota: int) -> float:
    return 440.0 * 2 ** ((nota - 69) / 12)


def sintetizar_drum(voz: str, material: str, genero: str, raiz: str, modo: str, tanda: int = 1, velocity: float = 0.85) -> tuple[np.ndarray, dict]:
    """Todas las voces van a la fundamental. La tanda 1 es la versión base; las siguientes varían con la semilla."""
    if modo not in MODOS:
        raise ValueError(f"modo desconocido: {modo!r}")
    mat = MATERIALES_DRUMS[material]
    base = base_hz(voz, mat)
    if base:
        nota = _tonica_cercana(base, raiz)
        afinar, factor, destino = _hz(nota), None, _hz(nota)
    else:
        destino = _armonico_cercano(_RESONANCIA_HZ[(voz, mat)], raiz)
        nota, afinar, factor = None, None, destino / _RESONANCIA_HZ[(voz, mat)]
    y, meta = sintetizar_voz(voz, mat, velocity, tanda * 1000, 1, _VARIACION_DRUMS if tanda > 1 else 0.0, afinar_hz=afinar, factor_afinacion=factor)
    return y, {**meta, "genero": genero, "raiz": raiz, "modo": modo, "nota_midi": nota, "afinado": "nota" if base else "resonancia", "resonancia_hz": round(destino, 1)}


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

"""Recetas de madera, piel y semillas.

Los valores son puntos de partida con criterio físico, no mediciones de instrumentos reales (teoría §10).
"""

from __future__ import annotations

from dataclasses import asdict, dataclass

import numpy as np
from scipy.signal import butter, iirpeak, lfilter
from scipy.special import jv

from .core import (
    SR,
    banco_modal,
    golpear,
    ruido_contacto,
    rng_para,
    sala,
    tasa_decay,
    terminar,
    tiempo,
)

# (m, n, j_mn): ceros de Bessel de una membrana circular ideal.
MEMBRANA = [
    (0, 1, 2.4048), (1, 1, 3.8317), (2, 1, 5.1356), (0, 2, 5.5201),
    (3, 1, 6.3802), (1, 2, 7.0156), (4, 1, 7.5883), (2, 2, 8.4172),
    (0, 3, 8.6537), (5, 1, 8.7715), (3, 2, 9.7610), (6, 1, 9.9361),
]
J11 = 3.8317
BARRA_LIBRE = np.array([1.0, 2.756, 5.404, 8.933])
ROOM_DEFAULT = 0.18


@dataclass(frozen=True)
class Madera:
    f0: float
    s0: float
    s1: float
    s2: float
    contacto_ms: float
    tubo_hz: float = 0.0
    tubo_amp: float = 0.0


@dataclass(frozen=True)
class Piel:
    f11: float
    compresion: float
    s0: float
    s1: float
    s2: float
    aire_hz: float
    aire_amp: float
    casco_hz: float
    casco_amp: float
    contacto_escala: float = 1.0


@dataclass(frozen=True)
class Semillas:
    resonancias: tuple[tuple[float, float, float], ...]  # (Hz, Q, ganancia)
    particulas: float
    tau_choque: float
    tope_hz: float


@dataclass(frozen=True)
class Articulacion:
    r: float
    contacto_ms: float
    amortigua: float
    amortigua_graves: float
    ruido: float
    aire: float
    glide: float


ARTICULACIONES = {
    "bajo": Articulacion(0.10, 8.0, 1.0, 1.0, 0.03, 1.0, 0.035),
    "abierto": Articulacion(0.75, 3.5, 1.0, 1.0, 0.07, 0.35, 0.025),
    "slap": Articulacion(0.92, 0.9, 1.5, 3.2, 0.35, 0.10, 0.015),
    "apagado": Articulacion(0.70, 3.0, 4.5, 4.5, 0.10, 0.15, 0.010),
}
GESTOS = {
    "corto": ((0.0, 1.0, 0.030),),
    "largo": ((0.0, 0.8, 0.060), (0.075, 1.0, 0.070)),
}
INSTRUMENTOS: dict[str, Madera | Piel | Semillas] = {
    "clave": Madera(f0=2450, s0=22, s1=8, s2=4.0, contacto_ms=0.5),
    "guadua": Madera(f0=790, s0=30, s1=35, s2=8.0, contacto_ms=0.9, tubo_hz=395, tubo_amp=0.65),
    "llamador": Piel(f11=270, compresion=0.92, s0=11, s1=26, s2=20, aire_hz=118, aire_amp=0.9, casco_hz=720, casco_amp=0.06),
    "alegre": Piel(f11=345, compresion=0.94, s0=8, s1=20, s2=14, aire_hz=142, aire_amp=0.7, casco_hz=840, casco_amp=0.05),
    "bombo": Piel(f11=115, compresion=0.90, s0=8, s1=42, s2=35, aire_hz=62, aire_amp=1.3, casco_hz=310, casco_amp=0.10, contacto_escala=1.6),
    "maracon": Semillas(((2600, 1.2, 1.0), (5200, 1.4, 0.55), (850, 2.0, 0.18)), particulas=850, tau_choque=0.0022, tope_hz=7000),
    "guache": Semillas(((4300, 3.2, 1.0), (7300, 3.5, 0.75), (10400, 3.0, 0.45)), particulas=1400, tau_choque=0.0012, tope_hz=14000),
}


def _variar(rng: np.random.Generator, valor: float, escala: float, extra: float) -> float:
    return float(valor * np.exp(rng.normal(0.0, escala * extra)))


def _madera(p: Madera, vel: float, timbre, golpe, var: float) -> tuple[np.ndarray, dict]:
    f0 = _variar(timbre, p.f0, 0.10, var)
    material = _variar(timbre, 1.0, 0.25, var)
    relaciones = BARRA_LIBRE * np.array([1.0, *golpe.uniform(0.985, 1.015, 3)])
    f = f0 * relaciones * (1.0 + golpe.normal(0.0, 0.002))
    # Parte del sonido viene del segundo palo que recibe el golpe.
    f = np.concatenate([f, [0.87 * f0, 2.31 * f0]])
    amp = np.array([1.0, 0.34, 0.11, 0.035, 0.22, 0.05]) * golpe.uniform(0.85, 1.15, 6)
    sigma = tasa_decay(f, p.s0, p.s1, p.s2) * material
    dur = 0.5
    y = banco_modal(f, amp, sigma, dur)
    if p.tubo_hz:
        tubo = _variar(timbre, p.tubo_hz, 0.08, var)
        y += banco_modal(np.array([tubo]), np.array([p.tubo_amp]), np.array([40.0]), dur)
    contacto = p.contacto_ms * (1.35 - 0.6 * vel) * golpe.uniform(0.85, 1.15)
    y = golpear(y, contacto)
    ruido = ruido_contacto(golpe, dur, 0.0018, min(f0 * 2.2, 9000.0), 1.0)
    # Un golpe más fuerte produce más ruido de contacto; normalizar antes conserva esa relación con la velocity.
    y = y / max(np.max(np.abs(y)), 1e-9) + (0.04 + 0.28 * vel) * ruido / max(np.max(np.abs(ruido)), 1e-9)
    return y, {"f0_hz": round(f0, 2), "material": round(material, 3), "contacto_ms": round(contacto, 3)}


def _piel(p: Piel, art: str, vel: float, timbre, golpe, var: float) -> tuple[np.ndarray, dict]:
    a = ARTICULACIONES[art]
    f11 = _variar(timbre, p.f11, 0.09, var)
    compresion = float(np.clip(_variar(timbre, p.compresion, 0.03, var), 0.8, 1.0))
    material = _variar(timbre, 1.0, 0.22, var)
    r = float(np.clip(a.r + golpe.normal(0.0, 0.025), 0.0, 0.98))

    f, amp, sigma = [], [], []
    for m, _n, j in MEMBRANA:
        fk = f11 * (j / J11) ** compresion
        ak = abs(float(jv(m, j * r)))
        sk = float(tasa_decay(fk, p.s0, p.s1, p.s2)) * material * a.amortigua
        if m == 0:
            sk *= 2.2  # Modos m = 0: irradian mucho y decaen antes (teoría §4).
        if fk < 1.7 * f11:
            sk *= a.amortigua_graves
        if m == 0:
            f.append(fk), amp.append(ak), sigma.append(sk)
            continue
        # Tensión no uniforme: cada modo se parte en dos y produce un batido (teoría §3).
        sep = golpe.uniform(0.002, 0.008)
        f += [fk * (1.0 - sep), fk * (1.0 + sep)]
        amp += [ak * 0.72, ak * 0.72 * golpe.uniform(0.75, 1.0)]
        sigma += [sk, sk * golpe.uniform(0.9, 1.15)]

    f, amp, sigma = np.array(f), np.array(amp), np.array(sigma)
    glide = a.glide * (0.4 + 1.2 * vel) * golpe.uniform(0.8, 1.2)
    dur = 2.0 if p.f11 < 200 else 1.2
    y = banco_modal(f, amp, sigma, dur, glide=glide)

    contacto = a.contacto_ms * p.contacto_escala * (1.5 - 0.8 * vel) * golpe.uniform(0.85, 1.15)
    y = golpear(y, contacto)
    y = y / max(np.max(np.abs(y)), 1e-9)

    aire_f = _variar(timbre, p.aire_hz, 0.07, var)
    aire = banco_modal(np.array([aire_f]), np.array([1.0]), np.array([34.0 * a.amortigua]), dur, glide=glide * 0.5)
    casco = banco_modal(np.array([p.casco_hz]), np.array([1.0]), np.array([95.0]), dur)
    piel = ruido_contacto(golpe, dur, 0.0035 if art != "slap" else 0.007, 2600.0 if art != "slap" else 3600.0, 0.8)
    piel = piel / max(np.max(np.abs(piel)), 1e-9)

    y = y + golpear(aire, max(contacto, 2.0)) * p.aire_amp * a.aire + golpear(casco, contacto) * p.casco_amp + piel * a.ruido * (0.35 + 1.1 * vel)
    return y, {
        "f11_hz": round(f11, 2),
        "compresion": round(compresion, 3),
        "material": round(material, 3),
        "punto_r": round(r, 3),
        "contacto_ms": round(contacto, 3),
        "glide": round(glide, 4),
    }


def _semillas(p: Semillas, gesto: str, vel: float, timbre, golpe, var: float) -> tuple[np.ndarray, dict]:
    dur = 0.45
    t = tiempo(dur)
    energia = np.zeros_like(t)
    for inicio, nivel, tau in GESTOS[gesto]:
        inicio_j = inicio * golpe.uniform(0.92, 1.08)
        activo = t >= inicio_j
        energia[activo] = np.maximum(
            energia[activo], nivel * golpe.uniform(0.85, 1.15) * np.exp(-(t[activo] - inicio_j) / tau)
        )
    particulas = _variar(timbre, p.particulas, 0.2, var) * (0.55 + 0.6 * vel)
    choques = golpe.random(t.size) < np.clip(particulas * energia / SR * 10.0, 0.0, 1.0)
    fuerza = np.where(choques, golpe.random(t.size) ** 2 * np.sqrt(energia), 0.0)
    # PhISEM: cada choque suma energía y esta modula ruido (Cook, 1997).
    nivel = lfilter([1.0], [1.0, -np.exp(-1.0 / (p.tau_choque * SR))], fuerza)
    ruido = golpe.normal(size=t.size) * nivel
    escala = _variar(timbre, 1.0, 0.12, var)
    y = np.zeros_like(t)
    for hz, q, g in p.resonancias:
        b, a = iirpeak(min(hz * escala, 0.45 * SR), q, fs=SR)
        y += g * lfilter(b, a, ruido)
    # El recipiente absorbe agudos; sin este tope el ruido blanco suena a siseo electrónico.
    b, a = butter(1, p.tope_hz, fs=SR)
    y = lfilter(b, a, y)
    return y, {"particulas": round(particulas, 1), "escala_recipiente": round(escala, 3), "choques": int(choques.sum())}


def articulaciones_de(instrumento: str) -> tuple[str, ...]:
    receta = INSTRUMENTOS[instrumento]
    if isinstance(receta, Piel):
        return tuple(ARTICULACIONES)
    if isinstance(receta, Semillas):
        return tuple(GESTOS)
    return ("golpe",)


def sintetizar(
    instrumento: str,
    articulacion: str | None = None,
    velocity: float = 0.8,
    seed_timbre: int = 1,
    seed_golpe: int = 1,
    variacion: float = 0.0,
    room: float = ROOM_DEFAULT,
) -> tuple[np.ndarray, dict]:
    """Genera un golpe. `seed_timbre` fija el instrumento y `seed_golpe` la ejecución (teoría §8)."""
    receta = INSTRUMENTOS[instrumento]
    articulacion = articulacion or articulaciones_de(instrumento)[0]
    if articulacion not in articulaciones_de(instrumento):
        raise ValueError(f"{instrumento} no admite la articulación {articulacion!r}")
    velocity = float(np.clip(velocity, 0.05, 1.0))
    timbre = rng_para("timbre", instrumento, seed_timbre)
    golpe = rng_para("golpe", instrumento, articulacion, seed_timbre, seed_golpe, velocity)
    vel = float(np.clip(velocity * golpe.uniform(0.96, 1.04), 0.05, 1.0))

    if isinstance(receta, Madera):
        y, medidos = _madera(receta, vel, timbre, golpe, variacion)
    elif isinstance(receta, Piel):
        y, medidos = _piel(receta, articulacion, vel, timbre, golpe, variacion)
    else:
        y, medidos = _semillas(receta, articulacion, vel, timbre, golpe, variacion)

    y = terminar(sala(y, room)) * (0.45 + 0.55 * velocity)
    return y, {
        "instrumento": instrumento,
        "articulacion": articulacion,
        "velocity": velocity,
        "seed_timbre": seed_timbre,
        "seed_golpe": seed_golpe,
        "variacion": variacion,
        "room": room,
        "receta": asdict(receta),
        "golpe": medidos,
    }

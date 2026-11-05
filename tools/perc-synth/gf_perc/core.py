"""Bloques de DSP: modos, contacto, resonadores y espacio."""

from __future__ import annotations

import hashlib

import numpy as np
from scipy.signal import fftconvolve, iirpeak, lfilter

SR = 48_000
T60_FACTOR = 6.91  # ln(1000): decay de 60 dB


def rng_para(*partes: object) -> np.random.Generator:
    """RNG estable entre corridas; `hash()` de Python cambia en cada proceso y rompería el determinismo."""
    digest = hashlib.sha256("|".join(map(str, partes)).encode()).digest()
    return np.random.default_rng(int.from_bytes(digest[:8], "little"))


def tiempo(segundos: float) -> np.ndarray:
    return np.arange(int(segundos * SR)) / SR


def tasa_decay(f: np.ndarray, s0: float, s1: float, s2: float) -> np.ndarray:
    """σ(f) en 1/s, con f en Hz. Teoría §4."""
    khz = np.asarray(f, dtype=float) / 1000.0
    return s0 + s1 * khz + s2 * khz**2


def banco_modal(
    f: np.ndarray,
    amp: np.ndarray,
    sigma: np.ndarray,
    dur: float,
    glide: float = 0.0,
    glide_tau: float = 0.04,
) -> np.ndarray:
    """Suma de senoidales amortiguadas. `glide` sube la afinación al inicio y decae con `glide_tau`."""
    t = tiempo(dur)
    deform = t + glide * glide_tau * (1.0 - np.exp(-t / glide_tau))
    y = np.zeros_like(t)
    for fk, ak, sk in zip(f, amp, sigma):
        # Evita aliasing si la subida de tensión empuja un modo cerca de Nyquist.
        if ak == 0.0 or fk * (1.0 + glide) >= 0.45 * SR:
            continue
        y += ak * np.exp(-sk * t) * np.sin(2.0 * np.pi * fk * deform)
    return y


def pulso_contacto(ms: float) -> np.ndarray:
    """Pulso de fuerza sen²; un contacto más largo quita agudos. Teoría §6."""
    n = max(2, round(ms * 1e-3 * SR))
    w = np.sin(np.pi * (np.arange(n) + 0.5) / n) ** 2
    return w / w.sum()


def golpear(modos: np.ndarray, contacto_ms: float) -> np.ndarray:
    return fftconvolve(modos, pulso_contacto(contacto_ms))[: len(modos)]


def ruido_contacto(
    rng: np.random.Generator, dur: float, tau: float, f_centro: float, q: float
) -> np.ndarray:
    t = tiempo(dur)
    b, a = iirpeak(min(f_centro, 0.45 * SR), q, fs=SR)
    return lfilter(b, a, rng.normal(size=t.size) * np.exp(-t / tau))


def sala(x: np.ndarray, cantidad: float, seed: int = 0) -> np.ndarray:
    """Reflexiones cortas y cola breve. La sala es fija para que todos los golpes compartan espacio. Teoría §9."""
    if cantidad <= 0.0:
        return x
    rng = rng_para("sala", seed)
    ir = np.zeros(int(0.18 * SR))
    tiempos = rng.uniform(0.003, 0.025, 8)
    ganancias = 0.55 * np.exp(-tiempos / 0.018) * rng.choice([-1.0, 1.0], 8)
    ir[(tiempos * SR).astype(int)] += ganancias
    t = np.arange(ir.size) / SR
    cola = rng.normal(size=ir.size) * np.exp(-t / 0.045) * 0.035
    ir += lfilter([0.35], [1.0, -0.65], cola)
    mojado = fftconvolve(x, ir)[: len(x)]
    return x + cantidad * mojado


def terminar(x: np.ndarray, pico: float = 0.8, umbral_db: float = -70.0) -> np.ndarray:
    """Normaliza, recorta el silencio final y evita un clic al terminar."""
    # Restar la media dejaría un escalón constante en la cola, y el recorte de silencio nunca actuaría.
    r = np.exp(-2.0 * np.pi * 20.0 / SR)
    x = lfilter([1.0, -1.0], [1.0, -r], x)
    maximo = np.max(np.abs(x))
    if maximo == 0.0:
        return x
    x = x / maximo
    activos = np.nonzero(np.abs(x) > 10 ** (umbral_db / 20))[0]
    x = x[: min(len(x), int(activos[-1]) + 1 + int(0.005 * SR))]
    fade = min(len(x), int(0.005 * SR))
    x[-fade:] *= np.linspace(1.0, 0.0, fade)
    return x * pico

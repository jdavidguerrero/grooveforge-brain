"""Kits tipo 808 con material. Teoría: apps/docs/theory/percusion-kits-808-organica.md"""

from __future__ import annotations

from dataclasses import asdict, dataclass, replace

import numpy as np
from scipy.signal import butter, iirpeak, lfilter

from .core import SR, banco_modal, golpear, rng_para, ruido_contacto, sala, terminar, tiempo
from .instrumentos import BARRA_LIBRE, MEMBRANA

MATERIALES = ("clasico", "madera", "piel")
# Relaciones respecto al modo (0,1), el más grave de la membrana.
PIEL_RELACIONES = np.array([j / MEMBRANA[0][2] for _m, _n, j in MEMBRANA[:6]])
PIEL_AMPS = np.array([1.0, 0.55, 0.34, 0.2, 0.14, 0.09])
MADERA_AMPS = np.array([1.0, 0.33, 0.12, 0.04])
HATS_808_HZ = (205.3, 304.4, 369.6, 522.7, 540.0, 800.0)
ROOM = {"clasico": 0.0, "madera": 0.12, "piel": 0.14}
_SIGMA = 6.91


@dataclass(frozen=True)
class Ajuste:
    """Parámetros que distinguen una variante; 1.0 es la versión base."""

    afinacion: float = 1.0
    decay: float = 1.0
    capa: float = 1.0
    ataque: float = 1.0
    brillo: float = 1.0
    gordura: float = 1.0


def ajuste_para(g, variacion: float, gordura: float = 1.0) -> Ajuste:
    def v(sd: float) -> float:
        return float(np.exp(g.normal(0.0, sd * variacion)))

    return Ajuste(v(0.07), v(0.22), v(0.3), v(0.35), v(0.25), gordura * v(0.25))


def _normal(x: np.ndarray) -> np.ndarray:
    return x / max(float(np.max(np.abs(x))), 1e-9)


def _banda(x: np.ndarray, hz: float, q: float) -> np.ndarray:
    b, a = iirpeak(min(hz, 0.45 * SR), q, fs=SR)
    return lfilter(b, a, x)


def _paso(x: np.ndarray, hz: float, tipo: str, orden: int = 2) -> np.ndarray:
    b, a = butter(orden, hz, btype=tipo, fs=SR)
    return lfilter(b, a, x)


def _capa_material(f_base: float, t60: float, material: str, vel: float, g, dur: float, omitir_primero: bool) -> np.ndarray:
    """Parciales del material por encima de la fundamental."""
    if material == "piel":
        relaciones, amps, caida = PIEL_RELACIONES, PIEL_AMPS, 0.8
    else:
        relaciones, amps, caida = BARRA_LIBRE, MADERA_AMPS, 1.2
    inicio = 1 if omitir_primero else 0
    f, a, s = [], [], []
    for r, ak in zip(relaciones[inicio:], amps[inicio:]):
        fk = f_base * r * g.uniform(0.992, 1.008)
        t = max(t60 * r**-caida, 0.012)
        sk = _SIGMA / t
        if material == "piel":
            # Cada modo se parte en dos: el batido es parte del carácter de una membrana.
            sep = g.uniform(0.002, 0.008)
            f += [fk * (1 - sep), fk * (1 + sep)]
            a += [ak * 0.72, ak * 0.72 * g.uniform(0.75, 1.0)]
            s += [sk, sk * g.uniform(0.9, 1.15)]
        else:
            f.append(fk), a.append(ak * g.uniform(0.85, 1.15)), s.append(sk)
    return banco_modal(np.array(f), np.array(a), np.array(s), dur)


def _tono(
    f0: float,
    t60: float,
    caida: float,
    material: str,
    vel: float,
    g,
    cantidad: float = 1.0,
    ruido: float = 1.0,
    base: np.ndarray | None = None,
) -> np.ndarray:
    """Voz afinada: fundamental (o `base` ya construida) + capa de material + ataque."""
    dur = min(max(1.6 * t60 + 0.08, 0.12), 2.0)
    glide = caida * (0.65 + 0.5 * vel)
    t60_real = t60 * (0.8 if material == "madera" else 1.0)
    if base is None:
        fundamental = banco_modal(np.array([f0]), np.array([1.0]), np.array([_SIGMA / t60_real]), dur, glide=glide, glide_tau=0.018)
    else:
        fundamental = base
    if material == "clasico":
        clic = ruido_contacto(g, dur, 0.0009, max(f0 * 5, 2200.0), 0.9)
        return fundamental + (0.03 + 0.05 * vel) * _normal(clic)

    # Bajo 150 Hz el material va varias octavas arriba para no ensuciar el grave (teoría §2).
    es_grave = f0 < 150.0
    f_base = f0 * (7.0 if material == "madera" else 3.5) if es_grave else f0
    t60_capa = min(t60 * (0.35 if material == "madera" else 0.6), 0.12 if material == "madera" else 0.35)
    capa = _capa_material(f_base, t60_capa, material, vel, g, dur, omitir_primero=not es_grave)
    contacto = (1.0 if material == "madera" else 3.2) * (1.45 - 0.75 * vel) * g.uniform(0.85, 1.15)
    # El pulso anula la señal cerca de 2/T: un contacto largo borraría la nota de las voces agudas y solo dejaría un golpe sordo.
    contacto = min(contacto, 300.0 / f0)
    capa = golpear(capa, contacto)
    fundamental = golpear(fundamental, contacto)
    peso_capa = (0.32 if es_grave else 0.62) * cantidad
    tau = 0.0016 if material == "madera" else 0.0032
    centro = 1900.0 if material == "madera" else 2800.0
    ataque = ruido_contacto(g, dur, tau, centro, 0.9)
    y = fundamental + peso_capa * _normal(capa) + ruido * (0.04 + 0.16 * vel) * _normal(ataque)
    # Un decay muy corto deja energía residual bajo la nota; en voces agudas suena como un retumbo ajeno.
    return _paso(y, 0.5 * f0, "high") if not es_grave else y


def _kick(material: str, vel: float, g, a: Ajuste) -> np.ndarray:
    """Teoría §9. Con `gordura` = 0 no hay capa de cuerpo ni saturación."""
    gordura = max(a.gordura, 0.0)
    f0 = 52.0 * a.afinacion
    t60 = 0.55 * a.decay * (1.0 + 0.22 * gordura)
    t60_real = t60 * (0.8 if material == "madera" else 1.0)
    dur = min(max(1.6 * t60 + 0.08, 0.12), 2.0)
    glide = 1.5 * (0.65 + 0.5 * vel) * (1.0 + 0.2 * gordura)
    tau = 0.018 + 0.014 * gordura
    sub = banco_modal(np.array([f0]), np.array([1.0]), np.array([_SIGMA / t60_real]), dur, glide=glide, glide_tau=tau)
    if gordura > 0.0:
        cuerpo_f = f0 * g.uniform(2.0, 2.3)
        cuerpo = banco_modal(np.array([cuerpo_f]), np.array([0.8 * gordura]), np.array([_SIGMA / (0.26 * a.decay)]), dur, glide=0.6 * glide, glide_tau=tau)
        drive = 1.0 + 1.9 * gordura
        desvio = 0.14 * min(gordura, 1.5)
        x = _normal(sub + cuerpo)
        sub = _normal(np.tanh(drive * (x + desvio)) - np.tanh(drive * desvio))
    y = _tono(f0, t60, 1.5, material, vel, g, cantidad=0.8 * a.capa, ruido=a.ataque, base=sub)
    if gordura > 0.0:
        # Sin esto el ataque fija el pico: al normalizar, el cuerpo queda bajo y el kick suena delgado aunque tenga grave.
        y = np.tanh((1.0 + 1.1 * gordura) * _normal(y))
    return y


def _kick_acustico(vel: float, g, a: Ajuste) -> np.ndarray:
    """Bombo tocado. Teoría §11: modos reales, mazo de fieltro y casco, sin capa 808 ni saturación."""
    f0 = 52.0 * a.afinacion
    t60 = 0.5 * a.decay
    dur = min(1.6 * t60 + 0.12, 1.2)
    # El golpe estira el parche: la nota sube un instante y cae rápido, mucho menos que el barrido de una 808.
    fundamental = banco_modal(np.array([f0]), np.array([1.0]), np.array([_SIGMA / t60]), dur, glide=0.14 * (0.65 + 0.5 * vel), glide_tau=0.012)
    capa = _capa_material(f0, 0.11 * a.decay, "piel", vel, g, dur, omitir_primero=True)
    casco_f = g.uniform(165.0, 215.0)
    casco = banco_modal(np.array([casco_f, casco_f * 1.48]), np.array([1.0, 0.45]), np.array([_SIGMA / 0.07, _SIGMA / 0.04]), dur)
    # Un contacto más largo anularía los parciales del parche y dejaría un golpe sordo (pulso sen²: nulo cerca de 2/T).
    contacto = (2.6 - 1.0 * vel) * g.uniform(0.9, 1.1)
    mezcla = fundamental + 0.32 * a.capa * _normal(capa) + 0.26 * a.capa * _normal(casco)
    mazo = ruido_contacto(g, dur, 0.0035, 1800.0, 0.7)
    # Se mezcla sobre el cuerpo ya normalizado: si el mazo fijara el pico, el bombo quedaría delgado al normalizar.
    return _normal(golpear(mezcla, contacto)) + min(a.ataque, 1.3) * (0.05 + 0.1 * vel) * _normal(mazo)


def _kick_base(f0: float, g_cabeza, vel: float, cuerpo: float = 1.0) -> np.ndarray:
    """Teoría §14-15: seno casi plano y poco barrido; `cuerpo` fija cuánto nivel sostiene y cuánto dura."""
    # Cada tanda varía aquí: sin esto, pedir otra tanda daría casi el mismo golpe.
    largo = g_cabeza.uniform(0.165, 0.215) * (0.7 + 0.3 * cuerpo)
    t = tiempo(largo)
    barrido = g_cabeza.uniform(14.0, 42.0)
    tau = g_cabeza.uniform(0.035, 0.08)
    f = f0 + barrido * np.exp(-t / tau)
    seno = np.sin(2.0 * np.pi * np.cumsum(f) / SR)
    # Llega al final todavía a la mitad del nivel; se corta con un fundido corto, como en la referencia.
    caida = 1.0 - (1.0 - 0.47 * cuerpo) * np.minimum(t / largo, 1.0)
    salida = np.minimum((largo - t) / 0.008, 1.0).clip(0.0, 1.0)
    clic = g_cabeza.normal(size=t.size) * np.exp(-t / 0.0006) * (0.1 + 0.15 * vel)
    return _normal(seno * caida * salida + clic)


def _kick_cola(tipo: str, vel: float, g_cabeza, g_cola, a: Ajuste) -> np.ndarray:
    """Teoría §12-13. La base depende solo de `g_cabeza`, así las opciones de una tanda comparten el mismo golpe."""
    f0 = 52.0 * a.afinacion
    y = _kick_base(f0, g_cabeza, vel, a.gordura)
    t60 = 0.36 * a.decay
    dur = max(len(y) / SR, 1.6 * t60 + 0.1)
    cola = np.zeros(int(dur * SR))

    def modos(relaciones, amps, caida, escala, peso):
        r = np.asarray(relaciones)
        f = f0 * escala * r * g_cola.uniform(0.992, 1.008, r.size)
        # Decaen más despacio que en la capa normal: aquí los parciales son la cola, no un adorno del ataque.
        s = _SIGMA / np.maximum(t60 * r**-caida, 0.02)
        return peso * _normal(banco_modal(f, np.asarray(amps), s, dur))[: len(cola)]

    if tipo in ("madera", "mixta"):
        # Una octava sobre la nota, con parciales de barra: suena hueco, a caja de madera, sin competir con el sub.
        cola += modos(BARRA_LIBRE[:4], (1.0, 0.8, 0.45, 0.2), 0.6, 2.0, 1.0 if tipo == "mixta" else 1.5)
    if tipo in ("piel", "mixta"):
        # Sobre la nota, con los modos de la membrana: quedan graves y juntos, como un parche que sigue sonando.
        cola += modos(PIEL_RELACIONES[:6], (0.7, 1.0, 0.8, 0.6, 0.45, 0.3), 0.4, 1.0, 1.0 if tipo == "mixta" else 1.5)
    t = tiempo(dur)[: len(cola)]
    # Entra tarde para no ensuciar los primeros ciclos de la base: ahí está el cuerpo, y el inicio no cambia entre opciones.
    entrada = 0.5 - 0.5 * np.cos(np.pi * np.clip((t - 0.05) / 0.07, 0.0, 1.0))
    n = max(len(y), len(cola))
    return _normal(np.pad(y, (0, n - len(y))) + a.capa * np.pad(entrada * cola, (0, n - len(cola))))


def _rafagas_clap(material: str, vel: float, g, a: Ajuste = Ajuste()) -> np.ndarray:
    dur = 0.34
    t = tiempo(dur)
    env = np.zeros_like(t)
    if material == "clasico":
        inicios = [0.0, 0.0105, 0.021]
    else:
        # Dos manos o palos nunca chocan con separación regular.
        inicios = np.cumsum([0.0, *g.uniform(0.007, 0.016, 3)])
    for i, inicio in enumerate(inicios):
        activo = t >= inicio
        nivel = g.uniform(0.75, 1.0) if material != "clasico" else 1.0
        env[activo] = np.maximum(env[activo], nivel * np.exp(-(t[activo] - inicio) / 0.0035))
    ultimo = inicios[-1]
    cola_tau = {"clasico": 0.055, "madera": 0.022, "piel": 0.042}[material] * a.decay
    cola = t >= ultimo
    env[cola] = np.maximum(env[cola], 0.72 * np.exp(-(t[cola] - ultimo) / cola_tau))
    ruido = g.normal(size=t.size) * env
    if material == "clasico":
        return _banda(ruido, 1050.0 * a.afinacion, 1.6)
    if material == "madera":
        f = a.afinacion
        return _banda(ruido, 780.0 * f, 3.4) + 0.75 * _banda(ruido, 1620.0 * f, 3.0) + 0.3 * _banda(ruido, 3300.0 * f, 2.5)
    f = a.afinacion
    return _banda(ruido, 1250.0 * f, 1.4) + 0.5 * _banda(ruido, 2400.0 * f, 1.6) + 0.22 * _banda(ruido, 520.0 * f, 2.5)


def _cuadradas_hat(dur: float, g, jitter: float = 0.0, escala: float = 1.0) -> np.ndarray:
    t = tiempo(dur)
    y = np.zeros_like(t)
    for hz in HATS_808_HZ:
        f = hz * escala * (1 + g.normal(0.0, jitter))
        armonicos = np.arange(1, int(0.45 * SR / f), 2)
        for k in armonicos:
            y += np.sin(2 * np.pi * k * f * t) / k
    return y


def _semillas_env(t60: float, dur: float, g, particulas: float, tau: float) -> np.ndarray:
    t = tiempo(dur)
    energia = np.exp(-_SIGMA * t / t60)
    choques = g.random(t.size) < np.clip(particulas * energia / SR * 10.0, 0.0, 1.0)
    fuerza = np.where(choques, g.random(t.size) ** 2 * np.sqrt(energia), 0.0)
    nivel = lfilter([1.0], [1.0, -np.exp(-1.0 / (tau * SR))], fuerza)
    return g.normal(size=t.size) * nivel


def _hat(abierto: bool, material: str, vel: float, g, a: Ajuste = Ajuste()) -> np.ndarray:
    f = a.afinacion
    t60 = (0.45 if abierto else 0.075) * g.uniform(0.92, 1.08) * a.decay
    dur = t60 * 1.3 + 0.03
    t = tiempo(dur)
    env = np.exp(-_SIGMA * t / t60)
    if material == "clasico":
        y = _banda(_cuadradas_hat(dur, g, escala=f), 7200.0 * f, 1.2)
        return _paso(y, 6500.0 * f, "high") * env
    if material == "madera":
        y = _semillas_env(t60, dur, g, 2200.0, 0.0014)
        y = _banda(y, 6100.0 * f, 1.4) + 0.5 * _banda(y, 9700.0 * f, 1.8)
        tic = banco_modal(np.array([3150.0, 5850.0]) * f, np.array([1.0, 0.45]), np.array([180.0, 320.0]), dur)
        return _normal(_paso(y, 3200.0, "high")) + (0.1 + 0.2 * vel) * _normal(tic)
    y = g.normal(size=t.size) * env
    y = _banda(y, 5200.0 * f, 1.0) + 0.7 * _banda(y, 8600.0 * f, 1.3)
    metal = _paso(_banda(_cuadradas_hat(dur, g, 0.004, f), 7600.0 * f, 1.3), 6000.0 * f, "high") * env
    tic = ruido_contacto(g, dur, 0.002, 3600.0, 1.0)
    return _normal(_paso(y, 2900.0, "high")) + 0.16 * _normal(metal) + (0.1 + 0.18 * vel) * _normal(tic)


def _snare(material: str, vel: float, g, a: Ajuste = Ajuste()) -> np.ndarray:
    f = a.afinacion
    t60_ruido = {"clasico": 0.2, "madera": 0.11, "piel": 0.17}[material] * g.uniform(0.92, 1.08) * a.decay
    cuerpo = _tono(180.0 * f, 0.13 * a.decay, 0.08, material, vel, g, cantidad=a.capa, ruido=0.6 * a.ataque)
    segundo = _tono(330.0 * f, 0.08 * a.decay, 0.04, material, vel, g, cantidad=0.5 * a.capa, ruido=0.2 * a.ataque)
    t = np.arange(max(len(cuerpo), len(segundo))) / SR
    bordon = g.normal(size=t.size) * np.exp(-_SIGMA * t / t60_ruido)
    if material == "clasico":
        bordon = _paso(bordon, 1900.0, "high")
        peso = 0.85
    elif material == "madera":
        bordon = _banda(bordon, 2500.0, 0.9)
        peso = 0.45
    else:
        bordon = _banda(bordon, 3100.0, 0.7) + 0.5 * _banda(bordon, 6200.0, 0.9)
        peso = 0.7
    y = np.zeros_like(t)
    y[: len(cuerpo)] += _normal(cuerpo)
    y[: len(segundo)] += 0.55 * _normal(segundo)
    return y + peso * (0.65 + 0.5 * vel) * _normal(bordon)


def _maraca(material: str, vel: float, g, a: Ajuste = Ajuste()) -> np.ndarray:
    f, d = a.afinacion, a.decay
    if material == "clasico":
        t = tiempo(0.09 * d + 0.01)
        return _paso(g.normal(size=t.size) * np.exp(-_SIGMA * t / (0.045 * d)), 5000.0 * f, "high")
    y = _semillas_env(0.085 * d, 0.12 * d + 0.02, g, 1100.0, 0.0018)
    if material == "madera":
        return _banda(y, 3000.0 * f, 1.2) + 0.5 * _banda(y, 5800.0 * f, 1.5)
    return _banda(y, 2100.0 * f, 1.0) + 0.4 * _banda(y, 4300.0 * f, 1.3)


# f0 (Hz), T60 (s), caída de afinación
_TONALES = {
    "conga_baja": (178.0, 0.34, 0.09),
    "conga_media": (245.0, 0.27, 0.08),
    "conga_alta": (345.0, 0.22, 0.07),
    "bongo_bajo": (410.0, 0.17, 0.05),
    "bongo_alto": (560.0, 0.14, 0.05),
}
VOCES = (
    "kick", "snare", "clap", "rim", "clave", "conga_baja", "conga_media", "conga_alta",
    "bongo_bajo", "bongo_alto", "hat_cerrado", "hat_abierto", "maraca",
)
NOTAS = {
    "conga_baja": 38, "conga_media": 39, "conga_alta": 40, "bongo_alto": 41, "rim": 42, "maraca": 43,
    "clave": 47, "snare": 48, "clap": 49, "kick": 51, "hat_cerrado": 52, "hat_abierto": 53, "bongo_bajo": 54,
}
_M, _P = "madera", "piel"
KITS = {
    "808_referencia": dict.fromkeys(VOCES, "clasico"),
    "808_madera": dict.fromkeys(VOCES, _M),
    "808_piel": dict.fromkeys(VOCES, _P),
    "808_mixto": {
        "kick": _P, "snare": _M, "clap": _P, "rim": _M, "clave": _M, "conga_baja": _P, "conga_media": _P,
        "conga_alta": _P, "bongo_bajo": _P, "bongo_alto": _M, "hat_cerrado": _M, "hat_abierto": _P, "maraca": _M,
    },
}


def base_hz(voz: str, material: str) -> float | None:
    """Registro natural de una voz afinable; None si no se afina."""
    if voz == "kick":
        return 52.0
    if voz == "snare":
        return 180.0
    if voz == "rim":
        return {"clasico": 455.0, "madera": 1050.0, "piel": 860.0}[material]
    if voz == "clave":
        return {"clasico": 2450.0, "madera": 2300.0, "piel": 1500.0}[material]
    return _TONALES[voz][0] if voz in _TONALES else None


def sintetizar_voz(
    voz: str,
    material: str,
    velocity: float = 0.8,
    seed_timbre: int = 1,
    seed_golpe: int = 1,
    variacion: float = 0.0,
    room: float | None = None,
    gordura: float = 1.0,
    centro: Ajuste = Ajuste(),
    afinar_hz: float | None = None,
    modelo: str = "808",
) -> tuple[np.ndarray, dict]:
    """Genera una voz. `gordura` solo afecta al kick; `variacion` = 0 es la versión base.

    `centro` desplaza la voz según un perfil de género. `afinar_hz` fija la nota exacta en voces afinables.
    `modelo` cambia el kick: "acustico" es un bombo tocado y "cola_madera", "cola_piel" o "cola_mixta" dejan el mismo
    golpe inicial para una semilla y cambian lo que viene después. No afecta a las demás voces.
    """
    if voz not in VOCES:
        raise ValueError(f"voz desconocida: {voz!r}")
    if material not in MATERIALES:
        raise ValueError(f"material desconocido: {material!r}")
    velocity = float(np.clip(velocity, 0.05, 1.0))
    # Cerrado y abierto son un solo instrumento: comparten semilla de timbre (teoría de géneros §4).
    familia = "hat" if voz.startswith("hat_") else voz
    timbre = rng_para("kit", familia, material, seed_timbre)
    g = rng_para("kit-golpe", voz, material, seed_timbre, seed_golpe, velocity)
    a = ajuste_para(timbre, variacion, gordura)
    a = Ajuste(*(x * c for x, c in zip(asdict(a).values(), asdict(centro).values())))
    base_voz = base_hz(voz, material)
    if afinar_hz is not None and base_voz is not None:
        a = replace(a, afinacion=afinar_hz / base_voz)
    # En `clasico` la fuerza no varía entre golpes: una caja de ritmos repite su velocity exacta.
    vel = velocity if material == "clasico" else float(np.clip(velocity * g.uniform(0.96, 1.04), 0.05, 1.0))

    if voz == "kick":
        if modelo.startswith("cola_"):
            y = _kick_cola(modelo.removeprefix("cola_"), vel, rng_para("kick-cabeza", seed_timbre), g, a)
        elif modelo == "acustico":
            y = _kick_acustico(vel, g, a)
        else:
            y = _kick(material, vel, g, a)
    elif voz == "snare":
        y = _snare(material, vel, g, a)
    elif voz == "clap":
        y = _rafagas_clap(material, vel, g, a)
    elif voz == "rim":
        y = _normal(_tono(base_hz("rim", material) * a.afinacion, 0.045 * a.decay, 0.015, material, vel, g, cantidad=a.capa, ruido=a.ataque))
        segundo = _normal(_tono(1667.0 * a.afinacion, 0.028 * a.decay, 0.0, material, vel, g, cantidad=0.4 * a.capa, ruido=0.2 * a.ataque))
        n = max(len(y), len(segundo))
        y = np.pad(y, (0, n - len(y))) + 0.55 * np.pad(segundo, (0, n - len(segundo)))
    elif voz == "clave":
        y = _tono(base_hz("clave", material) * a.afinacion, 0.075 * a.decay, 0.0, material, vel, g, cantidad=a.capa, ruido=a.ataque)
    elif voz in _TONALES:
        f0, t60, caida = _TONALES[voz]
        y = _tono(f0 * a.afinacion, t60 * a.decay, caida, material, vel, g, cantidad=a.capa, ruido=a.ataque)
    elif voz in ("hat_cerrado", "hat_abierto"):
        y = _hat(voz == "hat_abierto", material, vel, g, a)
    else:
        y = _maraca(material, vel, g, a)

    if a.brillo != 1.0:
        # Inclinación simple de agudos; se limita para no anular el cuerpo de la voz.
        y = y + float(np.clip(a.brillo - 1.0, -0.6, 0.8)) * _paso(y, 2500.0, "high", 1)

    room_real = ROOM[material] if room is None else room
    y = terminar(sala(y, room_real)) * (0.45 + 0.55 * velocity)
    ajuste = {k: round(v, 4) for k, v in asdict(a).items()}
    if voz != "kick":
        ajuste.pop("gordura")
    return y, {
        "voz": voz,
        "material": material,
        "modelo": modelo,
        "nota": NOTAS[voz],
        "velocity": velocity,
        "seed_timbre": seed_timbre,
        "seed_golpe": seed_golpe,
        "variacion": variacion,
        "room": room_real,
        "afinar_hz": round(afinar_hz, 3) if afinar_hz is not None and base_voz is not None else None,
        "ajuste": ajuste,
    }

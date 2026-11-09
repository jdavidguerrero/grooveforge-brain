"""Reproductor de E2 (apps/docs/consola/04-reproductor-e2.md): samples de la drum machine por el Audio+ del Pi,
con el reloj en el callback de audio, y medición de cortes y carga.

En el Pi:
    uv run --project tools/consola python tools/consola/reproductor.py --minutos 30 --bloque 256 [--carga]
"""

from __future__ import annotations

import argparse
import multiprocessing as mp
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

SR = 48_000
_RAIZ = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(_RAIZ / "tools" / "perc-synth"), str(_RAIZ / "tools" / "motor")]

PASOS_POR_COMPAS = 16
COMPASES = 16

# Groove del set de Juan (organic-id-GF): pasos de semicorchea en un compás.
PATRON = {
    "kick": (0, 4, 8, 12),
    "clave": (0, 3, 6, 10, 12),  # son 3-2
    "clap": (4, 12),
    "hat_cerrado": (2, 6, 10, 14),
}
GANANCIA = {"kick": 0.9, "clave": 0.5, "clap": 0.6, "hat_cerrado": 0.25, "hat_abierto": 0.3}
CHOKE = {"hat_abierto": "hat_cerrado", "hat_cerrado": "hat_abierto"}


def muestras_por_paso(bpm: float) -> float:
    return SR * 60.0 / bpm / 4


def eventos(bpm: float) -> tuple[list[tuple[int, str]], int]:
    """(muestra de inicio, voz) en un ciclo de COMPASES, ordenados, y el largo del ciclo en muestras."""
    paso = muestras_por_paso(bpm)
    salida = []
    for c in range(COMPASES):
        for voz, pasos in PATRON.items():
            for p in pasos:
                if voz == "hat_cerrado" and p == 14 and c % 4 == 3:
                    salida.append((round((c * 16 + p) * paso), "hat_abierto"))  # cierre de frase
                else:
                    salida.append((round((c * 16 + p) * paso), voz))
    return sorted(salida), round(COMPASES * PASOS_POR_COMPAS * paso)


@dataclass
class Voz:
    nombre: str
    datos: np.ndarray
    pos: int = 0


@dataclass
class Secuenciador:
    """Programa y mezcla por bloque. Sin E/S: se prueba en el Mac."""

    sonidos: dict[str, np.ndarray]
    bpm: float = 122.0
    posicion: int = 0
    activas: list[Voz] = field(default_factory=list)

    def __post_init__(self) -> None:
        self.lista, self.ciclo = eventos(self.bpm)
        self._i = 0
        self._vuelta = 0

    def _disparar(self, nombre: str, desfase: int, salida: np.ndarray) -> None:
        cortar = CHOKE.get(nombre)
        if cortar:
            self.activas = [v for v in self.activas if v.nombre != cortar]
        voz = Voz(nombre, self.sonidos[nombre] * GANANCIA[nombre])
        self._sumar(voz, salida, desfase)
        if voz.pos < len(voz.datos):
            self.activas.append(voz)

    @staticmethod
    def _sumar(voz: Voz, salida: np.ndarray, desde: int = 0) -> None:
        n = min(len(salida) - desde, len(voz.datos) - voz.pos)
        if n > 0:
            salida[desde:desde + n] += voz.datos[voz.pos:voz.pos + n]
            voz.pos += n

    def bloque(self, n: int) -> np.ndarray:
        """Mono de n muestras. Las voces que siguen sonando van primero; luego, los eventos que caen en el bloque,
        cada uno en su muestra exacta."""
        salida = np.zeros(n, dtype=np.float32)
        for v in self.activas:
            self._sumar(v, salida)
        self.activas = [v for v in self.activas if v.pos < len(v.datos)]
        inicio, fin = self.posicion, self.posicion + n
        while True:
            t, nombre = self.lista[self._i]
            absoluto = t + self._vuelta * self.ciclo
            if absoluto >= fin:
                break
            self._disparar(nombre, absoluto - inicio, salida)
            self._i += 1
            if self._i == len(self.lista):
                self._i, self._vuelta = 0, self._vuelta + 1
        self.posicion = fin
        np.clip(salida, -1.0, 1.0, out=salida)
        return salida


def sonidos_drum_machine() -> dict[str, np.ndarray]:
    from gf_perc.caracteres import sintetizar_cola, sintetizar_drum

    s = {"kick": sintetizar_cola("CUERPO MIX SECA", "organic", "A", "lidia_dominante")[0]}
    for voz in ("clave", "snare", "clap", "hat_cerrado", "hat_abierto"):
        s[voz] = sintetizar_drum(voz, "MADERA", "organic", "A", "lidia_dominante")[0]
    return {k: np.asarray(v, dtype=np.float32) for k, v in s.items()}


def _carga(parar) -> None:
    """El motor trabajando al lado: genera racks de drums sin pausa, como en un set."""
    from gf_perc.caracteres import GRUPOS_DRUMS, MATERIALES_DRUMS, sintetizar_drum

    while not parar.is_set():
        for voces in GRUPOS_DRUMS.values():
            for v in voces:
                for m in MATERIALES_DRUMS:
                    sintetizar_drum(v, m, "organic", "A", "lidia_dominante")


def tocar(minutos: float, bloque: int, dispositivo: str, con_carga: bool, bpm: float) -> dict:
    import sounddevice as sd

    sec = Secuenciador(sonidos_drum_machine(), bpm)
    duraciones: list[float] = []
    cortes = 0

    def callback(out, frames, _tiempo, status):
        nonlocal cortes
        t0 = time.perf_counter()
        if status.output_underflow:
            cortes += 1
        mono = sec.bloque(frames)
        out[:, 0] = mono
        out[:, 1] = mono
        duraciones.append(time.perf_counter() - t0)

    parar = mp.Event()
    procesos = [mp.Process(target=_carga, args=(parar,), daemon=True) for _ in range(3)] if con_carga else []
    for p in procesos:
        p.start()
    with sd.OutputStream(device=dispositivo, samplerate=SR, blocksize=bloque, channels=2, dtype="float32",
                         latency="low", callback=callback) as stream:
        latencia = stream.latency
        fin = time.time() + minutos * 60
        while time.time() < fin:
            time.sleep(1)
    parar.set()
    for p in procesos:
        p.join(timeout=5)
    d = np.array(duraciones)
    presupuesto = bloque / SR
    return {
        "minutos": minutos, "bloque": bloque, "carga": con_carga, "bpm": bpm, "callbacks": len(d), "cortes": cortes,
        "latencia_salida_ms": round(latencia * 1000, 2),
        "callback_media_us": round(float(d.mean()) * 1e6, 1), "callback_p99_us": round(float(np.percentile(d, 99)) * 1e6, 1),
        "callback_max_us": round(float(d.max()) * 1e6, 1), "carga_max_pct": round(float(d.max()) / presupuesto * 100, 1),
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--minutos", type=float, default=30)
    ap.add_argument("--bloque", type=int, default=256)
    ap.add_argument("--dispositivo", default="hw:0,0")
    ap.add_argument("--bpm", type=float, default=122.0)
    ap.add_argument("--carga", action="store_true", help="3 procesos generando racks de drums sin pausa")
    a = ap.parse_args()
    r = tocar(a.minutos, a.bloque, a.dispositivo, a.carga, a.bpm)
    for k, v in r.items():
        print(f"{k}: {v}")


if __name__ == "__main__":
    main()

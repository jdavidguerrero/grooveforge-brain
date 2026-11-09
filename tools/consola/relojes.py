"""E3: sala + cue con dos relojes durante una hora, con detección automática de cortes.

El Pi manda un seno continuo de 1 kHz a la Teensy por USB (cue) y graba lo que la Teensy devuelve (su mezcla, que
incluye el cue: `@GF RUTA SINTE` y `@GF CUE 1`). Si una muestra se pierde o se repite entre los dos relojes (Pi a
44 100 Hz, Teensy a 44 117,647 Hz), la fase del seno salta: se mide la fase instantánea por segundo y se cuentan los
saltos. Mientras tanto, el reproductor de E2 toca la batería por el Audio+ (sala) en otro proceso.

    uv run --project tools/consola python tools/consola/relojes.py --minutos 60
    uv run --project tools/consola python tools/consola/relojes.py --minutos 60 --dispositivo pipewire --sr 48000
"""

from __future__ import annotations

import argparse
import queue
import threading
import time

import numpy as np
from scipy.signal import hilbert

SR = 44_100
F = 1000.0
NIVEL = 10 ** (-20 / 20)
SALTO_RAD = 0.05  # una muestra perdida a 1 kHz mueve la fase 2π·1000/44100 ≈ 0,14 rad


def saltos_de_fase(x: np.ndarray) -> np.ndarray:
    """Índices donde la fase del seno salta respecto de su avance normal."""
    fase = np.unwrap(np.angle(hilbert(x)))
    d = np.diff(fase)
    borde = 200  # la transformada se deforma en los extremos
    d = d[borde:-borde]
    return np.nonzero(np.abs(d - np.median(d)) > SALTO_RAD)[0] + borde


def correr(minutos: float, dispositivo: str, bloque: int, sr: int = SR) -> dict:
    import sounddevice as sd

    cola: queue.Queue = queue.Queue()
    fase = [0.0]
    flags = {"underflow": 0, "overflow": 0}
    paso = 2 * np.pi * F / sr

    def cb(indata, outdata, frames, _t, status):
        if status.output_underflow:
            flags["underflow"] += 1
        if status.input_overflow:
            flags["overflow"] += 1
        n = np.arange(frames)
        s = (NIVEL * np.sin(fase[0] + paso * n)).astype(np.float32)
        fase[0] = (fase[0] + paso * frames) % (2 * np.pi)
        outdata[:, 0] = s
        outdata[:, 1] = s
        cola.put(indata[:, 0].copy())

    eventos: list[tuple[float, int]] = []
    sin_senal = [0]
    inicio = time.time()

    def analizar():
        buf = np.zeros(0, dtype=np.float32)
        while True:
            parte = cola.get()
            if parte is None:
                return
            buf = np.concatenate([buf, parte])
            if len(buf) >= sr + 400:
                trozo = buf[: sr + 400]
                buf = buf[sr:]  # se solapan 400 muestras para no perder saltos en el borde
                if np.sqrt(np.mean(trozo ** 2)) < NIVEL * 0.2:
                    sin_senal[0] += 1
                    continue
                n = len(saltos_de_fase(trozo))
                if n:
                    eventos.append((round((time.time() - inicio) / 60, 2), n))

    hilo = threading.Thread(target=analizar, daemon=True)
    hilo.start()
    with sd.Stream(device=dispositivo, samplerate=sr, blocksize=bloque, channels=2, dtype="float32", callback=cb):
        fin = time.time() + minutos * 60
        while time.time() < fin:
            time.sleep(1)
    cola.put(None)
    hilo.join(timeout=10)
    return {"minutos": minutos, "segundos_con_saltos": len(eventos), "saltos_totales": sum(n for _, n in eventos),
            "segundos_sin_senal": sin_senal[0], **flags, "primeros_eventos(min, saltos)": eventos[:20]}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--minutos", type=float, default=60)
    ap.add_argument("--dispositivo", default="hw:3,0")
    ap.add_argument("--bloque", type=int, default=256)
    ap.add_argument("--sr", type=int, default=SR, help="48000 al pasar por PipeWire (el grafo corre al reloj del Audio+)")
    a = ap.parse_args()
    for k, v in correr(a.minutos, a.dispositivo, a.bloque, a.sr).items():
        print(f"{k}: {v}")


if __name__ == "__main__":
    main()

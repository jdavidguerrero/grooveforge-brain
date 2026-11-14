"""E3: latencia de ida y vuelta (apps/docs/consola/05-e3-teensy-a.md §4).

El Pi pone un clic en el bloque de salida del Audio+, el clic sale por el cable a la entrada de línea del SGTL5000, la
Teensy lo devuelve por USB y el Pi lo busca en lo que llega. Mide desde que el programa escribe el clic hasta que lo
recibe: es la vuelta completa (buffer de salida + DAC + ADC + USB + buffer de entrada), con el reloj monotónico del Pi.
Antes: la Teensy en `@GF RUTA LINEA`.

    uv run --project tools/consola python tools/consola/latencia.py --clics 30 --bloque 256
"""

from __future__ import annotations

import argparse
import threading
import time

import numpy as np

SR_SALIDA, SR_ENTRADA = 48_000, 44_100
NIVEL = 10 ** (-20 / 20)  # −20 dBFS: no satura la entrada de línea
UMBRAL = 0.02


def medir(clics: int, bloque: int, salida: str, entrada: str) -> list[float]:
    import sounddevice as sd

    pedido = threading.Event()
    escrito: list[float] = []
    llegado: list[float] = []
    armado = [False]

    def cb_salida(out, frames, _t, _status):
        out.fill(0)
        if pedido.is_set():
            pedido.clear()
            out[0:48, :] = NIVEL  # 1 ms de pulso
            escrito.append(time.monotonic())
            armado[0] = True

    def cb_entrada(indata, frames, _t, _status):
        ahora = time.monotonic()
        if not armado[0]:
            return
        mono = np.abs(indata[:, 0])
        sobre = np.nonzero(mono > UMBRAL)[0]
        if len(sobre):
            k = int(sobre[0])
            llegado.append(ahora - (frames - k) / SR_ENTRADA)  # la muestra llegó antes que el callback
            armado[0] = False

    with sd.OutputStream(device=salida, samplerate=SR_SALIDA, blocksize=bloque, channels=2, dtype="float32",
                         latency="low", callback=cb_salida), \
         sd.InputStream(device=entrada, samplerate=SR_ENTRADA, blocksize=bloque, channels=2, dtype="float32",
                        latency="low", callback=cb_entrada):
        time.sleep(1.0)
        for _ in range(clics):
            pedido.set()
            time.sleep(0.25)
    n = min(len(escrito), len(llegado))
    return [(llegado[i] - escrito[i]) * 1000 for i in range(n)]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--clics", type=int, default=30)
    ap.add_argument("--bloque", type=int, default=256)
    ap.add_argument("--salida", default="hw:0,0")
    ap.add_argument("--entrada", default="hw:3,0")
    a = ap.parse_args()
    ms = medir(a.clics, a.bloque, a.salida, a.entrada)
    if not ms:
        print("No llegó ningún clic: revisa el cable y que la Teensy esté en @GF RUTA LINEA")
        return
    v = np.array(ms)
    print(f"clics medidos: {len(v)} de {a.clics} · bloque {a.bloque}")
    print(f"ida y vuelta: mediana {np.median(v):.1f} ms · mín {v.min():.1f} · máx {v.max():.1f} · desvío {v.std():.1f}")


if __name__ == "__main__":
    main()

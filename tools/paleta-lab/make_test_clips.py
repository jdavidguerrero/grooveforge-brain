#!/usr/bin/env python3
"""Clips MIDI de prueba por rol (apps/docs/produccion/protocolo-captura.md §2).

Patrones neutros para medir sonidos siempre igual (como un tono de prueba): no son música ni material de identidad.
Se escriben como MIDI estándar (tipo 0, 480 ticks por negra) en $GF_DATASET/midi/<rol>/.

    python3 tools/paleta-lab/make_test_clips.py            # La menor, 124 BPM
    python3 tools/paleta-lab/make_test_clips.py --key 5    # transpuesto (semitonos desde La)
"""
from __future__ import annotations

import argparse
import os
import struct
from pathlib import Path

TPQ = 480          # ticks por negra
BAR = 4 * TPQ
S16 = TPQ // 4     # semicorchea


def vlq(n: int) -> bytes:
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


def write_mid(path: Path, notes: list[tuple[int, int, int, int]], bpm: float, name: str):
    """notes: (inicio en ticks, duración en ticks, nota, velocity)."""
    events = []
    for start, dur, note, vel in notes:
        events.append((start, 1, bytes([0x90, note, vel])))
        events.append((start + dur, 0, bytes([0x80, note, 0])))   # los NOTE OFF antes que un NOTE ON en el mismo tick
    events.sort(key=lambda e: (e[0], e[1]))
    tempo = int(60_000_000 / bpm)
    track = b"\x00\xff\x03" + vlq(len(name)) + name.encode("ascii")
    track += b"\x00\xff\x51\x03" + tempo.to_bytes(3, "big")
    track += b"\x00\xff\x58\x04\x04\x02\x18\x08"                 # 4/4
    t = 0
    for at, _, data in events:
        track += vlq(at - t) + data
        t = at
    track += b"\x00\xff\x2f\x00"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPQ) + b"MTrk" + struct.pack(">I", len(track)) + track)


def clips(k: int) -> dict[str, tuple[str, list]]:
    A1, A2, A3, A4 = 33 + k, 45 + k, 57 + k, 69 + k   # La en cada octava
    out = {}

    # BASS: 2 compases con notas fuertes (78–84), ghost notes (44–52) y un salto de octava. Mide bounce y velocity.
    pat = [(0, 3, 0, 82), (3, 1, 0, 48), (4, 2, 12, 78), (6, 1, 0, 46), (7, 1, 0, 50),
           (8, 3, 0, 84), (11, 1, 3, 44), (12, 2, 5, 80), (14, 2, 7, 52)]
    bass = [(bar * BAR + s * S16, d * S16 - 10, A1 + 12 + n, v) for bar in range(2) for s, d, n, v in pat]
    out["bass/bass_ghost_notes"] = ("bass ghost notes", bass)

    # CHORDS: i–VI–III–VII sostenidos (1 compás cada uno) y después stabs cortos en corcheas sincopadas
    prog = [(0, [0, 3, 7, 10]), (8, [0, 4, 7, 11]), (3, [0, 4, 7, 11]), (10, [0, 4, 7, 10])]
    chords = [(i * BAR, BAR - 20, A3 + r + iv, 90) for i, (r, ivs) in enumerate(prog) for iv in ivs]
    for i, (r, ivs) in enumerate(prog):
        for s in (2, 6, 10, 14):
            chords += [(4 * BAR + i * BAR + s * S16, S16, A3 + r + iv, 100) for iv in ivs]
    out["chords/chords_sostenido_y_stabs"] = ("chords sostenido y stabs", chords)

    # PLUCK: motivo neutro de 4 notas en semicorcheas, repetido una octava arriba, y una nota larga
    motif = [0, 3, 7, 5]
    pluck = [(b * BAR + s * S16, S16 - 10, A3 + 12 * (b % 2) + motif[s % 4], 96) for b in range(2) for s in range(16)]
    pluck.append((2 * BAR, 2 * BAR - 20, A4, 100))
    out["pluck/pluck_motivo_y_nota_larga"] = ("pluck motivo y nota larga", pluck)

    # PAD: nota sola 4 compases y el acorde i 4 compases
    pad = [(0, 4 * BAR - 20, A3, 90)] + [(4 * BAR, 4 * BAR - 20, A3 + iv, 90) for iv in (0, 3, 7, 10)]
    out["pad/pad_nota_y_acorde"] = ("pad nota y acorde", pad)

    # PERC: un golpe por compás en dos alturas, a velocity 40, 80 y 120
    perc = []
    for i, v in enumerate((40, 80, 120)):
        perc.append((2 * i * BAR, S16, A2, v))
        perc.append((2 * i * BAR + BAR, S16, A3 + 7, v))
    out["perc/perc_dinamica"] = ("perc dinamica", perc)

    # REGISTRO: una nota por negra de La1 a La5 para encontrar el key_range de cada sonido
    reg = [(i * TPQ, TPQ - 20, A1 + 3 * i, 90) for i in range(17)]
    out["fx/registro_la1_la5"] = ("registro La1 La5", reg)
    return out


def main(argv=None):
    p = argparse.ArgumentParser(description="Clips MIDI de prueba por rol")
    p.add_argument("--key", type=int, default=0, help="semitonos desde La menor")
    p.add_argument("--bpm", type=float, default=124)
    p.add_argument("--out", help="carpeta (por defecto $GF_DATASET/midi o el dataset de la paleta)")
    a = p.parse_args(argv)
    base = Path(a.out or Path(os.environ.get("GF_DATASET") or Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1") / "midi")
    for rel, (name, notes) in clips(a.key).items():
        path = base / f"{rel}.mid"
        write_mid(path, notes, a.bpm, name)
        print(path)


if __name__ == "__main__":
    main()

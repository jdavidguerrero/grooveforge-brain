#!/usr/bin/env python3
"""Lab de acordes: 3 progresiones × órdenes de notas × ritmos (apps/docs/produccion/lab-acordes.md).

Los acordes se conducen a la inversión más cercana (las notas comunes se quedan), entre Do3 y Mi4; los patrones que
llevan bajo (1-5-8, décimas) lo ponen entre Re2 y Do#3. El 4.º compás anticipa el acorde de la vuelta siguiente una
semicorchea. Escribe $GF_DATASET/midi/adn/chords/lab/<progresion>__<orden>__<ritmo>.mid.

    python3 tools/paleta-lab/make_chord_lab.py
"""
from __future__ import annotations

import argparse
import itertools
import os
from pathlib import Path

from make_lead_lab import A3, CH, SCALES
from make_test_clips import BAR, S16, write_mid

PROGRESSIONS = ("menor_v_prestado", "kumoi", "lidia")          # lab-acordes.md §1: tensión, luz y vamp, lidia
LOW, HIGH = A3 - 9, A3 + 7                                     # Do3–Mi4: el voicing; el lead vive arriba de Mi4
BASS_LOW, BASS_HIGH = A3 - 19, A3 - 8                          # Re2–Do#3
CENTER = A3 - 1                                                # el primer acorde, alrededor de Sol#3

# Orden de las notas (lab-acordes.md §3, eje 1): "B" = bajo; 1, 3, 5 = voicing de abajo arriba; 8 = la 1 una octava
# arriba; "T" = la décima (la tercera del acorde, una octava arriba del bajo). Una tupla suena junta.
ORDERS = {
    "bloque": [("1", "3", "5")],
    "alberti": ["1", "5", "3", "5"],
    "arpegio": ["1", "3", "5", "8"],
    "arco": ["1", "3", "5", "8", "5", "3"],
    "158": ["B", "5", "8"],
    "decimas": [("B", "T")],
}
# Ritmo (eje 2): (semicorchea, duración, velocity)
RHYTHMS = {
    "corcheas": [(s, 2, 90 if s % 4 == 0 else 74) for s in range(0, 16, 2)],
    "tresillo": [(0, 3, 96), (3, 3, 84), (6, 2, 86), (8, 3, 94), (11, 3, 84), (14, 2, 86)],
    "contratiempo": [(2, 1, 100), (6, 1, 96), (9, 1, 62), (10, 1, 100), (14, 1, 96)],
    "respiro": [(0, 6, 88), (8, 3, 80), (12, 2, 72)],
}
BASS_ORDERS = {"158", "decimas"}


def progression(name: str) -> tuple[str, ...]:
    return next(sc.prog for sc in SCALES if sc.name == name)


def pcs(chord: str) -> list[int]:
    root, ivs = CH[chord]
    return [(root + iv) % 12 for iv in ivs]


def voicings(chord: str, k: int = 0) -> list[tuple[int, ...]]:
    """Todas las tríadas del acorde entre LOW y HIGH, con una nota por clase y a menos de una octava."""
    opts = [[n for n in range(LOW + k, HIGH + k + 1) if (n - A3 - k) % 12 == p] for p in pcs(chord)]
    out = {tuple(sorted(c)) for c in itertools.product(*opts)}
    return sorted(v for v in out if v[-1] - v[0] < 12)


def nearest_voicing(chord: str, prev: tuple[int, ...] | None, k: int = 0) -> tuple[int, ...]:
    def cost(v):
        move = sum(abs(a - b) for a, b in zip(v, prev)) if prev else 0
        return move, abs(sum(v) / 3 - CENTER - k), v
    return min(voicings(chord, k), key=cost)


def voice_lead(prog: tuple[str, ...], k: int = 0) -> list[tuple[int, ...]]:
    """La inversión más cercana a la anterior; dos vueltas para que el cierre también llegue cerca del primero."""
    v = [nearest_voicing(prog[0], None, k)]
    for _ in range(2):
        v = v[:1]
        for chord in prog[1:]:
            v.append(nearest_voicing(chord, v[-1], k))
        v[0] = nearest_voicing(prog[0], v[-1], k)
    return v


def bass(chord: str, k: int = 0) -> int:
    root = CH[chord][0]
    return next(n for n in range(BASS_LOW + k, BASS_HIGH + k + 1) if (n - A3 - k) % 12 == root)


def tones(chord: str, voicing: tuple[int, ...], k: int = 0, octave: bool = False) -> dict[str, int]:
    """Los nombres del §3 para un acorde. Con octave (arpegio, arco): si la 8 pasaría de Mi4, los cuatro pasos arrancan
    con la nota de arriba una octava abajo."""
    b = bass(chord, k)
    third = CH[chord][1][1]
    t = {"1": voicing[0], "3": voicing[1], "5": voicing[2], "B": b, "T": b + 12 + third}
    if voicing[0] + 12 <= HIGH + k:
        t["8"] = voicing[0] + 12
    elif octave:                                                       # 5-abajo, 1, 3, 5: los mismos cuatro pasos
        t.update({"1": voicing[2] - 12, "3": voicing[0], "5": voicing[1], "8": voicing[2]})
    return t


def pattern(prog_name: str, order: str, rhythm: str, k: int = 0) -> list[tuple[int, int, int, int]]:
    prog = progression(prog_name)
    vs = voice_lead(prog, k)
    seq = ORDERS[order]
    out = []
    for bar, chord in enumerate(prog):
        hits = RHYTHMS[rhythm]
        for h, (step, dur, vel) in enumerate(hits):
            c, v = chord, vs[bar]
            if bar == len(prog) - 1 and h == len(hits) - 1:     # A A A B: anticipa la vuelta
                c, v, step, dur = prog[0], vs[0], step - 1, dur + 1
            t = tones(c, v, k, octave="8" in seq)
            item = seq[h % len(seq)]
            for name in item if isinstance(item, tuple) else (item,):
                if order == "158" and name in ("5", "8"):
                    n = t["B"] + (7 if name == "5" else 12)
                else:
                    n = t[name]
                out.append((bar * BAR + step * S16, dur * S16 - 10, n, vel))
    return sorted(out)


def clip_name(prog_name: str, order: str, rhythm: str) -> str:
    return f"{prog_name}__{order}__{rhythm}"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--key", type=int, default=0, help="semitonos desde La")
    ap.add_argument("--bpm", type=float, default=124.0)
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    root = Path(a.dataset) / "midi" / "adn" / "chords" / "lab"
    for p, o, r in itertools.product(PROGRESSIONS, ORDERS, RHYTHMS):
        name = clip_name(p, o, r)
        write_mid(root / f"{name}.mid", pattern(p, o, r, a.key), a.bpm, name)
    print(f"{len(PROGRESSIONS) * len(ORDERS) * len(RHYTHMS)} patrones de acordes en {root}")


if __name__ == "__main__":
    main()

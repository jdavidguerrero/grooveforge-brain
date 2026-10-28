#!/usr/bin/env python3
"""Lab de leads: 12 escalas × 4 rítmicas, cada escala con su progresión (apps/docs/produccion/lab-leads.md).

Todas con La como centro, 124 BPM, 4 compases (un acorde por compás). La rítmica y el contorno son los mismos en todas
las escalas; las alturas salen de la escala del compás, y las notas en la negra o largas se ajustan al tono del acorde
más cercano. Cada escala trae un clip de acordes (para un instrumento de Live: el Brain toca el lead). Escribe en
$GF_DATASET/midi/adn/lead/lab/<familia>/<escala>__<ritmica>__<dialectos>.mid y <escala>__acordes.mid.

    python3 tools/paleta-lab/make_lead_lab.py
    python3 tools/paleta-lab/make_lead_lab.py --key 2     # transpuesto (semitonos desde La)
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

from make_test_clips import BAR, S16, write_mid

A3 = 57
# Clases de altura desde La (0 = La, 1 = Si♭, … 11 = Sol#)
EOLICA = (0, 2, 3, 5, 7, 8, 10)
DORICA = (0, 2, 3, 5, 7, 9, 10)
FRIGIA = (0, 1, 3, 5, 7, 8, 10)
MAYOR = (0, 2, 4, 5, 7, 9, 11)
# Acorde: (fundamental, intervalos)
CH = {"Am": (0, (0, 3, 7)), "A": (0, (0, 4, 7)), "Asus4": (0, (0, 5, 7)), "B": (2, (0, 4, 7)),
      "Bb": (1, (0, 4, 7)), "C": (3, (0, 4, 7)), "D": (5, (0, 4, 7)), "Dm": (5, (0, 3, 7)), "E": (7, (0, 4, 7)),
      "Em": (7, (0, 3, 7)), "F": (8, (0, 4, 7)), "F#m": (9, (0, 3, 7)), "G": (10, (0, 4, 7)), "Gm": (10, (0, 3, 7))}


class Scale:
    """notes: la escala de la melodía; parent: la heptatónica que arma los acordes (en las pentatónicas, su madre);
    borrowed: {compás: (escala de la melodía, escala de los acordes)} para el compás del acorde prestado."""

    def __init__(self, name, family, notes, prog, dialects, parent=None, borrowed=None):
        self.name, self.family, self.notes, self.prog, self.dialects = name, family, notes, prog, dialects
        self.parent = parent or notes
        self.borrowed = borrowed or {}

    def bar_notes(self, bar: int) -> tuple:
        return self.borrowed.get(bar, (self.notes, self.parent))[0]

    def bar_parent(self, bar: int) -> tuple:
        return self.borrowed.get(bar, (self.notes, self.parent))[1]


V_PRESTADO = (0, 2, 3, 5, 7, 8, 11)           # eólica con Sol# (menor armónica): el compás del E
IV_PRESTADO = EOLICA                          # el Dm viene de La menor: su compás usa la menor paralela
SCALES = [
    Scale("eolica", "f1_menores", EOLICA, ("Am", "F", "C", "G"), ("afro", "melodic")),
    Scale("dorica", "f1_menores", DORICA, ("Am", "D", "C", "G"), ("afro", "disco")),
    Scale("menor_v_prestado", "f1_menores", EOLICA, ("Am", "F", "C", "E"), ("melodic",),
          borrowed={3: (V_PRESTADO, V_PRESTADO)}),
    Scale("frigia", "f2_exoticas", FRIGIA, ("Am", "Bb", "Am", "Gm"), ("organic", "indie")),
    Scale("frigia_dominante", "f2_exoticas", (0, 1, 4, 5, 7, 8, 10), ("A", "Bb", "Gm", "A"), ("indie",)),
    Scale("hungara_menor", "f2_exoticas", (0, 2, 3, 6, 7, 8, 11), ("Am", "F", "Am", "E"), ("melodic", "organic")),
    Scale("hirajoshi", "f3_japonesas", (0, 2, 3, 7, 8), ("Am", "F", "Am", "Em"), ("organic",), parent=EOLICA),
    Scale("in_sen", "f3_japonesas", (0, 1, 5, 7, 10), ("Asus4", "Bb", "Asus4", "Gm"), ("indie",), parent=FRIGIA),
    Scale("kumoi", "f3_japonesas", (0, 2, 3, 7, 9), ("Am", "D", "Am", "D"), ("organic",), parent=DORICA),
    Scale("lidia", "f4_mayores", (0, 2, 4, 6, 7, 9, 11), ("A", "B", "F#m", "B"), ("melodic",)),
    Scale("mixolidia", "f4_mayores", (0, 2, 4, 5, 7, 9, 10), ("A", "G", "D", "A"), ("afro", "disco")),
    Scale("mayor_iv_prestado", "f4_mayores", MAYOR, ("A", "D", "Dm", "A"), ("disco",),
          borrowed={2: (IV_PRESTADO, IV_PRESTADO)}),
]

# Rítmicas (lab-leads.md §4): por compás, (semicorchea, duración, velocity, contorno en grados de la escala desde el
# ancla del compás). Compás 4 = la variación que cierra la vuelta (A A A B). mixta_b alterna pregunta / respuesta.
ACC, NRM = 100, 78
RHYTHMS = {
    "corta": ([(0, 1, ACC, 0), (2, 1, NRM, 1), (5, 1, NRM, 2), (7, 1, NRM, 1), (10, 1, ACC, 3), (12, 1, NRM, 0)],
              [(0, 1, ACC, 0), (2, 1, NRM, -1), (5, 1, NRM, -2), (7, 1, NRM, -1), (10, 1, ACC, 1), (12, 1, NRM, -2)]),
    "mixta_a": ([(2, 1, NRM, -1), (3, 1, NRM, 0), (6, 3, ACC, 2), (10, 1, NRM, 1), (12, 1, NRM, 0)],
                [(2, 1, NRM, 1), (3, 1, NRM, 0), (6, 3, ACC, -1), (10, 1, NRM, -2), (12, 1, NRM, -3)]),
    "mixta_b": ([(0, 1, NRM, 0), (2, 1, NRM, 1), (3, 4, ACC, 2)],
                [(6, 1, NRM, 2), (8, 1, NRM, 1), (10, 1, NRM, 0), (11, 4, ACC, -1)],
                [(6, 1, NRM, 1), (8, 1, NRM, 0), (10, 1, NRM, -1), (11, 4, ACC, -3)]),
    "larga": ([(0, 6, ACC, 0), (9, 1, NRM, 1), (11, 1, NRM, 2)],
              [(0, 6, ACC, 0), (9, 1, NRM, -1), (11, 1, NRM, -2)]),
}
CENTER = {"corta": A3 + 7, "mixta_a": A3 + 7, "mixta_b": A3 + 7, "larga": A3 - 3}     # Mi4; Fa#3 en la larga
RANGE = {"corta": (A3, A3 + 19), "mixta_a": (A3, A3 + 19), "mixta_b": (A3, A3 + 19), "larga": (A3 - 9, A3 + 7)}
OCCUPANCY = {"corta": 0.45, "mixta_a": 0.60, "mixta_b": 0.60, "larga": 0.75}   # máximo del tiempo con nota sonando


def rhythm_bar(rhythm: str, bar: int) -> list:
    pats = RHYTHMS[rhythm]
    if rhythm == "mixta_b":
        return pats[0] if bar % 2 == 0 else pats[1] if bar == 1 else pats[2]
    return pats[1] if bar == 3 else pats[0]


def chord_pcs(name: str) -> set[int]:
    root, ivs = CH[name]
    return {(root + iv) % 12 for iv in ivs}


def is_strong(step: int, dur: int) -> bool:
    """En la negra o larga: tiene que ser tono del acorde (lab-leads.md §4)."""
    return step % 4 == 0 or dur >= 4


def pitches(pcs, low: int, high: int, k: int) -> list[int]:
    return [n for n in range(low, high + 1) if (n - A3 - k) % 12 in pcs]


def nearest(pool: list[int], target: int) -> int:
    return min(pool, key=lambda n: (abs(n - target), n))


def snap(tones: list[int], n: int, prev: int | None, up: bool) -> int:
    """El tono del acorde más cercano; en empate, el que sigue la dirección del contorno, y si repite la nota
    anterior, el siguiente más cercano (a una tercera mayor como mucho), para que no se quede pegada."""
    order = sorted(tones, key=lambda t: (abs(t - n), (t < n) if up else (t > n)))
    if prev is not None and order[0] == prev and len(order) > 1 and abs(order[1] - n) <= 4:
        return order[1]
    return order[0]


def lead(sc: Scale, rhythm: str, k: int = 0) -> list[tuple[int, int, int, int]]:
    low, high = (r + k for r in RANGE[rhythm])
    out = []
    for bar, chord in enumerate(sc.prog):
        scale = pitches(sc.bar_notes(bar), low - 12, high + 12, k)
        tones = pitches(chord_pcs(chord) & set(sc.bar_notes(bar)), low, high, k)
        anchor = nearest(tones, CENTER[rhythm] + k)
        i0, last = scale.index(anchor), 0
        for step, dur, vel, deg in rhythm_bar(rhythm, bar):
            n = scale[i0 + deg]
            if is_strong(step, dur):
                n = snap(tones, n, out[-1][2] if out else None, deg >= last)
            last = deg
            n = min(max(n, low), high)
            if (n - A3 - k) % 12 not in sc.bar_notes(bar):     # recortado al borde: vuelve a la escala
                n = nearest(pitches(sc.bar_notes(bar), low, high, k), n)
            out.append((bar * BAR + step * S16, dur * S16 - 10, n, vel))
    return out


def chords(sc: Scale, k: int = 0) -> list[tuple[int, int, int, int]]:
    """La progresión en tríadas cerradas, fundamental entre La2 y Sol#3; acorde largo que respira."""
    out = []
    for bar, name in enumerate(sc.prog):
        root, ivs = CH[name]
        base = A3 - 12 + k + root
        for iv in ivs:
            out.append((bar * BAR, 3 * BAR // 4, base + iv, 84))
    return out


def clip_name(sc: Scale, rhythm: str) -> str:
    return f"{sc.name}__{rhythm}__{'_'.join(sc.dialects)}"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--key", type=int, default=0, help="semitonos desde La")
    ap.add_argument("--bpm", type=float, default=124.0)
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    root = Path(a.dataset) / "midi" / "adn" / "lead" / "lab"
    for sc in SCALES:
        for rhythm in RHYTHMS:
            name = clip_name(sc, rhythm)
            write_mid(root / sc.family / f"{name}.mid", lead(sc, rhythm, a.key), a.bpm, name)
        write_mid(root / sc.family / f"{sc.name}__acordes.mid", chords(sc, a.key), a.bpm, f"{sc.name} acordes")
    print(f"{len(SCALES) * len(RHYTHMS)} leads y {len(SCALES)} clips de acordes en {root}")


if __name__ == "__main__":
    main()

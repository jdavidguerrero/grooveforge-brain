#!/usr/bin/env python3
"""Clips del estudio de acordes (motor-asistencia.md §10): los patrones del ADN sobre la progresión del estudio.

El estudio usa los acordes de kumoi en décimas y tresillo (Am – D – Am – D, la vamp dórica del lab de acordes), así que
el bajo no puede ser el bass_adn_octavas de la sesión ADN (Am – F – C – G): chocaría con el Fa# del Re. Mismo patrón de
octavas, fundamentales La y Re.

    python3 tools/paleta-lab/make_estudio_clips.py
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

import make_adn_clips as ma
from make_test_clips import BAR, S16, write_mid

ROOTS = (0, 5, 0, 5)                       # La, Re, La, Re (kumoi: Am – D)
CHORDS = ((0, 3, 7, 10), (0, 4, 7, 10), (0, 3, 7, 10), (0, 4, 7, 10))   # Am7 · D7 (el IV dórico, con Fa#)


def clips() -> dict[str, list]:
    out = {"bass_octavas_kumoi": [(b * BAR + s * S16, d * S16 - 10, ma.A1 + ROOTS[b] + iv, v)
                                  for b in range(4) for s, d, iv, v in ma.BASS["bass_adn_octavas"]]}
    arp = []                                 # pluck_adn_arp16 (sube y baja por el acorde, acentos 3-3-2) sobre Am7 – D7
    for b in range(4):
        tones = [ma.A1 + 36 + ROOTS[b] + iv for iv in CHORDS[b]]
        seq = tones + [tones[0] + 12] + tones[::-1][:3]
        for s in range(16):
            arp.append((b * BAR + s * S16, S16 - 10, seq[s % 8], 100 if s in (0, 3, 6, 8, 11, 14) else 66))
    out["pluck_arp16_kumoi"] = arp
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    out = Path(a.dataset) / "midi" / "adn" / "estudio"
    out.mkdir(parents=True, exist_ok=True)
    for name, notes in clips().items():
        write_mid(out / f"{name}.mid", notes, 124, name)
        print(out / f"{name}.mid")


if __name__ == "__main__":
    main()

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


def clips() -> dict[str, list]:
    return {"bass_octavas_kumoi": [(b * BAR + s * S16, d * S16 - 10, ma.A1 + ROOTS[b] + iv, v)
                                   for b in range(4) for s, d, iv, v in ma.BASS["bass_adn_octavas"]]}


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

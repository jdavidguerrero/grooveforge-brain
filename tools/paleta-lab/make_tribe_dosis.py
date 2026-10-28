#!/usr/bin/env python3
"""Dosis de Tribe por dialecto: un solo elemento de Tribe, suave, encima del kit de cada dialecto
(apps/docs/produccion/sesion-paleta-adn.md §1d). Escribe 4 compases por dialecto, ya en el teclado de Tribe
(Colombian Percussion), en $GF_DATASET/midi/perc/dosis_tribe/<dialecto>_tribe.mid. Organic no lleva: allí Tribe es
el kit entero.

    python3 tools/paleta-lab/make_tribe_dosis.py
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

from make_perc_patterns import notes_of, to_tribe
from make_test_clips import write_mid

# (rol del mapa común, línea de 16 semicorcheas, swing, ms de adelanto/atraso): solo ghosts y golpes suaves (g · o)
DOSIS = {
    "afro": ("agudo_abierto", "..o...g...o...g.", 54, 3),      # llamador en los contratiempos
    "melodic": ("sacudido_largo", "..g...o...g...o.", 52, 0),  # maracón: aire, sin otra semicorchea
    "indie": ("aro_madera", "o..g..o..g..o...", 50, -2),       # cáscara de tambora cada 3 (polirritmo)
    "disco": ("raspado", "gogogogogogogogo", 54, -2),          # guacharaca como un hat con swing
}


def dosis_notes(dialect: str, bpm: float = 124.0) -> list[tuple[int, int, int, int]]:
    role, line, swing, ms = DOSIS[dialect]
    p = {"roles": {role: [line] * 4}, "swing": swing, "micro": {role: ms}}
    notes, missing = to_tribe(notes_of(p, bpm, micro=True, seed=sum(map(ord, dialect))))
    assert not missing, missing
    return notes


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--bpm", type=float, default=124.0)
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    root = Path(a.dataset) / "midi" / "perc" / "dosis_tribe"
    for dialect in DOSIS:
        write_mid(root / f"{dialect}_tribe.mid", dosis_notes(dialect, a.bpm), a.bpm, f"dosis tribe {dialect}")
    print(f"{len(DOSIS)} dosis de Tribe en {root}")


if __name__ == "__main__":
    main()

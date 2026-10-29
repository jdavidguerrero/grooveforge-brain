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
import make_perc_patterns as mp
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
    out.update(roll_acelerado())
    return out


INDIE_PROGRESSIONS = ("menor_v_prestado", "frigia_dominante", "frigia")   # caso 2: tensión al V, color indie, oscura


def indie_progresiones() -> dict[str, list]:
    """Caso 2 (est-indie): la misma figura que ganó en el caso 1 (décimas en tresillo) sobre tres progresiones, para
    que lo único que cambie sea la armonía."""
    import make_chord_lab as mcl
    return {f"acordes_{p}__decimas__tresillo": mcl.pattern(p, "decimas", "tresillo") for p in INDIE_PROGRESSIONS}


def roll_acelerado() -> dict[str, list]:
    """8 compases: negras, corcheas, semicorcheas y fusas, dos compases cada una (percusion-groove.md rev. 9)."""
    hits = []                                             # (tick, duración, velocity)
    for pair, sub in enumerate((1, 2, 4, 8)):             # golpes por negra
        step = BAR // 4 // sub
        for t in range(pair * 2 * BAR, (pair + 1) * 2 * BAR, step):
            v = 40 + 80 * t / (8 * BAR)
            hits.append((t, max(step - 10, 20), int(min(127, v + (12 if t % (BAR // 4) == 0 else 0)))))
    grave = [(t, BAR // 4 - 20, int(60 + 60 * t / (8 * BAR))) for t in range(4 * BAR, 8 * BAR, BAR // 4)]
    caja = mp.TRIBE_ROLES["caja"]
    tribe = [(t, d, mp.TRIBE[caja[i % 2]], v) for i, (t, d, v) in enumerate(hits)]          # manos alternando por golpe
    tambora = mp.TRIBE_ROLES["grave_abierto"]
    tribe += [(t, d, mp.TRIBE[tambora[i % 2]], v) for i, (t, d, v) in enumerate(grave)]
    brasil = [(t, d, mp.ROLES["caja"], v) for t, d, v in hits] + [(t, d, mp.ROLES["grave_abierto"], v) for t, d, v in grave]
    return {"roll_acelerado_tribe": sorted(tribe), "roll_acelerado_brasil": sorted(brasil)}


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
    indie = Path(a.dataset) / "midi" / "adn" / "estudio_indie"
    indie.mkdir(parents=True, exist_ok=True)
    for name, notes in indie_progresiones().items():
        write_mid(indie / f"{name}.mid", notes, 124, name)
        print(indie / f"{name}.mid")


if __name__ == "__main__":
    main()

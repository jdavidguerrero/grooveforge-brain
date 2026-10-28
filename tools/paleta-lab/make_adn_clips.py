#!/usr/bin/env python3
"""Patrones para tocar en la sesión de paleta por ADN (apps/docs/produccion/sesion-paleta-adn.md).

Una sola progresión (Am – F – C – G, 124 BPM) para todos los roles, patrones que mezclan matices de los estilos de
Juan, un clip por capa híbrida (CC 18 elige la capa, CC 17 = MORPH 0 / 50 / 100 %) y tres contextos de percusión de
su selección (Tribe). Escribe en $GF_DATASET/midi/adn/.

    python3 tools/paleta-lab/make_adn_clips.py
    python3 tools/paleta-lab/make_adn_clips.py --key 2     # transpuesto (semitonos desde La)
"""
from __future__ import annotations

import argparse
import math
import os
import shutil
from pathlib import Path

from make_test_clips import BAR, S16, write_mid

A1 = 33
# Am – F – C – G: fundamental (semitonos desde La) y tríada / cuatríada de cada compás
PROG = [(0, (0, 3, 7, 10)), (-4, (0, 4, 7, 11)), (3, (0, 4, 7, 11)), (-2, (0, 4, 7, 10))]
SCALE = (0, 2, 3, 5, 7, 8, 10)                             # La menor natural
KICK_STEPS = (0, 4, 8, 12)

# (semicorchea, duración en semicorcheas, intervalo sobre la fundamental, velocity)
BASS = {
    "bass_adn_tambora": [(2, 2, 0, 96), (5, 1, 0, 48), (6, 2, 12, 84), (10, 2, 0, 98), (11, 1, 0, 46),
                         (13, 1, 7, 80), (14, 2, 0, 88)],
    "bass_adn_octavas": [(0, 1, 0, 90), (2, 1, 12, 80), (4, 1, 0, 88), (6, 1, 12, 78), (7, 1, 12, 48),
                         (8, 1, 0, 90), (10, 1, 12, 80), (12, 1, 0, 86), (13, 1, 0, 46), (14, 1, 12, 84),
                         (15, 1, 7, 60)],
    "bass_adn_rodante": [(s, 1, 12 if s == 14 else 0, 90 if s % 4 == 2 else 52) for s in range(16) if s % 4],
    "bass_adn_cumbia": [(0, 3, 0, 92), (6, 1, 7, 70), (7, 1, 7, 50), (8, 3, 7, 88), (14, 1, 0, 64), (15, 1, 0, 48)],
}
# (semicorchea, duración, velocity): el acorde entero
CHORDS = {
    "chords_adn_contratiempo": [(2, 1, 100), (6, 1, 96), (9, 1, 62), (10, 1, 100), (14, 1, 96)],
    "chords_adn_respiro": [(0, 6, 88), (8, 3, 80), (12, 2, 72)],
    "chords_adn_llamada": [(0, 2, 96), (3, 2, 90), (6, 2, 94), (10, 1, 70), (11, 2, 92), (14, 2, 88)],
}
# Capas de fábrica (firmware ui/layers.h, en ese orden) → patrón y transposición
LAYERS = [("woodbass", "bass/bass_adn_tambora", 0), ("kalimba", "pluck/pluck_adn_marimba", 0),
          ("french_stab", "chords/chords_adn_contratiempo", 0), ("acid_arp", "pluck/pluck_adn_arp16", -12),
          ("desert_lead", "lead/lead_adn_frase", 0), ("shaker_zap", "perc/perc_adn_madera", 0),
          ("roller_sub", "bass/bass_adn_rodante", 0), ("retro_brass", "chords/chords_adn_respiro", 0)]
CC_MORPH, CC_LAYER, CC_MOVE = 17, 18, 19
# Contextos de escucha (sesion-paleta-adn.md §2): (perc, top) de midi/perc/seleccion/
CONTEXTS = {
    "A_tierra": ("organic_house/perc/peak/v4/cumbia_afro_v4_peak_tribe.mid",
                 "afro_house/top/peak/v2/afro_house_groove_v2_peak_tribe.mid"),
    "B_noche": ("indie_tech/perc/peak/v2/tribal_toms_v2_peak_tribe.mid",
                "melodic_progressive/top/peak/v3/prog_rolling16_v3_peak_tribe.mid"),
    "C_fiesta": ("afro_house/perc/main_q/v2/djembe_sang_v2_main_q_tribe.mid",
                 "disco_house/top/peak/v4/samba_house_v4_peak_tribe.mid"),
}


def layer_cc(i: int, count: int = len(LAYERS)) -> int:
    """El valor de CC 18 que el firmware (layer_from_cc) lleva a la capa i (0 = la primera)."""
    return 1 + math.ceil(i * 127 / count)


def chord(bar: int, base: int) -> list[int]:
    r, ivs = PROG[bar % 4]
    return [base + r + iv for iv in ivs]


def clips(k: int) -> dict[str, list]:
    out = {}
    for name, pat in BASS.items():
        out[f"bass/{name}"] = [(b * BAR + s * S16, d * S16 - 10, A1 + k + PROG[b][0] + iv, v)
                               for b in range(4) for s, d, iv, v in pat]
    for name, pat in CHORDS.items():
        out[f"chords/{name}"] = [(b * BAR + s * S16, d * S16 - 10, n, v)
                                 for b in range(4) for s, d, v in pat for n in chord(b, A1 + 24 + k)]
    # Marimba: tonos del acorde en 3-3-2 (0, 3, 6 · 8, 11, 14), subiendo; un ghost repite antes del remate
    marimba = []
    for b in range(4):
        tones = chord(b, A1 + 36 + k)
        for j, s in enumerate((0, 3, 6, 8, 11, 14)):
            marimba.append((b * BAR + s * S16, 2 * S16 - 10, tones[j % 4] + (12 if j >= 4 else 0), 96 if j % 3 == 0 else 80))
        marimba.append((b * BAR + 15 * S16, S16 - 10, tones[0] + 12, 50))
    out["pluck/pluck_adn_marimba"] = marimba
    # Motivo en grados de la escala: compás 1 el motivo (La Do Mi Re), 2 una tercera arriba, 3 invertido, 4 resuelve
    # en una nota larga (Sol)
    base = A1 + 36 + k
    deg = lambda d: base + 12 * (d // 7) + SCALE[d % 7]
    mot = []
    for b, notes in enumerate(([0, 2, 4, 3], [2, 4, 6, 5], [4, 3, 2, 0])):
        for j, s in enumerate((0, 2, 3, 6)):
            mot.append((b * BAR + s * S16, (2 if j < 3 else 4) * S16 - 10, deg(notes[j]), 92 if j == 0 else 80))
    mot.append((3 * BAR, BAR - 20, deg(-1), 90))
    out["pluck/pluck_adn_motivo"] = mot
    # Arpegio de semicorcheas sube y baja por el acorde; acentos en 3-3-2
    arp = []
    for b in range(4):
        tones = chord(b, A1 + 36 + k)
        seq = tones + [tones[0] + 12] + tones[::-1][:3]
        for s in range(16):
            arp.append((b * BAR + s * S16, S16 - 10, seq[s % 8], 100 if s in (0, 3, 6, 8, 11, 14) else 66))
    out["pluck/pluck_adn_arp16"] = arp
    # Log drum: grave y sincopado (La1 / Mi2 / La2)
    ld = [(3, 0, 96), (6, 7, 84), (10, 0, 100), (13, 12, 80), (14, 7, 70)]
    out["perc/perc_adn_logdrum"] = [(b * BAR + s * S16, 2 * S16 - 10, A1 + 12 + k + n, v)
                                    for b in range(4) for s, n, v in ld]
    # Madera: tresillo (0, 3, 6) agudo y 10, 12 grave
    md = [(0, 24, 104), (3, 24, 90), (6, 24, 96), (10, 12, 92), (12, 12, 84)]
    out["perc/perc_adn_madera"] = [(b * BAR + s * S16, S16, A1 + 24 + k + n, v) for b in range(4) for s, n, v in md]
    out["pad/pad_adn_progresion"] = [(b * BAR, BAR - 20, n, 88) for b in range(4) for n in chord(b, A1 + 24 + k)]
    out["pad/pad_adn_dron"] = [(0, 8 * BAR - 20, A1 + 12 + k, 86), (0, 8 * BAR - 20, A1 + 19 + k, 80)]
    # Lead: pregunta (compases 1–2) y respuesta (3–4) en la pentatónica, notas largas
    L = A1 + 36 + k
    lead = [(0, 0, 6, 96), (6, 3, 2, 84), (8, 5, 8, 92), (16, 7, 4, 96), (20, 10, 4, 84), (24, 7, 8, 90),
            (32, 12, 6, 98), (38, 10, 2, 84), (40, 7, 8, 90), (48, 5, 4, 88), (52, 3, 4, 82), (56, 0, 8, 92)]
    out["lead/lead_adn_frase"] = [(s * S16, d * S16 - 10, L + n, v) for s, n, d, v in lead]
    # Hook: 2 compases que se repiten (el gancho del drop); pentatónica, síncopa en el 2.º compás
    hook = [(0, 0, 2, 96), (3, 3, 1, 80), (4, 5, 2, 90), (7, 3, 1, 70), (10, 7, 3, 94), (16, 5, 2, 90),
            (19, 3, 2, 82), (22, 0, 2, 88), (26, -2, 4, 92)]
    out["lead/lead_adn_hook"] = [(rep * 2 * BAR + s * S16, d * S16 - 10, L + n, v)
                                 for rep in range(2) for s, n, d, v in hook]
    # Profunda: por compás la novena y la séptima del acorde (en la escala), notas largas que descienden
    deep = [(0, 14, 6, 90), (6, 10, 10, 80), (16, 11, 6, 88), (22, 7, 10, 78), (32, 14, 6, 90), (38, 11, 10, 80),
            (48, 9, 8, 86), (56, 5, 8, 80)]
    out["lead/lead_adn_profunda"] = [(s * S16, d * S16 - 10, L - 12 + PROG[s // 16][0] + iv, v)
                                     for s, iv, d, v in deep]
    return out


def layer_clip(i: int, notes: list, shift: int) -> tuple[list, list]:
    """El patrón tres veces (12 compases) con MORPH 0 / 50 / 100 %; CC 18 elige la capa al comenzar."""
    span = 4 * BAR
    out = [(t + rep * span, d, n + shift, v) for rep in range(3) for t, d, n, v in notes if t < span]
    ccs = [(0, CC_LAYER, layer_cc(i))] + [(rep * span, CC_MORPH, val) for rep, val in enumerate((0, 64, 127))]
    return out, ccs


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--key", type=int, default=0, help="semitonos desde La menor")
    ap.add_argument("--bpm", type=float, default=124.0)
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    root = Path(a.dataset) / "midi" / "adn"
    pats = clips(a.key)
    for rel, notes in pats.items():
        write_mid(root / f"{rel}.mid", notes, a.bpm, rel.split("/")[1].replace("_", " "))
    for i, (name, rel, shift) in enumerate(LAYERS):
        notes, ccs = layer_clip(i, pats[rel], shift)
        write_mid(root / "capas" / f"capa_{i + 1}_{name}.mid", notes, a.bpm, f"capa {i + 1} {name}", ccs)
    sel = Path(a.dataset) / "midi" / "perc" / "seleccion"
    for ctx, (perc, top) in CONTEXTS.items():
        for part, src in (("perc", perc), ("top", top)):
            dst = root / "contexto" / ctx / f"{part}_tribe.mid"
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(sel / src, dst)
    # Build: La sostenido 8 compases y MOVE (CC 19) de 0 a 127, un paso por negra
    write_mid(root / "build" / "build_adn_move.mid", [(0, 8 * BAR - 20, A1 + 24 + a.key, 90)], a.bpm,
              "build adn move", [(q * BAR // 4, CC_MOVE, round(127 * q / 31)) for q in range(32)])
    write_mid(root / "contexto" / "kick_4x4.mid", [(q * BAR // 4, S16, 36, 110) for q in range(16)], a.bpm, "kick 4x4")
    print(f"{len(pats) + 1} patrones, {len(LAYERS)} capas y {len(CONTEXTS)} contextos en {root}")


if __name__ == "__main__":
    main()

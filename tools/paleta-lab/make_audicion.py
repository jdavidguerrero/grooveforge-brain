#!/usr/bin/env python3
"""Prepara una audición a ciegas de capas del Brain: A.mid / B.mid / C.mid para arrastrar a un set que ya abre.

Cada opción es un preset de capa: el mismo clip con un CC 18 al inicio que elige esa capa (firmware:
studio::layer_from_cc), en orden barajado. La correspondencia letra → capa queda en metadata/audiciones/<id>.json con el
hash del raw que el Brain tiene en ese slot (lo que de verdad suena). La opción marcada con --dejar es "conservar lo que
hay". El .als no se toca: Live 12 beta se cayó con copias generadas (envolventes de CC escritas a mano, y también al
vaciar clips de audio y recortar escenas), así que Juan arrastra los .mid a escenas de un set suyo y se declara cuáles. El orden se baraja; la correspondencia
letra → capa queda en metadata/audiciones/<id>.json, junto con el hash del raw que el Brain tiene en ese slot (lo que
de verdad suena). La opción marcada con --dejar es "conservar lo que hay".

    python3 tools/paleta-lab/make_audicion.py --pendiente <id> --salida "<…>/GFExploration Project/aud_bajo" \
        --clip adn/estudio/bass_octavas_kumoi.mid --set GF_Acordes_Lab.als --escenas 16-18 \
        --backing "kit afro + dosis Tribe; acordes WOOD2 grabados" --dejar "JD SUB MID" "JD SUB MID" "ROLLER SUB"
"""
from __future__ import annotations

import argparse
import hashlib
import json
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "preset-librarian"))
import fill_lab_set as fls  # noqa: E402
import gf_log  # noqa: E402
import make_test_clips as mtc  # noqa: E402

CC_LAYER = 18
LETTERS = "ABC"


def cc_for_layer(slot: int, count: int) -> int:
    """El menor valor de CC 18 que layer_from_cc lleva al slot (1…count)."""
    for v in range(1, 128):
        if min(count, 1 + (v - 1) * count // 127) == slot:
            return v
    raise ValueError(slot)


def prepare(out_dir: Path, clip_rel: str, options: list[str], keep: str, layers: list[tuple[str, bytes]],
            pending: str, context: dict, seed: int | None = None, bpm: float = 124.0) -> dict:
    if not 1 <= len(options) <= 3 or keep not in options:
        raise SystemExit("hasta 3 opciones, y --dejar tiene que ser una de ellas")
    names = [n for n, _ in layers]
    for o in options:
        if o not in names:
            raise SystemExit(f"{o} no está en el Brain")
    if out_dir.exists() and any(out_dir.iterdir()):
        raise SystemExit(f"{out_dir} ya tiene archivos: cada audición va en una carpeta nueva")
    out_dir.mkdir(parents=True, exist_ok=True)
    notes = fls.read_mid(gf_log.dataset_dir() / "midi" / clip_rel)
    order = options[:]
    random.Random(seed).shuffle(order)
    snapshot = gf_log.latest_snapshot()
    opts = []
    for k, name in enumerate(order):
        label, slot = LETTERS[k], names.index(name) + 1
        mtc.write_mid(out_dir / f"{label}.mid", [(round(t * mtc.TPQ), round(d * mtc.TPQ), n, v) for t, d, n, v in notes],
                      bpm, label, ccs=[(0, CC_LAYER, cc_for_layer(slot, len(names)))])
        opts.append({"id": f"opt{k + 1}", "letter": label, "kind": "keep_current" if name == keep else "option",
                     "patch": {"name": name, "slot": slot,
                               "sha256_brain_raw": hashlib.sha256(layers[slot - 1][1]).hexdigest(), "match": "exact"},
                     "midi": gf_log.asset(f"midi/{clip_rel}", snapshot), "origin": "generator_proposal",
                     "exploration": False})
    aid = gf_log.new_id("a", pending)
    prep = {"audicion_id": aid, "pendiente_id": pending, "pieza": None,
            "context": {**context, "midi_dir": str(out_dir), "bpm": bpm, "declared_by": "asistente, según lo que se le pidió a Juan"},
            "options": opts,
            "presentation": {"order": [o["letter"] for o in opts], "blind": True, "seed": seed,
                             "level_matched": "unknown", "exploration_policy": "none (cerrar decisión)"},
            "firmware_loaded": "unknown", "snapshot": snapshot.name if snapshot else None}
    gf_log.auditions_dir().mkdir(parents=True, exist_ok=True)
    (gf_log.auditions_dir() / f"{aid}.json").write_text(json.dumps(prep, indent=2, ensure_ascii=False) + "\n")
    return prep


def brain_layers(port: str | None) -> list[tuple[str, bytes]]:
    import gf_presets as gp
    return [(gp.decode_layer(raw)["name"], raw) for _, raw in gp.Brain(port).layer_export()]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--pendiente", required=True)
    ap.add_argument("--salida", required=True, help="carpeta nueva para A.mid / B.mid / C.mid")
    ap.add_argument("--clip", required=True, help="ruta dentro de midi/ del dataset")
    ap.add_argument("--set", required=True, help="el .als de Juan donde se arrastran")
    ap.add_argument("--escenas", required=True, help="las escenas donde van A/B/C, p. ej. 16-18")
    ap.add_argument("--backing", required=True, help="lo que suena con las opciones")
    ap.add_argument("--seccion", default="groove")
    ap.add_argument("--dejar", required=True, help="la opción que es conservar lo que hay")
    ap.add_argument("--semilla", type=int)
    ap.add_argument("--port")
    ap.add_argument("opciones", nargs="+")
    a = ap.parse_args(argv)
    ctx = {"als": a.set, "scenes": a.escenas, "section": {"name": a.seccion}, "backing": a.backing}
    prep = prepare(Path(a.salida), a.clip, a.opciones, a.dejar, brain_layers(a.port), a.pendiente, ctx, a.semilla)
    print(f"audición {prep['audicion_id']}: {len(prep['options'])} opciones en {a.salida}")


if __name__ == "__main__":
    main()

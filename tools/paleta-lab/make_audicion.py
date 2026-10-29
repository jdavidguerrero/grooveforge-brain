#!/usr/bin/env python3
"""Prepara una audición a ciegas de capas del Brain sobre una copia de un set de Live (motor-asistencia.md §3, §10).

Cada opción es un preset de capa: la escena A / B / C toca el mismo clip en BRAIN MIDI con una envolvente de CC 18 que
elige esa capa (firmware: studio::layer_from_cc), sobre el kit de un dialecto. El orden se baraja; la correspondencia
letra → capa queda en metadata/audiciones/<id>.json, junto con el hash del raw que el Brain tiene en ese slot (lo que
de verdad suena). La opción marcada con --dejar es "conservar lo que hay".

    python3 tools/paleta-lab/make_audicion.py --pendiente <id> --plantilla "<…>/GF_Acordes_Lab.als" \
        --salida "<…>/GF_Est_Acordes.als" --clip adn/chords/lab/kumoi__decimas__tresillo.mid --dialecto afro \
        --dejar "JD PLK WOOD" "JD PLK WOOD" "JD PLK WOOD2" "JD PLK WOOD3"
"""
from __future__ import annotations

import argparse
import copy
import gzip
import hashlib
import json
import random
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "preset-librarian"))
import fill_lab_set as fls  # noqa: E402
import gf_log  # noqa: E402

CC_LAYER = 18
LETTERS = "ABC"


def cc_for_layer(slot: int, count: int) -> int:
    """El menor valor de CC 18 que layer_from_cc lleva al slot (1…count)."""
    for v in range(1, 128):
        if min(count, 1 + (v - 1) * count // 127) == slot:
            return v
    raise ValueError(slot)


def cc_target_id(track: ET.Element, cc: int) -> str:
    t = track.find(f".//MidiControllers/ControllerTargets.{cc}")
    if t is None:
        raise SystemExit(f"la pista no tiene ControllerTargets.{cc}")
    return t.get("Id")


def add_cc_envelope(clip: ET.Element, pointee: str, value: int):
    """Envolvente de clip constante (el evento en -63072000 es el valor de base de Live)."""
    env = ET.fromstring(
        f'<ClipEnvelope Id="0"><EnvelopeTarget><PointeeId Value="{pointee}" /></EnvelopeTarget><Automation><Events>'
        f'<FloatEvent Id="1" Time="-63072000" Value="{value}" /></Events><AutomationTransformViewState>'
        '<IsTransformPending Value="false" /><TimeAndValueTransforms /></AutomationTransformViewState></Automation>'
        '</ClipEnvelope>')
    clip.find("Envelopes/Envelopes").append(env)


def prepare(template: Path, out: Path, clip_rel: str, dialect: str, options: list[str], keep: str,
            layers: list[tuple[str, bytes]], pending: str, seed: int | None = None, bpm: float = 124.0) -> dict:
    if out.exists():
        raise SystemExit(f"{out} ya existe: la audición se arma sobre una copia nueva")
    if not 1 <= len(options) <= 3 or keep not in options:
        raise SystemExit("hasta 3 opciones, y --dejar tiene que ser una de ellas")
    names = [n for n, _ in layers]
    for o in options:
        if o not in names:
            raise SystemExit(f"{o} no está en el Brain")
    ds = gf_log.dataset_dir()
    midi = ds / "midi"
    root = ET.fromstring(gzip.decompress(template.read_bytes()))
    ls = root.find("LiveSet")
    tracks = {t.find("Name/EffectiveName").get("Value"): t for t in ls.find("Tracks")}
    order = options[:]
    random.Random(seed).shuffle(order)
    for row in range(len(ls.find("Scenes"))):                      # la copia arranca sin clips (el bajo sigue abierto)
        ls.find("Scenes")[row].find("Name").set("Value", "")
        for tr in ls.find("Tracks"):
            if tr.tag in ("MidiTrack", "AudioTrack"):
                slots = tr.find(".//MainSequencer/ClipSlotList")
                if slots is not None and row < len(slots):
                    fls.put(tr, row, None)
    rows = ["contexto"] + [f"{LETTERS[i]}" for i in range(len(order))]
    fls.ensure_scenes(ls, len(rows))
    fls.trim_scenes(ls, len(rows))
    cid = 1 + max((int(c.get("Id")) for c in root.iter("MidiClip")), default=0)
    kick = fls.read_mid(midi / "adn" / "contexto" / "kick_4x4.mid")
    parts = {}
    for track, rel, keep_notes in fls.DIALECT_KITS[dialect]:
        parts.setdefault(track, []).extend(n for n in fls.read_mid(midi / rel) if keep_notes is None or n[2] in keep_notes)
    dose = midi / "perc" / "dosis_tribe" / f"{dialect}_tribe.mid"
    if dose.exists():
        parts.setdefault("PERC", []).extend(fls.read_mid(dose))
    chords = fls.read_mid(midi / clip_rel)
    pointee = cc_target_id(tracks["BRAIN MIDI"], CC_LAYER)
    snapshot = gf_log.latest_snapshot()
    opts = []
    for row, label in enumerate(rows):
        ls.find("Scenes")[row].find("Name").set("Value", label)
        fls.put(tracks["KICK"], row, fls.make_clip(kick, "kick", fls.COLORS["KICK"], cid)); cid += 1
        for track, notes in parts.items():
            fls.put(tracks[track], row, fls.make_clip(sorted(notes), f"{dialect}", fls.COLORS.get(track, 26), cid))
            cid += 1
        if row == 0:
            continue
        name = order[row - 1]
        slot = names.index(name) + 1
        clip = fls.make_clip(chords, label, fls.COLORS["chords"], cid); cid += 1
        add_cc_envelope(clip, pointee, cc_for_layer(slot, len(names)))
        fls.put(tracks["BRAIN MIDI"], row, clip)
        raw = layers[slot - 1][1]
        opts.append({"letter": label, "id": f"opt{row}", "kind": "keep_current" if name == keep else "option",
                     "patch": {"name": name, "slot": slot, "sha256_brain_raw": hashlib.sha256(raw).hexdigest(),
                               "match": "exact"},
                     "midi": gf_log.asset(f"midi/{clip_rel}", snapshot), "origin": "generator_proposal",
                     "exploration": False})
    tempo = ls.find(".//MainTrack//Tempo/Manual")
    if tempo is not None:
        tempo.set("Value", f"{bpm:g}")
    xml = '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(root, encoding="unicode")
    out.write_bytes(gzip.compress(xml.encode("utf-8")))
    aid = gf_log.new_id("a", pending)
    prep = {"audicion_id": aid, "pendiente_id": pending, "pieza": None,
            "context": {"als_template": str(template), "als_copy": str(out),
                        "als_copy_sha256": hashlib.sha256(out.read_bytes()).hexdigest(),
                        "section": {"name": "groove", "scene": "contexto + A/B/C"}, "bpm": bpm, "dialect": dialect,
                        "backing": {"kick": gf_log.asset("midi/adn/contexto/kick_4x4.mid", snapshot),
                                    "kit": [gf_log.asset(f"midi/{r}", snapshot) for _, r, _ in fls.DIALECT_KITS[dialect]]
                                    + ([gf_log.asset(f"midi/perc/dosis_tribe/{dialect}_tribe.mid", snapshot)]
                                       if dose.exists() else []),
                                    "bass": "unknown"}},
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
    ap.add_argument("--plantilla", required=True)
    ap.add_argument("--salida", required=True)
    ap.add_argument("--clip", required=True, help="ruta dentro de midi/ del dataset")
    ap.add_argument("--dialecto", default="afro", choices=sorted(fls.DIALECT_KITS))
    ap.add_argument("--dejar", required=True, help="la opción que es conservar lo que hay")
    ap.add_argument("--semilla", type=int)
    ap.add_argument("--port")
    ap.add_argument("opciones", nargs="+")
    a = ap.parse_args(argv)
    prep = prepare(Path(a.plantilla), Path(a.salida), a.clip, a.dialecto, a.opciones, a.dejar,
                   brain_layers(a.port), a.pendiente, a.semilla)
    print(f"audición {prep['audicion_id']}: {len(prep['options'])} escenas ciegas en {a.salida}")


if __name__ == "__main__":
    main()

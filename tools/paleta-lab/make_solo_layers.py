#!/usr/bin/env python3
"""Capas "solo": un preset de engine envuelto en una capa con MORPH en 0 (motor-asistencia.md, pendiente de synths
individuales).

Con MORPH en 0 el crossfade de igual potencia deja B en silencio (studio::layer_gains: gb = sin(0) = 0) y, por si se
mueve el MORPH, B va además a −60 dB. El firmware exige engines distintos en A y B (studio::layer_decode), así que B
lleva un preset cualquiera de otro engine, que no suena. Así un synth individual se elige por CC 18 como cualquier capa y entra en una
audición a ciegas sin tocar el firmware. Escribe la ficha en patches/brain/layers/; se sube con
`gf_presets.py layer-import`.

    python3 tools/paleta-lab/make_solo_layers.py prophet_5/gf-talking-pluck.json "S TALK PLUCK"
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "preset-librarian"))
import gf_presets as gp  # noqa: E402

GAIN_B_DB = -60


def solo_layer(preset_raw: bytes, filler_raw: bytes, name: str) -> bytes:
    ea, eb = gp.decode(preset_raw)["engine"], gp.decode(filler_raw)["engine"]
    if ea == eb:
        raise ValueError("el relleno de B tiene que ser de otro engine")
    head = gp.LAYER_MAGIC + bytes([gp.LAYER_FORMAT])
    head += name.encode("ascii", "replace")[:gp.LAYER_NAME_LEN].ljust(gp.LAYER_NAME_LEN, b"\0")
    head += bytes([ea, eb, 0, 0]) + struct.pack("<bbb", 0, 0, GAIN_B_DB)   # MORPH 0: solo suena A
    raw = head + preset_raw + filler_raw
    assert len(raw) == gp.LAYER_SIZE
    return raw


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("preset", help="ruta dentro de patches/brain/ (p. ej. prophet_5/gf-talking-pluck.json)")
    ap.add_argument("nombre", help=f"nombre de la capa ({gp.LAYER_NAME_LEN} caracteres como máximo)")
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    lib = Path(a.dataset) / "patches" / "brain"
    src = json.loads((lib / a.preset).read_text())
    engine = a.preset.split("/")[0]
    filler = next(f for f in sorted(lib.glob("*/*.json")) if f.parent.name not in (engine, "layers"))
    raw = solo_layer(bytes.fromhex(src["raw"]), bytes.fromhex(json.loads(filler.read_text())["raw"]), a.nombre)
    entry = gp.layer_entry_from_raw(raw)
    entry.update(role=src.get("role"), notes=f"capa solo de {src['name']} ({a.preset}): MORPH 0, B −60 dB")
    out = lib / "layers" / f"{gp.slug(a.nombre)}.json"
    out.write_text(json.dumps(entry, indent=2, ensure_ascii=False) + "\n")
    print(out)


if __name__ == "__main__":
    main()

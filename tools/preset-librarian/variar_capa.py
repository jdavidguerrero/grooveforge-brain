#!/usr/bin/env python3
"""Variaciones acotadas de una capa del Brain. Diseño: apps/docs/produccion/motor-bajo.md §4.1.

    uv run --with pyserial python tools/preset-librarian/variar_capa.py          # solo a la librería
    uv run --with pyserial python tools/preset-librarian/variar_capa.py --subir  # también al Brain, al final del banco
"""
from __future__ import annotations

import argparse
import copy
import json
import struct
from pathlib import Path

import gf_presets as g

_OP_MOD = 1
_OSC_SUB_B = 2


def _cutoff(d: dict, hz: float) -> None:
    d["a"]["patch"]["cutoff_hz"] = hz


def _menos_sub(d: dict) -> None:
    d["morph"] = 42
    d["b"]["patch"]["osc"][_OSC_SUB_B][2] = 50.0


def _osc(d: dict) -> None:
    _cutoff(d, 6500.0)


def _mad1(d: dict) -> None:
    _menos_sub(d)


def _mad2(d: dict) -> None:
    _menos_sub(d)
    d["a"]["patch"]["osc"][_OP_MOD][1] = 85.0
    _cutoff(d, 7000.0)


def _mad3(d: dict) -> None:
    _menos_sub(d)
    d["a"]["patch"]["osc"][_OP_MOD][0] = 3.0
    d["a"]["patch"]["osc"][_OP_MOD][1] = 60.0
    _cutoff(d, 7000.0)


VARIACIONES = {"TRESB OSC": _osc, "TRESB MAD1": _mad1, "TRESB MAD2": _mad2, "TRESB MAD3": _mad3}


def encode_layer(d: dict) -> bytes:
    """Inverso de `decode_layer`, con el orden de LAYER_BYTES en layer_bank.h."""
    cabeza = g.LAYER_MAGIC + bytes([g.LAYER_FORMAT]) + d["name"].encode("ascii", "replace")[: g.LAYER_NAME_LEN].ljust(g.LAYER_NAME_LEN, b"\0")
    fijo = bytes([g.ENGINES.index(d["engine_a"]), g.ENGINES.index(d["engine_b"]), d["morph"], d["delay_b_ms"]])
    fijo += struct.pack("<bbb", d["transpose_b"], d["gain_a_db"], d["gain_b_db"])
    a = g.encode(g.ENGINES.index(d["engine_a"]), d["a"]["name"], d["a"]["patch"])
    b = g.encode(g.ENGINES.index(d["engine_b"]), d["b"]["name"], d["b"]["patch"])
    return cabeza + fijo + a + b


def variar(base: dict, nombre: str) -> dict:
    d = copy.deepcopy(base)
    d["name"] = nombre
    VARIACIONES[nombre](d)
    return d


def main(argv=None) -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--lib")
    ap.add_argument("--port")
    ap.add_argument("--subir", action="store_true")
    a = ap.parse_args(argv)
    carpeta = g.library_dir(a.lib) / "brain" / "layers"
    base = g.decode_layer(bytes.fromhex(json.loads((carpeta / "tresbass.json").read_text())["raw"]))
    rutas = []
    for nombre in VARIACIONES:
        if g.find_by_name(carpeta, nombre):
            raise SystemExit(f"Ya existe {nombre!r}: no se pisa")
        entrada = g.layer_entry_from_raw(encode_layer(variar(base, nombre)))
        entrada.update(origin="propio", role="bass", notes="Variación de TRESBASS; motor-bajo.md §4.1")
        ruta = carpeta / f"{g.slug(nombre)}.json"
        ruta.write_text(json.dumps(entrada, indent=2, ensure_ascii=False) + "\n")
        rutas.append(str(ruta))
        print("En la librería:", ruta.name)
    if a.subir:
        g.cmd_layer_import(argparse.Namespace(files=rutas, reemplazar=False, port=a.port))


if __name__ == "__main__":
    main()

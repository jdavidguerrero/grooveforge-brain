#!/usr/bin/env python3
"""Presets JD KEYS: pianos eléctricos parecidos al Electric de Live (apps/docs/produccion/lab-acordes.md §8).

Lee presets que ya existen en la librería (exportados con gf_presets.py), les aplica los cambios de §8 y escribe los
JSON nuevos al lado, con su ficha. No toca el Brain: después se suben con gf_presets.py (import / layer-import).

    JD KEYS JUNO     (Juno-106)          ← PLUCK KEYS
    JD KEYS PROPHET  (Prophet-5)         ← WARM KEYS
    JD KEYS CAPA     (capa Juno + DX7)   ← JD KEYS JUNO + E.PIANO reducido al golpe (mitades JD KEYS BODY / JD KEYS TINE)

    ~/.platformio/penv/bin/python tools/paleta-lab/make_jd_keys.py
"""
from __future__ import annotations

import argparse
import copy
import json
import struct
from pathlib import Path

from make_jd_leads import g

# Índices de gf_params.h: JUNO OSC (WAVE · SUB · NOISE · PW | CHORUS · HPF · RANGE · VCA), PROPHET OSC A / B / MIXER
JUNO_WAVE_PULSE, JUNO_CHORUS_I = 2, 1
TINE_RATIO = 14.0                                       # la relación clásica de la lengüeta del Rhodes en FM
CAPA_GAIN_B_DB = -8


def keys_env(p: dict, decay: float, cutoff: float, env_amt: float, f_decay: float, vel_cut: float = 70.0):
    """El molde común (§8): sin sustain plano (decae solo, rasgo 4), filtro que abre al golpe y se cierra (rasgo 2) y
    mucha VEL → filtro (el bark, rasgo 3)."""
    p.update(attack_ms=2.0, decay_ms=decay, sustain=0.12, release_ms=300.0, f_attack_ms=1.0, f_decay_ms=f_decay,
             f_sustain=0.0, f_release_ms=300.0, env_amt=env_amt, cutoff_hz=cutoff, reso_norm=0.1, glide=0.0,
             vel_cut=vel_cut, vel_amp=70.0)
    p["lfo_amt"] = [0.0] * len(p["lfo_amt"])
    p["groove_on"] = [False] * len(p["groove_on"])
    p["sfx_on"] = [False] * len(p["sfx_on"])
    p["delay_on"] = False


def juno(src: dict) -> dict:
    p = copy.deepcopy(src)
    keys_env(p, decay=2600.0, cutoff=800.0, env_amt=0.45, f_decay=450.0)
    o = p["osc"][0]
    o[0], o[1], o[2], o[3] = JUNO_WAVE_PULSE, 45.0, 0.0, 35.0      # pulso 35 % + SUB: hueco redondo, sin zumbido
    o[4], o[5], o[6], o[7] = JUNO_CHORUS_I, 1.0, 1.0, 0.0           # chorus I · HPF 1 · 8' · VCA por envolvente
    return p


def prophet(src: dict) -> dict:
    p = copy.deepcopy(src)
    keys_env(p, decay=2800.0, cutoff=900.0, env_amt=0.5, f_decay=180.0, vel_cut=75.0)
    p["drive"] = 0.3
    a, b, mix = p["osc"][0], p["osc"][1], p["osc"][2]
    a[:5] = [24.0, 15.0, 0.0, 1.0, 0.0]          # A: pulso angosto dos octavas arriba, sin sync: el clic
    b[:8] = [0.0, 0.0, 50.0, 1.0, 0.0, 1.0, 0.0, 0.0]   # B: triángulo en 8', el cuerpo
    mix[:5] = [12.0, 80.0, 0.0, 0.0, 1.0]         # A bajito (solo se oye mientras el filtro abre) · B · FILTER KBD
    p["mod"][0][0] = 0.0                          # sin POLY-MOD
    return p


def tine(src: dict) -> dict:
    """E.PIANO reducido al golpe: algoritmo 5 (2 → 1, 4 → 3), solo la pila 2 → 1 con el modulador a 14:1, y todo
    se apaga antes de 200 ms."""
    p = copy.deepcopy(src)
    keys_env(p, decay=150.0, cutoff=15000.0, env_amt=0.0, f_decay=150.0)
    p.update(sustain=0.0, release_ms=100.0, vel_amp=100.0)
    #           RATIO      LEVEL ALG FB  A    D      S    R
    p["osc"] = [[1.0,        99.0, 5.0, 0.0, 0.0, 120.0, 0.0, 100.0],
                [TINE_RATIO, 55.0, 0.0, 0.0, 0.0, 90.0, 0.0, 80.0],
                [1.0,         0.0, 0.0, 0.0, 0.0, 120.0, 0.0, 100.0],
                [1.0,         0.0, 0.0, 0.0, 0.0, 120.0, 0.0, 100.0]]
    return p


NOTES = {
    "JD KEYS JUNO": "de PLUCK KEYS: pulso 35 % + SUB 45, pasabajos 800 Hz con env corta, decae en 2,6 s (S 0,12), "
                    "VEL -> filtro 70, chorus I (lab-acordes.md §8)",
    "JD KEYS PROPHET": "de WARM KEYS: triángulo en B (cuerpo) + pulso angosto +24 al 12 % en A (clic), filtro 900 Hz "
                       "con env de 180 ms, VEL -> filtro 75, drive 0,3, decae en 2,8 s (lab-acordes.md §8)",
    "JD KEYS CAPA": "JD KEYS JUNO + DX7 solo golpe (2 -> 1 a 14:1, se apaga en ~120 ms) a -8 dB: cuerpo + tine "
                    "(lab-acordes.md §8)",
}


def entry(raw: bytes, name: str) -> dict:
    e = g.entry_from_raw(raw)
    e.update(role="chords", origin="propio", tags=["lab-acordes", "fase2", "electric"], notes=NOTES[name])
    return e


def build(lib: Path) -> dict[str, dict]:
    """{ruta relativa: JSON} de los tres presets nuevos, a partir de los presets de la librería."""
    def src(engine: str, name: str) -> dict:
        path = g.find_by_name(lib / engine, name)
        if not path:
            raise SystemExit(f"no está {name} en {lib / engine}: correr antes gf_presets.py export")
        return json.loads(path.read_text())["patch"]

    j = juno(src("juno_106", "PLUCK KEYS"))
    out = {}
    for engine, name, patch in (("juno_106", "JD KEYS JUNO", j),
                                ("prophet_5", "JD KEYS PROPHET", prophet(src("prophet_5", "WARM KEYS")))):
        out[f"{engine}/{g.slug(name)}.json"] = entry(g.encode(g.ENGINES.index(engine), name, patch), name)
    ea, eb = g.ENGINES.index("juno_106"), g.ENGINES.index("dx7")
    head = (g.LAYER_MAGIC + bytes([g.LAYER_FORMAT]) + b"JD KEYS CAPA".ljust(g.LAYER_NAME_LEN, b"\0")
            + bytes([ea, eb, 50, 0]) + struct.pack("<bbb", 0, 0, CAPA_GAIN_B_DB))
    raw = head + g.encode(ea, "JD KEYS BODY", j) + g.encode(eb, "JD KEYS TINE", tine(src("dx7", "E.PIANO")))
    le = g.layer_entry_from_raw(raw)
    le.update(role="chords", origin="propio", tags=["lab-acordes", "fase2", "electric"], notes=NOTES["JD KEYS CAPA"])
    out[f"layers/{g.slug('JD KEYS CAPA')}.json"] = le
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lib", help="librería (por defecto la de gf_presets.py)")
    a = ap.parse_args(argv)
    lib = g.library_dir(a.lib) / "brain"
    for rel, e in build(lib).items():
        path = lib / rel
        if path.exists():
            old = json.loads(path.read_text())
            for k in g.META_KEYS:                     # conservar lo que Juan anotó a mano
                if old.get(k) not in (None, "", []):
                    e[k] = old[k]
        path.write_text(json.dumps(e, indent=2, ensure_ascii=False) + "\n")
        print(f"{e['name']:16} → {path}")


if __name__ == "__main__":
    main()

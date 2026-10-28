#!/usr/bin/env python3
"""Presets JD LEAD: leads menos estridentes derivados de los que le gustaron a Juan (apps/docs/produccion/lab-leads.md §5).

Lee los originales de la librería (exportados con gf_presets.py), aplica los cambios de §5 y escribe los JSON nuevos al
lado, con su ficha. No toca el Brain: después se suben con gf_presets.py (import / layer-import).

    JD LEAD MELODIC  (OB-6)            ← GF MELODIC LEAD
    JD LEAD BODY     (Prophet-5)       ← JD RUN LEAD BODY
    JD DESERT LD     (capa OB-6 + P-5) ← DESERT LEAD (mitades JD DESERT A / JD DESERT B)

    ~/.platformio/penv/bin/python tools/paleta-lab/make_jd_leads.py
"""
from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "preset-librarian"))
import gf_presets as g  # noqa: E402

# Índices de gf_params.h (bridge-protocol)
DEST_CUTOFF, DEST_PITCH = 0, 1
UNISON, MACRO = 3, 4
UNISON_VOICES, UNISON_DETUNE = 0, 1                 # VOICES es un índice de GF_UNISON_COUNT {1, 2, 3, 6}
DRIFT, GRIT = 0, 1


def envelopes(p: dict, attack: float, sustain: float, release: float, f_decay: float, f_sustain: float,
              env_amt: float, cutoff: float, reso: float, vel_cut: float = 65.0):
    """El molde común (§5): ataque sin clic, menos sustain y cola corta; el brillo lo da la envolvente del filtro."""
    p.update(attack_ms=attack, sustain=sustain, release_ms=release, f_attack_ms=3.0, f_decay_ms=f_decay,
             f_sustain=f_sustain, f_release_ms=release, env_amt=env_amt, cutoff_hz=cutoff, reso_norm=reso,
             vel_cut=vel_cut)
    p["lfo_amt"][DEST_PITCH] = 0.0                    # sin vibrato fijo: va en la rueda / MOVE


def prophet_no_sync(p: dict, fine_b: float = 7.0, level_a: float = 70.0, level_b: float = 45.0):
    """Prophet sin sync: OSC A en unísono con B (con sync apagado, un intervalo en A sonaría como acorde), B un poco
    desafinado para el cuerpo y sin POLY-MOD de la envolvente a FREQ A (el barrido agudo del sync)."""
    p["osc"][0][0] = 0.0                              # FREQ A
    p["osc"][0][4] = 0.0                              # SYNC
    p["osc"][1][1] = fine_b                           # FINE B
    p["osc"][2][0], p["osc"][2][1] = level_a, level_b
    p["mod"][0][0] = 0.0                              # POLY-MOD FILT ENV


def melodic(src: dict) -> dict:
    p = copy.deepcopy(src)
    envelopes(p, attack=12, sustain=0.55, release=200, f_decay=220, f_sustain=0.15, env_amt=0.55, cutoff=1200,
              reso=0.15)
    p["glide"] = 0.03
    p["groove"][UNISON][UNISON_VOICES] = 1.0          # ×2 (era ×3)
    p["groove"][UNISON][UNISON_DETUNE] = 10.0         # 10 ¢ (eran 18)
    p["groove"][MACRO][DRIFT] = 15.0
    p["groove"][MACRO][GRIT] = 30.0                   # cuerpo en la zona media
    return p


def body(src: dict) -> dict:
    p = copy.deepcopy(src)
    envelopes(p, attack=10, sustain=0.55, release=180, f_decay=250, f_sustain=0.2, env_amt=0.55, cutoff=1100,
              reso=0.2)
    prophet_no_sync(p, level_a=70.0, level_b=50.0)
    return p


def desert_a(src: dict) -> dict:
    p = copy.deepcopy(src)
    envelopes(p, attack=12, sustain=0.6, release=220, f_decay=250, f_sustain=0.25, env_amt=0.45, cutoff=1300,
              reso=0.15, vel_cut=60.0)
    p["osc"][0][6] = 15.0                             # FLT MORPH: casi pasabajos (50 = el notch nasal)
    p["lfo_amt"][DEST_CUTOFF] = 0.1                   # el barrido lento, apenas
    return p


def desert_b(src: dict) -> dict:
    p = copy.deepcopy(src)
    envelopes(p, attack=12, sustain=0.55, release=200, f_decay=220, f_sustain=0.15, env_amt=0.5, cutoff=1100,
              reso=0.15, vel_cut=60.0)
    prophet_no_sync(p, level_a=70.0, level_b=40.0)
    return p


NOTES = {
    "JD LEAD MELODIC": "de GF MELODIC LEAD: sin vibrato, unison x2 10 c, cutoff 1,2 kHz con env corta, ataque 12 ms, "
                       "release 200 ms, GRIT 30 (lab-leads.md §5)",
    "JD LEAD BODY": "de JD RUN LEAD BODY: sin sync ni poly-mod, B +7 c, cutoff 1,1 kHz con env corta, ataque 10 ms "
                    "(lab-leads.md §5)",
    "JD DESERT LD": "de DESERT LEAD: A sin notch (morph 15) y barrido leve; B sin sync ni poly-mod; ataque 12 ms, "
                    "release ~200 ms, cutoff 1,1-1,3 kHz (lab-leads.md §5)",
}


def entry(raw: bytes, name: str) -> dict:
    e = g.entry_from_raw(raw)
    e.update(role="lead", origin="propio", tags=["lab-leads", "fase2"], notes=NOTES[name])
    return e


def build(lib: Path) -> dict[str, dict]:
    """{ruta relativa: JSON} de los tres presets nuevos, a partir de los originales de la librería."""
    def src(engine: str, name: str) -> dict:
        path = g.find_by_name(lib / engine, name)
        if not path:
            sys.exit(f"no está {name} en {lib / engine}: correr antes gf_presets.py export")
        return json.loads(path.read_text())

    out = {}
    for engine, src_name, name, fn in (("ob_6", "GF MELODIC LEAD", "JD LEAD MELODIC", melodic),
                                       ("prophet_5", "JD RUN LEAD BODY", "JD LEAD BODY", body)):
        s = src(engine, src_name)
        raw = g.encode(g.ENGINES.index(engine), name, fn(s["patch"]))
        out[f"{engine}/{g.slug(name)}.json"] = entry(raw, name)
    lpath = g.find_by_name(lib / "layers", "DESERT LEAD")
    if not lpath:
        sys.exit("no está la capa DESERT LEAD: correr antes gf_presets.py layers")
    old = bytes.fromhex(json.loads(lpath.read_text())["raw"])
    d = g.decode_layer(old)
    a = g.encode(d["a"]["engine"], "JD DESERT A", desert_a(d["a"]["patch"]))
    b = g.encode(d["b"]["engine"], "JD DESERT B", desert_b(d["b"]["patch"]))
    head = g.LAYER_MAGIC + bytes([g.LAYER_FORMAT]) + old[4:4 + g.LAYER_NAME_LEN] + old[16:23]
    raw = g.rename_layer(head + a + b, "JD DESERT LD")
    le = g.layer_entry_from_raw(raw)
    le.update(role="lead", origin="propio", tags=["lab-leads", "fase2"], notes=NOTES["JD DESERT LD"])
    out[f"layers/{g.slug('JD DESERT LD')}.json"] = le
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

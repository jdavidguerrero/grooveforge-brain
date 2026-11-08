#!/usr/bin/env python3
"""Presets JD KEYS: pianos eléctricos parecidos al Electric de Live (apps/docs/produccion/lab-acordes.md §8).

Lee presets que ya existen en la librería (exportados con gf_presets.py), les aplica los cambios de §8 y escribe los
JSON nuevos al lado, con su ficha. No toca el Brain: después se suben con gf_presets.py (import / layer-import).

    JD KEYS JUNO     (Juno-106)          ← PLUCK KEYS
    JD KEYS PROPHET  (Prophet-5)         ← WARM KEYS
    JD KEYS CAPA     (capa Juno + DX7)   ← JD KEYS JUNO + E.PIANO reducido al golpe (mitades JD KEYS BODY / JD KEYS TINE)
    JD PLK KEYS · JD PLK WOOD · JD PLK BELL · JD SUS KEYS · JD SUS RHODE · JD SUS DARK: capas Juno + DX7 por familia de
    patrón (§9: pluck para corcheas y tresillo, sostenidas para respiro y bloque largo)

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


# Capas Juno + DX7 por familia de patrón (lab-acordes.md §9)
JUNO_WAVE_SQR, JUNO_WAVE_SAW, JUNO_WAVE_SAW_PLS, JUNO_CHORUS_II, JUNO_CHORUS_I_II = 1, 0, 3, 2, 3


def juno_half(src: dict, wave: int, sub: float, cutoff: float, env_amt: float, f_decay: float, decay: float,
              sustain: float, release: float, chorus: int, attack: float = 2.0, pw: float = 35.0,
              hpf: float = 1.0) -> dict:
    p = copy.deepcopy(src)
    keys_env(p, decay=decay, cutoff=cutoff, env_amt=env_amt, f_decay=f_decay)
    p.update(attack_ms=attack, sustain=sustain, release_ms=release, f_release_ms=release)
    o = p["osc"][0]
    o[0], o[1], o[2], o[3] = wave, sub, 0.0, pw
    o[4], o[5], o[6], o[7] = chorus, hpf, 1.0, 0.0
    return p


def dx7_half(src: dict, body: float, body_mod: float, body_decay: float, mod_decay: float, tine: float,
             tine_mod: float, tine_decay: float, sustain: float = 0.0, release: float = 150.0, fb: float = 0.0) -> dict:
    """Algoritmo 5: pila 2 → 1 = cuerpo (el índice decae en mod_decay: brillo al golpe, redondo después); pila 4 → 3 =
    tine (modulador a 14:1). LEVEL 0 apaga una pila."""
    p = copy.deepcopy(src)
    longest = max(body_decay, tine_decay)
    keys_env(p, decay=longest, cutoff=15000.0, env_amt=0.0, f_decay=150.0)
    p.update(sustain=sustain, release_ms=release, vel_amp=100.0)
    s = sustain * 100
    #           RATIO       LEVEL     ALG  FB  A    D           S  R
    p["osc"] = [[1.0,        body,     5.0, fb,  0.0, body_decay, s, release],
                [1.0,        body_mod, 0.0, 0.0, 0.0, mod_decay,  0.0, release],
                [1.0,        tine,     0.0, 0.0, 0.0, tine_decay, 0.0, min(release, 100.0)],
                [TINE_RATIO, tine_mod, 0.0, 0.0, 0.0, tine_decay * 0.7, 0.0, min(release, 80.0)]]
    return p


# (nombre, familia, ganancia A dB, ganancia B dB, mitad Juno, mitad DX7)
LAYERS = [
    ("JD PLK KEYS", "pluck", 0, -4,
     dict(wave=JUNO_WAVE_PULSE, sub=40.0, cutoff=700.0, env_amt=0.55, f_decay=220.0, decay=600.0, sustain=0.0,
          release=180.0, chorus=JUNO_CHORUS_I),
     dict(body=85.0, body_mod=45.0, body_decay=500.0, mod_decay=250.0, tine=70.0, tine_mod=60.0, tine_decay=150.0)),
    ("JD PLK WOOD", "pluck", 0, -6,
     dict(wave=JUNO_WAVE_SQR, sub=50.0, cutoff=500.0, env_amt=0.6, f_decay=160.0, decay=450.0, sustain=0.0,
          release=150.0, chorus=0),
     dict(body=0.0, body_mod=0.0, body_decay=100.0, mod_decay=100.0, tine=75.0, tine_mod=65.0, tine_decay=100.0)),
    ("JD PLK BELL", "pluck", -6, 0,
     dict(wave=JUNO_WAVE_SAW, sub=35.0, cutoff=400.0, env_amt=0.4, f_decay=200.0, decay=500.0, sustain=0.0,
          release=200.0, chorus=JUNO_CHORUS_II),
     dict(body=99.0, body_mod=60.0, body_decay=700.0, mod_decay=300.0, tine=65.0, tine_mod=70.0, tine_decay=180.0,
          fb=2.0)),
    ("JD SUS KEYS", "sostenido", 0, -9,
     dict(wave=JUNO_WAVE_PULSE, sub=30.0, cutoff=900.0, env_amt=0.35, f_decay=600.0, decay=3500.0, sustain=0.35,
          release=500.0, chorus=JUNO_CHORUS_I, attack=3.0),
     dict(body=0.0, body_mod=0.0, body_decay=150.0, mod_decay=100.0, tine=60.0, tine_mod=50.0, tine_decay=150.0)),
    ("JD SUS RHODE", "sostenido", -4, 0,
     dict(wave=JUNO_WAVE_PULSE, sub=45.0, cutoff=600.0, env_amt=0.3, f_decay=500.0, decay=3000.0, sustain=0.3,
          release=450.0, chorus=JUNO_CHORUS_I),
     dict(body=95.0, body_mod=50.0, body_decay=3000.0, mod_decay=1500.0, tine=60.0, tine_mod=55.0, tine_decay=150.0,
          sustain=0.3, release=450.0)),
    ("JD SUS DARK", "sostenido", 0, -10,
     dict(wave=JUNO_WAVE_SAW, sub=30.0, cutoff=550.0, env_amt=0.3, f_decay=700.0, decay=4000.0, sustain=0.4,
          release=600.0, chorus=JUNO_CHORUS_I_II, attack=12.0),
     dict(body=0.0, body_mod=0.0, body_decay=150.0, mod_decay=100.0, tine=50.0, tine_mod=40.0, tine_decay=140.0)),
    # Variaciones de la escucha 2026-10-28 (§9, ronda 2): WOOD gustó; DARK va, pero grave y sin brillo
    ("JD PLK WOOD2", "pluck", 0, -3,
     dict(wave=JUNO_WAVE_SQR, sub=50.0, cutoff=500.0, env_amt=0.7, f_decay=120.0, decay=300.0, sustain=0.0,
          release=100.0, chorus=0),
     dict(body=0.0, body_mod=0.0, body_decay=100.0, mod_decay=100.0, tine=80.0, tine_mod=70.0, tine_decay=90.0)),
    ("JD PLK WOOD3", "pluck", 0, -6,
     dict(wave=JUNO_WAVE_SQR, sub=50.0, cutoff=650.0, env_amt=0.6, f_decay=200.0, decay=650.0, sustain=0.0,
          release=200.0, chorus=JUNO_CHORUS_I),
     dict(body=0.0, body_mod=0.0, body_decay=100.0, mod_decay=100.0, tine=75.0, tine_mod=65.0, tine_decay=100.0)),
    ("JD SUS DARK2", "sostenido", 0, -7,
     dict(wave=JUNO_WAVE_SAW, sub=15.0, cutoff=850.0, env_amt=0.3, f_decay=700.0, decay=4000.0, sustain=0.4,
          release=600.0, chorus=JUNO_CHORUS_I_II, attack=12.0, hpf=2.0),
     dict(body=0.0, body_mod=0.0, body_decay=150.0, mod_decay=100.0, tine=55.0, tine_mod=45.0, tine_decay=140.0)),
    ("JD SUS DARK3", "sostenido", 0, -6,
     dict(wave=JUNO_WAVE_SAW_PLS, sub=10.0, cutoff=1100.0, env_amt=0.4, f_decay=700.0, decay=4000.0, sustain=0.4,
          release=600.0, chorus=JUNO_CHORUS_I_II, attack=12.0, hpf=2.0),
     dict(body=0.0, body_mod=0.0, body_decay=150.0, mod_decay=100.0, tine=60.0, tine_mod=50.0, tine_decay=150.0)),
]


def prophet_half(src: dict, a_wave: str, a_freq: float, b_wave: str, b_freq: float, b_fine: float, level_a: float,
                 level_b: float, cutoff: float, env_amt: float, f_decay: float, attack: float, decay: float,
                 sustain: float, release: float, vel_cut: float = 60.0, drive: float = 0.2) -> dict:
    """Cuerpo sostenido para acordes largos (motor-acordes.md, Sonido): la mitad Prophet de las capas Prophet + DX7.
    Ondas: "saw", "pulse" o "tri" (solo B). B FINE en cents desafina contra A: el ancho sin chorus del Prophet."""
    p = copy.deepcopy(src)
    keys_env(p, decay=decay, cutoff=cutoff, env_amt=env_amt, f_decay=f_decay, vel_cut=vel_cut)
    p.update(attack_ms=attack, sustain=sustain, release_ms=release, f_release_ms=release, drive=drive, vel_amp=60.0)
    a, b, mix = p["osc"][0], p["osc"][1], p["osc"][2]
    a[:5] = [a_freq, 40.0, float(a_wave == "saw"), float(a_wave == "pulse"), 0.0]
    b[:8] = [b_freq, b_fine, 45.0, 1.0, float(b_wave == "saw"), float(b_wave == "tri"), float(b_wave == "pulse"), 0.0]
    mix[:5] = [level_a, level_b, 0.0, 0.0, 1.0]
    p["mod"][0][0] = 0.0                          # sin POLY-MOD
    return p


# Capas Prophet + DX7 para acordes largos (pedido de Juan, 2026-11-08). Mismo esquema que Juno + DX7: el analógico
# pone el cuerpo y el DX7 solo la tine del golpe.
LAYERS_PROPHET = [
    ("JD SUS WARM", "sostenido", 0, -9,
     dict(a_wave="pulse", a_freq=0.0, b_wave="saw", b_freq=-12.0, b_fine=0.0, level_a=60.0, level_b=40.0,
          cutoff=750.0, env_amt=0.3, f_decay=700.0, attack=8.0, decay=4000.0, sustain=0.45, release=600.0),
     dict(body=0.0, body_mod=0.0, body_decay=150.0, mod_decay=100.0, tine=55.0, tine_mod=45.0, tine_decay=140.0)),
    ("JD SUS WARM2", "sostenido", 0, -5,
     dict(a_wave="pulse", a_freq=0.0, b_wave="saw", b_freq=0.0, b_fine=0.0, level_a=60.0, level_b=35.0,
          cutoff=1200.0, env_amt=0.4, f_decay=600.0, attack=6.0, decay=4000.0, sustain=0.45, release=600.0,
          vel_cut=70.0),
     dict(body=0.0, body_mod=0.0, body_decay=150.0, mod_decay=100.0, tine=65.0, tine_mod=55.0, tine_decay=160.0)),
    ("JD SUS POLY", "sostenido", 0, -10,
     dict(a_wave="saw", a_freq=0.0, b_wave="saw", b_freq=0.0, b_fine=7.0, level_a=50.0, level_b=50.0,
          cutoff=1000.0, env_amt=0.25, f_decay=900.0, attack=20.0, decay=5000.0, sustain=0.55, release=900.0),
     dict(body=0.0, body_mod=0.0, body_decay=150.0, mod_decay=100.0, tine=50.0, tine_mod=40.0, tine_decay=90.0)),
]
VARIATIONS = {"JD PLK WOOD": ("JD PLK WOOD2", "JD PLK WOOD3"), "JD SUS DARK": ("JD SUS DARK2", "JD SUS DARK3")}


def layer_raw(name: str, gain_a: int, gain_b: int, a: dict, b: dict, engine_a: str = "juno_106") -> bytes:
    ea, eb = g.ENGINES.index(engine_a), g.ENGINES.index("dx7")
    head = (g.LAYER_MAGIC + bytes([g.LAYER_FORMAT]) + name.encode().ljust(g.LAYER_NAME_LEN, b"\0")
            + bytes([ea, eb, 50, 0]) + struct.pack("<bbb", 0, gain_a, gain_b))
    return head + g.encode(ea, f"{name} A", a) + g.encode(eb, f"{name} B", b)


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
    juno_src, dx7_src = src("juno_106", "PLUCK KEYS"), src("dx7", "E.PIANO")
    for name, family, ga, gb, ja, db in LAYERS:
        le = g.layer_entry_from_raw(layer_raw(name, ga, gb, juno_half(juno_src, **ja), dx7_half(dx7_src, **db)))
        le.update(role="chords", origin="propio", tags=["lab-acordes", "fase2", family],
                  notes=f"capa Juno + DX7, familia {family} (lab-acordes.md §9)")
        out[f"layers/{g.slug(name)}.json"] = le
    prophet_src = src("prophet_5", "WARM KEYS")
    for name, family, ga, gb, pa, db in LAYERS_PROPHET:
        raw = layer_raw(name, ga, gb, prophet_half(prophet_src, **pa), dx7_half(dx7_src, **db), "prophet_5")
        le = g.layer_entry_from_raw(raw)
        le.update(role="chords", origin="propio", tags=["motor-acordes", "largo", family],
                  notes="capa Prophet + DX7 para acordes largos (motor-acordes.md, Sonido)")
        out[f"layers/{g.slug(name)}.json"] = le
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

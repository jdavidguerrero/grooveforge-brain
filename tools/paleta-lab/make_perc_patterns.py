#!/usr/bin/env python3
"""Biblioteca de patrones de percusión sobre el GF Perc Map (apps/docs/produccion/percusion-groove.md §3, §7).

Escribe en $GF_DATASET (por defecto ~/GROOVEFORGE/GrooveForge_Dataset_v1):
  kits/<kit>.json                     qué toca cada rol en cada kit (Tribe Colombian, djembes/dununs, Brasil, 808)
  midi/perc/<patron>.mid              patrón en el mapa común, con microtiming
  midi/perc/<patron>_q.mid            el mismo, cuantizado (para comparar)
  midi/perc/<patron>.json             ficha (§5)
  midi/perc/tribe_colombia/<patron>.mid   convertido al teclado de Tribe Colombian Percussion

    python3 tools/paleta-lab/make_perc_patterns.py
    python3 tools/paleta-lab/make_perc_patterns.py --bpm 122
"""
from __future__ import annotations

import argparse
import json
import os
import random
from pathlib import Path

from make_test_clips import BAR, S16, TPQ, write_mid

# GF Perc Map v1: un rol rítmico por nota (C1 = 36, convención de Live)
ROLES = {"grave_abierto": 36, "grave_apagado": 37, "medio_abierto": 38, "medio_apagado": 39, "agudo_abierto": 40,
         "agudo_slap": 41, "aro_madera": 42, "sacudido_corto": 43, "sacudido_largo": 44, "raspado": 45,
         "campana_grave": 46, "campana_aguda": 47, "caja": 48, "pandereta": 49, "efecto": 50}

# Tribe Colombian Percussion, del teclado del plugin (capturas de Juan, 2026-10-27), en la numeración de Live
TRIBE = {"Bombo Open": 36, "Bombo Cascara L": 38, "Bombo Cascara R": 40, "Tambora Open L": 41, "Tambora Open R": 43,
         "Tambora Cascara L": 44, "Tambora Damped L": 45, "Tambora Cascara R": 46, "Tambora Damped R": 47,
         "Llamador Bajoneo": 48, "Llamador Abierto": 50, "Alegre Bajoneo": 52, "Alegre Abierto": 53,
         "Alegre Quemao": 55, "Alegre Latigazo": 57, "Guache Up": 59, "Guache Down": 60, "Guache Rain": 62,
         "Guache Tremolo": 64, "Maracon Up": 65, "Maracon Down": 67, "Maracon Tremolo": 69, "Caja Fondo L": 71,
         "Caja Fondo R": 72, "Caja Quemao L": 74, "Caja Quemao R": 76, "Caja Latigazo": 77, "Guacharaca Shot": 79,
         "Guacharaca Short": 81, "Guacharaca Mid": 83, "Guacharaca Long": 84, "Guacharaca Longer": 86}

# Rol → articulaciones de Tribe. Dos = [semicorchea par, impar] (manos L / R; guache y maracón: Down / Up).
TRIBE_ROLES = {
    "grave_abierto": ["Tambora Open L", "Tambora Open R"],
    "grave_apagado": ["Tambora Damped L", "Tambora Damped R"],
    "aro_madera": ["Tambora Cascara L", "Tambora Cascara R"],
    "medio_abierto": ["Alegre Abierto"],
    "medio_apagado": ["Alegre Bajoneo"],
    "agudo_abierto": ["Llamador Abierto"],
    "agudo_slap": ["Alegre Quemao"],
    "sacudido_corto": ["Guache Down", "Guache Up"],
    "sacudido_largo": ["Maracon Down", "Maracon Up"],
    "raspado": ["Guacharaca Short"],
    "caja": ["Caja Fondo L", "Caja Fondo R"],
    "pandereta": ["Caja Quemao L", "Caja Quemao R"],
    "efecto": ["Alegre Latigazo"],
    "campana_grave": ["Bombo Cascara L", "Bombo Cascara R"],   # sin campanas: el aro del bombo hace de madera grave
    "campana_aguda": ["Llamador Bajoneo"],
}

# Drum Racks: el sample de cada rol (nombres como los tiene Juan; los vacíos quedan para completar)
DJEMBE = {"grave_abierto": "Dundumba ab", "grave_apagado": "Djembe bass", "medio_abierto": "Sangban ab",
          "medio_apagado": "Sangban pres", "agudo_abierto": "Kenke ab", "agudo_slap": "Djembe slap seco",
          "aro_madera": "Kenke pres", "sacudido_corto": "Xeke Xeke", "sacudido_largo": "Tchekere cabaz",
          "raspado": "", "campana_grave": "Campana dun", "campana_aguda": "Campana kenke", "caja": "Djembe tono",
          "pandereta": "Djembe agudo", "efecto": "Djembe long"}
BRASIL = {"grave_abierto": "Surdo marcação", "grave_apagado": "Surdo abafado", "medio_abierto": "Atabaque",
          "medio_apagado": "Alfaia", "agudo_abierto": "Repinique", "agudo_slap": "Timbal",
          "aro_madera": "Caixa rim", "sacudido_corto": "Ganzá", "sacudido_largo": "Chocalho",
          "raspado": "Reco-reco", "campana_grave": "Agogô (grave)", "campana_aguda": "Agogô (aguda)",
          "caja": "Caixa", "pandereta": "Pandeiro", "efecto": "Cuíca"}
TR808 = {"grave_abierto": "", "grave_apagado": "", "medio_abierto": "Conga low", "medio_apagado": "Conga mid",
         "agudo_abierto": "Conga high", "agudo_slap": "Bongo high", "aro_madera": "Rimshot",
         "sacudido_corto": "Maracas", "sacudido_largo": "", "raspado": "", "campana_grave": "Cowbell",
         "campana_aguda": "Claves", "caja": "Snare", "pandereta": "Tambourine", "efecto": "Bongo low"}

# Patrones: rol → [compás 1, compás 2] en semicorcheas. X acento (112) · x normal (88) · g ghost (52) · . silencio.
# swing: % del contratiempo de semicorchea (50 = recto) · micro: ms de adelanto (−) o atraso (+) por rol.
PATTERNS = {
    "cumbia_base": dict(estilo="cumbia", origen="ritmo tradicional", densidad="media", swing=53,
                        micro={"agudo_abierto": 6, "medio_abierto": -4}, roles={
        "grave_abierto":  ["X.......x..g....", "X.......x..g..x."],
        "aro_madera":     ["..g.x.g...g.x.g.", "..g.x.g...g.x..."],
        "agudo_abierto":  ["..x...x...x...x.", "..x...x...x...x."],
        "medio_abierto":  ["x..x..x...x..x..", "x..x..x.x.x....x"],
        "medio_apagado":  ["...g......g.....", "...g............"],
        "sacudido_corto": ["XgxgXgxgXgxgXgxg", "XgxgXgxgXgxgXgxg"]}),
    "puya_vallenata": dict(estilo="vallenato", origen="ritmo tradicional", densidad="alta", swing=52,
                           micro={"raspado": -3}, roles={
        "grave_abierto": ["x.......x.......", "x.......x...x..."],
        "caja":          ["x.gxx.gxx.gxx.gx", "x.gxx.gxx.gxxxgx"],
        "raspado":       ["XgxgXgxgXgxgXgxg", "XgxgXgxgXgxgXxxx"]}),
    "bullerengue": dict(estilo="bullerengue", origen="ritmo tradicional", densidad="media", swing=54,
                        micro={"medio_abierto": -5}, roles={
        "agudo_abierto":  ["x...x...x...x...", "x...x...x...x..."],
        "medio_abierto":  ["x..x...x.x..x...", "x..x...x.x..x.xx"],
        "grave_apagado":  ["....x.......x...", "....x.......x..x"],
        "sacudido_largo": ["x.......x.......", "x.......x......."]}),
    "mapale": dict(estilo="mapalé", origen="ritmo tradicional", densidad="alta", swing=50,
                   micro={"medio_abierto": -3, "agudo_abierto": 3}, roles={
        "grave_abierto":  ["X..xX..xX..xX..x", "X..xX..xX..xXxxx"],
        "medio_abierto":  ["xxx.xxx.xxx.xx.x", "xxx.xxx.xxx.xxxx"],
        "agudo_abierto":  ["..x...x...x...x.", "..x...x...x...x."],
        "sacudido_corto": ["xgxgxgxgxgxgxgxg", "xgxgxgxgxgxgxgxg"]}),
    "kuku_afro": dict(estilo="afro (kuku)", origen="ritmo tradicional", densidad="media", swing=55,
                      micro={"agudo_slap": -6, "campana_grave": 2}, roles={
        "grave_abierto":  ["x.....x...x.....", "x.....x...x...x."],
        "grave_apagado":  ["x.........x.....", "x.........x....."],
        "campana_grave":  ["x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x."],
        "medio_abierto":  ["....x.x.....x.x.", "....x.x.....x.xx"],
        "agudo_abierto":  ["..x...x...x...x.", "..x...x...x...x."],
        "agudo_slap":     ["...x..x....x.x..", "...x..x....xxx.x"],
        "sacudido_corto": ["x.xxx.xxx.xxx.xx", "x.xxx.xxx.xxx.xx"]}),
    "samba_reggae": dict(estilo="samba reggae", origen="ritmo tradicional", densidad="media", swing=52,
                         micro={"agudo_slap": -4}, roles={
        "grave_abierto": ["X......xX.......", "X......xX......."],
        "grave_apagado": ["....x.......x...", "....x.......x.x."],
        "agudo_slap":    ["x..x..x...x.x...", "x..x..x...x.x.xx"],
        "caja":          ["x.gx.gx.x.gx.gxg", "x.gx.gx.x.gxxxxx"]}),
    "maracatu": dict(estilo="maracatu", origen="ritmo tradicional", densidad="alta", swing=50,
                     micro={"campana_aguda": -3}, roles={
        "grave_abierto": ["x.......x......x", "x.......x....x.x"],
        "medio_apagado": ["x..x..x.x.x.x...", "x..x..x.x.x.xxx."],
        "campana_grave": ["x.x.....x.x.....", "x.x.....x.x....."],
        "campana_aguda": ["....x.xx....x.xx", "....x.xx....x.xx"],
        "caja":          ["xgxgxgxgxgxgxgxg", "xgxgxgxgxgxgXXXX"]}),
    "baiao": dict(estilo="baião", origen="ritmo tradicional", densidad="baja", swing=52,
                  micro={"campana_aguda": -2}, roles={
        "grave_abierto": ["X..x....X..x....", "X..x....X..x..x."],
        "aro_madera":    ["....x.......x...", "....x.......x..."],
        "campana_aguda": ["xgxXxgxXxgxXxgxX", "xgxXxgxXxgxXxgxX"]}),
    "afro_house_top": dict(estilo="afro house", origen="propio", densidad="media", swing=56,
                           micro={"sacudido_corto": 0, "agudo_slap": -5, "medio_abierto": 3}, roles={
        "sacudido_corto": ["gxXxgxXxgxXxgxXx", "gxXxgxXxgxXxgxXx"],
        "medio_abierto":  ["..x.....x.x.....", "..x.....x.x..x.."],
        "agudo_slap":     ["......x.......x.", "......x....x.xx."],
        "grave_abierto":  ["..x.......x.....", "..x.......x....x"],
        "campana_grave":  ["..x...x...x...x.", "..x...x...x...x."]}),
    "house_top_conga": dict(estilo="house", origen="propio", densidad="media", swing=56,
                            micro={"agudo_abierto": 4, "pandereta": -2}, roles={
        "sacudido_corto": ["gxXxgxXxgxXxgxXx", "gxXxgxXxgxXxgxXx"],
        "agudo_abierto":  ["......x..x....x.", "..x...x..x..x..."],
        "medio_abierto":  ["x.........x.....", "x.........x...x."],
        "aro_madera":     ["....x.......x..x", "....x.......x..."],
        "pandereta":      ["..x...x...x...x.", "..x...x...x...x."]}),
    "organic_shaker_wood": dict(estilo="orgánico", origen="propio", densidad="baja", swing=57,
                                micro={"raspado": -4, "aro_madera": 3}, roles={
        "sacudido_largo": ["x.......x.......", "x.......x......."],
        "sacudido_corto": ["..x...x...x...xg", "..x...x...x..gxg"],
        "raspado":        ["....x.......x...", "....x.......x.x."],
        "aro_madera":     ["...x.....x...x..", "...x.....x..x..."]}),
    "tribal_toms": dict(estilo="indie tech", origen="propio", densidad="media", swing=51,
                        micro={"agudo_abierto": -3}, roles={
        "medio_abierto": ["x..x..x.x..x..x.", "x..x..x.x..x.xxx"],
        "agudo_abierto": ["..x.....x.x.....", "..x.....x.x.x..."],
        "grave_abierto": ["x.......x.......", "x.......x...x.xx"],
        "efecto":        ["................", "...............x"]}),
}

VEL = {"X": 112, "x": 88, "g": 52}


def notes_of(p: dict, bpm: float, micro: bool, seed: int) -> list[tuple[int, int, str, int]]:
    """(tick, duración, rol, velocity). Con micro: swing en las semicorcheas impares, ms por rol y velocity ±5."""
    rnd = random.Random(seed)
    ms = TPQ * bpm / 60000.0                         # ticks por milisegundo
    swing = int((p["swing"] - 50) / 50.0 * S16) if micro else 0
    out = []
    for role, bars in p["roles"].items():
        shift = int(p["micro"].get(role, 0) * ms) if micro else 0
        for b, line in enumerate(bars):
            assert len(line) == 16, (role, line)
            for step, ch in enumerate(line):
                if ch == ".":
                    continue
                t = b * BAR + step * S16 + (swing if step % 2 else 0) + shift
                v = VEL[ch] + (rnd.randint(-5, 5) if micro else 0)
                out.append((max(0, t), S16 - 20, role, max(1, min(127, v))))
    return out


def to_tribe(notes, octave: int = 0) -> tuple[list, list]:
    """Al teclado de Tribe: dos articulaciones = mano / dirección por la semicorchea (par / impar)."""
    out, missing = [], set()
    for t, d, role, v in notes:
        arts = TRIBE_ROLES.get(role)
        if not arts:
            missing.add(role)
            continue
        step = int(round(t / S16))
        art = arts[step % 2] if len(arts) == 2 else arts[0]
        out.append((t, d, TRIBE[art] + 12 * octave, v))
    return out, sorted(missing)


def kit_files() -> dict[str, dict]:
    tribe_roles = {r: {"articulaciones": a, "notas": [TRIBE[x] for x in a]} for r, a in TRIBE_ROLES.items()}
    return {
        "tribe_colombia": {"kit": "tribe_colombia", "tipo": "plugin", "instrumento": "Tribe · Colombian Percussion",
                           "origen": "tribe-instruments", "octava": 0,
                           "verificar": "tocar C1 (MIDI 36) en Live: si suena Bombo Open la octava es 0",
                           "permisos": {"entrenar": "revisar licencia", "publicar": "sí (en tu música)",
                                        "samplear": "revisar licencia"},
                           "teclado": TRIBE, "roles": tribe_roles},
        "djembe_dundun": {"kit": "djembe_dundun", "tipo": "samples (Drum Rack)", "origen": "otro",
                          "permisos": {"entrenar": "revisar", "publicar": "revisar"},
                          "roles": {r: {"nota": ROLES[r], "sample": s} for r, s in DJEMBE.items()},
                          "extras": ["Djembe bass con xeke", "Dundumba doble", "YGK_Perc_Dundun_03_Dry",
                                     "Campana sang", "TCP_Conga_Bell_5"]},
        "brasil_samples": {"kit": "brasil_samples", "tipo": "samples (Drum Rack)", "origen": "otro",
                           "permisos": {"entrenar": "revisar", "publicar": "revisar"},
                           "roles": {r: {"nota": ROLES[r], "sample": s} for r, s in BRASIL.items()},
                           "extras": ["Zabumba", "Rum / Rumpi / Le", "Tantan", "Rebolo", "Tamborim", "Xequerê",
                                      "Rocar", "Triangle (abierto / cerrado)", "Cowbell", "Bell", "Caxixi", "Coco",
                                      "Berimbau", "Apito"],
                           "notas": "baião y maracatu suenan mejor con Zabumba y Alfaia en grave_abierto"},
        "808_perc": {"kit": "808_perc", "tipo": "samples (Drum Rack)", "origen": "otro",
                     "permisos": {"entrenar": "sí", "publicar": "sí"},
                     "roles": {r: {"nota": ROLES[r], "sample": s} for r, s in TR808.items()}},
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--bpm", type=float, default=124.0)
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    root = Path(a.dataset)
    for name, k in kit_files().items():
        path = root / "kits" / f"{name}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(k, indent=2, ensure_ascii=False) + "\n")
    perc = root / "midi" / "perc"
    octave = kit_files()["tribe_colombia"]["octava"]
    for i, (name, p) in enumerate(PATTERNS.items()):
        micro = notes_of(p, a.bpm, True, seed=i + 1)
        quant = notes_of(p, a.bpm, False, seed=i + 1)
        write_mid(perc / f"{name}.mid", [(t, d, ROLES[r], v) for t, d, r, v in micro], a.bpm, name)
        write_mid(perc / f"{name}_q.mid", [(t, d, ROLES[r], v) for t, d, r, v in quant], a.bpm, name + " q")
        tribe, missing = to_tribe(micro, octave)
        write_mid(perc / "tribe_colombia" / f"{name}.mid", tribe, a.bpm, name + " tribe")
        ficha = {"patron": name, "estilo": p["estilo"], "bpm": a.bpm, "compases": 2, "roles": list(p["roles"]),
                 "swing": f"{p['swing']} %", "microtiming_ms": p["micro"], "densidad": p["densidad"],
                 "origen": p["origen"], "version_cuantizada": f"{name}_q.mid",
                 "tribe": {"archivo": f"tribe_colombia/{name}.mid", "roles_sin_tribe": missing},
                 "kits_probados": [], "notas": ""}
        fpath = perc / f"{name}.json"
        if fpath.exists():                                  # conservar lo que Juan anotó
            old = json.loads(fpath.read_text())
            for key in ("kits_probados", "notas"):
                ficha[key] = old.get(key, ficha[key])
        fpath.write_text(json.dumps(ficha, indent=2, ensure_ascii=False) + "\n")
        print(f"{name:22} {len(micro):3} golpes · {p['origen']}")
    print(f"{len(PATTERNS)} patrones en {perc}")


if __name__ == "__main__":
    main()

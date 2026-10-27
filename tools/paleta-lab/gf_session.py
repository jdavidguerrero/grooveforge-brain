#!/usr/bin/env python3
"""Ficha de sesión del laboratorio de paletas (apps/docs/produccion/protocolo-captura.md §5).

Crea la ficha de una sesión con el montaje y anota decisiones pareadas y rechazados mientras escuchas:

    python3 tools/paleta-lab/gf_session.py new bass "JD RUN FUNKBASS" "GF POCKET BASS" "HY WOODBASS B" --ganancia "11 h"
    python3 tools/paleta-lab/gf_session.py decide "JD RUN FUNKBASS" "GF POCKET BASS" a "las ghost notes se sienten"
    python3 tools/paleta-lab/gf_session.py reject "HY WOODBASS B" "el golpe de madera tapa la tambora"
    python3 tools/paleta-lab/gf_session.py permiso "LOOP TRIBE 03" tribe-instruments --no-entrenar
    python3 tools/paleta-lab/gf_session.py show

Las fichas van en $GF_DATASET/metadata/sesiones/ (por defecto ~/GROOVEFORGE/GrooveForge_Dataset_v1). Los comandos
después de `new` usan la última sesión creada, o la que indique --sesion.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
from pathlib import Path

ROLES = ["bass", "chords", "pad", "lead", "pluck", "perc", "fx", "layer"]
MAX_CANDIDATES = 3   # más opciones por decisión bloquean (ia-local-refinamiento §3)


def dataset_dir() -> Path:
    return Path(os.environ.get("GF_DATASET") or Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")


def sessions_dir() -> Path:
    d = dataset_dir() / "metadata" / "sesiones"
    d.mkdir(parents=True, exist_ok=True)
    return d


def firmware_commit() -> str:
    repo = Path(__file__).resolve().parents[2]
    try:
        return subprocess.run(["git", "-C", str(repo), "rev-parse", "--short", "HEAD"], capture_output=True,
                              text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def latest_session() -> Path:
    files = sorted(sessions_dir().glob("*.json"), key=lambda f: f.stat().st_mtime)
    if not files:
        sys.exit("No hay sesiones: crear una con `new`.")
    return files[-1]


def load(path: Path) -> dict:
    return json.loads(path.read_text())


def save(path: Path, data: dict):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def session_path(a) -> Path:
    return Path(a.sesion) if a.sesion else latest_session()


def cmd_new(a):
    if a.rol not in ROLES:
        sys.exit(f"rol desconocido: {a.rol} (usar {', '.join(ROLES)})")
    if len(a.candidatos) > MAX_CANDIDATES:
        print(f"aviso: más de {MAX_CANDIDATES} candidatos; mejor elegir de a tres", file=sys.stderr)
    fecha = a.fecha or dt.date.today().isoformat()
    path = sessions_dir() / f"{fecha}_{a.rol}.json"
    n = 2
    while path.exists():                                  # otra sesión del mismo rol el mismo día
        path = sessions_dir() / f"{fecha}_{a.rol}_{n}.json"
        n += 1
    data = {
        "fecha": fecha,
        "montaje": {"ganancia_interfaz": a.ganancia, "bpm": a.bpm, "vol_brain_db": a.vol, "firmware": firmware_commit()},
        "rol": a.rol,
        "candidatos": a.candidatos,
        "decisiones": [],
        "rechazados": [],
        "origen_permisos": {},
        "notas": "",
    }
    save(path, data)
    print(path)


def cmd_decide(a):
    path = session_path(a)
    data = load(path)
    elegido = a.elegido.lower()
    if elegido not in ("a", "b"):
        elegido = "a" if a.elegido == a.a else "b" if a.elegido == a.b else ""
    if not elegido:
        sys.exit("elegido tiene que ser a, b o el nombre de uno de los dos")
    data["decisiones"].append({"a": a.a, "b": a.b, "elegido": elegido, "por_que": a.por_que,
                               "contexto": a.contexto or ""})
    save(path, data)
    print(f"{a.a if elegido == 'a' else a.b} sobre {a.b if elegido == 'a' else a.a}: {a.por_que}")


def cmd_reject(a):
    path = session_path(a)
    data = load(path)
    data["rechazados"].append({"sonido": a.sonido, "por_que": a.por_que})
    save(path, data)
    print(f"rechazado {a.sonido}: {a.por_que}")


def cmd_permiso(a):
    path = session_path(a)
    data = load(path)
    data["origen_permisos"][a.sonido] = {"origen": a.origen, "entrenar": not a.no_entrenar,
                                         "publicar": not a.no_publicar}
    save(path, data)
    print(f"{a.sonido}: {a.origen} · entrenar {'no' if a.no_entrenar else 'sí'} · publicar {'no' if a.no_publicar else 'sí'}")


def cmd_show(a):
    path = session_path(a)
    d = load(path)
    print(f"{path.name} · {d['rol']} · {d['montaje']['bpm']} BPM · firmware {d['montaje']['firmware']}")
    print("candidatos:", ", ".join(d["candidatos"]) or "—")
    for x in d["decisiones"]:
        win, lose = (x["a"], x["b"]) if x["elegido"] == "a" else (x["b"], x["a"])
        print(f"  ✔ {win} > {lose}: {x['por_que']}")
    for x in d["rechazados"]:
        print(f"  ✘ {x['sonido']}: {x['por_que']}")


def main(argv=None):
    p = argparse.ArgumentParser(description="Ficha de sesión del laboratorio de paletas")
    p.add_argument("--sesion", help="ficha a usar (por defecto la última creada)")
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("new", help="crear la ficha de una sesión")
    s.add_argument("rol")
    s.add_argument("candidatos", nargs="*")
    s.add_argument("--fecha")
    s.add_argument("--bpm", type=float, default=124)
    s.add_argument("--vol", type=float, default=0.0, help="VOL de salida del Brain en dB")
    s.add_argument("--ganancia", default="", help="posición de la ganancia de la interfaz")
    s.set_defaults(fn=cmd_new)
    s = sub.add_parser("decide", help="anotar una decisión A contra B")
    s.add_argument("a")
    s.add_argument("b")
    s.add_argument("elegido", help="a, b o el nombre del elegido")
    s.add_argument("por_que")
    s.add_argument("--contexto")
    s.set_defaults(fn=cmd_decide)
    s = sub.add_parser("reject", help="anotar un sonido descartado")
    s.add_argument("sonido")
    s.add_argument("por_que")
    s.set_defaults(fn=cmd_reject)
    s = sub.add_parser("permiso", help="origen y permisos de un sonido o sample")
    s.add_argument("sonido")
    s.add_argument("origen", help="propio · fabrica-brain · tribe-instruments · kick3 · splice · loopcloud · otro")
    s.add_argument("--no-entrenar", action="store_true")
    s.add_argument("--no-publicar", action="store_true")
    s.set_defaults(fn=cmd_permiso)
    sub.add_parser("show", help="resumen de la sesión").set_defaults(fn=cmd_show)
    a = p.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()

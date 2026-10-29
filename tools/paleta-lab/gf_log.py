#!/usr/bin/env python3
"""Registro de eventos v2 del motor de asistencia (apps/docs/produccion/motor-asistencia.md §5).

Un evento por línea en $GF_DATASET/metadata/eventos/<AAAA-MM-DD>.jsonl; solo se agrega, nunca se reescribe. Juan dice
una frase; lo verificable (hashes del snapshot, orden de presentación) lo llena la herramienta y lo demás queda
`unknown` / `missing` sin preguntar.

    gf_log.py pendiente abrir "el bajo se come el kick pero me gusta el groove" --alcance paleta --rol bass
    gf_log.py pendiente estado <id> resolved --frase "quedó"
    gf_log.py audicion <audicion_id> "la B, deja oír la tambora; le quitaría cola" --estado prefer --elegido B \
        --escuchadas A,B,C
    gf_log.py show
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import sys
from pathlib import Path

SCHEMA_VERSION = 2
LISTEN = ("confirmed", "played_detected", "not_auditioned", "unknown")
DECISIONS = ("prefer", "tie", "multiple_useful", "neither", "uncertain", "keep_current")
PENDING_STATES = ("open", "resolved", "postponed", "cancelled")


def dataset_dir() -> Path:
    return Path(os.environ.get("GF_DATASET") or Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")


def events_dir() -> Path:
    return dataset_dir() / "metadata" / "eventos"


def auditions_dir() -> Path:
    return dataset_dir() / "metadata" / "audiciones"


def now() -> dt.datetime:
    return dt.datetime.now().astimezone()


def new_id(prefix: str, seed: str = "") -> str:
    t = now()
    h = hashlib.sha1(f"{t.isoformat()}{seed}".encode()).hexdigest()[:6]
    return f"{t.strftime('%Y%m%dT%H%M%S')}-{prefix}{h}"


def append(event: dict) -> dict:
    """Agrega un evento al .jsonl del día. Nunca reescribe una línea existente."""
    event = {"schema_version": SCHEMA_VERSION, "ts": now().isoformat(timespec="seconds"), **event}
    event.setdefault("event_id", new_id(event["type"][:1]))
    events_dir().mkdir(parents=True, exist_ok=True)
    with (events_dir() / f"{event['ts'][:10]}.jsonl").open("a") as f:
        f.write(json.dumps(event, ensure_ascii=False) + "\n")
    return event


def load_events() -> list[dict]:
    out = []
    for f in sorted(events_dir().glob("*.jsonl")) if events_dir().is_dir() else []:
        out += [json.loads(l) for l in f.read_text().splitlines() if l.strip()]
    return out


def latest_snapshot() -> Path | None:
    snaps = [p for p in (dataset_dir() / "snapshots").glob("*") if (p / "manifest.json").exists()]
    return max(snaps, key=lambda p: json.loads((p / "manifest.json").read_text())["creado"]) if snaps else None


def asset(rel: str, snapshot: Path | None = None) -> dict:
    """Referencia de un archivo del dataset contra el snapshot: `exact` solo si el archivo actual tiene el mismo hash
    que el congelado; si no, el hash actual con `match: unknown` (el snapshot no prueba lo que sonó)."""
    snapshot = snapshot or latest_snapshot()
    cur = dataset_dir() / rel
    sha = hashlib.sha256(cur.read_bytes()).hexdigest() if cur.exists() else None
    ref = {"path": rel, "sha256": sha, "match": "unknown", "snapshot": snapshot.name if snapshot else None}
    if snapshot and sha:
        files = {f["path"]: f["sha256"] for f in json.loads((snapshot / "manifest.json").read_text())["archivos"]}
        frozen = files.get(f"dataset/{rel}") or files.get(f"brain_export/{rel.removeprefix('patches/')}")
        if frozen == sha:
            ref["match"] = "exact"
    return ref


def preferences(events: list[dict]) -> list[tuple[str, str]]:
    """(ganador, perdedor) solo de decisiones `prefer` / `keep_current` sobre opciones escuchadas: `multiple_useful`,
    `tie`, `neither`, `uncertain` y las respuestas `missing` no generan preferencias."""
    pairs = []
    for e in events:
        d = e.get("decision") or {}
        if e.get("type") != "audicion" or e.get("response") != "provided" or d.get("state") not in ("prefer",
                                                                                                  "keep_current"):
            continue
        heard = [o["id"] for o in e["options"] if o.get("listen") == "confirmed"]
        pairs += [(w, l) for w in d["chosen"] for l in heard if l not in d["chosen"]]
    return pairs


def open_pending(phrase: str, scope: str = "paleta", role: str = "", piece: str = "", section: str = "",
                 problem: str = "", keep: str = "", change: str = "", criterion: str = "",
                 source: str = "chat") -> dict:
    return append({"type": "pendiente", "action": "open", "pendiente_id": new_id("p", phrase), "state": "open",
                   "scope": scope, "role": role or None, "pieza": piece or None, "section": section or None,
                   "phrase": phrase, "source": source,
                   "structured": {"problem": problem or "missing", "keep": keep or "missing",
                                  "change": change or "missing", "criterion": criterion or "missing"}})


def set_pending(pid: str, state: str, phrase: str = "") -> dict:
    if state not in PENDING_STATES:
        sys.exit(f"estado: {', '.join(PENDING_STATES)}")
    return append({"type": "pendiente", "action": "state", "pendiente_id": pid, "state": state,
                   "phrase": phrase or None})


def pending_state(events: list[dict]) -> dict[str, dict]:
    out = {}
    for e in events:
        if e["type"] == "pendiente":
            if e["action"] == "open":
                out[e["pendiente_id"]] = dict(e)
            elif e["pendiente_id"] in out:
                out[e["pendiente_id"]]["state"] = e["state"]
    return out


def record_audition(aid: str, phrase: str, state: str | None, chosen: list[str], heard: list[str],
                    change: str = "", inference: str = "") -> dict:
    """La respuesta de Juan a una audición preparada (metadata/audiciones/<id>.json). Las letras se traducen a las
    opciones reales acá; sin frase, `response: missing` y sin decisión."""
    prep = json.loads((auditions_dir() / f"{aid}.json").read_text())
    letters = {o["letter"]: o for o in prep["options"]}
    if state and state not in DECISIONS:
        sys.exit(f"estado: {', '.join(DECISIONS)}")
    for x in chosen + heard:
        if x not in letters:
            sys.exit(f"{x}: las opciones son {', '.join(letters)}")
    options = [{**{k: v for k, v in o.items() if k != "letter"}, "letter": o["letter"],
                "listen": "confirmed" if o["letter"] in heard else "unknown"} for o in prep["options"]]
    event = {"type": "audicion", "audicion_id": aid, "pendiente_id": prep.get("pendiente_id"),
             "pieza": prep.get("pieza"), "context": prep.get("context"), "options": options,
             "presentation": prep.get("presentation"), "firmware_loaded": prep.get("firmware_loaded", "unknown")}
    if not phrase:
        return append({**event, "response": "missing"})
    if state == "prefer" and len(chosen) != 1:
        sys.exit("prefer lleva una sola opción elegida (varias: multiple_useful)")
    if state in ("prefer", "keep_current", "multiple_useful") and not chosen:
        sys.exit(f"{state} necesita --elegido")
    event["response"] = "provided"
    event["decision"] = {"state": state or "uncertain", "chosen": [letters[c]["id"] for c in chosen],
                         "reason": phrase, "change": change or None}
    if inference:
        event["inference"] = {"claim": inference, "status": "hypothesis"}
    return append(event)


def cmd_show(_a):
    ev = load_events()
    print(f"{len(ev)} eventos")
    for pid, p in pending_state(ev).items():
        if p["state"] == "open":
            crit = p["structured"]["criterion"]
            print(f"  [{p['scope']}/{p.get('role') or '-'}] {pid}: {p['phrase']}  · criterio: {crit}")
    for e in ev[-5:]:
        if e["type"] == "audicion":
            d = e.get("decision") or {}
            print(f"  audición {e['audicion_id']}: {e['response']} {d.get('state', '')} {d.get('chosen', '')}")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pendiente")
    psub = p.add_subparsers(dest="accion", required=True)
    o = psub.add_parser("abrir")
    o.add_argument("frase")
    for f in ("alcance", "rol", "pieza", "seccion", "problema", "conservar", "cambiar", "criterio"):
        o.add_argument(f"--{f}", default="paleta" if f == "alcance" else "")
    s = psub.add_parser("estado")
    s.add_argument("id")
    s.add_argument("estado", choices=PENDING_STATES)
    s.add_argument("--frase", default="")
    a = sub.add_parser("audicion")
    a.add_argument("id")
    a.add_argument("frase", nargs="?", default="")
    a.add_argument("--estado", choices=DECISIONS)
    a.add_argument("--elegido", default="")
    a.add_argument("--escuchadas", default="")
    a.add_argument("--cambiar", default="")
    a.add_argument("--inferencia", default="")
    sub.add_parser("show")
    x = ap.parse_args(argv)
    split = lambda s: [t.strip() for t in s.split(",") if t.strip()]
    if x.cmd == "pendiente" and x.accion == "abrir":
        e = open_pending(x.frase, x.alcance, x.rol, x.pieza, x.seccion, x.problema, x.conservar, x.cambiar,
                         x.criterio)
        print(e["pendiente_id"])
    elif x.cmd == "pendiente":
        set_pending(x.id, x.estado, x.frase)
    elif x.cmd == "audicion":
        e = record_audition(x.id, x.frase, x.estado, split(x.elegido), split(x.escuchadas), x.cambiar, x.inferencia)
        print(e["event_id"], e["response"])
    else:
        cmd_show(x)


if __name__ == "__main__":
    main()

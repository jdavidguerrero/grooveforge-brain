#!/usr/bin/env python3
"""Registro de eventos v2 del motor de asistencia (apps/docs/produccion/motor-asistencia.md §5).

Un evento por línea en $GF_DATASET/metadata/eventos/<AAAA-MM-DD>.jsonl; solo se agrega, nunca se reescribe. Juan dice
una frase; lo verificable (hashes del snapshot, orden de presentación) lo llena la herramienta y lo demás queda
`unknown` / `missing` sin preguntar.

    gf_log.py pendiente abrir "el bajo se come el kick pero me gusta el groove" --alcance paleta --rol bass
    gf_log.py pendiente estado <id> resolved --frase "quedó"
    gf_log.py audicion <audicion_id> "la B, deja oír la tambora; le quitaría cola" --estado prefer --elegido B \
        --escuchadas A,B,C
    gf_log.py presencia est-acordes PLUCK "pluck KALIMBA (grabado)" --compases 41-56
    gf_log.py uso est-acordes incorporated "acordes WOOD2" --frase "ya armé el estudio…"
    gf_log.py edicion est-acordes midi/adn/estudio/roll_acelerado_brasil.mid "acortar de 8 a 5 compases" --frase "…"
    gf_log.py regla "en las transiciones uso riser y snare roll" --clase preferencia_declarada --alcance breaks
    gf_log.py regla-estado <event_id> confirmed --frase "sí, así es"
    gf_log.py revision <audicion_id> "sigue siendo la B" --estado prefer --elegido B --escuchadas A,B
    gf_log.py show
    gf_log.py stats
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
USE_STATES = ("incorporated", "kept_after_review", "removed", "not_present", "confirmed_in_context")
RULE_KINDS = ("preferencia_declarada", "tendencia_observada")
RULE_STATES = ("hypothesis", "confirmed", "rejected")


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
                    change: str = "", inference: str = "", kind: str = "audicion") -> dict:
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
    event = {"type": kind, "audicion_id": aid, "pendiente_id": prep.get("pendiente_id"),
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


def piece_ref(piece: str) -> dict:
    """La pieza por id, con el último snapshot (lo que la herramienta puede probar que había)."""
    snap = latest_snapshot()
    return {"id": piece, "snapshot": snap.name if snap else None}


def record_presence(piece: str, track: str, what: str, bars: list[int], muted: bool = False,
                    in_arrangement: bool = True, detected_by: str = "lectura del .als") -> dict:
    """Lo que la herramienta ve en una versión guardada. No dice nada de uso (§6)."""
    return append({"type": "presencia", "pieza": piece_ref(piece), "track": track, "what": what, "bars": bars,
                   "detected_by": detected_by, "in_arrangement": in_arrangement, "muted": muted})


def record_use(piece: str, state: str, what: str, phrase: str = "", reason: str = "") -> dict:
    """Uso confirmado por Juan. Quitar no es rechazo: sin motivo, `reason: missing`."""
    if state not in USE_STATES:
        sys.exit(f"estado: {', '.join(USE_STATES)}")
    e = {"type": "uso", "pieza": piece_ref(piece), "state": state, "what": what, "phrase": phrase or None}
    if state in ("removed", "not_present"):
        e["reason"] = reason or "missing"
    return append(e)


def record_edit(piece: str, derived_from: str, transformation: str, phrase: str = "") -> dict:
    return append({"type": "edicion", "pieza": piece_ref(piece), "origin": "juan_edited",
                   "derived_from": asset(derived_from), "transformation": transformation,
                   "phrase": phrase or None})


def record_rule(claim: str, kind: str, scope: str, evidence: list[str], phrase: str = "") -> dict:
    """Una preferencia declarada vale con la frase de Juan; una tendencia nace `hypothesis` (§8)."""
    if kind not in RULE_KINDS:
        sys.exit(f"clase: {', '.join(RULE_KINDS)}")
    declared = kind == "preferencia_declarada"
    if declared and not phrase:
        sys.exit("una preferencia declarada lleva la frase de Juan (--frase)")
    return append({"type": "regla", "rule_kind": kind, "claim": claim, "scope": scope,
                   "status": "confirmed" if declared else "hypothesis",
                   "confirmed_by": "juan" if declared else None, "evidence": evidence,
                   "phrase": phrase or None, "exceptions": []})


def set_rule(rule_id: str, state: str, phrase: str) -> dict:
    """Solo Juan confirma o descarta una regla, y con su frase (§8)."""
    if state not in RULE_STATES:
        sys.exit(f"estado: {', '.join(RULE_STATES)}")
    if state != "hypothesis" and not phrase:
        sys.exit("confirmar o descartar una regla lleva la frase de Juan (--frase)")
    if rule_id not in {e.get("event_id") for e in load_events() if e["type"] == "regla"}:
        sys.exit(f"{rule_id}: no es una regla del registro")
    return append({"type": "regla", "action": "state", "rule_id": rule_id, "status": state,
                   "confirmed_by": "juan" if state == "confirmed" else None, "phrase": phrase})


def rule_state(events: list[dict]) -> dict[str, dict]:
    out = {}
    for e in events:
        if e["type"] != "regla":
            continue
        if e.get("action") == "state":
            if e["rule_id"] in out:
                out[e["rule_id"]]["status"] = e["status"]
        else:
            out[e["event_id"]] = dict(e)
    return out


def stats(events: list[dict]) -> dict:
    """Conteos de §11 que ya se pueden sacar sin pieza terminada."""
    auds = [e for e in events if e["type"] in ("audicion", "revision")]
    states = {}
    for e in auds:
        k = (e.get("decision") or {}).get("state", "missing")
        states[k] = states.get(k, 0) + 1
    heard = sum(1 for e in auds for o in e["options"] if o.get("listen") == "confirmed")
    chosen = sum(len((e.get("decision") or {}).get("chosen", [])) for e in auds)
    pend = pending_state(events)
    rules = rule_state(events)
    return {"eventos": len(events), "audiciones": len(auds), "migradas": sum(1 for e in auds if e.get("migrated")),
            "decisiones": states, "opciones_escuchadas": heard, "opciones_elegidas": chosen,
            "pares_de_preferencia": len(preferences(events)),
            "pendientes": {s: sum(1 for p in pend.values() if p["state"] == s) for s in PENDING_STATES},
            "usos": sum(1 for e in events if e["type"] == "uso"),
            "reglas": {s: sum(1 for r in rules.values() if r["status"] == s) for s in RULE_STATES}}


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
    r = sub.add_parser("revision")
    for arg in a._actions[1:]:
        r._add_action(arg)
    pr = sub.add_parser("presencia")
    for f in ("pieza", "pista", "que"):
        pr.add_argument(f)
    pr.add_argument("--compases", default="", help="p. ej. 17-24,33-56")
    pr.add_argument("--silenciada", action="store_true")
    pr.add_argument("--fuera-del-arreglo", action="store_true")
    u = sub.add_parser("uso")
    u.add_argument("pieza")
    u.add_argument("estado", choices=USE_STATES)
    u.add_argument("que")
    u.add_argument("--frase", default="")
    u.add_argument("--motivo", default="")
    ed = sub.add_parser("edicion")
    for f in ("pieza", "origen", "transformacion"):
        ed.add_argument(f)
    ed.add_argument("--frase", default="")
    rg = sub.add_parser("regla")
    rg.add_argument("afirmacion")
    rg.add_argument("--clase", choices=RULE_KINDS, default="tendencia_observada")
    rg.add_argument("--alcance", required=True)
    rg.add_argument("--evidencia", default="", help="event_ids separados por coma")
    rg.add_argument("--frase", default="")
    rs = sub.add_parser("regla-estado")
    rs.add_argument("id")
    rs.add_argument("estado", choices=RULE_STATES)
    rs.add_argument("--frase", default="")
    sub.add_parser("show")
    sub.add_parser("stats")
    x = ap.parse_args(argv)
    split = lambda s: [t.strip() for t in s.split(",") if t.strip()]
    if x.cmd == "pendiente" and x.accion == "abrir":
        e = open_pending(x.frase, x.alcance, x.rol, x.pieza, x.seccion, x.problema, x.conservar, x.cambiar,
                         x.criterio)
        print(e["pendiente_id"])
    elif x.cmd == "pendiente":
        set_pending(x.id, x.estado, x.frase)
    elif x.cmd in ("audicion", "revision"):
        e = record_audition(x.id, x.frase, x.estado, split(x.elegido), split(x.escuchadas), x.cambiar, x.inferencia,
                            x.cmd)
        print(e["event_id"], e["response"])
    elif x.cmd == "presencia":
        bars = [int(b) for r in split(x.compases) for b in r.split("-")]
        print(record_presence(x.pieza, x.pista, x.que, bars, x.silenciada, not x.fuera_del_arreglo)["event_id"])
    elif x.cmd == "uso":
        print(record_use(x.pieza, x.estado, x.que, x.frase, x.motivo)["event_id"])
    elif x.cmd == "edicion":
        print(record_edit(x.pieza, x.origen, x.transformacion, x.frase)["event_id"])
    elif x.cmd == "regla":
        print(record_rule(x.afirmacion, x.clase, x.alcance, split(x.evidencia), x.frase)["event_id"])
    elif x.cmd == "regla-estado":
        print(set_rule(x.id, x.estado, x.frase)["event_id"])
    elif x.cmd == "stats":
        print(json.dumps(stats(load_events()), indent=2, ensure_ascii=False))
    else:
        cmd_show(x)


if __name__ == "__main__":
    main()

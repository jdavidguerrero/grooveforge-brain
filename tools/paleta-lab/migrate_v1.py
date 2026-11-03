#!/usr/bin/env python3
"""Migra las fichas v1 del 28-oct (metadata/sesiones/*.json) a eventos v2 (motor-asistencia.md §7).

Lo explícito de la ficha pasa con `response: provided`; lo que no dice queda `unknown` / `missing`. El snapshot del 29
no prueba lo que sonó el 28: presets y clips se enlazan por nombre (`name_match_only`) o quedan `unknown`; nunca
`exact` sin regenerar el MIDI en el commit histórico (no se hace en v1). Idempotente: el event_id sale del `source`,
y un evento cuyo id ya está en el registro no se vuelve a escribir.

    python3 tools/paleta-lab/migrate_v1.py            # muestra lo que escribiría
    python3 tools/paleta-lab/migrate_v1.py --escribir
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gf_log  # noqa: E402

FICHA_GLOB = "2026-10-28_*.json"


def mid_id(source: str) -> str:
    return "m-" + hashlib.sha1(source.encode()).hexdigest()[:12]


def clean(name: str) -> str:
    return re.sub(r"\s*\((capa|capa [^)]*)\)\s*$", "", name).strip()


class Refs:
    """Enlaza nombres de la ficha con assets del primer snapshot: solo por nombre, nunca `exact`."""

    def __init__(self, snapshot: Path | None):
        self.snapshot = snapshot.name if snapshot else None
        paths = [f["path"] for f in json.loads((snapshot / "manifest.json").read_text())["archivos"]] if snapshot else []
        self.patches = {Path(p).stem: p for p in paths if p.startswith("dataset/patches/")}
        self.midis = {Path(p).stem: p for p in paths if p.startswith("dataset/midi/") and p.endswith(".mid")}

    def patch(self, name: str) -> dict:
        slug = re.sub(r"[^a-z0-9]+", "-", clean(name).lower()).strip("-")
        hit = self.patches.get(slug)
        return {"original_ref": name, "snapshot_asset": hit, "snapshot": self.snapshot,
                "match": "name_match_only" if hit else "unknown"}

    def midi(self, name: str) -> dict:
        hits = [p for stem, p in self.midis.items() if stem == name or stem.startswith(name + "__")]
        return {"original_ref": name, "snapshot_asset": hits[0] if len(hits) == 1 else None,
                "candidates": hits if len(hits) > 1 else None, "snapshot": self.snapshot,
                "match": "name_match_only" if len(hits) == 1 else "unknown"}


def audition(source: str, role: str, what: str, names: list[str], heard: set[str], state: str | None,
             chosen: list[str], reason: str, refs: Refs, **extra) -> dict:
    kind = refs.midi if what == "midi" else refs.patch
    opts, seen = [], []
    for n in names:
        if n not in seen:
            seen.append(n)
    for k, n in enumerate(seen):
        opts.append({"id": f"opt{k + 1}", "kind": "option", what: kind(n),
                     "listen": "confirmed" if n in heard or n in chosen else "unknown"})
    by_name = {n: o["id"] for n, o in zip(seen, opts)}
    ev = {"type": "audicion", "event_id": mid_id(source), "migrated": {"source": source, "schema_from": 1},
          "audicion_id": None, "pendiente_id": None, "pieza": None, "role": role,
          "context": extra.pop("context", None), "options": opts,
          "presentation": {"blind": "unknown", "level_matched": "unknown"}, "firmware_loaded": "unknown"}
    if state is None:
        ev["response"] = "missing"
    else:
        ev["response"] = "provided"
        ev["decision"] = {"state": state, "chosen": [by_name[c] for c in chosen], "reason": reason or None,
                          "change": None, **extra}
    return ev


def from_ficha(path: Path, refs: Refs) -> list[dict]:
    f = json.loads(path.read_text())
    role, src = f["rol"], path.name
    out = []
    pre = f.get("prefiltro") or {}
    if pre.get("probados"):
        tried = [clean(n) for n in pre["probados"]]
        passed = pre["pasan"]
        passed = [clean(n) for n in (passed.values() if isinstance(passed, dict) else passed)]
        failed = [clean(n) for n in pre.get("no_pasa", [])]
        out.append(audition(f"{src}#prefiltro", role, "patch", tried + passed, set(tried + passed),
                            "multiple_useful", passed, "pasan el prefiltro", refs,
                            rejected=[*failed] or None, context={"note": pre.get("contexto")}))
        pat = pre.get("patrones")
        if isinstance(pat, dict) and pat.get("preferido"):
            pats = [pat["preferido"], *pat.get("tambien", []), *pat.get("descartados", [])]
            out.append(audition(f"{src}#prefiltro.patrones", role, "midi", pats, set(pats), "prefer",
                                [pat["preferido"]], pat.get("por_que", ""), refs,
                                also_useful=pat.get("tambien"), rejected=pat.get("descartados")))
    for i, d in enumerate(f.get("decisiones", [])):
        a, b, el = clean(d["a"]), clean(d["b"]), d["elegido"]
        state, chosen = (("multiple_useful", [a, b]) if el.startswith("ambos") else ("prefer", [clean(el)]))
        out.append(audition(f"{src}#decisiones[{i}]", role, "patch", [a, b], {a, b}, state, chosen,
                            d.get("por_que", ""), refs, context={"note": d.get("contexto"), "pattern": d.get("patron")}))
    el = f.get("elegidos") or []
    if len(el) > 1:
        names = [clean(e["sonido"]) for e in el]
        rej = [clean(r["sonido"]) for r in f.get("rechazados", [])]
        winners = [clean(e["sonido"]) for e in el if e.get("ganador")]
        state, chosen = ("prefer", winners) if len(winners) == 1 else ("multiple_useful", names)
        reasons = "; ".join(f"{clean(e['sonido'])}: {e['por_que']}" for e in el if e.get("por_que"))
        out.append(audition(f"{src}#elegidos", role, "patch", names + rej, set(names + rej), state, chosen,
                            reasons, refs, also_useful=[n for n in names if n not in chosen] or None,
                            rejected=[f"{clean(r['sonido'])}: {r['por_que']}" for r in f.get("rechazados", [])] or None))
    lab = f.get("lab_leads")
    if lab:
        order = lab["presets_orden"]
        flat = [n for x in order for n in (x if isinstance(x, list) else [x])]
        out.append(audition(f"{src}#lab_leads.presets_orden", role, "patch", flat, set(flat), "prefer", [flat[0]],
                            lab.get("presets_nota", ""), refs, ranking=order))
        out.append(audition(f"{src}#lab_leads.clips_que_gustan", role, "midi", lab["clips_que_gustan"],
                            set(), "multiple_useful", lab["clips_que_gustan"], lab.get("ritmo", ""), refs,
                            rejected=[f"familia {s}: no pasó ningún clip" for s in lab.get("sin_ninguno", [])] or None,
                            warnings=lab.get("avisos"), context={"note": lab.get("contexto"), "als": lab.get("set")}))
        rd = lab["ronda_dialectos"]
        every = sorted({c for v in rd["pasan"].values() for c in v} | set(rd["nunca"]))
        for dialect, passed in rd["pasan"].items():
            out.append(audition(f"{src}#lab_leads.ronda_dialectos.pasan.{dialect}", role, "midi", every, set(),
                                "multiple_useful", passed, rd.get("juan", ""), refs,
                                context={"dialect": dialect, "als": rd.get("set"), "sound": rd.get("sonido")}))
        dc = lab.get("dosis_color")
        if dc:
            out.append(audition(f"{src}#lab_leads.dosis_color", role, "midi", [], set(), "neither", [], dc["juan"],
                                refs, context={"als": dc.get("set")}))
    lab = f.get("lab_acordes")
    if lab:
        clips = lab["clips_que_gustan"]
        out.append(audition(f"{src}#lab_acordes.clips_que_gustan", "chords", "midi", clips + lab.get("sin_marcar", []),
                            set(), "multiple_useful", clips, "", refs, context={"als": lab.get("set")}))
        dx7 = ["GF HOUSE KEYS", "E.PIANO", "JD VELVET RHODES"]
        out.append(audition(f"{src}#lab_acordes.presets", "chords", "patch", dx7, set(dx7), "neither", [],
                            lab["presets"], refs))
        jk = lab["jd_keys"]
        out.append(audition(f"{src}#lab_acordes.jd_keys", "chords", "patch", jk["probados"], set(jk["probados"]),
                            "neither", [], jk["juan"], refs))
        rc = jk.get("ronda_capas") or {}
        if rc:
            out.append(audition(f"{src}#lab_acordes.jd_keys.ronda_capas", "chords", "patch",
                                ["JD PLK WOOD", "JD SUS DARK"], {"JD PLK WOOD", "JD SUS DARK"}, "multiple_useful",
                                ["JD PLK WOOD", "JD SUS DARK"], f"pluck: {rc['pluck']}; sostenido: {rc['sostenido']}",
                                refs))
    return out


def lead_dna_rule(events: list[dict]) -> dict | None:
    """Tendencia observada (hipótesis): clips de lead que pasaron en ≥4 de los 5 dialectos."""
    rounds = [e for e in events if ".ronda_dialectos.pasan." in e["migrated"]["source"]]
    if not rounds:
        return None
    count = Counter()
    for e in rounds:
        names = {o["id"]: o["midi"]["original_ref"] for o in e["options"]}
        count.update(names[c] for c in e["decision"]["chosen"])
    strong = sorted(c for c, n in count.items() if n >= 4)
    source = "2026-10-28_lead.json#lab_leads.ronda_dialectos (regla derivada)"
    return {"type": "regla", "event_id": mid_id(source), "migrated": {"source": source, "schema_from": 1},
            "rule_kind": "tendencia_observada", "status": "hypothesis",
            "claim": f"con solo kick + kit del dialecto + dosis de Tribe, pasaron en ≥4 de 5 dialectos: {', '.join(strong)}",
            "scope": "lead, ronda de dialectos del 28-oct (los ✓ no son de un solo preset)",
            "counts": dict(count.most_common()), "evidence": [e["event_id"] for e in rounds], "exceptions": [],
            "confirmed_by": None}


def build(dataset: Path | None = None) -> list[dict]:
    meta = (dataset or gf_log.dataset_dir()) / "metadata" / "sesiones"
    snaps = sorted((gf_log.dataset_dir() / "snapshots").glob("*/manifest.json"),
                   key=lambda p: json.loads(p.read_text())["creado"])
    refs = Refs(snaps[0].parent if snaps else None)
    events = [e for p in sorted(meta.glob(FICHA_GLOB)) for e in from_ficha(p, refs)]
    rule = lead_dna_rule(events)
    return events + ([rule] if rule else [])


def migrate(write: bool) -> tuple[list[dict], list[dict]]:
    have = {e.get("event_id") for e in gf_log.load_events()}
    events = build()
    new = [e for e in events if e["event_id"] not in have]
    if write:
        for e in new:
            gf_log.append(e)
    return events, new


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--escribir", action="store_true", help="sin esto solo muestra el plan")
    a = ap.parse_args(argv)
    events, new = migrate(a.escribir)
    for e in events:
        d = e.get("decision") or {}
        match = Counter(o.get("patch", o.get("midi", {})).get("match") for o in e.get("options", []))
        mark = "nuevo" if e in new else "ya está"
        print(f"  [{mark}] {e['migrated']['source']}: {e['type']} {d.get('state', e.get('status', ''))} "
              f"{dict(match)}")
    print(f"{len(new)} de {len(events)} eventos {'escritos' if a.escribir else 'por escribir'}")


if __name__ == "__main__":
    main()

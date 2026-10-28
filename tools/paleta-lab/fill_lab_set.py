#!/usr/bin/env python3
"""Carga los clips de midi/adn/ en el set de Live de Juan (apps/docs/produccion/sesion-paleta-adn.md §5).

Toma el .als que Juan creó con las pistas KICK, PERC, TOP, BRAIN MIDI y BRAIN AUDIO (así el archivo es de su versión
de Live) y llena los clip slots: escenas 1–6 = contextos A tierra · B noche · C fiesta · D orgánico suave ·
E hipnótico · F melódico (KICK, PERC, TOP); desde la escena 7, un patrón del Brain por escena en BRAIN MIDI. Agrega
escenas si faltan, pone el tempo en 124 y guarda una copia del original (.als.bak) la primera vez. Los CC de las
capas no van en el .als (son envolventes con referencias internas de Live): para MORPH y CC 18 se arrastra el .mid de
capas/ o se elige la capa en el Brain.

    python3 tools/paleta-lab/fill_lab_set.py "<ruta>/GF_Paleta_Lab.als"

Con --leads **agrega** el lab de leads (lab-leads.md §7) debajo de lo que ya tiene el set, sin tocarlo; el set
necesita además una pista ACORDES (un instrumento de Live). Primero la mixta A de las 12 escalas (eólica primero:
rondas 1–3) y luego las otras rítmicas de cada escala (ronda 4); en cada escena, el kick, el lead en BRAIN MIDI y la
progresión de su escala en ACORDES. Volver a correrlo agrega otra vez: se corre una sola vez por set.

Con --dialectos <ficha.json> agrega la ronda de dialectos (lab-leads.md §7, ronda 3): los clips que le gustaron a Juan
(campo lab_leads.clips_que_gustan de la ficha) sobre la percusión de cada dialecto, un bloque por dialecto; en cada
escena kick, perc, top, lead y acordes.

    python3 tools/paleta-lab/fill_lab_set.py --dialectos <ficha.json> "<ruta>/GF_Lead_Dialectos.als"

    python3 tools/paleta-lab/fill_lab_set.py --leads "<ruta>/GF_Lead_Lab.als"
"""
from __future__ import annotations

import argparse
import copy
import gzip
import json
import os
import shutil
import struct
import xml.etree.ElementTree as ET
from pathlib import Path

from make_lead_lab import SCALES

TPQ = 480
CONTEXTS = [("A_tierra", "A · tierra"), ("B_noche", "B · noche"), ("C_fiesta", "C · fiesta"),
            ("D_organico_suave", "D · orgánico suave"), ("E_hipnotico", "E · hipnótico"), ("F_melodico", "F · melódico")]
ROLE_ORDER = ("bass", "perc", "lead", "pluck", "chords", "pad", "build", "capas")   # el orden de la sesión (§1b)
COLORS = {"bass": 14, "perc": 2, "lead": 25, "pluck": 20, "chords": 9, "pad": 17, "build": 5, "capas": 22,
          "KICK": 69, "PERC": 26, "TOP": 26}

CLIP = """<MidiClip Id="0" Time="0"><LomId Value="0" /><LomIdView Value="0" /><CurrentStart Value="0" />
<CurrentEnd Value="0" /><Loop><LoopStart Value="0" /><LoopEnd Value="0" /><StartRelative Value="0" />
<LoopOn Value="true" /><OutMarker Value="0" /><HiddenLoopStart Value="0" /><HiddenLoopEnd Value="0" /></Loop>
<Name Value="" /><Annotation Value="" /><Color Value="0" /><LaunchMode Value="0" /><LaunchQuantisation Value="0" />
<TimeSignature><TimeSignatures><RemoteableTimeSignature Id="0"><Numerator Value="4" /><Denominator Value="4" />
<Time Value="0" /></RemoteableTimeSignature></TimeSignatures></TimeSignature><Envelopes><Envelopes /></Envelopes>
<ScrollerTimePreserver><LeftTime Value="0" /><RightTime Value="0" /></ScrollerTimePreserver><TimeSelection>
<AnchorTime Value="0" /><OtherTime Value="0" /></TimeSelection><Legato Value="false" /><Ram Value="false" />
<GrooveSettings><GrooveId Value="-1" /></GrooveSettings><Disabled Value="false" /><VelocityAmount Value="0" />
<FollowAction><FollowTime Value="4" /><IsLinked Value="true" /><LoopIterations Value="1" /><FollowActionA Value="4" />
<FollowActionB Value="0" /><FollowChanceA Value="100" /><FollowChanceB Value="0" /><JumpIndexA Value="1" />
<JumpIndexB Value="1" /><FollowActionEnabled Value="false" /></FollowAction><Grid><FixedNumerator Value="1" />
<FixedDenominator Value="16" /><GridIntervalPixel Value="20" /><Ntoles Value="2" /><SnapToGrid Value="true" />
<Fixed Value="false" /></Grid><FreezeStart Value="0" /><FreezeEnd Value="0" /><IsWarped Value="true" />
<TakeId Value="-1" /><IsInKey Value="true" /><ScaleInformation><Root Value="0" /><Name Value="0" /></ScaleInformation>
<AutomationEnvelopesListWrapper LomId="0" /><Notes><KeyTracks /><PerNoteEventStore><EventLists /></PerNoteEventStore>
<NoteProbabilityGroups /><ProbabilityGroupIdGenerator><NextId Value="1" /></ProbabilityGroupIdGenerator>
<NoteIdGenerator><NextId Value="1" /></NoteIdGenerator></Notes><BankSelectCoarse Value="-1" />
<BankSelectFine Value="-1" /><ProgramChange Value="-1" /><NoteEditorFoldInZoom Value="-1" />
<NoteEditorFoldInScroll Value="0" /><NoteEditorFoldOutZoom Value="1152" /><NoteEditorFoldOutScroll Value="-959" />
<NoteEditorFoldScaleZoom Value="-1" /><NoteEditorFoldScaleScroll Value="0" /><NoteSpellingPreference Value="0" />
<AccidentalSpellingPreference Value="3" /><PreferFlatRootNote Value="false" /><ExpressionGrid>
<FixedNumerator Value="1" /><FixedDenominator Value="16" /><GridIntervalPixel Value="20" /><Ntoles Value="2" />
<SnapToGrid Value="false" /><Fixed Value="false" /></ExpressionGrid></MidiClip>"""


def read_mid(path: Path) -> list[tuple[float, float, int, int]]:
    """(inicio en negras, duración en negras, nota, velocity) de un MIDI tipo 0 / pista única."""
    data = path.read_bytes()
    tpq = struct.unpack(">H", data[12:14])[0]
    n = struct.unpack(">I", data[18:22])[0]
    trk, i, t, on, out, status = data[22:22 + n], 0, 0, {}, [], 0
    while i < len(trk):
        d = 0
        while True:
            b = trk[i]; i += 1
            d = (d << 7) | (b & 0x7F)
            if not b & 0x80:
                break
        t += d
        if trk[i] & 0x80:
            status = trk[i]; i += 1
        if status == 0xFF:
            i += 2 + trk[i + 1]
            continue
        kind = status & 0xF0
        if kind in (0x80, 0x90):
            note, vel = trk[i], trk[i + 1]
            if kind == 0x90 and vel:
                on.setdefault(note, []).append((t, vel))
            elif on.get(note):
                start, v = on[note].pop(0)
                out.append((start / tpq, (t - start) / tpq, note, v))
            i += 2
        elif kind in (0xC0, 0xD0):
            i += 1
        else:
            i += 2
    return sorted(out)


def make_clip(notes: list, name: str, color: int, clip_id: int) -> ET.Element:
    c = ET.fromstring(CLIP)
    c.set("Id", str(clip_id))
    end = max(4.0, -(-max(s + d for s, d, *_ in notes) // 4) * 4)       # compases enteros
    for tag in ("CurrentEnd", "Loop/LoopEnd", "Loop/OutMarker", "Loop/HiddenLoopEnd"):
        c.find(tag).set("Value", f"{end:g}")
    c.find("Name").set("Value", name)
    c.find("Color").set("Value", str(color))
    keys, nid = c.find("Notes/KeyTracks"), 1
    for k, key in enumerate(sorted({n for _, _, n, _ in notes})):
        kt = ET.SubElement(keys, "KeyTrack", Id=str(k))
        ev = ET.SubElement(kt, "Notes")
        for s, d, n, v in notes:
            if n == key:
                ET.SubElement(ev, "MidiNoteEvent", Time=f"{s:.6f}", Duration=f"{d:.6f}", Velocity=str(v),
                              OffVelocity="0", NoteId=str(nid))
                nid += 1
        ET.SubElement(kt, "MidiKey", Value=str(key))
    c.find("Notes/NoteIdGenerator/NextId").set("Value", str(nid))
    return c


def clip_lists(track: ET.Element) -> list[ET.Element]:
    return [e for e in track.iter("ClipSlotList")]


def ensure_scenes(ls: ET.Element, count: int):
    scenes = ls.find("Scenes")
    while len(scenes) < count:
        s = copy.deepcopy(scenes[-1])
        s.set("Id", str(max(int(x.get("Id")) for x in scenes) + 1))
        s.find("Name").set("Value", "")
        scenes.append(s)
        for tr in ls.find("Tracks"):
            for lst in clip_lists(tr):
                if len(lst):
                    slot = copy.deepcopy(lst[-1])
                    slot.set("Id", str(max(int(x.get("Id")) for x in lst) + 1))
                    slot.find("ClipSlot/Value").clear()
                    lst.append(slot)


def put(track: ET.Element, row: int, clip: ET.Element):
    value = track.find(".//MainSequencer/ClipSlotList")[row].find("ClipSlot/Value")
    value.clear()
    if clip is not None:
        value.append(clip)


def brain_clips(adn: Path) -> list[tuple[str, str, Path]]:
    out = []
    for role in ROLE_ORDER:
        for f in sorted((adn / role).glob("*.mid")):
            out.append((role, f.stem, f))
    return out


ORDER = {sc.name: i for i, sc in enumerate(SCALES)}                     # eólica primero (ronda 1)
LEAD_RHYTHMS = ("mixta_a", "corta", "mixta_b", "larga")


def lead_lab_rows(lab: Path) -> list[tuple[str, Path, Path]]:
    """(nombre de escena, lead, acordes): la mixta A de todas las escalas y después las demás rítmicas por escala."""
    by_scale = {}
    for f in sorted(lab.glob("*/*.mid")):
        scale, rhythm, *dial = f.stem.split("__")
        if rhythm != "acordes":
            by_scale.setdefault((f.parent.name, scale), {})[rhythm] = (f, dial[0].replace("_", " ") if dial else "")
    rows = []
    for rhythms in (LEAD_RHYTHMS[:1], LEAD_RHYTHMS[1:]):
        for (family, scale), clips in sorted(by_scale.items(), key=lambda kv: (kv[0][0], ORDER.get(kv[0][1], 99))):
            for r in rhythms:
                if r in clips:
                    f, dial = clips[r]
                    name = f"{family.split('_')[0]} {scale} · {r} · {dial}"
                    rows.append((name, f, f.parent / f"{scale}__acordes.mid"))
    return rows


# Percusión de cada dialecto, armada con los contextos de midi/adn/contexto/: (de dónde sale la perc, de dónde el top).
# afro y organic se arman puros cruzando A y C / A y D; disco no tiene perc propia en la selección y usa C (afro + disco).
DIALECTS = {"afro": ("C_fiesta", "A_tierra"), "organic": ("A_tierra", "D_organico_suave"),
            "melodic": ("F_melodico", "F_melodico"), "indie": ("E_hipnotico", "E_hipnotico"),
            "disco": ("C_fiesta", "C_fiesta")}


def dialect_rows(lab: Path, liked: list[str]) -> list[tuple[str, str, Path, Path]]:
    """(dialecto, nombre de escena, lead, acordes): un bloque por dialecto con todos los clips que gustaron, en el orden
    de las escalas (familias 1–4) y de las rítmicas."""
    files = {f.stem: f for f in lab.glob("*/*.mid")}
    missing = [c for c in liked if c not in files]
    if missing:
        raise SystemExit(f"no encuentro en {lab}: {', '.join(missing)}")
    key = lambda c: (ORDER.get(c.split("__")[0], 99), LEAD_RHYTHMS.index(c.split("__")[1]))   # noqa: E731
    rows = []
    for dialect in DIALECTS:
        for c in sorted(liked, key=key):
            scale, rhythm = c.split("__")[:2]
            rows.append((dialect, f"{dialect} · {scale} · {rhythm}", files[c], files[c].parent / f"{scale}__acordes.mid"))
    return rows


def fill(als: Path, adn: Path, bpm: float = 124.0, leads: bool = False, liked: list[str] | None = None) -> int:
    root = ET.fromstring(gzip.decompress(als.read_bytes()))
    ls = root.find("LiveSet")
    tracks = {t.find("Name/EffectiveName").get("Value"): t for t in ls.find("Tracks")}
    missing = ({"KICK", "PERC", "TOP", "BRAIN MIDI"} | ({"ACORDES"} if leads or liked else set())) - set(tracks)
    if missing:
        raise SystemExit(f"faltan pistas en el set: {', '.join(sorted(missing))}")
    scenes = ls.find("Scenes")
    cid = 1 + max((int(c.get("Id")) for c in root.iter("MidiClip")), default=0)   # sin chocar con los que ya hay
    cid0 = cid
    kick = read_mid(adn / "contexto" / "kick_4x4.mid")
    if liked:
        start = first_free_row(ls)
        rows = dialect_rows(adn / "lead" / "lab", liked)
        ensure_scenes(ls, start + len(rows))
        for k, (dialect, scene, f, chords) in enumerate(rows):
            row = start + k
            perc_ctx, top_ctx = DIALECTS[dialect]
            scenes[row].find("Name").set("Value", scene)
            put(tracks["KICK"], row, make_clip(kick, "kick 4x4", COLORS["KICK"], cid)); cid += 1
            for part, track, ctx in (("perc", "PERC", perc_ctx), ("top", "TOP", top_ctx)):
                src = adn / "contexto" / ctx / f"{part}_tribe.mid"
                put(tracks[track], row, make_clip(read_mid(src), f"{dialect} {part}", COLORS[track], cid)); cid += 1
            put(tracks["BRAIN MIDI"], row, make_clip(read_mid(f), f.stem, COLORS["lead"], cid)); cid += 1
            put(tracks["ACORDES"], row, make_clip(read_mid(chords), chords.stem, COLORS["chords"], cid)); cid += 1
        return save(als, root, ls, bpm, cid - cid0)
    if leads:
        start = first_free_row(ls)
        rows = lead_lab_rows(adn / "lead" / "lab")
        ensure_scenes(ls, start + len(rows))
        for k, (scene, f, chords) in enumerate(rows):
            row = start + k
            scenes[row].find("Name").set("Value", scene)
            put(tracks["KICK"], row, make_clip(kick, "kick 4x4", COLORS["KICK"], cid)); cid += 1
            put(tracks["BRAIN MIDI"], row, make_clip(read_mid(f), f.stem, COLORS["lead"], cid)); cid += 1
            put(tracks["ACORDES"], row, make_clip(read_mid(chords), chords.stem, COLORS["chords"], cid)); cid += 1
        return save(als, root, ls, bpm, cid - cid0)
    brain = brain_clips(adn)
    ensure_scenes(ls, len(CONTEXTS) + len(brain))
    for row, (folder, label) in enumerate(CONTEXTS):
        scenes[row].find("Name").set("Value", label)
        put(tracks["KICK"], row, make_clip(kick, "kick 4x4", COLORS["KICK"], cid)); cid += 1
        for t in ("BRAIN MIDI", "ACORDES"):                       # las escenas de contexto no llevan Brain
            if t in tracks:
                put(tracks[t], row, None)
        for part, track in (("perc", "PERC"), ("top", "TOP")):
            src = adn / "contexto" / folder / f"{part}_tribe.mid"
            if not src.exists():                                  # D · orgánico suave no tiene perc
                put(tracks[track], row, None)
                continue
            put(tracks[track], row, make_clip(read_mid(src), f"{label} {part}", COLORS[track], cid)); cid += 1
    for k, (role, name, f) in enumerate(brain):
        row = len(CONTEXTS) + k
        scenes[row].find("Name").set("Value", role)
        put(tracks["BRAIN MIDI"], row, make_clip(read_mid(f), name, COLORS[role], cid)); cid += 1
    return save(als, root, ls, bpm, cid - cid0)


def first_free_row(ls: ET.Element) -> int:
    """La primera escena debajo del último clip de cualquier pista."""
    last = -1
    for tr in ls.find("Tracks"):
        slots = tr.find(".//MainSequencer/ClipSlotList")
        for i, slot in enumerate(slots if slots is not None else []):
            if len(slot.find("ClipSlot/Value")):
                last = max(last, i)
    return last + 1


def save(als: Path, root: ET.Element, ls: ET.Element, bpm: float, count: int) -> int:
    tempo = ls.find(".//MainTrack//Tempo/Manual")
    if tempo is not None:
        tempo.set("Value", f"{bpm:g}")
    bak = als.with_suffix(".als.bak")
    if not bak.exists():
        shutil.copy2(als, bak)
    xml = '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(root, encoding="unicode")
    als.write_bytes(gzip.compress(xml.encode("utf-8")))
    return count


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("als")
    ap.add_argument("--leads", action="store_true", help="cargar el lab de leads (necesita la pista ACORDES)")
    ap.add_argument("--dialectos", metavar="FICHA", help="agregar la ronda de dialectos con los clips de la ficha")
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    liked = json.loads(Path(a.dialectos).read_text())["lab_leads"]["clips_que_gustan"] if a.dialectos else None
    n = fill(Path(a.als), Path(a.dataset) / "midi" / "adn", leads=a.leads, liked=liked)
    print(f"{n} clips cargados en {a.als} (original en .als.bak)")


if __name__ == "__main__":
    main()

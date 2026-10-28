#!/usr/bin/env python3
"""Carga los clips de midi/adn/ en el set de Live de Juan (apps/docs/produccion/sesion-paleta-adn.md §5).

Toma el .als que Juan creó con las pistas KICK, PERC, TOP, BRAIN MIDI y BRAIN AUDIO (así el archivo es de su versión
de Live) y llena los clip slots: escenas 1–3 = contextos A tierra · B noche · C fiesta (KICK, PERC, TOP); desde la
escena 4, un patrón del Brain por escena en BRAIN MIDI. Agrega escenas si faltan, pone el tempo en 124 y guarda una
copia del original (.als.bak) la primera vez. Los CC de las capas no van en el .als (son envolventes con referencias
internas de Live): para MORPH y CC 18 se arrastra el .mid de capas/ o se elige la capa en el Brain.

    python3 tools/paleta-lab/fill_lab_set.py "<ruta>/GF_Paleta_Lab.als"
"""
from __future__ import annotations

import argparse
import copy
import gzip
import os
import shutil
import struct
import xml.etree.ElementTree as ET
from pathlib import Path

TPQ = 480
CONTEXTS = [("A_tierra", "A · tierra"), ("B_noche", "B · noche"), ("C_fiesta", "C · fiesta")]
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
    value.append(clip)


def brain_clips(adn: Path) -> list[tuple[str, str, Path]]:
    out = []
    for role in ROLE_ORDER:
        for f in sorted((adn / role).glob("*.mid")):
            out.append((role, f.stem, f))
    return out


def fill(als: Path, adn: Path, bpm: float = 124.0) -> int:
    root = ET.fromstring(gzip.decompress(als.read_bytes()))
    ls = root.find("LiveSet")
    tracks = {t.find("Name/EffectiveName").get("Value"): t for t in ls.find("Tracks")}
    missing = {"KICK", "PERC", "TOP", "BRAIN MIDI"} - set(tracks)
    if missing:
        raise SystemExit(f"faltan pistas en el set: {', '.join(sorted(missing))}")
    brain = brain_clips(adn)
    ensure_scenes(ls, len(CONTEXTS) + len(brain))
    scenes, cid = ls.find("Scenes"), 1
    kick = read_mid(adn / "contexto" / "kick_4x4.mid")
    for row, (folder, label) in enumerate(CONTEXTS):
        scenes[row].find("Name").set("Value", label)
        put(tracks["KICK"], row, make_clip(kick, "kick 4x4", COLORS["KICK"], cid)); cid += 1
        for part, track in (("perc", "PERC"), ("top", "TOP")):
            notes = read_mid(adn / "contexto" / folder / f"{part}_tribe.mid")
            put(tracks[track], row, make_clip(notes, f"{label} {part}", COLORS[track], cid)); cid += 1
    for k, (role, name, f) in enumerate(brain):
        row = len(CONTEXTS) + k
        scenes[row].find("Name").set("Value", role)
        put(tracks["BRAIN MIDI"], row, make_clip(read_mid(f), name, COLORS[role], cid)); cid += 1
    tempo = ls.find(".//MainTrack//Tempo/Manual")
    if tempo is not None:
        tempo.set("Value", f"{bpm:g}")
    bak = als.with_suffix(".als.bak")
    if not bak.exists():
        shutil.copy2(als, bak)
    xml = '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(root, encoding="unicode")
    als.write_bytes(gzip.compress(xml.encode("utf-8")))
    return cid - 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("als")
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    n = fill(Path(a.als), Path(a.dataset) / "midi" / "adn")
    print(f"{n} clips cargados en {a.als} (original en .als.bak)")


if __name__ == "__main__":
    main()

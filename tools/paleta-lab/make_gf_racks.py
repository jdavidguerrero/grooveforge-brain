#!/usr/bin/env python3
"""Drum Racks GF a partir de los racks de Juan (apps/docs/produccion/percusion-groove.md §7).

Lee sus racks de Ableton (.adg = XML con gzip), pone cada sample en la nota de su rol del GF Perc Map (C1 = 36) y lo
que no tiene rol en las notas de arriba (51 en adelante), y guarda racks NUEVOS ("GF …"): los suyos no se tocan. Al de
djembe le agrega los samples que le faltaban (slap, xeke, tchekere, long, bass con xeke) clonando un pad; los copia a
la User Library como hace Live al importar.

    python3 tools/paleta-lab/make_gf_racks.py           # escribe en la User Library de Live y en el dataset
    python3 tools/paleta-lab/make_gf_racks.py --dry-run # solo muestra el mapa
"""
from __future__ import annotations

import argparse
import gzip
import os
import re
import shutil
import struct
import xml.etree.ElementTree as ET
from pathlib import Path

from make_perc_patterns import ROLES

HOME = Path.home()
RACKS = HOME / "Music/Ableton/User Library/Presets/Instruments/Drum Rack"
IMPORTED = HOME / "Music/Ableton/User Library/Samples/Imported"
ICLOUD_BANK = HOME / "Library/Mobile Documents/com~apple~CloudDocs/Documents/Banco de Sonidos"

# Rol → nombre del pad (el del sample, o el comienzo del nombre). * = no estaba en el rack: se clona.
KITS = {
    "GF Djembe Dunun": dict(source="Djembe Assembly.adg", clone_from="Djembe agudo", add=[
        "Djembe slap seco", "Xeke Xeke", "Tchekere cabaz", "Djembe long", "Djembe bass con xeke"], roles={
        "grave_abierto": "dundumba ab", "grave_apagado": "Djembe bass", "medio_abierto": "Sangban ab",
        "medio_apagado": "Sangban pres", "agudo_abierto": "kenke ab", "agudo_slap": "Djembe slap seco",
        "aro_madera": "kenke pres", "sacudido_corto": "Xeke Xeke", "sacudido_largo": "Tchekere cabaz",
        "campana_grave": "campana sang", "campana_aguda": "campana kenke", "caja": "Djembe tono",
        "pandereta": "Djembe agudo", "efecto": "Djembe long"}),
    "GF Brasil": dict(source="Brazilian Percussion Rack.adg", roles={
        "grave_abierto": "Surdo one shot -", "grave_apagado": "Surdo one shot (2)",
        "medio_abierto": "Atabaque one shot -", "medio_apagado": "Zabumba one shot -",
        "agudo_abierto": "Repinique one shot -", "agudo_slap": "Tamborim one shot -", "aro_madera": "Coco one shot -",
        "sacudido_corto": "Caxixi one shot", "sacudido_largo": "Ganza one shot -", "raspado": "Reco reco one shot",
        "campana_grave": "Agogo one shot", "campana_aguda": "Agogo (whistle) one shot (2)",
        "caja": "Caixa (snare) one shot -", "pandereta": "Pandeiro one shot -", "efecto": "Apito (whistle) one shot -"}),
    "GF 808 Perc": dict(source="808 Core Kit.adg", roles={
        "grave_abierto": "Tom-808-Low", "grave_apagado": "Tom-808-Mid", "medio_abierto": "Conga-808-Low",
        "medio_apagado": "Conga-808-Mid", "agudo_abierto": "Conga-808-Hi", "agudo_slap": "Tom-808-Hi",
        "aro_madera": "Rim-808", "sacudido_corto": "Maracas-808", "campana_grave": "Cowbell-808",
        "campana_aguda": "Clave-808", "caja": "Snare 808", "pandereta": "Clap-808", "efecto": "Cymbal 808"}),
}

def pad_name(branch: str) -> str:
    m = re.search(r'<MultiSamplePart.*?<Name Value="([^"]*)"', branch, re.S)
    return m.group(1) if m else ""


def wav_frames(path: Path) -> tuple[int, int]:
    """(frames, sample rate) leyendo los chunks fmt y data del WAV."""
    data = path.read_bytes()
    i, rate, block, frames = 12, 44100, 4, 0
    while i + 8 <= len(data):
        cid, size = data[i:i + 4], struct.unpack("<I", data[i + 4:i + 8])[0]
        if cid == b"fmt ":
            rate = struct.unpack("<I", data[i + 12:i + 16])[0]
            block = struct.unpack("<H", data[i + 20:i + 22])[0]
        elif cid == b"data":
            frames = size // max(block, 1)
        i += 8 + size + (size & 1)
    return frames, rate


def clone_branch(template: str, old: str, new: str, src: Path, new_id: int) -> str:
    """Copia un pad cambiando el sample: nombre, rutas, tamaño, duración y frecuencia."""
    dur = int(re.search(r'<DefaultDuration Value="(\d+)"', template).group(1))
    frames, rate = wav_frames(src)
    b = template.replace(old, new)
    b = re.sub(r'<DrumBranchPreset Id="\d+">', f'<DrumBranchPreset Id="{new_id}">', b, count=1)
    b = re.sub(r'<OriginalFileSize Value="[1-9]\d*"', f'<OriginalFileSize Value="{src.stat().st_size}"', b)
    b = re.sub(r'<OriginalCrc Value="[1-9]\d*"', '<OriginalCrc Value="0"', b)
    b = re.sub(r'<LastModDate Value="\d+"', f'<LastModDate Value="{int(src.stat().st_mtime)}"', b)
    b = b.replace(f'Value="{dur}"', f'Value="{frames}"').replace(f'Value="{dur - 1}"', f'Value="{frames - 1}"')
    b = re.sub(r'<DefaultSampleRate Value="\d+"', f'<DefaultSampleRate Value="{rate}"', b)
    return b


def build(name: str, kit: dict, dry: bool) -> list[tuple[int, str, str]]:
    root = ET.fromstring(gzip.decompress((RACKS / kit["source"]).read_bytes()).decode("utf-8"))
    preset = root.find("GroupDevicePreset")
    pads = preset.find("BranchPresets")                     # los pads del rack (hijos directos del preset)
    branches = list(pads)
    as_text = lambda e: ET.tostring(e, encoding="unicode")
    if kit.get("add"):
        tpl = as_text(next(b for b in branches if pad_name(as_text(b)) == kit["clone_from"]))
        next_id = max(int(b.get("Id")) for b in branches) + 1
        for extra in kit["add"]:
            src = ICLOUD_BANK / f"{extra}.wav"
            dst = IMPORTED / f"{extra}.wav"
            if not dry and not dst.exists():
                shutil.copy2(src, dst)                     # como importa Live: a la User Library
            branches.append(ET.fromstring(clone_branch(tpl, kit["clone_from"], extra, src if dry else dst, next_id)))
            next_id += 1
    names = [pad_name(as_text(b)) for b in branches]
    used, plan, out = set(), {}, []
    for role, prefix in kit["roles"].items():
        i = next((i for i, n in enumerate(names) if i not in plan and n.startswith(prefix)), None)
        if i is None:
            raise SystemExit(f"{name}: no encuentro el pad de {role} ({prefix})")
        plan[i] = (ROLES[role], role)
        used.add(ROLES[role])
    free = (n for n in range(51, 128) if n not in used)
    notes = []
    for i, b in enumerate(branches):
        note, role = plan.get(i) or (next(free), "extra")
        b.find("ZoneSettings/ReceivingNote").set("Value", str(128 - note))   # Live guarda 128 − nota
        notes.append(note)
        out.append((note, role, names[i]))
    for b in list(pads):
        pads.remove(b)
    for _, b in sorted(zip(notes, branches), key=lambda t: t[0]):
        pads.append(b)
    root.find(".//DrumGroupDevice/UserName").set("Value", name)
    if not dry:
        data = gzip.compress(('<?xml version="1.0" encoding="UTF-8"?>\n' + as_text(root)).encode("utf-8"))
        (RACKS / f"{name}.adg").write_bytes(data)
        ds = Path(os.environ.get("GF_DATASET", HOME / "GROOVEFORGE" / "GrooveForge_Dataset_v1")) / "kits" / "racks"
        ds.mkdir(parents=True, exist_ok=True)
        (ds / f"{name}.adg").write_bytes(data)
    return sorted(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args(argv)
    for name, kit in KITS.items():
        print(f"== {name}  (de {kit['source']})")
        for note, role, pad in build(name, kit, a.dry_run):
            if role != "extra" or note < 56:
                print(f"  {note:3}  {role:15} {pad}")
        print()


if __name__ == "__main__":
    main()

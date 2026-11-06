"""Drum Rack con las opciones de kick. Teoría: apps/docs/theory/motor-primera-entrega.md §5."""

from __future__ import annotations

import gzip
import os
import xml.etree.ElementTree as ET
from pathlib import Path

from make_gf_racks import clone_branch, pad_name  # noqa: E402

NOTA_PRIMER_PAD = 36  # C1, la primera fila de pads en Live.


def plantilla_por_defecto() -> Path:
    # Se parte de un rack real de Juan: el formato .adg no está documentado y cambia entre versiones de Live.
    return Path(
        os.environ.get("GF_RACK_PLANTILLA")
        or Path.home() / "Music/Ableton/User Library/Presets/Instruments/Drum Rack/808 Core Kit.adg"
    )


def _apuntar_a(branch: ET.Element, wav: Path) -> None:
    """Deja una sola referencia absoluta al WAV; así el rack no hereda rutas ni datos del sample original."""
    for ref in branch.iter("SampleRef"):
        contexto = ref.find("SourceContext")
        if contexto is not None:
            for hijo in list(contexto):
                contexto.remove(hijo)
        archivo = ref.find("FileRef")
        archivo.find("RelativePathType").set("Value", "0")
        archivo.find("RelativePath").set("Value", "")
        archivo.find("Path").set("Value", str(wav))
    # El molde puede venir transpuesto (el de 808 Core Kit trae +3), y el kick ya sale afinado a la tonalidad.
    for ruta in ("Pitch/TransposeKey/Manual", "Pitch/TransposeFine/Manual"):
        for simpler in branch.iter("OriginalSimpler"):
            control = simpler.find(ruta)
            if control is not None:
                control.set("Value", "0")


def crear_rack(
    wavs: list[Path], nombre: str, destino: Path, plantilla: Path | None = None, notas: list[int] | None = None, choke: dict[int, int] | None = None
) -> list[int]:
    """Escribe un .adg con un pad por WAV y devuelve sus notas. `choke` da el grupo por nota; 0 es sin grupo."""
    fuente = plantilla or plantilla_por_defecto()
    if not fuente.exists():
        raise ValueError(f"No encuentro el rack de plantilla: {fuente}")
    raiz = ET.fromstring(gzip.decompress(fuente.read_bytes()).decode("utf-8"))
    pads = raiz.find("GroupDevicePreset/BranchPresets")
    a_texto = lambda e: ET.tostring(e, encoding="unicode")  # noqa: E731
    molde = a_texto(next(b for b in pads if len(list(b.iter("SampleRef"))) == 1))
    viejo = pad_name(molde)

    for b in list(pads):
        pads.remove(b)
    usadas = []
    for i, wav in enumerate(wavs):
        pad = ET.fromstring(clone_branch(molde, viejo, wav.stem, wav, i + 1))
        _apuntar_a(pad, wav)
        nota = notas[i] if notas else NOTA_PRIMER_PAD + i
        pad.find("ZoneSettings/ReceivingNote").set("Value", str(128 - nota))  # Live guarda 128 − nota.
        grupo = pad.find("ZoneSettings/ChokeGroup")
        if grupo is not None:
            grupo.set("Value", str((choke or {}).get(nota, 0)))
        pads.append(pad)
        usadas.append(nota)
    raiz.find(".//DrumGroupDevice/UserName").set("Value", nombre)

    destino.parent.mkdir(parents=True, exist_ok=True)
    destino.write_bytes(gzip.compress(('<?xml version="1.0" encoding="UTF-8"?>\n' + a_texto(raiz)).encode("utf-8")))
    return usadas

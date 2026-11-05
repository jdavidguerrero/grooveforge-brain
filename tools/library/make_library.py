#!/usr/bin/env python3
"""Genera GrooveForge_Library desde el dataset. Diseño y reglas: apps/docs/produccion/library-grooveforge.md."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

import mido

GENEROS = {
    "organic": "Organic", "house": "House", "nu_disco": "Nu Disco", "indie_tech": "Indie Tech",
    "organic_house": "Organic House", "afro_house": "Afro House", "disco_house": "Disco House",
    "melodic_progressive": "Melodic Progressive",
}
RACKS = {"tribe": "Tribe", "808": "808", "brazilian": "Brazilian", "djembe": "Djembe"}
INSTRUMENTOS = {
    "kick": "Kick", "snare": "Snare", "clap": "Clap", "rim": "Rim", "clave": "Clave", "hat_cerrado": "Hat Cerrado",
    "hat_abierto": "Hat Abierto", "conga_baja": "Conga", "conga_media": "Conga", "conga_alta": "Conga",
    "bongo_alto": "Bongo", "bongo_bajo": "Bongo", "maraca": "Shaker",
}
ROLES_ADN = {
    "bass": "Bass", "chords": "Chords", "lead": "Leads", "pluck": "Plucks", "pad": "Pads", "capas": "Capas", "build": "Builds", "perc": "Perc Brain",
}
ROLES_PRUEBA = {"bass", "chords", "fx", "pad", "pluck"}
SECCIONES = {"intro", "main", "peak", "main_q"}
_TONALIDAD = re.compile(r"^(?P<genero>.+)_(?P<raiz>[A-G][#b]?)_(?P<modo>[a-z]+)_(?P<bpm>\d+)$")


@dataclass
class Entrada:
    origen: Path
    destino: str
    meta: dict = field(default_factory=dict)


def _hash(ruta: Path) -> str:
    return hashlib.sha256(ruta.read_bytes()).hexdigest()


def _datos_midi(ruta: Path) -> dict | None:
    try:
        m = mido.MidiFile(ruta)
    except Exception:
        return None
    notas = [x.note for t in m.tracks for x in t if x.type == "note_on" and x.velocity > 0]
    ticks = max((sum(x.time for x in t) for t in m.tracks), default=0)
    tempo = next((x.tempo for t in m.tracks for x in t if x.type == "set_tempo"), None)
    return {
        "bpm_archivo": round(mido.tempo2bpm(tempo), 2) if tempo else None,
        "notas": len(notas),
        "nota_min": min(notas, default=None),
        "nota_max": max(notas, default=None),
        "tiempos": round(ticks / m.ticks_per_beat, 3) if m.ticks_per_beat else None,
    }


def _cargar_manifiesto(ruta: Path) -> dict[str, dict]:
    if not ruta.exists():
        return {}
    filas = (json.loads(x) for x in ruta.read_text().splitlines() if x.strip())
    return {f["archivo"]: f for f in filas if "archivo" in f}


def _meta_sintesis(f: dict) -> dict:
    claves = ("version", "velocity", "seed_timbre", "seed_golpe", "variacion", "room", "afinar_hz", "ajuste", "nota", "articulacion")
    return {k: f[k] for k in claves if k in f}


def _perc_synth(renders: Path) -> list[Entrada]:
    salida: list[Entrada] = []

    # v0.4: kits por género y tonalidad.
    v4 = renders / "v0.4"
    man4 = _cargar_manifiesto(v4 / "manifiesto.jsonl")
    for carpeta in sorted(p for p in v4.iterdir() if p.is_dir()) if v4.exists() else []:
        t = _TONALIDAD.match(carpeta.name)
        if not t:
            continue
        g, raiz, modo, bpm = t["genero"], t["raiz"], t["modo"], int(t["bpm"])
        nombre_g, base = GENEROS.get(g, g), {"genero": g, "tonalidad": f"{raiz} {modo}", "bpm": bpm}
        for w in sorted(carpeta.glob("golpes/*/*.wav")):
            voz = w.parent.name
            etiqueta = w.stem.rsplit("_", 1)[-1]
            f = man4.get(str(w.relative_to(v4)), {})
            salida.append(
                Entrada(
                    w,
                    f"01 One Shots/{INSTRUMENTOS.get(voz, voz)}/{nombre_g}/{voz}_{g}_{raiz}{modo}_{etiqueta}.wav",
                    {**base, "tipo": "one_shot", "rol": voz, "material": f.get("material"), "sintesis": _meta_sintesis(f), "grupo": f"synth:{g}:{voz}:{f.get('seed_timbre', etiqueta)}", "uso_ia": "entrenable"},
                )
            )
        for w in sorted(carpeta.glob("loop_*.wav")):
            salida.append(Entrada(w, f"05 Loops/{nombre_g}/{w.name}", {**base, "tipo": "loop", "rol": "kit", "grupo": f"synth:{g}:loop", "uso_ia": "solo_catalogo"}))
        for w in sorted(carpeta.glob("audicion/*.wav")):
            salida.append(Entrada(w, f"08 Audiciones/{nombre_g}/{w.stem}_{g}.wav", {**base, "tipo": "audicion", "rol": w.stem, "grupo": f"synth:{g}:audicion", "uso_ia": "excluido", "motivo": "concatena variantes"}))

    # v0.3: variantes y kits por material.
    v3 = renders / "v0.3"
    man3 = _cargar_manifiesto(v3 / "manifiesto.jsonl")
    for w in sorted(v3.glob("variantes/*/*/*.wav")):
        material, voz = w.parent.parent.name, w.parent.name
        etiqueta = w.stem.rsplit("_", 1)[-1]
        f = man3.get(str(w.relative_to(v3)), {})
        salida.append(
            Entrada(
                w,
                f"01 One Shots/{INSTRUMENTOS.get(voz, voz)}/{material.capitalize()}/{voz}_{material}_{etiqueta}.wav",
                {"tipo": "one_shot", "rol": voz, "material": material, "sintesis": _meta_sintesis(f), "grupo": f"synth:{material}:{voz}:{f.get('seed_timbre', etiqueta)}", "uso_ia": "entrenable"},
            )
        )
    for w in sorted(v3.glob("kits/*/*.wav")):
        kit = w.parent.name
        f = man3.get(str(w.relative_to(v3)), {})
        salida.append(Entrada(w, f"02 Kits/{kit}/{w.name}", {"tipo": "one_shot", "rol": f.get("voz"), "material": f.get("material"), "sintesis": _meta_sintesis(f), "grupo": f"synth:{kit}:{f.get('voz')}", "uso_ia": "entrenable"}))
    for w in sorted(v3.glob("loops/*.wav")):
        salida.append(Entrada(w, f"05 Loops/Kits 808/{w.name}", {"tipo": "loop", "rol": "kit", "grupo": f"synth:{w.stem}", "uso_ia": "solo_catalogo"}))
    for w in sorted(v3.glob("audicion/*/*.wav")):
        salida.append(Entrada(w, f"08 Audiciones/{w.parent.name.capitalize()}/{w.name}", {"tipo": "audicion", "rol": w.stem.split("_")[0], "material": w.parent.name, "grupo": f"synth:{w.stem}", "uso_ia": "excluido", "motivo": "concatena variantes"}))

    # v0.1: instrumentos modales.
    v1 = renders / "v0.1"
    man1 = _cargar_manifiesto(v1 / "manifiesto.jsonl")
    for w in sorted(v1.glob("golpes/*/*.wav")):
        inst = w.parent.name
        f = man1.get(str(w.relative_to(v1)), {})
        salida.append(Entrada(w, f"01 One Shots/Modales/{inst.capitalize()}/{w.name}", {"tipo": "one_shot", "rol": inst, "sintesis": _meta_sintesis(f), "grupo": f"synth:modal:{inst}", "uso_ia": "entrenable"}))
    for w in sorted(v1.glob("*.wav")):
        salida.append(Entrada(w, f"05 Loops/Modales/{w.name}", {"tipo": "loop", "rol": "kit", "grupo": f"synth:{w.stem}", "uso_ia": "solo_catalogo"}))
    return salida


def _desglosar_patron(nombre: str) -> dict:
    partes = nombre.split("_")
    rack = partes.pop() if partes and partes[-1] in RACKS else None
    grupo = "_".join(partes)
    grupo = re.sub(r"_(intro|main_q|main|peak|entrada(_\dbar)?|salida(_\dbar)?|\dbar)(?=_|$)", "", grupo)
    grupo = re.sub(r"_v\d+(?=_|$)", "", grupo)
    # Cuantizado, break y remate derivan del patrón base: separarlos filtraría datos entre entrenamiento y prueba.
    grupo = re.sub(r"_(q|break|remate(_\dbeat)?)(?=_|$)", "", grupo)
    version = re.search(r"_v(\d+)(?:_|$)", nombre)
    return {"rack": rack, "patron": grupo, "version": int(version[1]) if version else 1}


def _midi_perc(perc: Path) -> list[Entrada]:
    salida: list[Entrada] = []
    for m in sorted(perc.rglob("*.mid")):
        rel = m.relative_to(perc)
        p = rel.parts
        d = _desglosar_patron(m.stem)
        rack = RACKS.get(d["rack"], "Sin rack")
        base = {"tipo": "midi", "rack": d["rack"], "version": d["version"], "grupo": f"patron:{d['patron']}", "uso_ia": "entrenable"}
        seccion = next((x for x in p if x in SECCIONES), None)
        # Top y perc de un patrón comparten nombre de archivo; la subcarpeta los distingue.
        parte = next((x for x in p[:-1] if x in ("top", "perc")), None)
        sub_parte = {"top": "Tops", "perc": "Perc"}.get(parte, "")
        if p[0] in ("favoritos", "seleccion"):
            carpeta = "Favoritos" if p[0] == "favoritos" else "Seleccion"
            g = p[1] if len(p) > 2 and p[1] in GENEROS else None
            base |= {"decision_juan": "favorito" if p[0] == "favoritos" else "seleccion", "genero": g, "rol": parte, "seccion": seccion}
            salida.append(Entrada(m, f"03 Drums MIDI/{carpeta}/{GENEROS.get(g, 'Otros')}/{sub_parte}/{m.name}", base))
        elif p[0] == "raices":
            salida.append(Entrada(m, f"03 Drums MIDI/Raices/{sub_parte or 'Otros'}/{m.name}", {**base, "genero": "raices", "rol": parte}))
        elif p[0] == "dosis_tribe":
            salida.append(Entrada(m, f"03 Drums MIDI/Dosis/{m.name}", {**base, "rol": "dosis"}))
        elif p[0] in GENEROS and len(p) > 1 and p[1] == "fills":
            salida.append(Entrada(m, f"03 Drums MIDI/Fills/{GENEROS[p[0]]}/{rack}/{sub_parte}/{m.name}", {**base, "genero": p[0], "rol": "fill", "parte": parte, "seccion": p[2] if len(p) > 3 else None}))
        elif p[0] in GENEROS and len(p) > 1 and p[1] in ("top", "perc"):
            carpeta = "Tops" if p[1] == "top" else "Perc"
            salida.append(Entrada(m, f"03 Drums MIDI/{carpeta}/{GENEROS[p[0]]}/{rack}/{m.name}", {**base, "genero": p[0], "rol": p[1], "seccion": seccion}))
        else:
            salida.append(Entrada(m, f"03 Drums MIDI/Otros/{m.name}", {**base, "rol": "perc"}))
    return salida


def _midi_resto(midi: Path) -> list[Entrada]:
    salida: list[Entrada] = []
    adn = midi / "adn"
    for m in sorted(adn.rglob("*.mid")) if adn.exists() else []:
        rel = m.relative_to(adn)
        rol = rel.parts[0]
        if rol in ROLES_ADN:
            medio = "/".join(x for x in rel.parts[1:-1] if x != "lab")
            ruta = "/".join(x for x in ("04 Melodic MIDI", ROLES_ADN[rol], medio, m.name) if x)
            salida.append(Entrada(m, ruta, {"tipo": "midi", "rol": rol, "grupo": f"adn:{rol}:{m.stem.split('__')[0]}", "uso_ia": "entrenable"}))
        else:
            ruta = "/".join(("07 Contexto", rol.capitalize(), *rel.parts[1:]))
            salida.append(Entrada(m, ruta, {"tipo": "midi", "rol": "contexto", "grupo": f"contexto:{rol}", "uso_ia": "solo_catalogo"}))
    for rol in sorted(ROLES_PRUEBA):
        for m in sorted((midi / rol).glob("*.mid")):
            salida.append(Entrada(m, f"07 Contexto/Pruebas/{m.name}", {"tipo": "midi", "rol": rol, "grupo": f"prueba:{m.stem}", "uso_ia": "solo_catalogo"}))
    return salida


def _racks(kits: Path) -> list[Entrada]:
    return [
        Entrada(a, f"06 Racks/{a.name}", {"tipo": "rack", "rol": "drum_rack", "grupo": f"rack:{a.stem}", "uso_ia": "excluido", "motivo": "referencia samples de terceros"})
        for a in sorted((kits / "racks").glob("*.adg"))
    ]


def recolectar(dataset: Path) -> list[Entrada]:
    return _perc_synth(dataset / "renders" / "perc-synth") + _midi_perc(dataset / "midi" / "perc") + _midi_resto(dataset / "midi") + _racks(dataset / "kits")


def construir(dataset: Path, library: Path) -> dict:
    catalogo_dir = library / "_catalogo"
    ruta_catalogo = catalogo_dir / "catalogo.jsonl"
    anterior: dict[str, str] = {}
    if ruta_catalogo.exists():
        filas = (json.loads(x) for x in ruta_catalogo.read_text().splitlines() if x.strip())
        # Una ruta ya marcada como editada queda fuera: al no estar aquí, nunca se sobrescribe.
        anterior = {f["ruta"]: f["sha256"] for f in filas if not f.get("editado_por_juan")}

    filas: dict[str, dict] = {}
    copiados = iguales = 0
    protegidos: list[str] = []
    ocupadas: dict[str, str] = {}

    for e in recolectar(dataset):
        h = _hash(e.origen)
        destino = str(Path(e.destino))
        if ocupadas.get(destino, h) != h:
            p = Path(destino)
            destino = str(p.with_name(f"{p.stem}__{h[:6]}{p.suffix}"))
        ocupadas[destino] = h
        ruta = library / destino
        origen_rel = str(e.origen.relative_to(dataset))

        if destino in filas:  # Mismo contenido desde otro origen: un archivo, dos procedencias.
            filas[destino]["origenes_extra"].append(origen_rel)
            continue

        if ruta.exists():
            actual = _hash(ruta)
            if actual == h:
                iguales += 1
            elif anterior.get(destino) == actual:
                shutil.copy2(e.origen, ruta)  # Nadie lo tocó desde la última corrida: cambió el origen.
                copiados += 1
            else:
                protegidos.append(destino)  # Editado por Juan, o ajeno al generador.
        else:
            ruta.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(e.origen, ruta)
            copiados += 1

        meta = dict(e.meta)
        fila = {
            "id": h[:16], "sha256": h, "ruta": destino, "origen": origen_rel, "origenes_extra": [],
            "tipo": meta.pop("tipo"), "rol": meta.pop("rol", None), "genero": meta.pop("genero", None), "material": meta.pop("material", None),
            "rack": meta.pop("rack", None), "seccion": meta.pop("seccion", None), "version": meta.pop("version", None),
            "tonalidad": meta.pop("tonalidad", None), "bpm": meta.pop("bpm", None), "autoria": "generado", "decision_juan": meta.pop("decision_juan", None),
            "grupo": meta.pop("grupo"), "uso_ia": meta.pop("uso_ia"), **meta,
        }
        if e.origen.suffix == ".mid":
            fila["midi"] = _datos_midi(e.origen)
        filas[destino] = fila

    # Un favorito es una copia exacta de un archivo: la decisión vale para el contenido, no para la carpeta.
    decisiones = {f["sha256"]: f["decision_juan"] for f in sorted(filas.values(), key=lambda x: x["decision_juan"] == "seleccion") if f["decision_juan"]}
    for f in filas.values():
        f["decision_juan"] = decisiones.get(f["sha256"])

    # Una ruta protegida conserva el hash real del archivo de Juan, para seguir protegida en la próxima corrida.
    for destino in protegidos:
        real = _hash(library / destino)
        filas[destino] |= {"id": real[:16], "sha256": real, "editado_por_juan": True}

    catalogo_dir.mkdir(parents=True, exist_ok=True)
    temporal = ruta_catalogo.with_suffix(".tmp")
    temporal.write_text("".join(json.dumps(f, ensure_ascii=False) + "\n" for f in sorted(filas.values(), key=lambda x: x["ruta"])))
    temporal.replace(ruta_catalogo)

    (library / "00 Elegidos").mkdir(parents=True, exist_ok=True)
    valores = list(filas.values())
    resumen = {
        "archivos": len(valores),
        "por_tipo": dict(Counter(f["tipo"] for f in valores)),
        "por_uso_ia": dict(Counter(f["uso_ia"] for f in valores)),
        "por_decision_juan": dict(Counter(f["decision_juan"] for f in valores if f["decision_juan"])),
        "grupos": len({f["grupo"] for f in valores}),
    }
    (catalogo_dir / "resumen.json").write_text(json.dumps(resumen, ensure_ascii=False, indent=1))
    return {**resumen, "copiados": copiados, "iguales": iguales, "protegidos": protegidos}


def main() -> None:
    home = Path.home() / "GROOVEFORGE"
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dataset", type=Path, default=Path(os.environ.get("GF_DATASET") or home / "GrooveForge_Dataset_v1"))
    ap.add_argument("--library", type=Path, default=Path(os.environ.get("GF_LIBRARY") or home / "GrooveForge_Library"))
    a = ap.parse_args()
    print(json.dumps(construir(a.dataset, a.library), ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main()

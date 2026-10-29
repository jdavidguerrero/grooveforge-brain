#!/usr/bin/env python3
"""Congela el estado del laboratorio en snapshots/<fecha>/ con un manifiesto de hashes (protocolo-captura.md §6).

Copia el MIDI, los presets, los kits y las fichas del dataset, los .als y las tomas de Live, y opcionalmente exporta los
presets que tiene el Brain. Un snapshot no se edita: si ya existe la carpeta, se aborta (usar --etiqueta).

    python3 tools/paleta-lab/gf_snapshot.py --brain --firmware "rev. 7 de HYBRID, sin verificar"
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import shutil
import subprocess
import sys
import wave
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DATASET_PARTS = ("midi", "patches", "kits", "metadata")
LIVE_DEFAULT = Path.home() / "MUSIC_PRODUCTION" / "Grooveforge" / "GFExploration Project"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def wav_info(path: Path) -> dict:
    try:
        with wave.open(str(path)) as w:
            return {"sample_rate": w.getframerate(), "bits": w.getsampwidth() * 8, "channels": w.getnchannels(),
                    "seconds": round(w.getnframes() / w.getframerate(), 3)}
    except (wave.Error, EOFError):
        return {"error": "cabecera WAV no legible"}   # p. ej. WAV float de Live: se guarda igual, sin datos de formato


def git_state() -> dict:
    def run(*args):
        return subprocess.run(["git", "-C", str(REPO), *args], capture_output=True, text=True).stdout.strip()
    dirty = [l for l in run("status", "--porcelain").splitlines() if l]
    return {"commit": run("rev-parse", "HEAD"), "branch": run("rev-parse", "--abbrev-ref", "HEAD"),
            "uncommitted": len(dirty)}


def library_vs_brain(lib: Path, brain: Path) -> dict:
    """Presets que solo están en un lado o cuyo raw difiere (el nombre igual no garantiza el mismo sonido)."""
    def load(root):
        return {str(p.relative_to(root)): json.loads(p.read_text()).get("raw") for p in root.rglob("*.json")}
    a, b = load(lib), load(brain)
    return {"solo_libreria": sorted(set(a) - set(b)), "solo_brain": sorted(set(b) - set(a)),
            "raw_distinto": sorted(k for k in set(a) & set(b) if a[k] != b[k])}


def copy_tree(src: Path, dst: Path, suffixes: tuple[str, ...] | None = None):
    for f in sorted(src.rglob("*")):
        if f.is_file() and not f.name.startswith(".") and (suffixes is None or f.suffix.lower() in suffixes):
            out = dst / f.relative_to(src)
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, out)


def snapshot(dataset: Path, live: Path, out: Path, firmware: str, brain_export: bool, port: str | None) -> dict:
    if out.exists():
        sys.exit(f"{out} ya existe: un snapshot no se edita (usar --etiqueta)")
    out.mkdir(parents=True)
    if brain_export:
        lib = out / "brain_export"
        cmd = [sys.executable, str(REPO / "tools" / "preset-librarian" / "gf_presets.py"), "--lib", str(lib)]
        if port:
            cmd += ["--port", port]
        for sub in ("export", "layers"):
            subprocess.run(cmd + [sub], check=True, stdout=subprocess.DEVNULL)
    for part in DATASET_PARTS:
        if (dataset / part).is_dir():
            copy_tree(dataset / part, out / "dataset" / part)
    if live.is_dir():
        for als in sorted(live.glob("*.als")):
            (out / "live").mkdir(parents=True, exist_ok=True)
            shutil.copy2(als, out / "live" / als.name)
        rec = live / "Samples" / "Recorded"
        if rec.is_dir():
            copy_tree(rec, out / "live" / "recorded", (".wav", ".aif", ".aiff"))
    files = []
    for f in sorted(out.rglob("*")):
        if f.is_file():
            entry = {"path": str(f.relative_to(out)), "bytes": f.stat().st_size, "sha256": sha256(f)}
            if f.suffix.lower() == ".wav":
                entry["audio"] = wav_info(f)
            files.append(entry)
    manifest = {
        "schema_version": 1,
        "creado": dt.datetime.now().isoformat(timespec="seconds"),
        "repo": git_state(),
        "firmware_cargado": firmware or "unknown",
        "origenes": {"dataset": str(dataset), "live": str(live)},
        "archivos": files,
    }
    if brain_export and (dataset / "patches" / "brain").is_dir():
        manifest["libreria_vs_brain"] = library_vs_brain(dataset / "patches" / "brain", out / "brain_export" / "brain")
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    return manifest


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    ap.add_argument("--live", default=str(LIVE_DEFAULT), help="carpeta del proyecto de Live del laboratorio")
    ap.add_argument("--fecha", default=dt.date.today().isoformat())
    ap.add_argument("--etiqueta", default="", help="sufijo si hay más de un snapshot el mismo día")
    ap.add_argument("--firmware", default="", help="qué firmware tiene la Teensy, si se sabe (si no: unknown)")
    ap.add_argument("--brain", action="store_true", help="exportar también los presets del Brain (puerto libre)")
    ap.add_argument("--port")
    a = ap.parse_args(argv)
    dataset = Path(a.dataset)
    name = a.fecha + (f"_{a.etiqueta}" if a.etiqueta else "")
    m = snapshot(dataset, Path(a.live), dataset / "snapshots" / name, a.firmware, a.brain, a.port)
    print(f"{len(m['archivos'])} archivos congelados en {dataset / 'snapshots' / name}")
    if "libreria_vs_brain" in m:
        for k, v in m["libreria_vs_brain"].items():
            if v:
                print(f"  {k}: {', '.join(v)}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Bibliotecario de presets del GrooveForge Brain (apps/docs/variants/studio/theory/11-studio-preset-librarian.md).

Saca los presets del Brain por el USB serie a una librería de archivos JSON (uno por sonido, con ficha) y los sube de
vuelta. Correr con el Python de PlatformIO, que ya trae pyserial:

    ~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py export
    ~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py import <archivo.json> …
    ~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py bank juno_106 <archivo.json> …
    ~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py list
    ~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py layers            # presets de capa (HYBRID SYNTH)
    ~/.platformio/penv/bin/python tools/preset-librarian/gf_presets.py layer-import <capa.json> …

La librería va en $GF_LIBRARY o, si no está definida, en ~/GROOVEFORGE/GrooveForge_Dataset_v1/patches (el dataset de la
paleta, apps/docs/produccion/protocolo-captura.md §4).
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import struct
import sys
import time
from pathlib import Path

ENGINES = ["model_d", "juno_106", "prophet_5", "ob_6", "dx7", "arp_2600"]
MAGIC = b"GFP"
NAME_LEN = 16
FORMAT = 4
# Tamaño del binario por formato: tiene que coincidir con PRESET_BYTES_V2 / _V3 / PRESET_BYTES de preset_bank.h
SIZE = {2: 355, 3: 519, 4: 527}
# Presets de capa de HYBRID SYNTH (theory/12 §3d): LAYER_BYTES de layer_bank.h
LAYER_MAGIC = b"GFL"
LAYER_FORMAT = 1
LAYER_NAME_LEN = 12
LAYER_SIZE = 4 + LAYER_NAME_LEN + 7 + 2 * SIZE[FORMAT]
LAYER_MAX = 32
META_KEYS = ["role", "tags", "key_range", "pairs_with", "notes", "origin", "permissions", "rating"]
ORIGINS = ["propio", "fabrica-brain", "tribe-instruments", "kick3", "splice", "loopcloud", "otro"]
ROLES = ["bass", "chords", "pad", "lead", "pluck", "perc", "fx"]

SCALARS = ["attack_ms", "decay_ms", "sustain", "release_ms", "f_attack_ms", "f_decay_ms", "f_sustain",
           "f_release_ms", "env_amt", "glide", "drive", "lfo_rate_hz"]


# ── Formato binario: el mismo orden que visit() de apps/firmware-teensy/src/ui/preset_bank.cpp ─────────────────

def _layout(fmt: int):
    """Campos en orden: (nombre, tipo, forma). tipo 'f' = float32, 'B' = uint8. forma () = escalar."""
    out = [(n, "f", ()) for n in SCALARS]
    out += [("lfo_amt", "f", (4,)), ("osc", "f", (4, 8)), ("mod", "f", (2, 8)), ("sfx_mix", "f", (4,)),
            ("sfx_p", "f", (4, 3)), ("cutoff_hz", "f", ()), ("reso_norm", "f", ()),
            ("lfo_shape", "B", ()), ("lfo_div", "B", ()), ("sfx_on", "B", (4,))]
    if fmt >= 3:
        out += [("groove", "f", (5, 8)), ("groove_on", "B", (4,))]
    if fmt >= 4:
        out += [("vel_amp", "f", ()), ("vel_cut", "f", ())]
    return out


def _count(shape) -> int:
    n = 1
    for d in shape:
        n *= d
    return n


def _nest(flat: list, shape):
    if len(shape) <= 1:
        return flat
    step = len(flat) // shape[0]
    return [_nest(flat[i * step:(i + 1) * step], shape[1:]) for i in range(shape[0])]


def _flatten(v):
    if isinstance(v, list):
        out = []
        for x in v:
            out += _flatten(x)
        return out
    return [v]


def decode(raw: bytes) -> dict:
    """Binario de un preset → {'format', 'engine', 'name', 'patch'}."""
    if len(raw) < 5 or raw[:3] != MAGIC:
        raise ValueError("no es un preset del Brain")
    fmt, engine = raw[3], raw[4]
    if SIZE.get(fmt) != len(raw):
        raise ValueError(f"formato {fmt} con {len(raw)} bytes (se esperaban {SIZE.get(fmt)})")
    name = raw[5:5 + NAME_LEN].split(b"\0", 1)[0].decode("ascii", "replace")
    patch, pos = {}, 5 + NAME_LEN
    for key, typ, shape in _layout(fmt):
        n = _count(shape)
        size = 4 if typ == "f" else 1
        vals = list(struct.unpack_from("<" + typ * n, raw, pos))
        pos += size * n
        if typ == "f":
            vals = [round(v, 6) for v in vals]
        if key in ("sfx_on", "groove_on"):
            vals = [bool(v) for v in vals]
        patch[key] = vals[0] if shape == () else _nest(vals, shape)
    return {"format": fmt, "engine": engine, "name": name, "patch": patch}


def encode(engine: int, name: str, patch: dict, fmt: int = FORMAT) -> bytes:
    """Lo inverso de decode(): sirve para el test de ida y vuelta (subir siempre usa 'raw')."""
    out = bytearray(MAGIC + bytes([fmt, engine]))
    out += name.encode("ascii", "replace")[:NAME_LEN].ljust(NAME_LEN, b"\0")
    for key, typ, shape in _layout(fmt):
        vals = _flatten(patch[key]) if shape != () else [patch[key]]
        if typ == "B":
            vals = [int(v) for v in vals]
        out += struct.pack("<" + typ * _count(shape), *vals)
    return bytes(out)


def rename(raw: bytes, name: str) -> bytes:
    """Cambia el nombre dentro del binario (16 caracteres ASCII como máximo)."""
    clean = name.encode("ascii", "replace")[:NAME_LEN].ljust(NAME_LEN, b"\0")
    return raw[:5] + clean + raw[5 + NAME_LEN:]


def decode_layer(raw: bytes) -> dict:
    """Binario de un preset de capa → nombre, engines, MORPH, B (retraso, transposición), mezcla y las dos mitades."""
    if len(raw) != LAYER_SIZE or raw[:3] != LAYER_MAGIC or raw[3] != LAYER_FORMAT:
        raise ValueError("no es un preset de capa del Brain")
    name = raw[4:4 + LAYER_NAME_LEN].split(b"\0", 1)[0].decode("ascii", "replace")
    p = 4 + LAYER_NAME_LEN
    ea, eb, morph, delay = raw[p], raw[p + 1], raw[p + 2], raw[p + 3]
    tr, ga, gb = struct.unpack_from("<bbb", raw, p + 4)
    p += 7
    half = SIZE[FORMAT]
    return {"name": name, "engine_a": ENGINES[ea], "engine_b": ENGINES[eb], "morph": morph, "delay_b_ms": delay,
            "transpose_b": tr, "gain_a_db": ga, "gain_b_db": gb,
            "a": decode(raw[p:p + half]), "b": decode(raw[p + half:p + 2 * half])}


def rename_layer(raw: bytes, name: str) -> bytes:
    clean = name.encode("ascii", "replace")[:LAYER_NAME_LEN].ljust(LAYER_NAME_LEN, b"\0")
    return raw[:4] + clean + raw[4 + LAYER_NAME_LEN:]


def slug(name: str) -> str:
    s = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    return s or "preset"


# ── Librería ────────────────────────────────────────────────────────────────────────────────────────────

def library_dir(arg: str | None) -> Path:
    return Path(arg or os.environ.get("GF_LIBRARY") or Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1" / "patches")


def find_by_name(folder: Path, name: str) -> Path | None:
    for f in folder.glob("*.json"):
        try:
            if json.loads(f.read_text()).get("name") == name:
                return f
        except (OSError, json.JSONDecodeError):
            continue
    return None


def is_factory_name(name: str) -> bool:
    """Los presets de fábrica del Brain: clásicos y los prefijos JD, GF y HY del firmware."""
    return not name.startswith("USER ")


def entry_from_raw(raw: bytes, old: dict | None = None) -> dict:
    d = decode(raw)
    # Un preset del Brain es material propio (o de fábrica del Brain): se puede entrenar y publicar
    origin = "fabrica-brain" if is_factory_name(d["name"]) else "propio"
    entry = {"name": d["name"], "engine": ENGINES[d["engine"]], "role": "", "tags": [], "key_range": "",
             "pairs_with": [], "notes": "", "origin": origin, "permissions": {"train": True, "publish": True},
             "rating": None, "format": d["format"], "patch": d["patch"], "raw": raw.hex().upper()}
    if old:
        for k in META_KEYS:
            if k in old:
                entry[k] = old[k]
    return entry


def layer_entry_from_raw(raw: bytes, old: dict | None = None) -> dict:
    d = decode_layer(raw)
    origin = "propio" if d["name"].startswith("LAYER ") else "fabrica-brain"
    entry = {"name": d["name"], "engine_a": d["engine_a"], "engine_b": d["engine_b"],
             "half_a": d["a"]["name"], "half_b": d["b"]["name"], "morph": d["morph"], "delay_b_ms": d["delay_b_ms"],
             "transpose_b": d["transpose_b"], "gain_a_db": d["gain_a_db"], "gain_b_db": d["gain_b_db"],
             "role": "", "tags": [], "key_range": "", "pairs_with": [], "notes": "", "origin": origin,
             "permissions": {"train": True, "publish": True}, "rating": None, "raw": raw.hex().upper()}
    if old:
        for k in META_KEYS:
            if k in old:
                entry[k] = old[k]
    return entry


# ── Brain por USB serie ─────────────────────────────────────────────────────────────────────────────────

class Brain:
    def __init__(self, port: str | None):
        try:
            import serial  # pyserial
        except ImportError:
            sys.exit("Falta pyserial: usar ~/.platformio/penv/bin/python o instalarlo (pip install pyserial).")
        port = port or self._find_port()
        self.ser = serial.Serial(port, 115200, timeout=0.2)
        time.sleep(0.2)
        self.ser.reset_input_buffer()
        hello = self.request("@GF HELLO", lambda l: l.startswith("@GF HELLO"))[-1].split()
        self.format, self.max = int(hello[2]), int(hello[4])
        if self.format != FORMAT:
            print(f"aviso: el Brain usa el formato {self.format} y este script el {FORMAT}", file=sys.stderr)

    @staticmethod
    def _find_port() -> str:
        ports = sorted(glob.glob("/dev/cu.usbmodem*")) + sorted(glob.glob("/dev/ttyACM*"))
        if not ports:
            sys.exit("No encuentro el Brain por USB (¿conectado y con el firmware corriendo?).")
        return ports[0]

    def request(self, line: str, done, timeout: float = 10.0) -> list[str]:
        """Manda una línea y junta las respuestas '@GF …' hasta que done(línea) sea verdadero."""
        self.ser.write((line + "\n").encode("ascii"))
        out, buf, t0 = [], b"", time.time()
        while time.time() - t0 < timeout:
            buf += self.ser.read(4096)
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                text = raw.decode("ascii", "replace").strip()
                if not text.startswith("@GF "):
                    continue                       # el resto es el log del Brain
                out.append(text)
                if text.startswith("@GF ERR"):
                    raise RuntimeError(text)
                if done(text):
                    return out
        raise TimeoutError(f"sin respuesta a: {line[:40]}")

    def export(self) -> list[tuple[int, int, bytes]]:
        lines = self.request("@GF EXPORT", lambda l: l == "@GF END", timeout=30.0)
        out = []
        for l in lines:
            if l.startswith("@GF P "):
                _, _, e, i, h = l.split()
                out.append((int(e), int(i), bytes.fromhex(h)))
        return out

    def put(self, engine: int, index: int, raw: bytes):
        self.request(f"@GF PUT {engine} {index} {raw.hex().upper()}", lambda l: l.startswith("@GF OK PUT"))

    def trunc(self, engine: int, n: int):
        self.request(f"@GF TRUNC {engine} {n}", lambda l: l.startswith("@GF OK TRUNC"))

    def layer_export(self) -> list[tuple[int, bytes]]:
        lines = self.request("@GF LEXPORT", lambda l: l == "@GF END", timeout=30.0)
        return [(int(l.split()[2]), bytes.fromhex(l.split()[3])) for l in lines if l.startswith("@GF L ")]

    def layer_put(self, index: int, raw: bytes):
        self.request(f"@GF LPUT {index} {raw.hex().upper()}", lambda l: l.startswith("@GF OK LPUT"))

    def layer_trunc(self, n: int):
        self.request(f"@GF LTRUNC {n}", lambda l: l.startswith("@GF OK LTRUNC"))


# ── Comandos ────────────────────────────────────────────────────────────────────────────────────────────

def cmd_export(a):
    lib = library_dir(a.lib) / "brain"
    brain = Brain(a.port)
    items = brain.export()
    for e, i, raw in items:
        folder = lib / ENGINES[e]
        folder.mkdir(parents=True, exist_ok=True)
        name = decode(raw)["name"]
        path = find_by_name(folder, name)
        old = json.loads(path.read_text()) if path else None
        path = path or folder / f"{slug(name)}.json"
        path.write_text(json.dumps(entry_from_raw(raw, old), indent=2, ensure_ascii=False) + "\n")
        print(f"{ENGINES[e]:10} {i:2}  {name:16}  → {path}")
    print(f"{len(items)} presets exportados a {lib}")


def load_entry(path: str) -> tuple[int, bytes]:
    d = json.loads(Path(path).read_text())
    raw = bytes.fromhex(d["raw"])
    if decode(raw)["name"] != d["name"]:
        raw = rename(raw, d["name"])                 # el nombre del JSON manda
    return ENGINES.index(d["engine"]), raw


def cmd_import(a):
    brain = Brain(a.port)
    counts = [0] * len(ENGINES)
    for e, i, _ in brain.export():
        counts[e] = max(counts[e], i + 1)
    for f in a.files:
        e, raw = load_entry(f)
        if counts[e] >= brain.max:
            print(f"{f}: el banco de {ENGINES[e]} está lleno ({brain.max})", file=sys.stderr)
            continue
        brain.put(e, counts[e], raw)
        print(f"{ENGINES[e]:10} {counts[e]:2}  {decode(raw)['name']}")
        counts[e] += 1


def cmd_bank(a):
    e = ENGINES.index(a.engine)
    entries = [load_entry(f) for f in a.files]
    if any(x[0] != e for x in entries):
        sys.exit(f"todos los archivos tienen que ser de {a.engine}")
    if len(entries) > 64:
        sys.exit("un engine tiene 64 presets como máximo (bancos A–D de 16)")
    brain = Brain(a.port)
    for i, (_, raw) in enumerate(entries):
        brain.put(e, i, raw)
        print(f"{a.engine:10} {i:2}  {decode(raw)['name']}")
    brain.trunc(e, len(entries))
    print(f"banco de {a.engine}: {len(entries)} presets")


def cmd_fs(a):
    parts = Brain(a.port).request("@GF FS", lambda l: l.startswith("@GF FS"))[-1].split()
    used, total = int(parts[2]), int(parts[3])
    print(f"flash de presets: {used} de {total} bytes ({100 * used / max(total, 1):.0f} %)")
    for name, n in zip(ENGINES, parts[4:]):
        print(f"{name:10} {n} presets")


def cmd_format(a):
    if not a.si:
        sys.exit("Esto BORRA todos los presets guardados en el Brain (los de fábrica vuelven solos). "
                 "Exportar antes con `export` y repetir con --si para confirmar.")
    Brain(a.port).request("@GF FORMAT YES", lambda l: l.startswith("@GF OK FORMAT"), timeout=30.0)
    print("área de presets formateada: el Brain está escribiendo los de fábrica (unos segundos)")


def slot(i: int) -> str:
    """Lugar en los bancos A–D de 16 (theory/13): 0 → A-01, 63 → D-16."""
    return f"{chr(ord('A') + i // 16)}-{i % 16 + 1:02d}"


def cmd_list(a):
    for e, i, raw in Brain(a.port).export():
        print(f"{ENGINES[e]:10} {slot(i)}  {decode(raw)['name']}")


def cmd_layers(a):
    """Baja los presets de capa a <librería>/brain/layers/, conservando la ficha de los que ya estaban."""
    folder = library_dir(a.lib) / "brain" / "layers"
    folder.mkdir(parents=True, exist_ok=True)
    items = Brain(a.port).layer_export()
    for i, raw in items:
        name = decode_layer(raw)["name"]
        path = find_by_name(folder, name)
        old = json.loads(path.read_text()) if path else None
        path = path or folder / f"{slug(name)}.json"
        path.write_text(json.dumps(layer_entry_from_raw(raw, old), indent=2, ensure_ascii=False) + "\n")
        print(f"{slot(i)}  {name:12}  → {path}")
    print(f"{len(items)} presets de capa exportados a {folder}")


def load_layer(path: str) -> bytes:
    d = json.loads(Path(path).read_text())
    raw = bytes.fromhex(d["raw"])
    return rename_layer(raw, d["name"]) if decode_layer(raw)["name"] != d["name"] else raw   # el nombre del JSON manda


def cmd_layer_import(a):
    raws = [load_layer(f) for f in a.files]
    brain = Brain(a.port)
    start = 0 if a.reemplazar else len(brain.layer_export())
    if start + len(raws) > LAYER_MAX:
        sys.exit(f"entran {LAYER_MAX} presets de capa como máximo (bancos A y B de 16)")
    for k, raw in enumerate(raws):
        brain.layer_put(start + k, raw)
        print(f"{slot(start + k)}  {decode_layer(raw)['name']}")
    if a.reemplazar:
        brain.layer_trunc(len(raws))
    print(f"{len(raws)} presets de capa subidos")


def main(argv=None):
    p = argparse.ArgumentParser(description="Bibliotecario de presets del GrooveForge Brain")
    p.add_argument("--port", help="puerto serie (por defecto el primer /dev/cu.usbmodem*)")
    p.add_argument("--lib", help="carpeta de la librería (por defecto $GF_LIBRARY o ~/GROOVEFORGE/sound-library)")
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("export", help="bajar todos los presets a la librería").set_defaults(fn=cmd_export)
    s = sub.add_parser("import", help="subir archivos al final del banco de su engine")
    s.add_argument("files", nargs="+")
    s.set_defaults(fn=cmd_import)
    s = sub.add_parser("bank", help="reescribir los presets de un engine en este orden (hasta 64: A-01 … D-16)")
    s.add_argument("engine", choices=ENGINES)
    s.add_argument("files", nargs="+")
    s.set_defaults(fn=cmd_bank)
    sub.add_parser("list", help="listar los presets del Brain").set_defaults(fn=cmd_list)
    sub.add_parser("fs", help="espacio del flash de presets y tamaño de cada banco").set_defaults(fn=cmd_fs)
    sub.add_parser("layers", help="bajar los presets de capa (HYBRID SYNTH) a brain/layers/").set_defaults(fn=cmd_layers)
    s = sub.add_parser("layer-import", help="subir presets de capa al final (o en este orden con --reemplazar)")
    s.add_argument("files", nargs="+")
    s.add_argument("--reemplazar", action="store_true", help="reescribir todos los presets de capa en este orden")
    s.set_defaults(fn=cmd_layer_import)
    s = sub.add_parser("format", help="BORRA todos los presets del Brain y regenera los de fábrica")
    s.add_argument("--si", action="store_true", help="confirmar")
    s.set_defaults(fn=cmd_format)
    a = p.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()

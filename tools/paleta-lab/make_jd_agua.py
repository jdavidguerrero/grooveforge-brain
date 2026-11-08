#!/usr/bin/env python3
"""Atmósferas orgánicas de agua y aire (apps/docs/produccion/motor-pads.md §5.1).

Ruido + filtro con resonancia + LFO (lento o S&H): el agua que corre es una nube de burbujas que resuenan
(Minnaert, 1933) y el viento, ruido con un filtro que se mueve despacio (Farnell, *Designing Sound*, 2010).
Parte de GF NOISE RISER (ARP 2600) y GF DARK DRONE (OB-6), que ya están en la librería. No toca el Brain.

    JD AIRE     ARP 2600            ruido, filtro que abre y cierra muy lento
    JD GOTAS    ARP 2600            ruido, resonancia alta, S&H mueve el filtro a saltos
    JD RIO      capa ARP 2600 + OB-6   gorgoteo (S&H rápido y poco profundo) + corriente grave que respira
    JD CASCADA  capa ARP 2600 + OB-6   ruido ancho y constante + golpe grave
"""
from __future__ import annotations

import argparse
import copy
import json
import struct
from pathlib import Path

from make_jd_leads import g

SINE, SH = 0, 4                                  # GF_LFO_SHAPES: SINE · TRI · SAW · SQR · S&H


def atmos(src: dict, cutoff: float, reso: float, lfo_shape: int, lfo_rate: float, lfo_cut: float,
          attack: float = 2500.0, release: float = 3000.0, lfo_amp: float = 0.0) -> dict:
    """Molde común: sostiene (S 1), entra y sale lento, el filtro no lo mueve la envolvente sino el LFO."""
    p = copy.deepcopy(src)
    p.update(attack_ms=attack, decay_ms=1000.0, sustain=1.0, release_ms=release, f_attack_ms=attack,
             f_decay_ms=1000.0, f_sustain=1.0, f_release_ms=release, env_amt=0.0, cutoff_hz=cutoff, reso_norm=reso,
             glide=0.0, vel_cut=0.0, vel_amp=30.0, lfo_shape=lfo_shape, lfo_rate_hz=lfo_rate, lfo_div=0)
    p["lfo_amt"] = [lfo_cut, 0.0, lfo_amp, 0.0]
    p["groove_on"] = [False] * len(p["groove_on"])
    p["sfx_on"] = [False] * len(p["sfx_on"])
    p["delay_on"] = False
    return p


def arp_ruido(src: dict, color: float, **kw) -> dict:
    """ARP 2600 solo ruido: VCO 1 y 2 a cero, NOISE al máximo; COLOR 0 blanco → 100 rosa (más grave)."""
    p = atmos(src, **kw)
    p["osc"][0][3], p["osc"][0][4], p["osc"][1][4] = 100.0, 0.0, 0.0
    p["mod"][0][0] = 0.0                         # polifónico: cada nota, su propia corriente
    p["mod"][0][3] = p["mod"][0][4] = 0.0        # VCO 3 sin modular: el movimiento lo pone el LFO
    p["mod"][0][5] = color
    return p


def ob6_ruido(src: dict, bandpass: bool, **kw) -> dict:
    """OB-6 solo ruido: VCO 1, VCO 2 y SUB a cero; BP MODE convierte el filtro en pasabanda."""
    p = atmos(src, **kw)
    o = p["osc"]
    o[0][1], o[0][2], o[0][3], o[1][2] = 0.0, 0.0, 100.0, 0.0
    o[0][7] = 1.0 if bandpass else 0.0
    p["mod"][0][0] = 0.0                         # sin X-MOD
    return p


# (nombre, motor, receta)
SOLOS = [
    ("JD AIRE", dict(color=30.0, cutoff=900.0, reso=0.45, lfo_shape=SINE, lfo_rate=0.1, lfo_cut=0.35)),
    ("JD GOTAS", dict(color=0.0, cutoff=1400.0, reso=0.85, lfo_shape=SH, lfo_rate=5.0, lfo_cut=0.3,
                      attack=800.0, release=2000.0)),
]
# (nombre, ganancia A dB, ganancia B dB, mitad ARP, mitad OB-6)
CAPAS = [
    ("JD RIO", 0, -3,
     dict(color=10.0, cutoff=1500.0, reso=0.4, lfo_shape=SH, lfo_rate=12.0, lfo_cut=0.15),
     dict(bandpass=False, cutoff=500.0, reso=0.2, lfo_shape=SINE, lfo_rate=0.07, lfo_cut=0.25, lfo_amp=0.3)),
    ("JD CASCADA", 0, -2,
     dict(color=20.0, cutoff=3000.0, reso=0.1, lfo_shape=SINE, lfo_rate=0.05, lfo_cut=0.05),
     dict(bandpass=False, cutoff=300.0, reso=0.15, lfo_shape=SH, lfo_rate=8.0, lfo_cut=0.1)),
]


def capa_raw(name: str, gain_a: int, gain_b: int, a: dict, b: dict) -> bytes:
    ea, eb = g.ENGINES.index("arp_2600"), g.ENGINES.index("ob_6")
    head = (g.LAYER_MAGIC + bytes([g.LAYER_FORMAT]) + name.encode().ljust(g.LAYER_NAME_LEN, b"\0")
            + bytes([ea, eb, 50, 0]) + struct.pack("<bbb", 0, gain_a, gain_b))
    return head + g.encode(ea, f"{name} A", a) + g.encode(eb, f"{name} B", b)


def build(lib: Path) -> dict[str, dict]:
    def src(engine: str, name: str) -> dict:
        path = g.find_by_name(lib / engine, name)
        if not path:
            raise SystemExit(f"no está {name} en {lib / engine}")
        return json.loads(path.read_text())["patch"]

    arp, ob6 = src("arp_2600", "GF NOISE RISER"), src("ob_6", "GF DARK DRONE")
    out = {}
    for name, receta in SOLOS:
        e = g.entry_from_raw(g.encode(g.ENGINES.index("arp_2600"), name, arp_ruido(arp, **receta)))
        e.update(role="atmos", origin="propio", tags=["motor-pads", "organico"],
                 notes="ruido + filtro + LFO, agua y aire (motor-pads.md §5.1)")
        out[f"arp_2600/{g.slug(name)}.json"] = e
    for name, ga, gb, ra, rb in CAPAS:
        e = g.layer_entry_from_raw(capa_raw(name, ga, gb, arp_ruido(arp, **ra), ob6_ruido(ob6, **rb)))
        e.update(role="atmos", origin="propio", tags=["motor-pads", "organico"],
                 notes="capa ARP 2600 + OB-6 de ruido, agua (motor-pads.md §5.1)")
        out[f"layers/{g.slug(name)}.json"] = e
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lib", help="librería (por defecto la de gf_presets.py)")
    a = ap.parse_args(argv)
    lib = g.library_dir(a.lib) / "brain"
    for rel, e in build(lib).items():
        path = lib / rel
        if path.exists():
            raise SystemExit(f"ya existe {path}: no lo piso")
        path.write_text(json.dumps(e, indent=2, ensure_ascii=False) + "\n")
        print(f"{e['name']:12} → {path}")


if __name__ == "__main__":
    main()

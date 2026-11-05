#!/usr/bin/env python3
"""Verifica los valores de los envíos contra los plugins instalados. Doc: apps/docs/produccion/template-envios-valhalla.md

Ajusta cada valor en el plugin, lo vuelve a leer e imprime lo que el plugin muestra. No escribe archivos.

    uv run --no-project --python 3.12 --with pedalboard python -I tools/live-template/make_valhalla_presets.py
"""
from __future__ import annotations

from pedalboard import load_plugin

VST3 = "/Library/Audio/Plug-Ins/VST3/{}.vst3"
VV = "ValhallaVintageVerb"
DL = "ValhallaDelay"
# Los menús de nota siempre dan 10 opciones en este orden, sea cual sea la sincronía.
NOTAS = ["1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1", "2/1", "4/1", "8/1"]

PRESETS = {
    "GF Send A Office": (VV, dict(mix=100, reverbmode="Ambience", colormode="seventies", predelay=8, decay=0.55, size=25, attack=0, bassmult=0.8, bassxover=200, highshelf=0, highfreq=10000, earlydiffusion=100, latediffusion=100, modrate=0.4, moddepth=6, highcut=10000, lowcut=200)),
    "GF Send B Plate": (VV, dict(mix=100, reverbmode="Plate", colormode="eighties", predelay=18, decay=2.0, size=38, attack=3, bassmult=1.0, bassxover=200, highshelf=-1, highfreq=8000, earlydiffusion=100, latediffusion=100, modrate=0.5, moddepth=12, highcut=8000, lowcut=180)),
    "GF Send C Hall": (VV, dict(mix=100, reverbmode="Concert Hall", colormode="now", predelay=25, decay=3.1, size=50, attack=5, bassmult=1.0, bassxover=250, highshelf=-2, highfreq=6500, earlydiffusion=95, latediffusion=95, modrate=0.6, moddepth=18, highcut=6800, lowcut=250)),
    "GF Send D Rumble": (VV, dict(mix=100, reverbmode="Dirty Hall", colormode="seventies", predelay=28, decay=4.2, size=55, attack=3, bassmult=1.2, bassxover=250, highshelf=-3, highfreq=6000, earlydiffusion=90, latediffusion=90, modrate=0.7, moddepth=22, highcut=3000, lowcut=40)),
    "GF Send E Delay": (DL, dict(mix=100, mode="HiFi", era="Present", delaystyle="PingPong", repeatswell="Repeat", delaylsync="Dotted", delayrsync="Dotted", delaylnote="1/8", delayrnote="1/8", feedback=30, width=100, drivein=0, age=0, diffusion="OFF", lowcut=200, highcut=6000, modrate=0.5, moddepth=5, wow=0, flutter=0, ducking=0)),
    "GF Send F Echo": (DL, dict(mix=100, mode="Tape", era="Past", delaystyle="Single", repeatswell="Repeat", delaylsync="Note", delaylnote="1/4", delayspread=5, feedback=42, width=60, drivein=3, age=35, diffusion="OFF", lowcut=250, highcut=3500, modrate=0.5, moddepth=0, wow=25, flutter=20, ducking=0)),
}


def poner(plugin, nombre: str, valor) -> str:
    parametro = plugin.parameters[nombre]
    if nombre in ("delaylnote", "delayrnote"):
        parametro.raw_value = NOTAS.index(valor) / (len(NOTAS) - 1)
    elif nombre == "decay":
        validos = parametro.valid_values
        setattr(plugin, nombre, min(validos, key=lambda v: abs(float(v.split()[0]) - valor)))
    else:
        setattr(plugin, nombre, valor)
    return parametro.string_value


def main() -> None:
    for nombre, (clase, valores) in PRESETS.items():
        plugin = load_plugin(VST3.format(clase))
        print(f"## {nombre} ({clase} {plugin.version})")
        for parametro, valor in valores.items():
            print(f"{parametro}\t{poner(plugin, parametro, valor)}")


if __name__ == "__main__":
    main()

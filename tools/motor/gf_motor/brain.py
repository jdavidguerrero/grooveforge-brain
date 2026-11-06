"""Sonidos en el Brain físico: lee las capas por USB y las elige por MIDI. Diseño: motor-bajo.md §4."""

from __future__ import annotations

import sys
from math import ceil
from pathlib import Path

PUERTO_MIDI = "GrooveForge"
CC_CAPA = 18
MELODICOS_CONOCIDOS = ("JD PLK WOOD", "JD PLK WOOD2", "JD PLK KEYS", "KALIMBA", "S LEAD BODY", "JD DESERT LD")
BAJOS_CONOCIDOS = ("JD SUB MID", "TRESBASS", "TRESB OSC", "TRESB MAD1", "TRESB MAD2", "TRESB MAD3", "ROLLER SUB", "WOODBASS")


def cc_de_capa(indice: int, total: int) -> int:
    """Inversa de `layer_from_cc` (firmware `ui/layers.h`): el menor CC que selecciona esa capa."""
    if not 0 <= indice < total:
        raise ValueError(f"No existe la capa {indice}")
    return 1 + ceil(indice * 127 / total)


class Brain:
    def capas(self) -> list[str]:
        """Solo lee. Importa aquí porque abrir el puerto requiere el Brain conectado."""
        sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "preset-librarian"))
        import gf_presets

        try:
            brain = gf_presets.Brain(None)
            try:
                items = brain.layer_export()
            finally:
                brain.ser.close()
        except Exception as exc:
            raise ValueError(f"No pude leer las capas del Brain por USB: {exc}") from exc
        return [gf_presets.decode_layer(raw)["name"] for _, raw in sorted(items)]

    def elegir_capa(self, indice: int, total: int) -> None:
        import mido

        try:
            with mido.open_output(PUERTO_MIDI) as salida:
                salida.send(mido.Message("control_change", channel=0, control=CC_CAPA, value=cc_de_capa(indice, total)))
        except OSError as exc:
            raise ValueError(f"No encuentro el puerto MIDI {PUERTO_MIDI}: {exc}") from exc

"""Flujo de la primera entrega. Cada método es un paso que Juan pide; ninguno encadena el siguiente."""

from __future__ import annotations

import json
import os
import shutil
import sys
import uuid
from dataclasses import dataclass
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(_TOOLS / "perc-synth"), str(_TOOLS / "paleta-lab")]

import gf_log  # noqa: E402
import numpy as np  # noqa: E402
import soundfile as sf  # noqa: E402
from gf_perc.core import SR  # noqa: E402
from gf_perc.generos import sintetizar_en_genero  # noqa: E402
from gf_perc.seleccion import huella, mas_distintas  # noqa: E402

from .borrador import PLANTILLAS, clips_guia, clips_kick  # noqa: E402
from .edicion import diferencia  # noqa: E402
from .live import ClienteLive, ErrorLive  # noqa: E402

LETRAS = "ABC"
_VARIACION = 0.6  # El mismo valor de `kit-genero`, cuyos kits aprobó Juan.
_CANDIDATOS = 12


class SetProtegido(ErrorLive):
    """El set abierto es el template original."""


@dataclass
class Partida:
    genero: str
    raiz: str
    modo: str
    bpm: float


def carpeta_live_por_defecto() -> Path:
    return Path(os.environ.get("GF_LIBRARY") or "/Volumes/GROOVE_SSD/GrooveForge_Library")


class Motor:
    def __init__(self, live: ClienteLive, carpeta_live: Path | None = None) -> None:
        self.live = live
        self.carpeta_live = carpeta_live or carpeta_live_por_defecto()
        self.estado: dict | None = None

    # ── Persistencia ────────────────────────────────────────────────────
    def _carpeta(self) -> Path:
        assert self.estado
        return gf_log.dataset_dir() / "metadata" / "motor" / self.estado["id"]

    def _guardar(self) -> None:
        c = self._carpeta()
        c.mkdir(parents=True, exist_ok=True)
        temporal = c / "estado.json.tmp"
        temporal.write_text(json.dumps(self.estado, ensure_ascii=False, indent=1))
        temporal.replace(c / "estado.json")

    def _evento(self, tipo: str, **datos: object) -> None:
        assert self.estado
        gf_log.append({"type": f"motor_{tipo}", "sesion_motor": self.estado["id"], "contexto": self.estado["partida"], **datos})

    # ── Comprobaciones ──────────────────────────────────────────────────
    def info_set(self) -> dict:
        s = self.live.sesion()
        ruta = s.get("file_path") or ""
        return {
            "ruta": ruta,
            "tempo": s["tempo"],
            "pistas": [t["name"] for t in s["tracks"]],
            "protegido": Path(ruta).name.upper().startswith("TEMPLATE"),
        }

    def _exigir_set_editable(self) -> None:
        info = self.info_set()
        if info["protegido"]:
            raise SetProtegido(f"El set abierto es el template original ({info['ruta']}). Abre uno nuevo desde el navegador de Live.")

    def _exigir_estado(self) -> dict:
        if not self.estado:
            raise ValueError("Primero elige género, tonalidad y BPM")
        return self.estado

    def _nombre_libre(self, base: str) -> str:
        pistas = set(self.info_set()["pistas"])
        if base not in pistas:
            return base
        n = 2
        while f"{base} {n}" in pistas:
            n += 1
        return f"{base} {n}"

    # ── Pasos ───────────────────────────────────────────────────────────
    def partida(self, genero: str, raiz: str, modo: str, bpm: float) -> dict:
        if genero not in PLANTILLAS:
            raise ValueError(f"Sin plantilla para {genero!r}")
        if not 60 <= bpm <= 200:
            raise ValueError("El BPM debe estar entre 60 y 200")
        self.estado = {
            "id": uuid.uuid4().hex[:10],
            "partida": {"genero": genero, "raiz": raiz, "modo": modo, "bpm": bpm},
            "guia": None,
            "borradores": [],
            "opciones": [],
            "elegido": None,
        }
        self._guardar()
        self._evento("partida")
        return self.estado

    def crear_guia(self) -> dict:
        e = self._exigir_estado()
        self._exigir_set_editable()
        p = e["partida"]
        self.live.poner_tempo(p["bpm"])
        nombre = self._nombre_libre("GUIA")
        pista = self.live.crear_pista_midi(nombre)
        clips = clips_guia(p["genero"])
        for c in clips:
            self.live.escribir_clip(pista, c["inicio"], c["longitud"], c["notas"], c["nombre"])
        e["guia"] = {"pista": nombre, "bloques": [{k: c[k] for k in ("nombre", "inicio", "longitud")} for c in clips]}
        self._guardar()
        self._evento("guia", pista=nombre, bloques=e["guia"]["bloques"], provisional=True)
        return e

    def borrador_kick(self) -> dict:
        """Regla 3: solo escribe en una pista sin clips de Arrangement; si no, crea otra."""
        e = self._exigir_estado()
        self._exigir_set_editable()
        indice = self.live.indice_pista("KICK")
        if indice is not None and self.live.pista_sin_clips(indice):
            nombre = "KICK"
        else:
            nombre = self._nombre_libre("KICK propuesta")
            indice = self.live.crear_pista_midi(nombre)
        clips = clips_kick(e["partida"]["genero"])
        for c in clips:
            self.live.escribir_clip(indice, c["inicio"], c["longitud"], c["notas"], c["nombre"])
        e["borradores"].append({"pista": nombre, "clips": clips})
        self._guardar()
        self._evento("borrador", parte="kick", pista=nombre, clips=[c["nombre"] for c in clips])
        return e

    def generar_kicks(self) -> dict:
        e = self._exigir_estado()
        p = e["partida"]
        semilla = 1 + len(e["opciones"]) * 1000
        candidatos = [
            sintetizar_en_genero("kick", p["genero"], p["raiz"], p["modo"], 0.85, semilla + i, 1, _VARIACION if i else 0.0)
            for i in range(_CANDIDATOS)
        ]
        elegidos = mas_distintas([huella(y) for y, _ in candidatos], len(LETRAS))
        tanda = len(e["opciones"]) + 1
        carpeta = self._carpeta() / f"kicks_{tanda}"
        carpeta.mkdir(parents=True, exist_ok=True)
        opciones = []
        for letra, i in zip(LETRAS, elegidos):
            y, meta = candidatos[i]
            ruta = carpeta / f"kick_{p['genero']}_{p['raiz']}_{p['modo']}_{tanda}{letra}.wav"
            sf.write(ruta, np.asarray(y), SR, subtype="PCM_24")
            opciones.append({"letra": letra, "ruta": str(ruta), "seed_timbre": semilla + i, "nota_midi": meta.get("nota_midi")})
        e["opciones"].append(opciones)
        e["elegido"] = None
        self._guardar()
        self._evento("opciones", parte="kick", opciones=[{k: o[k] for k in ("letra", "seed_timbre", "nota_midi")} for o in opciones])
        return e

    def ruta_opcion(self, letra: str) -> Path:
        e = self._exigir_estado()
        for o in e["opciones"][-1] if e["opciones"] else []:
            if o["letra"] == letra:
                return Path(o["ruta"])
        raise ValueError(f"No existe la opción {letra!r}")

    def elegir_kick(self, letra: str) -> dict:
        e = self._exigir_estado()
        origen = self.ruta_opcion(letra)
        if not self.carpeta_live.parent.exists():
            raise ValueError(f"No encuentro {self.carpeta_live.parent}: conecta el SSD para guardar tu elección")
        destino = self.carpeta_live / "00 Elegidos" / origen.name
        destino.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(origen, destino)
        e["elegido"] = {"letra": letra, "ruta": str(destino)}
        self._guardar()
        self._evento("eleccion", parte="kick", elegido=letra, escuchadas="unknown", presentadas=list(LETRAS))
        return e

    def revisar_edicion(self) -> dict:
        """Solo lee Live. El borrador guardado no cambia; cada revisión agrega un evento."""
        e = self._exigir_estado()
        if not e["borradores"]:
            raise ValueError("Aún no hay borrador que revisar")
        b = e["borradores"][-1]
        indice = self.live.indice_pista(b["pista"])
        if indice is None:
            raise ErrorLive(f"No encuentro la pista {b['pista']!r}; si la renombraste, devuélvele el nombre para comparar")
        resultado = []
        for c in b["clips"]:
            leido = self.live.leer_clip(indice, c["inicio"])
            if leido is None:
                resultado.append({"clip": c["nombre"], "estado": "movido_o_borrado"})
            else:
                resultado.append({"clip": c["nombre"], "estado": "leido", **diferencia(c["notas"], leido["notes"])})
        e["ultima_revision"] = resultado
        self._guardar()
        self._evento("edicion", parte="kick", pista=b["pista"], clips=resultado)
        return e

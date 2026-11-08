"""Flujo de la primera entrega. Cada método es un paso que Juan pide; ninguno encadena el siguiente."""

from __future__ import annotations

import json
import re
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
from gf_perc.caracteres import COLAS_KICK, GRUPOS_DRUMS, MATERIALES_DRUMS, sintetizar_cola, sintetizar_drum  # noqa: E402
from gf_perc.kit import NOTAS as NOTAS_PERC  # noqa: E402
from gf_perc.tonalidad import MODOS, clase_de  # noqa: E402

from .detalles import PATRONES_PLUCK, PATRONES_STAB, avisos as avisos_detalles, recomendar, clip_detalle, huecos_lead  # noqa: E402
from .armado import NADA, PARCIAL, PISTAS_FIJAS, TODO, clips_de_tramo, material_de, presencia_inicial, tramo  # noqa: E402
from .transiciones import ELEMENTOS, NOTA_SUBDROP, clips_transicion, secciones  # noqa: E402
from gf_perc.subdrop import SUBDROPS, sintetizar_subdrop  # noqa: E402
from .pads import PATRONES_ATMOS, PATRONES_PAD, clip_pad  # noqa: E402
from .acordes import PROGRESIONES, choques, clip_acordes, nombres_progresion, patrones as patrones_acordes  # noqa: E402
from .bajo import FAMILIAS, patron  # noqa: E402
from .borrador import PLANTILLAS, clip_kick, clips_guia  # noqa: E402
from .brain import BAJOS_CONOCIDOS, MELODICOS_CONOCIDOS, Brain  # noqa: E402
from .motivo import motivo as crear_motivo, opciones as ritmos_motivo  # noqa: E402
from .drums import analizar, clip_drums, opciones as opciones_drums  # noqa: E402
from .edicion import diferencia  # noqa: E402
from .live import ClienteLive, ErrorLive  # noqa: E402
from .tops import candidatos, clip_loop, encaje, leer_midi  # noqa: E402
from .rack import NOTA_PRIMER_PAD, crear_rack  # noqa: E402

LETRAS = "ABCDEFGH"
TAMANO_RACK = 3
CHOKE_HATS = 1


def _tops_vacio() -> dict:
    return {r: {"opciones": [], "actual": None, "clip": None, "pista": None} for r in ("perc", "top")}


def _motivo_vacio() -> dict:
    return {"tandas": 0, "opciones": [], "actual": None, "clip": None, "pista": None, "capas": [], "total_capas": 0, "capa": None}


def notas_en_arreglo(clip: dict, siguiente: float | None) -> list[dict]:
    """Solo las notas que suenan en Arrangement, en tiempo absoluto.

    Un clip partido devuelve todas las notas del original; lo que suena es el tramo desde su marcador. Sin los datos
    del script nuevo, el tramo termina donde empieza el clip siguiente o al final de su largo, y parte del marcador 0.
    """
    marcador = clip.get("marker") or 0.0
    fin = clip.get("end")
    if fin is None:
        fin = siguiente if siguiente is not None else clip["start"] + clip["length"]
    tramo = fin - clip["start"]
    return [
        {**n, "start_beat": round(n["start_beat"] - marcador + clip["start"], 4)}
        for n in (clip.get("notes") or [])
        if marcador - 1e-6 <= n["start_beat"] < marcador + tramo - 1e-6
    ]


def _acordes_vacio() -> dict:
    return {"opciones": [], "actual": None, "clip": None, "pista": None, "melodia": None}


def _pads_vacio() -> dict:
    parte = lambda: {"opciones": [], "actual": None, "clip": None, "pista": None}  # noqa: E731
    return {"progresion": None, "tanda": 0, "pad": parte(), "atmos": parte()}


TRIBE_PLUGIN, GF_PLUGIN = "TRIBE Player 2", "GrooveForge Brain Prototype"
# Pistas que crea el motor y su instrumento. ROLL lleva el rack de la drum machine (sonido_roll).
INSTRUMENTOS = {
    "FILL": TRIBE_PLUGIN, "ROLL TRIBE": TRIBE_PLUGIN,
    **{p: GF_PLUGIN for p in ("CHORDS", "PAD", "ATMOS", "STABS", "PLUCKS", "RISER", "FX", "EAR CANDY")},
}


def _armado_vacio() -> dict:
    return {"secciones": [], "roles": [], "materiales": {}, "presencia": {}, "escritos": {}, "saltados": [], "registrado": False}


def _detalles_vacio() -> dict:
    parte = lambda: {"opciones": [], "actual": None, "clip": None, "pista": None, "aviso": None}  # noqa: E731
    return {"progresion": None, "tanda": 0, "libres": [], "kick": [], "lead": None, "stab": parte(), "pluck": parte(), "riff_aviso": None}


def _transiciones_vacio() -> dict:
    return {"secciones": [], "rack_roll": None, "rack_subdrop": None, **{k: {"opciones": [], "actual": None, "clips": None, "pista": None} for k in ELEMENTOS}}


def _unir_con_previos(nuevos: list[dict], previos: list[dict]) -> list[dict]:
    """GFMotor no borra clips del arreglo: cada clip nuevo se estira hasta cubrir los anteriores que toca,
    y los anteriores que ya no toca ninguno se tapan con un clip vacío."""
    finales = []
    usados = set()
    for n in nuevos:
        inicio, fin = n["inicio"], n["inicio"] + n["longitud"]
        for i, v in enumerate(previos):
            if v["inicio"] < fin and v["inicio"] + v["longitud"] > inicio:
                usados.add(i)
                inicio, fin = min(inicio, v["inicio"]), max(fin, v["inicio"] + v["longitud"])
        corrimiento = n["inicio"] - inicio
        notas = [{**x, "start_beat": round(x["start_beat"] + corrimiento, 4)} for x in n["notas"]]
        finales.append({**n, "inicio": inicio, "longitud": fin - inicio, "notas": notas})
    for i, v in enumerate(previos):
        if i not in usados:
            finales.append({"nombre": "—", "inicio": v["inicio"], "longitud": v["longitud"], "notas": []})
    return sorted(finales, key=lambda c: c["inicio"])


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
    def __init__(self, live: ClienteLive, carpeta_live: Path | None = None, brain: Brain | None = None) -> None:
        self.brain = brain or Brain()
        self.live = live
        self.carpeta_live = carpeta_live or carpeta_live_por_defecto()
        self.estado: dict | None = self._cargar_ultimo()

    # ── Persistencia ────────────────────────────────────────────────────
    def _carpeta(self) -> Path:
        assert self.estado
        return gf_log.dataset_dir() / "metadata" / "motor" / self.estado["id"]

    @staticmethod
    def _cargar_ultimo() -> dict | None:
        """La sesión sobrevive a reiniciar el motor; `Empezar` es lo único que abre una nueva."""
        base = gf_log.dataset_dir() / "metadata" / "motor"
        rutas = sorted(base.glob("*/estado.json"), key=lambda p: p.stat().st_mtime, reverse=True)
        for ruta in rutas:
            try:
                e = json.loads(ruta.read_text())
            except (OSError, json.JSONDecodeError):
                continue  # Un archivo dañado no debe impedir abrir la sesión anterior.
            e.setdefault("seleccion", [])
            e.setdefault("racks", [])
            e.setdefault("rack", None)
            e.setdefault("registros", [])
            e.setdefault("drums", {"tandas": 0, "opciones": [], "seleccion": {}, "racks": [], "rack": None})
            e.setdefault("tops", _tops_vacio())
            e.setdefault("motivo", _motivo_vacio())
            e.setdefault("acordes", _acordes_vacio())
            e.setdefault("pads", _pads_vacio())
            e.setdefault("detalles", _detalles_vacio())
            e.setdefault("armado", _armado_vacio())
            e.setdefault("transiciones", _transiciones_vacio())
            for k, v in _transiciones_vacio().items():
                e["transiciones"].setdefault(k, v)
            if e["transiciones"]["tribe"].get("pista") == "FILL TRIBE":  # antes los fills de Tribe iban ahí
                e["transiciones"]["tribe"] = _transiciones_vacio()["tribe"]
            e["drums"].setdefault("patrones", {"analisis": None, "opciones": [], "actual": None, "clip": None, "pista": None})
            e.setdefault("bajo", {"tandas": 0, "patrones": [], "actual": None, "clip": None, "pista": None, "capas": [], "total_capas": 0, "capa": None, "revision": None})
            return e
        return None

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

    def _exigir_library(self) -> None:
        if not self.carpeta_live.parent.exists():
            raise ValueError(f"No encuentro {self.carpeta_live.parent}: conecta el SSD")

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
            "seleccion": [],
            "racks": [],
            "rack": None,
            "elegido": None,
            "registros": [],
            "tops": _tops_vacio(),
            "motivo": _motivo_vacio(),
            "acordes": _acordes_vacio(),
            "pads": _pads_vacio(),
            "detalles": _detalles_vacio(),
            "armado": _armado_vacio(),
            "transiciones": _transiciones_vacio(),
            "drums": {"tandas": 0, "opciones": [], "seleccion": {}, "racks": [], "rack": None, "patrones": {"analisis": None, "opciones": [], "actual": None, "clip": None, "pista": None}},
            "bajo": {"tandas": 0, "patrones": [], "actual": None, "clip": None, "pista": None, "capas": [], "total_capas": 0, "capa": None, "revision": None},
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
            self.live.escribir_clip(pista, c["inicio"], c["longitud"], c["notas"], c["nombre"], c["color"])
        e["guia"] = {"pista": nombre, "bloques": [{k: c[k] for k in ("nombre", "inicio", "longitud", "color")} for c in clips]}
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
        c = clip_kick(self._nota_elegida() or NOTA_PRIMER_PAD)
        self.live.escribir_clip(indice, c["inicio"], c["longitud"], c["notas"], c["nombre"])
        e["borradores"].append({"pista": nombre, "clips": [c]})
        self._guardar()
        self._evento("borrador", parte="kick", pista=nombre, clips=[c["nombre"]])
        return e

    def _nota_elegida(self) -> int | None:
        assert self.estado
        el = self.estado["elegido"]
        return el["nota_pad"] if el else None

    def _carpeta_kicks(self) -> Path:
        assert self.estado
        p = self.estado["partida"]
        return self.carpeta_live / "00 Motor" / f"KICK {p['genero']} {p['raiz']} {p['modo']} {self.estado['id'][:4]}"

    def generar_kicks(self) -> dict:
        """Las dos familias de kick (teoría de kits §16). El rack se crea cuando Juan haya elegido los tres que quiere."""
        e = self._exigir_estado()
        self._exigir_library()
        p = e["partida"]
        tanda = len(e["opciones"]) + 1
        carpeta = self._carpeta_kicks()
        carpeta.mkdir(parents=True, exist_ok=True)
        opciones = []
        for letra, cola in zip(LETRAS, COLAS_KICK):
            y, meta = sintetizar_cola(cola, p["genero"], p["raiz"], p["modo"], tanda)
            ident = f"{tanda}{letra}"
            nombre = cola.lower().replace(" + ", "_").replace(" ", "_")
            ruta = carpeta / f"kick_{nombre}_{p['raiz']}_{p['modo']}_{ident}.wav"
            sf.write(ruta, np.asarray(y), SR, subtype="PCM_24")
            opciones.append({"id": ident, "caracter": cola, "ruta": str(ruta), "seed_timbre": meta["seed_timbre"], "nota_midi": meta.get("nota_midi")})
        e["opciones"].append(opciones)
        self._guardar()
        self._evento("opciones", parte="kick", opciones=[{k: o[k] for k in ("id", "caracter", "seed_timbre", "nota_midi")} for o in opciones])
        return e

    def ruta_opcion(self, ident: str) -> Path:
        e = self._exigir_estado()
        for tanda in e["opciones"]:
            for o in tanda:
                if o["id"] == ident:
                    return Path(o["ruta"])
        raise ValueError(f"No existe el kick {ident!r}")

    def alternar_kick(self, ident: str) -> dict:
        """Suma o quita un kick de la selección. Al llegar a tres se crea el rack y se pone en la pista."""
        e = self._exigir_estado()
        if e["rack"]:
            raise ValueError("Este rack ya está creado: pulsa Nueva selección para armar otro")
        self.ruta_opcion(ident)
        if ident in e["seleccion"]:
            e["seleccion"].remove(ident)
        else:
            e["seleccion"].append(ident)
        if len(e["seleccion"]) == TAMANO_RACK:
            self._crear_y_cargar_rack()
        self._guardar()
        return e

    def _crear_y_cargar_rack(self) -> None:
        assert self.estado
        e = self.estado
        self._exigir_library()
        numero = len(e["racks"]) + 1
        nombre = f"{self._carpeta_kicks().name} R{numero}"
        ruta = self._carpeta_kicks() / f"{nombre}.adg"
        notas = crear_rack([self.ruta_opcion(i) for i in e["seleccion"]], nombre, ruta)
        pista = e["borradores"][-1]["pista"] if e["borradores"] else "KICK"
        rack = {"nombre": nombre, "ruta": str(ruta), "pista": pista, "pads": [{"id": i, "nota": n} for i, n in zip(e["seleccion"], notas)], "cargado": False, "motivo": None}
        try:
            self._exigir_set_editable()
            self.live.cargar_rack([self.carpeta_live.name, "00 Motor", self._carpeta_kicks().name, ruta.name], pista)
            rack["cargado"] = True
        except ErrorLive as exc:
            rack["motivo"] = str(exc)
        e["rack"] = rack
        e["racks"].append(rack)
        presentadas = [o["id"] for tanda in e["opciones"] for o in tanda]
        self._evento("seleccion", parte="kick", seleccion=list(e["seleccion"]), presentadas=presentadas, escuchadas="unknown", rack=nombre, cargado=rack["cargado"])

    def nueva_seleccion(self) -> dict:
        e = self._exigir_estado()
        e["seleccion"], e["rack"] = [], None
        self._guardar()
        return e

    def usar_kick(self, ident: str) -> dict:
        """Regla 3: solo cambia la nota del clip si Juan no lo ha editado."""
        e = self._exigir_estado()
        self._exigir_library()
        if not e["rack"]:
            raise ValueError("Primero elige tres kicks para crear el rack")
        pad = next((p for p in e["rack"]["pads"] if p["id"] == ident), None)
        if pad is None:
            raise ValueError(f"El kick {ident!r} no está en este rack")
        origen = self.ruta_opcion(ident)
        destino = self.carpeta_live / "00 Elegidos" / origen.name
        destino.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(origen, destino)
        colocado = self._colocar_en_clip(pad["nota"])
        e["elegido"] = {"id": ident, "ruta": str(destino), "nota_pad": pad["nota"], "colocado": colocado}
        self._guardar()
        self._evento("eleccion", parte="kick", elegido=ident, presentadas=[p["id"] for p in e["rack"]["pads"]], escuchadas="unknown", colocado=colocado)
        return e

    def _colocar_en_clip(self, nota: int) -> bool:
        assert self.estado
        if not self.estado["borradores"]:
            return False
        b = self.estado["borradores"][-1]
        c = b["clips"][0]
        indice = self.live.indice_pista(b["pista"])
        leido = None if indice is None else self.live.leer_clip(indice, c["inicio"])
        if leido is None or not diferencia(c["notas"], leido["notes"])["sin_cambios"]:
            return False
        if self.info_set()["protegido"]:
            return False
        nuevo = {**c, "notas": [{**n, "pitch": nota} for n in c["notas"]]}
        self.live.escribir_clip(indice, nuevo["inicio"], nuevo["longitud"], nuevo["notas"], nuevo["nombre"])
        self.estado["borradores"].append({"pista": b["pista"], "clips": [nuevo], "deriva_de_eleccion": True})
        return True

    # ── Bajo (motor-bajo.md) ──────────────────────────────────────────
    def generar_patrones(self) -> dict:
        e = self._exigir_estado()
        b = e["bajo"]
        b["tandas"] += 1
        for familia in FAMILIAS:
            b["patrones"].append({"id": f"{b['tandas']}{list(FAMILIAS).index(familia) + 1}", "familia": familia, "tanda": b["tandas"]})
        self._guardar()
        self._evento("opciones", parte="bajo", tanda=b["tandas"], opciones=list(FAMILIAS))
        return e

    def poner_patron(self, ident: str) -> dict:
        """Regla 3: reemplaza el clip solo si sigue como lo escribió el motor; si no, va a otra pista."""
        e = self._exigir_estado()
        self._exigir_set_editable()
        b = e["bajo"]
        op = next((p for p in b["patrones"] if p["id"] == ident), None)
        if op is None:
            raise ValueError(f"No existe el patrón {ident!r}")
        clip = patron(op["familia"], e["partida"]["raiz"], op["tanda"])
        nombre = b["pista"] or "BASS"
        indice = self.live.indice_pista(nombre)
        if indice is None:
            indice = self.live.crear_pista_midi(nombre)
        elif not self.live.pista_sin_clips(indice):
            leido = self.live.leer_clip(indice, 0.0)
            intacto = b["clip"] is not None and leido is not None and diferencia(b["clip"]["notas"], leido["notes"])["sin_cambios"]
            if not intacto:
                nombre = self._nombre_libre("BASS propuesta")
                indice = self.live.crear_pista_midi(nombre)
        self.live.escribir_clip(indice, clip["inicio"], clip["longitud"], clip["notas"], clip["nombre"])
        b.update(pista=nombre, clip=clip, actual=ident)
        self._guardar()
        self._evento("borrador", parte="bajo", pista=nombre, patron=ident, familia=op["familia"], tanda=op["tanda"], capa=b["capa"])
        return e

    def cargar_capas(self) -> dict:
        e = self._exigir_estado()
        nombres = self.brain.capas()
        b = e["bajo"]
        b["total_capas"] = len(nombres)
        b["capas"] = [{"nombre": n, "indice": nombres.index(n)} for n in BAJOS_CONOCIDOS if n in nombres]
        self._guardar()
        return e

    def usar_capa(self, nombre: str) -> dict:
        e = self._exigir_estado()
        b = e["bajo"]
        capa = next((c for c in b["capas"] if c["nombre"] == nombre), None)
        if capa is None:
            raise ValueError(f"La capa {nombre!r} no está en la lista")
        self.brain.elegir_capa(capa["indice"], b["total_capas"])
        b["capa"] = nombre
        self._guardar()
        self._evento("eleccion", parte="sonido_bajo", elegido=nombre, presentadas=[c["nombre"] for c in b["capas"]], escuchadas="unknown", patron=b["actual"])
        return e

    def revisar_bajo(self) -> dict:
        """Solo lee Live y registra."""
        e = self._exigir_estado()
        b = e["bajo"]
        if not b["clip"]:
            raise ValueError("Aún no hay patrón que revisar")
        indice = self.live.indice_pista(b["pista"])
        leido = None if indice is None else self.live.leer_clip(indice, 0.0)
        if leido is None:
            b["revision"] = {"estado": "movido_o_borrado"}
        else:
            b["revision"] = {"estado": "leido", **diferencia(b["clip"]["notas"], leido["notes"])}
        self._guardar()
        self._evento("edicion", parte="bajo", pista=b["pista"], patron=b["actual"], clips=[b["revision"]])
        return e

    # ── Drums (motor-drums.md) ────────────────────────────────────────
    def cambiar_tonalidad(self, raiz: str, modo: str) -> dict:
        """Afecta solo a lo que se genere después."""
        e = self._exigir_estado()
        clase_de(raiz)
        if modo not in MODOS:
            raise ValueError(f"Modo desconocido: {modo!r}")
        if (e["partida"]["raiz"], e["partida"]["modo"]) != (raiz, modo):
            e["partida"].update(raiz=raiz, modo=modo)
            self._guardar()
            self._evento("tonalidad", raiz=raiz, modo=modo)
        return e

    def _carpeta_drums(self) -> Path:
        assert self.estado
        p = self.estado["partida"]
        return self.carpeta_live / "00 Motor" / f"DRUMS {p['genero']} {p['raiz']} {p['modo']} {self.estado['id'][:4]}"

    def generar_drums(self) -> dict:
        e = self._exigir_estado()
        self._exigir_library()
        p = e["partida"]
        d = e["drums"]
        d["tandas"] += 1
        tanda = d["tandas"]
        carpeta = self._carpeta_drums()
        carpeta.mkdir(parents=True, exist_ok=True)
        nuevas = []
        for grupo, voces in GRUPOS_DRUMS.items():
            for material in MATERIALES_DRUMS:
                archivos = {}
                nota = None
                for voz in voces:
                    y, meta = sintetizar_drum(voz, material, p["genero"], p["raiz"], p["modo"], tanda)
                    ruta = carpeta / f"{voz}_{material.lower()}_{p['raiz']}_{p['modo']}_t{tanda}.wav"
                    sf.write(ruta, np.asarray(y), SR, subtype="PCM_24")
                    archivos[voz] = str(ruta)
                    nota = meta.get("nota_midi")
                nuevas.append({"id": f"{grupo.lower()}-{material.lower()}-{tanda}", "grupo": grupo, "material": material, "tanda": tanda, "archivos": archivos, "nota_midi": nota, "raiz": p["raiz"], "modo": p["modo"]})
        d["opciones"].extend(nuevas)
        self._guardar()
        self._evento("opciones", parte="drums", tanda=tanda, opciones=[o["id"] for o in nuevas])
        return e

    def ruta_drum(self, ident: str, voz: str) -> Path:
        e = self._exigir_estado()
        for o in e["drums"]["opciones"]:
            if o["id"] == ident and voz in o["archivos"]:
                return Path(o["archivos"][voz])
        raise ValueError(f"No existe {ident!r} para {voz!r}")

    def elegir_drum(self, ident: str) -> dict:
        """Al completar los cuatro grupos se crea el rack; cambiar uno después crea el siguiente."""
        e = self._exigir_estado()
        d = e["drums"]
        op = next((o for o in d["opciones"] if o["id"] == ident), None)
        if op is None:
            raise ValueError(f"No existe la opción {ident!r}")
        d["seleccion"][op["grupo"]] = ident
        if len(d["seleccion"]) == len(GRUPOS_DRUMS):
            self._crear_rack_drums()
        self._guardar()
        return e

    def _crear_rack_drums(self) -> None:
        assert self.estado
        e = self.estado
        d = e["drums"]
        self._exigir_library()
        por_id = {o["id"]: o for o in d["opciones"]}
        wavs, notas, pads = [], [], []
        for grupo, voces in GRUPOS_DRUMS.items():
            op = por_id[d["seleccion"][grupo]]
            for voz in voces:
                wavs.append(Path(op["archivos"][voz]))
                notas.append(NOTAS_PERC[voz])
                pads.append({"voz": voz, "nota": NOTAS_PERC[voz], "id": op["id"], "material": op["material"]})
        numero = len(d["racks"]) + 1
        nombre = f"{self._carpeta_drums().name} R{numero}"
        ruta = self._carpeta_drums() / f"{nombre}.adg"
        ruta.parent.mkdir(parents=True, exist_ok=True)
        crear_rack(wavs, nombre, ruta, notas=notas, choke={NOTAS_PERC["hat_cerrado"]: CHOKE_HATS, NOTAS_PERC["hat_abierto"]: CHOKE_HATS})
        rack = {"nombre": nombre, "ruta": str(ruta), "pista": "DRUMS", "pads": pads, "cargado": False, "motivo": None}
        try:
            self._exigir_set_editable()
            if self.live.indice_pista("DRUMS") is None:
                self.live.crear_pista_midi("DRUMS")
            self.live.cargar_rack([self.carpeta_live.name, "00 Motor", self._carpeta_drums().name, ruta.name], "DRUMS")
            rack["cargado"] = True
        except ErrorLive as exc:
            rack["motivo"] = str(exc)
        d["rack"] = rack
        d["racks"].append(rack)
        presentadas = [o["id"] for o in d["opciones"]]
        self._evento("seleccion", parte="drums", seleccion=dict(d["seleccion"]), presentadas=presentadas, escuchadas="unknown", rack=nombre, cargado=rack["cargado"])

    def _notas_de(self, pista: str) -> list[dict]:
        """Notas de una pista de Live, con su tiempo absoluto."""
        indice = self.live.indice_pista(pista)
        if indice is None:
            return []
        clips = self.live.listar_clips(indice)
        notas = []
        for i, c in enumerate(clips):
            leido = self.live.leer_clip(indice, c["start"])
            if leido:
                notas += notas_en_arreglo({**c, "notes": leido["notes"]}, clips[i + 1]["start"] if i + 1 < len(clips) else None)
        return notas

    def proponer_patrones_drums(self) -> dict:
        """Solo lee Live. Cada propuesta usa el kick y el bajo como están ahora, con las ediciones de Juan."""
        e = self._exigir_estado()
        p = e["drums"]["patrones"]
        kick, bajo = self._notas_de("KICK"), self._notas_de("BASS")
        p["analisis"] = analizar(kick, bajo)
        p["opciones"] = opciones_drums(p["analisis"])
        p["sin_bajo"] = not bajo
        self._guardar()
        self._evento("opciones", parte="patron_drums", analisis=p["analisis"], opciones=p["opciones"])
        return e

    def poner_patron_drums(self, nombre: str) -> dict:
        """Regla 3: reemplaza el clip solo si sigue como lo escribió el motor; si no, va a otra pista."""
        e = self._exigir_estado()
        self._exigir_set_editable()
        p = e["drums"]["patrones"]
        op = next((o for o in p["opciones"] if o["nombre"] == nombre), None)
        if op is None:
            raise ValueError(f"No existe el patrón {nombre!r}")
        clip = clip_drums(op)
        pista = p["pista"] or "DRUMS"
        indice = self.live.indice_pista(pista)
        if indice is None:
            indice = self.live.crear_pista_midi(pista)
        elif not self.live.pista_sin_clips(indice):
            leido = self.live.leer_clip(indice, 0.0)
            intacto = p["clip"] is not None and leido is not None and diferencia(p["clip"]["notas"], leido["notes"])["sin_cambios"]
            if not intacto:
                pista = self._nombre_libre("DRUMS propuesta")
                indice = self.live.crear_pista_midi(pista)
        self.live.escribir_clip(indice, clip["inicio"], clip["longitud"], clip["notas"], clip["nombre"])
        p.update(actual=nombre, clip=clip, pista=pista)
        self._guardar()
        self._evento("borrador", parte="patron_drums", pista=pista, patron=nombre, clave=op["clave"], analisis=p["analisis"])
        return e

    # ── Tops (motor-tops.md) ──────────────────────────────────────────
    def proponer_tops(self, rol: str) -> dict:
        """Solo lee Live y la Library."""
        e = self._exigir_estado()
        self._exigir_library()
        if rol not in e["tops"]:
            raise ValueError(f"Papel desconocido: {rol!r}")
        kick, bajo = self._notas_de("KICK"), self._notas_de("BASS")
        a = analizar(kick, bajo)
        clave = sorted({round(n["start_beat"] * 4) % 16 for n in self._notas_de("DRUMS") if n["pitch"] == 47})
        opciones = candidatos(self.carpeta_live, rol, e["partida"]["genero"])
        for o in opciones:
            notas, _ = leer_midi(self.carpeta_live / o["ruta"])
            o["encaje"] = encaje(notas, a["kick"], a["bajo"], clave)
        e["tops"][rol]["opciones"] = opciones
        self._guardar()
        self._evento("opciones", parte=rol, opciones=[{k: o[k] for k in ("id", "nombre", "decision_juan", "encaje")} for o in opciones])
        return e

    def poner_top(self, rol: str, ident: str) -> dict:
        """Regla 3: reemplaza el clip solo si sigue como lo escribió el motor; si no, va a otra pista."""
        e = self._exigir_estado()
        self._exigir_set_editable()
        estado = e["tops"].get(rol)
        if estado is None:
            raise ValueError(f"Papel desconocido: {rol!r}")
        op = next((o for o in estado["opciones"] if o["id"] == ident), None)
        if op is None:
            raise ValueError(f"No existe la opción {ident!r}")
        notas, largo = leer_midi(self.carpeta_live / op["ruta"])
        clip = clip_loop(op["nombre"], notas, largo)
        base = rol.upper()
        pista = estado["pista"] or base
        indice = self.live.indice_pista(pista)
        if indice is None:
            indice = self.live.crear_pista_midi(pista)
        elif not self.live.pista_sin_clips(indice):
            leido = self.live.leer_clip(indice, 0.0)
            intacto = estado["clip"] is not None and leido is not None and diferencia(estado["clip"]["notas"], leido["notes"])["sin_cambios"]
            if not intacto:
                pista = self._nombre_libre(f"{base} propuesta")
                indice = self.live.crear_pista_midi(pista)
        self.live.escribir_clip(indice, clip["inicio"], clip["longitud"], clip["notas"], clip["nombre"])
        estado.update(actual=ident, clip=clip, pista=pista)
        self._guardar()
        self._evento("borrador", parte=rol, pista=pista, origen=op["ruta"], grupo=op["grupo"], decision_juan=op["decision_juan"], encaje=op.get("encaje"))
        return e

    # ── Motivo (motor-motivo.md) ──────────────────────────────────────
    def generar_motivos(self) -> dict:
        """Solo lee Live: los ritmos RESPUESTA y CON CLAVE salen del groove actual."""
        e = self._exigir_estado()
        m = e["motivo"]
        a = analizar(self._notas_de("KICK"), self._notas_de("BASS"))
        clave = sorted({round(n["start_beat"] * 4) % 16 for n in self._notas_de("DRUMS") if n["pitch"] == 47})
        ritmos = ritmos_motivo(a["libres"], clave)
        m["tandas"] += 1
        p = e["partida"]
        for nombre, ritmo in ritmos.items():
            m["opciones"].append({"id": f"{m['tandas']}-{nombre}", "nombre": nombre, "tanda": m["tandas"], "ritmo": [list(x) for x in ritmo], "raiz": p["raiz"], "modo": p["modo"]})
        self._guardar()
        self._evento("opciones", parte="motivo", tanda=m["tandas"], opciones=list(ritmos), modo=p["modo"], libres=a["libres"], clave=clave)
        return e

    def poner_motivo(self, ident: str) -> dict:
        """Regla 3: reemplaza el clip solo si sigue como lo escribió el motor; si no, va a otra pista."""
        e = self._exigir_estado()
        self._exigir_set_editable()
        m = e["motivo"]
        op = next((o for o in m["opciones"] if o["id"] == ident), None)
        if op is None:
            raise ValueError(f"No existe el motivo {ident!r}")
        ritmo = tuple(tuple(x) for x in op["ritmo"])
        clip = crear_motivo(op["nombre"], ritmo, op["raiz"], op["modo"], MODOS[op["modo"]], op["tanda"])
        pista = m["pista"] or "LEAD"
        indice = self.live.indice_pista(pista)
        if indice is None:
            indice = self.live.crear_pista_midi(pista)
        elif not self.live.pista_sin_clips(indice):
            leido = self.live.leer_clip(indice, 0.0)
            intacto = m["clip"] is not None and leido is not None and diferencia(m["clip"]["notas"], leido["notes"])["sin_cambios"]
            if not intacto:
                pista = self._nombre_libre("LEAD propuesta")
                indice = self.live.crear_pista_midi(pista)
        self.live.escribir_clip(indice, clip["inicio"], clip["longitud"], clip["notas"], clip["nombre"])
        m.update(actual=ident, clip=clip, pista=pista)
        self._guardar()
        self._evento("borrador", parte="motivo", pista=pista, motivo=ident, modo=op["modo"], capa=m["capa"])
        return e

    def cargar_capas_motivo(self) -> dict:
        e = self._exigir_estado()
        nombres = self.brain.capas()
        m = e["motivo"]
        m["total_capas"] = len(nombres)
        m["capas"] = [{"nombre": n, "indice": nombres.index(n)} for n in MELODICOS_CONOCIDOS if n in nombres]
        self._guardar()
        return e

    def usar_capa_motivo(self, nombre: str) -> dict:
        e = self._exigir_estado()
        m = e["motivo"]
        capa = next((c for c in m["capas"] if c["nombre"] == nombre), None)
        if capa is None:
            raise ValueError(f"La capa {nombre!r} no está en la lista")
        self.brain.elegir_capa(capa["indice"], m["total_capas"])
        m["capa"] = nombre
        self._guardar()
        self._evento("eleccion", parte="sonido_motivo", elegido=nombre, presentadas=[c["nombre"] for c in m["capas"]], escuchadas="unknown", motivo=m["actual"])
        return e

    # ── Acordes (motor-acordes.md) ────────────────────────────────────
    def proponer_acordes(self) -> dict:
        """Solo lee Live. Vuelve a calcular con el groove y la melodía como están ahora."""
        e = self._exigir_estado()
        p = e["partida"]
        escala = MODOS[p["modo"]]
        a = analizar(self._notas_de("KICK"), self._notas_de("BASS"))
        clave = sorted({round(n["start_beat"] * 4) % 16 for n in self._notas_de("DRUMS") if n["pitch"] == 47})
        pista_melodia = next((n for n in ("LEAD", "MELODY", "MOTIVO") if self._notas_de(n)), None)
        melodia = self._notas_de(pista_melodia) if pista_melodia else []
        patrones = patrones_acordes(a["libres"], clave)
        opciones = []
        for prog in PROGRESIONES:
            for nombre, definicion in patrones.items():
                opciones.append(
                    {
                        "id": f"{prog}|{nombre}", "progresion": prog, "acordes": nombres_progresion(prog, p["raiz"], escala), "patron": nombre, "tipo": definicion[0],
                        "pasos": list(definicion[1]), "raiz": p["raiz"], "modo": p["modo"], "choques": choques(melodia, prog, definicion[0] == "septima", p["raiz"], escala),
                    }
                )
        estado = e["acordes"]
        estado["opciones"] = opciones
        estado["melodia"] = pista_melodia
        self._guardar()
        self._evento("opciones", parte="acordes", modo=p["modo"], progresiones=list(PROGRESIONES), patrones=list(patrones), melodia=pista_melodia)
        return e

    def poner_acordes(self, ident: str) -> dict:
        """Regla 3: reemplaza el clip solo si sigue como lo escribió el motor; si no, va a otra pista."""
        e = self._exigir_estado()
        self._exigir_set_editable()
        estado = e["acordes"]
        op = next((o for o in estado["opciones"] if o["id"] == ident), None)
        if op is None:
            raise ValueError(f"No existe la opción {ident!r}")
        clip = clip_acordes(op["progresion"], op["patron"], (op["tipo"], tuple(op["pasos"])), op["raiz"], MODOS[op["modo"]])
        pista = estado["pista"] or "CHORDS"
        indice = self.live.indice_pista(pista)
        if indice is None:
            indice = self.live.crear_pista_midi(pista)
        elif not self.live.pista_sin_clips(indice):
            leido = self.live.leer_clip(indice, 0.0)
            intacto = estado["clip"] is not None and leido is not None and diferencia(estado["clip"]["notas"], leido["notes"])["sin_cambios"]
            if not intacto:
                pista = self._nombre_libre("CHORDS propuesta")
                indice = self.live.crear_pista_midi(pista)
        self._asegurar_instrumento(pista)
        self.live.escribir_clip(indice, clip["inicio"], clip["longitud"], clip["notas"], clip["nombre"])
        estado.update(actual=ident, clip=clip, pista=pista)
        self._guardar()
        self._evento("borrador", parte="acordes", pista=pista, progresion=op["progresion"], acordes=op["acordes"], patron=op["patron"], modo=op["modo"], choques=op["choques"])
        return e

    # ── Pads y atmósfera (motor-pads.md) ─────────────────────────────
    def proponer_pads(self) -> dict:
        """Solo lee el estado: la progresión que quedó en Acordes. Cada lectura da otra tanda de la nube."""
        e = self._exigir_estado()
        pads = e["pads"]
        pads["progresion"] = self._progresion_actual(e)
        pads["tanda"] += 1
        for parte, patrones in (("pad", PATRONES_PAD), ("atmos", PATRONES_ATMOS)):
            pads[parte]["opciones"] = [{"id": p, "patron": p, "tanda": pads["tanda"]} for p in patrones]
        self._guardar()
        self._evento("opciones", parte="pads", progresion=pads["progresion"], tanda=pads["tanda"], pad=list(PATRONES_PAD), atmos=list(PATRONES_ATMOS))
        return e

    def _progresion_actual(self, e: dict) -> str | None:
        """La que se puso desde Acordes; si esta sesión no la tiene, la del nombre de un clip en una pista CHORDS*."""
        actual = e["acordes"]["actual"]
        if actual:
            return actual.split("|")[0]
        for p in self.live.sesion()["tracks"]:
            if p["name"].upper().startswith("CHORDS"):
                for c in self.live.listar_clips(p["index"]):
                    primera = (c.get("name") or "").split(" ")[0]
                    if primera in PROGRESIONES:
                        return primera
        return None

    def poner_pad(self, parte: str, ident: str) -> dict:
        """Regla 3: reemplaza el clip solo si sigue como lo escribió el motor; si no, va a otra pista."""
        e = self._exigir_estado()
        self._exigir_set_editable()
        if parte not in ("pad", "atmos"):
            raise ValueError(f"Parte desconocida {parte!r}")
        pads, p = e["pads"], e["partida"]
        estado = pads[parte]
        op = next((o for o in estado["opciones"] if o["id"] == ident), None)
        if op is None:
            raise ValueError(f"No existe la opción {ident!r}")
        clip = clip_pad(parte, op["patron"], pads["progresion"], p["raiz"], MODOS[p["modo"]], op["tanda"])
        base = "PAD" if parte == "pad" else "ATMOS"
        pista = estado["pista"] or base
        indice = self.live.indice_pista(pista)
        if indice is None:
            indice = self.live.crear_pista_midi(pista)
        elif not self.live.pista_sin_clips(indice):
            leido = self.live.leer_clip(indice, 0.0)
            intacto = estado["clip"] is not None and leido is not None and diferencia(estado["clip"]["notas"], leido["notes"])["sin_cambios"]
            if not intacto:
                pista = self._nombre_libre(f"{base} propuesta")
                indice = self.live.crear_pista_midi(pista)
        self._asegurar_instrumento(pista)
        self.live.escribir_clip(indice, clip["inicio"], clip["longitud"], clip["notas"], clip["nombre"])
        estado.update(actual=ident, clip=clip, pista=pista)
        self._guardar()
        self._evento("borrador", parte=parte, pista=pista, patron=op["patron"], progresion=pads["progresion"], tanda=op["tanda"])
        return e

    # ── Stabs y plucks (motor-detalles.md) ───────────────────────────
    def proponer_detalles(self) -> dict:
        """Solo lee Live: acordes, lead y kick. Avisa si otra pista ya cumple el papel; no quita opciones."""
        e = self._exigir_estado()
        d = e["detalles"]
        d["progresion"] = self._progresion_actual(e)
        d["tanda"] += 1
        pista_lead = next((n for n in ("LEAD", "MELODY", "MOTIVO") if self._notas_de(n)), None)
        lead = self._notas_de(pista_lead) if pista_lead else []
        lead = [{**n, "start_beat": n["start_beat"] % 64} for n in lead]  # Se pliega a los 16 compases del clip.
        nombres = [p["name"] for p in self.live.sesion()["tracks"] if p["name"].upper().startswith("CHORDS")]
        acordes = {n: self._notas_de(n) for n in nombres}
        libres = huecos_lead(lead)
        a = avisos_detalles(acordes, lead, libres)
        d.update(libres=libres, kick=analizar(self._notas_de("KICK"), [])["kick"], lead=pista_lead, riff_aviso=a["riff"], lead_notas=lead, recomendacion=recomendar(acordes, lead, libres))
        d["stab"]["aviso"], d["pluck"]["aviso"] = a["stab"], a["pluck"]
        for parte, patrones in (("stab", PATRONES_STAB), ("pluck", PATRONES_PLUCK)):
            d[parte]["opciones"] = [{"id": p, "patron": p, "tanda": d["tanda"]} for p in patrones]
        self._guardar()
        self._evento("opciones", parte="detalles", progresion=d["progresion"], lead=pista_lead, avisos=a, tanda=d["tanda"])
        return e

    def poner_detalle(self, parte: str, ident: str) -> dict:
        e = self._exigir_estado()
        self._exigir_set_editable()
        if parte not in ("stab", "pluck"):
            raise ValueError(f"Parte desconocida {parte!r}")
        d, p = e["detalles"], e["partida"]
        estado = d[parte]
        op = next((o for o in estado["opciones"] if o["id"] == ident), None)
        if op is None:
            raise ValueError(f"No existe la opción {ident!r}")
        clip = clip_detalle(parte, op["patron"], d["progresion"], p["raiz"], MODOS[p["modo"]], d["libres"], d["kick"], d.get("lead_notas") or [], op["tanda"])
        self._poner_unico(estado, "STABS" if parte == "stab" else "PLUCKS", clip)
        estado["actual"] = ident
        self._guardar()
        self._evento("borrador", parte=parte, pista=estado["pista"], patron=op["patron"], progresion=d["progresion"], aviso=estado["aviso"])
        return e

    def _poner_unico(self, estado: dict, base: str, clip: dict) -> None:
        """Regla 3: reemplaza el clip solo si sigue como lo escribió el motor; si no, va a otra pista."""
        pista = estado["pista"] or base
        indice = self.live.indice_pista(pista)
        if indice is None:
            indice = self.live.crear_pista_midi(pista)
        elif not self.live.pista_sin_clips(indice):
            leido = self.live.leer_clip(indice, 0.0)
            intacto = estado["clip"] is not None and leido is not None and diferencia(estado["clip"]["notas"], leido["notes"])["sin_cambios"]
            if not intacto:
                pista = self._nombre_libre(f"{base} propuesta")
                indice = self.live.crear_pista_midi(pista)
        self._asegurar_instrumento(pista)
        self.live.escribir_clip(indice, clip["inicio"], clip["longitud"], clip["notas"], clip["nombre"])
        estado.update(clip=clip, pista=pista)

    # ── Transiciones (motor-transiciones.md) ─────────────────────────
    def proponer_transiciones(self) -> dict:
        """Solo lee la pista ARRANGE."""
        e = self._exigir_estado()
        indice = self.live.indice_pista("ARRANGE")
        if indice is None:
            raise ValueError("No hay pista ARRANGE: crea la guía en Arrangement primero")
        t = e["transiciones"]
        t["secciones"] = secciones(self.live.listar_clips(indice))
        for elemento, (_, patrones) in ELEMENTOS.items():
            t[elemento]["opciones"] = [{"id": p, "patron": p, "clips": len(clips_transicion(elemento, p, t["secciones"], e["partida"]["raiz"], MODOS[e["partida"]["modo"]]))} for p in patrones]
        self._guardar()
        self._evento("opciones", parte="transiciones", secciones=[s["nombre"] for s in t["secciones"]])
        return e

    def poner_transicion(self, elemento: str, ident: str) -> dict:
        e = self._exigir_estado()
        self._exigir_set_editable()
        if elemento not in ELEMENTOS:
            raise ValueError(f"Elemento desconocido {elemento!r}")
        t, p = e["transiciones"], e["partida"]
        estado = t[elemento]
        op = next((o for o in estado["opciones"] if o["id"] == ident), None)
        if op is None:
            raise ValueError(f"No existe la opción {ident!r}")
        clips = clips_transicion(elemento, op["patron"], t["secciones"], p["raiz"], MODOS[p["modo"]])
        if not clips:
            raise ValueError(f"Tu arreglo no tiene dónde poner {op['patron']}: revisa los nombres de ARRANGE")
        if elemento == "subdrop" and not t["rack_subdrop"]:
            self.sonido_subdrop("CORTA")  # sin sonido elegido, el corto: cabe en un INTER de un compás
        base = ELEMENTOS[elemento][0]
        pista = estado["pista"] or base
        indice = self.live.indice_pista(pista)
        previos: list[dict] = []
        if indice is None:
            indice = self.live.crear_pista_midi(pista)
        else:
            actuales = self.live.listar_clips(indice)
            if actuales and not self._clips_intactos(indice, actuales, estado["clips"]):
                pista = self._nombre_libre(f"{base} propuesta")
                indice = self.live.crear_pista_midi(pista)
            else:
                previos = estado["clips"] or []
        finales = _unir_con_previos(clips, previos)
        self._asegurar_instrumento(pista)
        for c in finales:
            self.live.escribir_clip(indice, c["inicio"], c["longitud"], c["notas"], c["nombre"])
        estado.update(actual=ident, clips=finales, pista=pista)
        self._guardar()
        self._evento("borrador", parte=elemento, pista=pista, patron=op["patron"], puntos=[c["inicio"] for c in clips])
        return e

    def sonido_subdrop(self, variante: str) -> dict:
        """Subdrop de la drum machine (seno que cae a la tónica) en un rack para la pista SUBDROP."""
        e = self._exigir_estado()
        self._exigir_library()
        if variante not in SUBDROPS:
            raise ValueError(f"Subdrop desconocido {variante!r}")
        p, t = e["partida"], e["transiciones"]
        numero = (t["rack_subdrop"] or {}).get("numero", 0) + 1
        carpeta = self.carpeta_live / "00 Motor" / f"SUBDROP {p['genero']} {p['raiz']} {p['modo']} {e['id'][:4]}"
        carpeta.mkdir(parents=True, exist_ok=True)
        y, _ = sintetizar_subdrop(variante, p["raiz"], p["modo"])
        wav = carpeta / f"subdrop_{variante.lower()}_{p['raiz']}_{p['modo']}.wav"
        sf.write(wav, np.asarray(y), SR, subtype="PCM_24")
        nombre = f"DRUMS subdrop-{variante.lower()} {p['raiz']} {p['modo']} {e['id'][:4]} R{numero}"
        ruta = carpeta / f"{nombre}.adg"
        crear_rack([wav], nombre, ruta, notas=[NOTA_SUBDROP])
        rack = {"nombre": nombre, "variante": variante, "numero": numero, "cargado": [], "motivo": None}
        try:
            self._exigir_set_editable()
            if self.live.indice_pista("SUBDROP") is None:
                self.live.crear_pista_midi("SUBDROP")
            self.live.cargar_rack([self.carpeta_live.name, "00 Motor", carpeta.name, ruta.name], "SUBDROP")
            rack["cargado"].append("SUBDROP")
        except ErrorLive as exc:
            rack["motivo"] = str(exc)
        t["rack_subdrop"] = rack
        self._guardar()
        self._evento("eleccion", parte="sonido_subdrop", elegido=variante, presentadas=list(SUBDROPS), rack=nombre)
        return e

    def sonido_roll(self, material: str) -> dict:
        """Snare y clap de la drum machine (afinados a la fundamental) en un rack para ROLL.
        El nombre empieza por DRUMS para que GFMotor quite el rack anterior de la pista al cargar el nuevo."""
        e = self._exigir_estado()
        self._exigir_library()
        if material not in MATERIALES_DRUMS:
            raise ValueError(f"Material desconocido {material!r}")
        p, t = e["partida"], e["transiciones"]
        numero = (t["rack_roll"] or {}).get("numero", 0) + 1
        carpeta = self.carpeta_live / "00 Motor" / f"ROLL {p['genero']} {p['raiz']} {p['modo']} {e['id'][:4]}"
        carpeta.mkdir(parents=True, exist_ok=True)
        wavs = []
        for voz in ("snare", "clap"):
            y, _ = sintetizar_drum(voz, material, p["genero"], p["raiz"], p["modo"], 1)
            ruta = carpeta / f"{voz}_{material.lower()}_{p['raiz']}_{p['modo']}.wav"
            sf.write(ruta, np.asarray(y), SR, subtype="PCM_24")
            wavs.append(ruta)
        nombre = f"DRUMS roll-{material.lower()} {p['raiz']} {p['modo']} {e['id'][:4]} R{numero}"
        ruta = carpeta / f"{nombre}.adg"
        crear_rack(wavs, nombre, ruta, notas=[NOTAS_PERC["snare"], NOTAS_PERC["clap"]])
        rack = {"nombre": nombre, "material": material, "numero": numero, "cargado": [], "motivo": None}
        try:
            self._exigir_set_editable()
            if self.live.indice_pista("ROLL") is None:
                self.live.crear_pista_midi("ROLL")
            self.live.cargar_rack([self.carpeta_live.name, "00 Motor", carpeta.name, ruta.name], "ROLL")
            rack["cargado"].append("ROLL")
        except ErrorLive as exc:
            rack["motivo"] = str(exc)
        t["rack_roll"] = rack
        self._guardar()
        self._evento("eleccion", parte="sonido_roll", elegido=material, presentadas=list(MATERIALES_DRUMS), rack=nombre)
        return e

    def _asegurar_instrumento(self, pista: str) -> None:
        """Pone el instrumento de la pista si aún no tiene uno, para que Juan solo elija el preset. Nunca reemplaza
        lo que haya y nunca frena el paso: si falla, se anota y sigue. BASS y LEAD quedan fuera: pueden ir al Brain."""
        base = re.sub(r"( propuesta)?( \d+)?$", "", pista)
        plugin = INSTRUMENTOS.get(base)
        if plugin is None or self.estado is None:
            return
        try:
            r = self.live.cargar_plugin(plugin, pista)
            self.estado.setdefault("instrumentos", {})[pista] = plugin if r.get("cargado") else r.get("motivo", "")
        except ErrorLive as exc:
            self.estado.setdefault("instrumentos", {})[pista] = f"no se cargó: {exc}"

    def _clips_intactos(self, indice: int, actuales: list[dict], escritos: list[dict] | None) -> bool:
        if not escritos or len(actuales) != len(escritos):
            return False
        for c in actuales:
            propio = next((x for x in escritos if abs(x["inicio"] - c["start"]) < 1e-3), None)
            leido = self.live.leer_clip(indice, c["start"])
            if propio is None or leido is None or not diferencia(propio["notas"], leido["notes"])["sin_cambios"]:
                return False
        return True

    # ── Armado (motor-armado.md) ─────────────────────────────────────
    def proponer_armado(self) -> dict:
        """Solo lee Live: secciones de ARRANGE y el material de cada pista (su clip del compás 1)."""
        e = self._exigir_estado()
        indice = self.live.indice_pista("ARRANGE")
        if indice is None:
            raise ValueError("No hay pista ARRANGE: crea la guía en Arrangement primero")
        a = e["armado"]
        secs = secciones(self.live.listar_clips(indice))
        roles, materiales = [], {}
        for p in self.live.sesion()["tracks"]:
            nombre = p["name"]
            if nombre.upper() in PISTAS_FIJAS or nombre.endswith(" propuesta") or nombre in roles:
                continue
            if nombre in a["escritos"]:
                mat = a["materiales"].get(nombre)  # ya armado: el material vive en el estado
            else:
                clips = self.live.listar_clips(p["index"])
                mat = material_de(clips, self._notas_de(nombre))
                if mat:
                    mat["clips"] = [
                        {"inicio": c["start"], "notas": (self.live.leer_clip(p["index"], c["start"]) or {}).get("notes", [])}
                        for c in clips if c["start"] < mat["desde"] + mat["largo"] - 1e-6
                    ]
            if mat:
                roles.append(nombre)
                materiales[nombre] = mat
        firma = ([s["nombre"] for s in secs], roles)
        if firma != ([s["nombre"] for s in a["secciones"]], a["roles"]):
            a["presencia"] = presencia_inicial(roles, secs)
        a.update(secciones=secs, roles=roles, materiales=materiales, saltados=[])
        self._guardar()
        self._evento("opciones", parte="armado", secciones=[s["nombre"] for s in secs], roles=roles)
        return e

    def cambiar_presencia(self, rol: str, i: int, valor: str) -> dict:
        e = self._exigir_estado()
        a = e["armado"]
        if rol not in a["presencia"] or not 0 <= i < len(a["secciones"]) or valor not in (TODO, NADA, PARCIAL):
            raise ValueError("Casilla inválida")
        a["presencia"][rol][i] = valor
        self._guardar()
        return e

    def llenar_armado(self) -> dict:
        """Registra el set, borra lo que escribió el motor (y el material ya guardado) y llena según la cuadrícula.
        Un tramo con un clip que no es del motor se salta."""
        e = self._exigir_estado()
        self._exigir_set_editable()
        a = e["armado"]
        if not a["roles"]:
            raise ValueError("Primero lee el arreglo y los materiales")
        if not a["registrado"]:
            self.registrar(nota="antes de armar: materiales del compás 1")
            a["registrado"] = True
        a["saltados"] = []
        for rol in a["roles"]:
            indice = self.live.indice_pista(rol)
            if indice is None:
                continue
            mat = a["materiales"][rol]
            nuevos = []
            for s, valor in zip(a["secciones"], a["presencia"][rol]):
                t = tramo(rol, s, valor)
                if t:
                    nuevos += clips_de_tramo(mat, t[0], t[1], f"{rol} · {s['nombre']}")
            conocidos = list(a["escritos"].get(rol, [])) + list(mat.get("clips", []))
            ajenos = []
            for c in self.live.listar_clips(indice):
                propio = next((x for x in conocidos if abs(x["inicio"] - c["start"]) < 1e-3), None)
                leido = self.live.leer_clip(indice, c["start"])
                if propio is not None and leido is not None and diferencia(propio["notas"], leido["notes"])["sin_cambios"]:
                    self.live.borrar_clip(indice, c["start"])
                else:
                    ajenos.append((c["start"], c.get("end", c["start"] + c["length"])))
            escritos = []
            for c in nuevos:
                fin = c["inicio"] + c["longitud"]
                if any(x < fin - 1e-6 and y > c["inicio"] + 1e-6 for x, y in ajenos):
                    a["saltados"].append(f"{rol} compás {c['inicio'] / 4 + 1:g}")
                    continue
                self.live.escribir_clip(indice, c["inicio"], c["longitud"], c["notas"], c["nombre"])
                escritos.append({"inicio": c["inicio"], "notas": c["notas"]})
            a["escritos"][rol] = escritos
            mat["clips"] = []  # el material ya no está en Live: vive en el estado y en el registro
        self._guardar()
        self._evento("borrador", parte="armado", roles=a["roles"], presencia=a["presencia"], saltados=a["saltados"])
        return e

    # ── Registro de lo que quedó ───────────────────────────────────────
    def registrar(self, sonido: str = "", kick: str = "", nota: str = "") -> dict:
        """Solo lee Live. Guarda todas las notas del set y los MIDI de kick y bajo; nunca sobrescribe un registro."""
        e = self._exigir_estado()
        s = self.live.sesion()
        marca = gf_log.now().strftime("%Y%m%dT%H%M%S")
        pistas = []
        for p in s["tracks"]:
            try:
                clips = self.live.listar_clips(p["index"])
            except ErrorLive:
                clips = []
            detalle = []
            for c in clips:
                try:
                    leido = self.live.leer_clip(p["index"], c["start"])
                except ErrorLive:
                    leido = None  # Pista de audio: el clip existe, pero no tiene notas.
                detalle.append({**c, "notes": leido["notes"] if leido else None})
            pistas.append({"name": p["name"], "index": p["index"], "clips": detalle})
        registro = {
            "ts": marca,
            "set": s.get("file_path") or "",
            "tempo": s["tempo"],
            "partida": e["partida"],
            "sonido_bajo": sonido.strip(),
            "kick": kick.strip(),
            "nota": nota.strip(),
            "pistas": pistas,
        }
        carpeta = self._carpeta() / "registros"
        carpeta.mkdir(parents=True, exist_ok=True)
        ruta = carpeta / f"{marca}.json"
        ruta.write_text(json.dumps(registro, ensure_ascii=False, indent=1))
        midis = self._exportar_midis(pistas, s, marca)
        resumen = {
            "ts": marca,
            "set": registro["set"],
            "sonido_bajo": registro["sonido_bajo"],
            "kick": registro["kick"],
            "nota": registro["nota"],
            "archivo": str(ruta),
            "midis": midis,
            "pistas": {p["name"]: sum(len(c["notes"] or []) for c in p["clips"]) for p in pistas},
            "sin_guardar": not registro["set"],
        }
        e["registros"].append(resumen)
        self._guardar()
        self._evento("registro", **{k: resumen[k] for k in ("set", "sonido_bajo", "kick", "nota", "archivo", "midis", "pistas")}, tempo=s["tempo"])
        return e

    def _exportar_midis(self, pistas: list[dict], s: dict, marca: str) -> list[str]:
        if not self.carpeta_live.parent.exists():
            return []  # Sin SSD: las notas ya quedaron en el JSON del dataset.
        import mido

        salida = []
        carpeta = self.carpeta_live / "00 Elegidos" / "registros"
        carpeta.mkdir(parents=True, exist_ok=True)
        for p in pistas:
            notas = [
                n
                for i, c in enumerate(p["clips"])
                for n in notas_en_arreglo(c, p["clips"][i + 1]["start"] if i + 1 < len(p["clips"]) else None)
            ]
            if p["name"] in ("SC GHOST", "ARRANGE", "GUIA") or not notas:  # Guías y sidechain no son material musical.
                continue
            archivo = mido.MidiFile(ticks_per_beat=480)
            pista = mido.MidiTrack()
            archivo.tracks.append(pista)
            pista.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(s["tempo"])))
            pista.append(mido.MetaMessage("track_name", name=p["name"]))
            eventos = []
            for n in notas:
                inicio = round(n["start_beat"] * 480)
                eventos += [(inicio + round(n["duration_beats"] * 480), 0, n["pitch"], 0), (inicio, 1, n["pitch"], n["velocity"])]
            ahora = 0
            for tiempo, tipo, nota, velocidad in sorted(eventos):
                pista.append(mido.Message("note_on" if tipo else "note_off", note=nota, velocity=velocidad, time=tiempo - ahora))
                ahora = tiempo
            ruta = carpeta / f"{p['name'].lower()}_{marca}.mid"
            archivo.save(ruta)
            salida.append(str(ruta))
        return salida

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

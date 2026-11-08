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
from gf_perc.caracteres import COLAS_KICK, GRUPOS_DRUMS, MATERIALES_DRUMS, sintetizar_cola, sintetizar_drum  # noqa: E402
from gf_perc.kit import NOTAS as NOTAS_PERC  # noqa: E402
from gf_perc.tonalidad import MODOS, clase_de  # noqa: E402

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
        actual = e["acordes"]["actual"]
        pads["progresion"] = actual.split("|")[0] if actual else None
        pads["tanda"] += 1
        for parte, patrones in (("pad", PATRONES_PAD), ("atmos", PATRONES_ATMOS)):
            pads[parte]["opciones"] = [{"id": p, "patron": p, "tanda": pads["tanda"]} for p in patrones]
        self._guardar()
        self._evento("opciones", parte="pads", progresion=pads["progresion"], tanda=pads["tanda"], pad=list(PATRONES_PAD), atmos=list(PATRONES_ATMOS))
        return e

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
        self.live.escribir_clip(indice, clip["inicio"], clip["longitud"], clip["notas"], clip["nombre"])
        estado.update(actual=ident, clip=clip, pista=pista)
        self._guardar()
        self._evento("borrador", parte=parte, pista=pista, patron=op["patron"], progresion=pads["progresion"], tanda=op["tanda"])
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

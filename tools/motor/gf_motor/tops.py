"""Loops y top loops desde la Library. Diseño: apps/docs/produccion/motor-tops.md."""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path

import mido

COMPASES = 16
GENEROS = {"organic": "organic_house", "house": "disco_house", "nu_disco": "disco_house", "indie_tech": "indie_tech"}
_DECISION = {"seleccion": 0, "favorito": 1, None: 2}
_SECCION = {"peak": 0, "main": 1, "main_q": 2, "intro": 3, None: 4}
_MAX_OPCIONES = 6
_MAX_POR_PATRON = 2
_VELOCITY_PRINCIPAL = 70
_TOLERANCIA_COMPAS = 0.1  # Una nota adelantada al compás siguiente no crea un compás nuevo.


def candidatos(library: Path, rol: str, genero: str, rack: str = "tribe") -> list[dict]:
    if genero not in GENEROS:
        raise ValueError(f"Sin equivalencia de género para {genero!r}")
    ruta = library / "_catalogo" / "catalogo.jsonl"
    if not ruta.exists():
        raise ValueError(f"No encuentro el catálogo en {ruta}: conecta el SSD")
    filas = [json.loads(x) for x in ruta.read_text().splitlines() if x.strip()]
    filas = [f for f in filas if f.get("tipo") == "midi" and f.get("rol") == rol and f.get("genero") == GENEROS[genero] and f.get("rack") == rack]
    filas.sort(key=lambda f: (_DECISION.get(f.get("decision_juan"), 2), _SECCION.get(f.get("seccion"), 4), f.get("version") or 1, f["ruta"]))
    salida, vistos, por_patron = [], set(), Counter()
    for f in filas:
        if f["sha256"] in vistos or por_patron[f["grupo"]] >= _MAX_POR_PATRON:
            continue
        vistos.add(f["sha256"])
        por_patron[f["grupo"]] += 1
        salida.append(
            {
                "id": f["id"], "ruta": f["ruta"], "nombre": Path(f["ruta"]).stem, "grupo": f["grupo"], "patron": f["grupo"].split(":", 1)[-1],
                "seccion": f.get("seccion"), "version": f.get("version"), "decision_juan": f.get("decision_juan"),
            }
        )
        if len(salida) == _MAX_OPCIONES:
            break
    return salida


def leer_midi(ruta: Path) -> tuple[list[dict], float]:
    """Notas en tiempos musicales, con su microtiming. Devuelve también el largo del loop en compases enteros."""
    archivo = mido.MidiFile(ruta)
    notas, abiertas = [], {}
    for pista in archivo.tracks:
        ticks = 0
        for m in pista:
            ticks += m.time
            t = ticks / archivo.ticks_per_beat
            if m.type == "note_on" and m.velocity > 0:
                abiertas.setdefault((m.channel, m.note), []).append((t, m.velocity))
            elif m.type in ("note_off", "note_on"):
                pila = abiertas.get((m.channel, m.note))
                if pila:
                    inicio, velocity = pila.pop(0)
                    notas.append({"pitch": m.note, "start_beat": round(inicio, 4), "duration_beats": round(max(t - inicio, 0.01), 4), "velocity": velocity})
    notas.sort(key=lambda n: (n["start_beat"], n["pitch"]))
    # Se mide por dónde empiezan las notas: con microtiming, la última puede terminar pasado el compás sin alargar el loop.
    fin = max((n["start_beat"] for n in notas), default=0.0)
    compases = max(1, int(-(-(fin - _TOLERANCIA_COMPAS) // 4)))
    return notas, float(compases * 4)


def clip_loop(nombre: str, notas: list[dict], largo: float) -> dict:
    total = float(COMPASES * 4)
    repeticiones = int(total // largo)
    salida = [{**n, "start_beat": round(n["start_beat"] + r * largo, 4)} for r in range(repeticiones) for n in notas]
    return {"nombre": nombre, "inicio": 0.0, "longitud": total, "notas": [n for n in salida if n["start_beat"] < total]}


def encaje(notas: list[dict], kick: list[int], bajo: list[int], clave: list[int]) -> dict:
    """Parte de los golpes principales que cae con cada voz. Información para Juan, no una nota."""
    principales = [n for n in notas if n["velocity"] >= _VELOCITY_PRINCIPAL] or notas
    pasos = [round(n["start_beat"] * 4) % 16 for n in principales]

    def parte(voz: list[int]) -> int:
        return round(100 * sum(p in voz for p in pasos) / len(pasos)) if pasos else 0

    return {"kick": parte(kick), "bajo": parte(bajo), "clave": parte(clave), "golpes": len(pasos)}

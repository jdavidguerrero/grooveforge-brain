"""Uso: uv run python -m gf_perc demo|variantes|kits|variantes-kit|kit-genero --out DIR"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import soundfile as sf

from . import __version__
from .core import SR, rng_para, tiempo
from .instrumentos import INSTRUMENTOS, articulaciones_de, sintetizar
from .kit import KITS, MATERIALES, NOTAS, VOCES, sintetizar_voz
from .generos import NIVELES, PATRONES, PERFILES, aplicar_choke, notas_de, sintetizar_en_genero
from .seleccion import huella, mas_distintas
from .tonalidad import MODOS, clase_de

# Paso de semicorchea, velocity. Patrón de prueba para oír en contexto, no un ritmo tradicional.
PATRON = {
    ("llamador", "abierto"): {2: 0.82, 6: 0.78, 10: 0.84, 14: 0.76},
    ("llamador", "apagado"): {7: 0.42, 15: 0.46},
    ("alegre", "abierto"): {3: 0.55, 11: 0.62},
    ("alegre", "slap"): {13: 0.72},
    ("bombo", "bajo"): {0: 0.7, 9: 0.5},
    ("clave", "golpe"): {0: 0.72, 3: 0.7, 6: 0.75, 10: 0.7, 12: 0.74},
    ("maracon", "corto"): {i: (0.72 if i % 4 == 2 else 0.5 if i % 2 == 0 else 0.34) for i in range(16)},
}
# Empuje fijo por posición en ms: da un toque repetible, no ruido aleatorio.
EMPUJE_MS = {"maracon": (0, 5, 2, 6), "llamador": (0, 2, -2, 2), "alegre": (0, 3, 1, 4), "clave": (0, -2, 0, -2), "bombo": (0, 0, 0, 0)}


def _guardar(y: np.ndarray, ruta: Path, meta: dict, manifiesto: list[dict], raiz: Path) -> None:
    ruta.parent.mkdir(parents=True, exist_ok=True)
    pcm = np.round(np.clip(y, -1.0, 1.0) * (2**23 - 1)).astype(np.int32)
    sf.write(ruta, pcm << 8, SR, subtype="PCM_24")
    manifiesto.append(
        {
            "archivo": str(ruta.relative_to(raiz)),
            "source": "synth",
            "version": __version__,
            "sha256_audio": hashlib.sha256(pcm.tobytes()).hexdigest(),
            "duracion_s": round(len(y) / SR, 3),
            **meta,
        }
    )


def _kick_referencia(vel: float = 0.9) -> np.ndarray:
    t = tiempo(0.35)
    fase = 2 * np.pi * (52 * t + (150 / 28) * (1 - np.exp(-t * 28)))
    return vel * np.sin(fase) * np.exp(-t * 11)


def _loop(bpm: float, compases: int, seed_timbre: int, kick: bool) -> np.ndarray:
    paso = 60.0 / bpm / 4.0
    total = 16 * compases * paso
    mezcla = np.zeros(int((total + 2.0) * SR))
    generador = rng_para("loop", seed_timbre)
    niveles = {"bombo": 0.75, "llamador": 0.7, "alegre": 0.58, "clave": 0.34, "maracon": 0.3}
    contador = 0
    for (instrumento, articulacion), pasos in PATRON.items():
        for compas in range(compases):
            for i, velocity in pasos.items():
                contador += 1
                vel = float(np.clip(velocity * generador.uniform(0.92, 1.06), 0.05, 1.0))
                y, _ = sintetizar(instrumento, articulacion, vel, seed_timbre, contador)
                empuje = EMPUJE_MS[instrumento][i % 4] + generador.normal(0.0, 1.0)
                inicio = int(max((compas * 16 + i) * paso + empuje * 1e-3, 0.0) * SR)
                mezcla[inicio : inicio + len(y)] += niveles[instrumento] * y
    if kick:
        k = _kick_referencia()
        for pulso in range(4 * compases):
            inicio = int(pulso * 4 * paso * SR)
            mezcla[inicio : inicio + len(k)] += 0.8 * k
    mezcla = mezcla[: int(total * SR)]
    return mezcla / max(np.max(np.abs(mezcla)), 1e-9) * 0.84


def demo(out: Path, seed: int, bpm: float, golpes: int) -> None:
    manifiesto: list[dict] = []
    for instrumento in INSTRUMENTOS:
        for articulacion in articulaciones_de(instrumento):
            for g in range(1, golpes + 1):
                y, meta = sintetizar(instrumento, articulacion, 0.8, seed, g)
                ruta = out / "golpes" / instrumento / f"{instrumento}_{articulacion}_{g:02d}.wav"
                _guardar(y, ruta, meta, manifiesto, out)
    for nombre, con_kick in (("demo_loop_124", False), ("demo_loop_124_con_kick", True)):
        y = _loop(bpm, 4, seed, con_kick)
        meta = {"instrumento": "loop", "bpm": bpm, "seed_timbre": seed, "kick_de_referencia": con_kick}
        _guardar(y, out / f"{nombre}.wav", meta, manifiesto, out)
    _escribir(out, manifiesto)


def variantes(out: Path, instrumento: str, n: int, seed: int, variacion: float) -> None:
    manifiesto: list[dict] = []
    for articulacion in articulaciones_de(instrumento):
        for i in range(n):
            y, meta = sintetizar(instrumento, articulacion, 0.8, seed + i, 1, variacion)
            ruta = out / "variantes" / instrumento / articulacion / f"{instrumento}_{articulacion}_t{seed + i:04d}.wav"
            _guardar(y, ruta, meta, manifiesto, out)
    _escribir(out, manifiesto)


# Mismo patrón para todos los kits: la comparación depende solo del sonido.
PATRON_KIT = {
    "kick": {0: 0.95, 4: 0.92, 8: 0.95, 12: 0.92},
    "clap": {4: 0.8, 12: 0.84},
    "snare": {7: 0.42, 15: 0.5},
    "hat_cerrado": {i: (0.68 if i % 4 == 2 else 0.34) for i in range(0, 16, 2) if i != 14},
    "hat_abierto": {14: 0.66},
    "maraca": {i: 0.32 for i in range(1, 16, 2)},
    "rim": {3: 0.55, 11: 0.5},
    "clave": {0: 0.6, 6: 0.62, 10: 0.58},
    "conga_baja": {5: 0.6, 13: 0.66},
    "conga_media": {9: 0.55},
    "conga_alta": {2: 0.68, 10: 0.62},
    "bongo_alto": {7: 0.5, 15: 0.56},
    "bongo_bajo": {11: 0.48},
}
NIVEL_KIT = {
    "kick": 0.85, "clap": 0.42, "snare": 0.28, "hat_cerrado": 0.2, "hat_abierto": 0.17, "maraca": 0.14, "rim": 0.22,
    "clave": 0.17, "conga_baja": 0.42, "conga_media": 0.36, "conga_alta": 0.34, "bongo_alto": 0.25, "bongo_bajo": 0.25,
}


def _loop_kit(nombre: str, bpm: float, compases: int, seed: int) -> np.ndarray:
    paso = 60.0 / bpm / 4.0
    total = 16 * compases * paso
    mezcla = np.zeros(int((total + 2.0) * SR))
    generador = rng_para("loop-kit", seed)
    contador = 0
    for voz, pasos in PATRON_KIT.items():
        for compas in range(compases):
            for i, velocity in pasos.items():
                contador += 1
                vel = float(np.clip(velocity * generador.uniform(0.93, 1.05), 0.05, 1.0))
                y, _ = sintetizar_voz(voz, KITS[nombre][voz], vel, seed, contador)
                inicio = int(((compas * 16 + i) * paso) * SR)
                mezcla[inicio : inicio + len(y)] += NIVEL_KIT[voz] * y
    mezcla = mezcla[: int(total * SR)]
    return mezcla / max(np.max(np.abs(mezcla)), 1e-9) * 0.84


def kits(out: Path, seed: int, bpm: float, golpes: int) -> None:
    manifiesto: list[dict] = []
    for nombre, materiales in KITS.items():
        for voz in VOCES:
            for g in range(1, golpes + 1):
                y, meta = sintetizar_voz(voz, materiales[voz], 0.85, seed, g)
                ruta = out / "kits" / nombre / f"{NOTAS[voz]}_{voz}_{g:02d}.wav"
                _guardar(y, ruta, {"kit": nombre, **meta}, manifiesto, out)
        y = _loop_kit(nombre, bpm, 4, seed)
        _guardar(y, out / "loops" / f"{nombre}_{int(bpm)}.wav", {"kit": nombre, "bpm": bpm, "seed_timbre": seed}, manifiesto, out)
    _escribir(out, manifiesto)


def variantes_kit(out: Path, por_voz: int, por_kick: int, factor: int, seed: int, variacion: float, voces: list[str], materiales: list[str]) -> None:
    """Base + las variantes más distintas de cada voz, con un archivo para oírlas en fila."""
    manifiesto: list[dict] = []
    for material in materiales:
        for voz in voces:
            cuantas = por_kick if voz == "kick" else por_voz
            candidatas = [sintetizar_voz(voz, material, 0.85, seed)]
            candidatas += [sintetizar_voz(voz, material, 0.85, seed + i, 1, variacion) for i in range(1, cuantas * factor + 1)]
            elegidas = mas_distintas([huella(y) for y, _ in candidatas], cuantas + 1)
            fila: list[np.ndarray] = []
            for orden, indice in enumerate(elegidas):
                y, meta = candidatas[indice]
                etiqueta = "base" if orden == 0 else f"v{orden:02d}"
                ruta = out / "variantes" / material / voz / f"{NOTAS[voz]}_{voz}_{material}_{etiqueta}.wav"
                _guardar(y, ruta, {**meta, "etiqueta": etiqueta}, manifiesto, out)
                fila.append(y)
            hueco = int(0.28 * SR)
            duracion = max(int(0.42 * SR), max(map(len, fila)) + hueco)
            audicion = np.zeros(duracion * len(fila))
            for i, y in enumerate(fila):
                audicion[i * duracion : i * duracion + len(y)] = y
            meta_fila = {"voz": voz, "material": material, "orden": [f"v{i:02d}" if i else "base" for i in range(len(fila))], "seg_por_sonido": duracion / SR}
            _guardar(audicion, out / "audicion" / material / f"{voz}_{material}.wav", meta_fila, manifiesto, out)
    _escribir(out, manifiesto)


def _loop_genero(genero: str, raiz: str, modo: str, bpm: float, compases: int, seed: int) -> np.ndarray:
    paso = 60.0 / bpm / 4.0
    total = 16 * compases * paso
    n = int((total + 2.0) * SR)
    mezcla = np.zeros(n)
    generador = rng_para("loop-genero", genero, seed)
    contador = 0
    hats_abiertos: tuple[list[int], list[np.ndarray]] = ([], [])
    cerrados: list[int] = []
    for voz, pasos in PATRONES[genero].items():
        for compas in range(compases):
            for i, velocity in pasos.items():
                contador += 1
                vel = float(np.clip(velocity * generador.uniform(0.93, 1.05), 0.05, 1.0))
                y, _ = sintetizar_en_genero(voz, genero, raiz, modo, vel, seed, contador)
                inicio = int((compas * 16 + i) * paso * SR)
                if voz == "hat_abierto":
                    hats_abiertos[0].append(inicio)
                    hats_abiertos[1].append(NIVELES[voz] * y)
                    continue
                if voz == "hat_cerrado":
                    cerrados.append(inicio)
                mezcla[inicio : inicio + len(y)] += NIVELES[voz] * y
    pista = np.zeros(n)
    aplicar_choke(pista, *hats_abiertos[:1], cerrados, hats_abiertos[1])
    mezcla = (mezcla + pista)[: int(total * SR)]
    return mezcla / max(np.max(np.abs(mezcla)), 1e-9) * 0.84


def kit_genero(out: Path, genero: str, raiz: str, modo: str, bpm: float | None, opciones: int, factor: int, seed: int, variacion: float) -> None:
    clase_de(raiz)
    tempo = bpm or PERFILES[genero].bpm
    carpeta = f"{genero}_{raiz.replace('#', 's')}_{modo}_{int(tempo)}"
    manifiesto: list[dict] = []
    raiz_out = out / carpeta
    for voz in VOCES:
        if voz == "hat_abierto":
            continue
        candidatas = [sintetizar_en_genero(voz, genero, raiz, modo, seed_timbre=seed)]
        candidatas += [
            sintetizar_en_genero(voz, genero, raiz, modo, seed_timbre=seed + i, variacion=variacion)
            for i in range(1, opciones * factor + 1)
        ]
        elegidas = mas_distintas([huella(y) for y, _ in candidatas], opciones + 1)
        for voz_final in ([voz, "hat_abierto"] if voz == "hat_cerrado" else [voz]):
            fila = []
            for orden, indice in enumerate(elegidas):
                # Par de hats: el abierto usa la semilla del cerrado elegido (teoría §4).
                if voz_final == voz:
                    y, meta = candidatas[indice]
                else:
                    semilla = candidatas[indice][1]["seed_timbre"]
                    y, meta = sintetizar_en_genero(voz_final, genero, raiz, modo, seed_timbre=semilla, variacion=0.0 if indice == 0 else variacion)
                etiqueta = "base" if orden == 0 else f"v{orden:02d}"
                ruta = raiz_out / "golpes" / voz_final / f"{NOTAS[voz_final]}_{voz_final}_{etiqueta}.wav"
                _guardar(y, ruta, {**meta, "etiqueta": etiqueta}, manifiesto, out)
                fila.append(y)
            hueco = int(0.28 * SR)
            duracion = max(int(0.42 * SR), max(map(len, fila)) + hueco)
            audicion = np.zeros(duracion * len(fila))
            for i, y in enumerate(fila):
                audicion[i * duracion : i * duracion + len(y)] = y
            _guardar(audicion, raiz_out / "audicion" / f"{voz_final}.wav", {"voz": voz_final, "genero": genero, "seg_por_sonido": duracion / SR}, manifiesto, out)
    loop = _loop_genero(genero, raiz, modo, tempo, 4, seed)
    _guardar(loop, raiz_out / f"loop_{carpeta}.wav", {"genero": genero, "raiz": raiz, "modo": modo, "bpm": tempo, "notas": notas_de(genero, raiz, modo)}, manifiesto, out)
    _escribir(out, manifiesto)


def _escribir(out: Path, manifiesto: list[dict]) -> None:
    ruta = out / "manifiesto.jsonl"
    with ruta.open("a", encoding="utf-8") as f:
        for fila in manifiesto:
            f.write(json.dumps(fila, ensure_ascii=False) + "\n")
    print(f"{len(manifiesto)} archivos en {out}")


def main() -> None:
    parser = argparse.ArgumentParser(prog="gf_perc", description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("demo", help="golpes sueltos y loop de prueba")
    d.add_argument("--out", type=Path, required=True)
    d.add_argument("--seed", type=int, default=1)
    d.add_argument("--bpm", type=float, default=124.0)
    d.add_argument("--golpes", type=int, default=4)
    v = sub.add_parser("variantes", help="muchos timbres de un instrumento")
    v.add_argument("--out", type=Path, required=True)
    v.add_argument("--instrumento", choices=sorted(INSTRUMENTOS), required=True)
    v.add_argument("-n", type=int, default=20)
    v.add_argument("--seed", type=int, default=1)
    v.add_argument("--variacion", type=float, default=1.0)
    k = sub.add_parser("kits", help="kits tipo 808 por material y su loop comparativo")
    k.add_argument("--out", type=Path, required=True)
    k.add_argument("--seed", type=int, default=1)
    k.add_argument("--bpm", type=float, default=124.0)
    k.add_argument("--golpes", type=int, default=3)
    w = sub.add_parser("variantes-kit", help="variantes de one-shots por voz y material")
    w.add_argument("--out", type=Path, required=True)
    w.add_argument("--por-voz", type=int, default=8)
    w.add_argument("--por-kick", type=int, default=16)
    w.add_argument("--factor", type=int, default=4, help="candidatas generadas por cada una conservada")
    w.add_argument("--seed", type=int, default=1)
    w.add_argument("--variacion", type=float, default=1.0)
    w.add_argument("--voces", nargs="+", choices=VOCES, default=list(VOCES))
    w.add_argument("--materiales", nargs="+", choices=MATERIALES, default=list(MATERIALES))
    h = sub.add_parser("kit-genero", help="un kit por género y tonalidad, bajo pedido")
    h.add_argument("--out", type=Path, required=True)
    h.add_argument("--genero", choices=sorted(PERFILES), required=True)
    h.add_argument("--raiz", default="A", help="A, C#, Bb o La, Do#...")
    h.add_argument("--modo", choices=sorted(MODOS), default="menor")
    h.add_argument("--bpm", type=float)
    h.add_argument("--opciones", type=int, default=4)
    h.add_argument("--factor", type=int, default=6)
    h.add_argument("--seed", type=int, default=1)
    h.add_argument("--variacion", type=float, default=0.6)
    a = parser.parse_args()
    if a.cmd == "kit-genero":
        kit_genero(a.out, a.genero, a.raiz, a.modo, a.bpm, a.opciones, a.factor, a.seed, a.variacion)
    elif a.cmd == "variantes-kit":
        variantes_kit(a.out, a.por_voz, a.por_kick, a.factor, a.seed, a.variacion, a.voces, a.materiales)
    elif a.cmd == "kits":
        kits(a.out, a.seed, a.bpm, a.golpes)
    elif a.cmd == "demo":
        demo(a.out, a.seed, a.bpm, a.golpes)
    else:
        variantes(a.out, a.instrumento, a.n, a.seed, a.variacion)


if __name__ == "__main__":
    main()

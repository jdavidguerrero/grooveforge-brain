"""Muestreo de Tribe Player 2 por su salida de audio (no se abren ni se copian sus archivos).

1. `midi`: escribe un MIDI con cada articulación del mapa de Tribe × velocities × repeticiones, separadas para que cada
   golpe termine, y un plan JSON con el segundo exacto de cada golpe.
2. Juan lo toca en Ableton con Tribe (preset limpio, Pitch 0, 440 Hz, delays/reverbs/macros en 0) y exporta el audio
   a 48 kHz.
3. `cortar`: alinea el primer golpe del audio con el plan, corta cada golpe por su tiempo conocido, recorta el silencio
   final con un fundido corto y escribe los WAV más un manifiesto con la nota de licencia.

Las velocities conservan su nivel relativo (no se normaliza cada golpe): eso es lo que hace útiles las capas de
velocity. Licencia: los sonidos de Tribe son para uso personal de Juan, sin redistribuir; nunca van al repo.

    uv run python muestreo_tribe.py midi --out ~/GROOVEFORGE/muestreo-tribe
    uv run python muestreo_tribe.py cortar --audio export.wav --plan ~/GROOVEFORGE/muestreo-tribe/plan.json \\
        --out ~/GROOVEFORGE/muestreo-tribe/kit
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

# Mapa de Tribe Colombian Percussion (capturas de Juan, 2026-10-27; tools/paleta-lab/make_perc_patterns.py), notas de Live.
TRIBE = {"Bombo Open": 36, "Bombo Cascara L": 38, "Bombo Cascara R": 40, "Tambora Open L": 41, "Tambora Open R": 43,
         "Tambora Cascara L": 44, "Tambora Damped L": 45, "Tambora Cascara R": 46, "Tambora Damped R": 47,
         "Llamador Bajoneo": 48, "Llamador Abierto": 50, "Alegre Bajoneo": 52, "Alegre Abierto": 53,
         "Alegre Quemao": 55, "Alegre Latigazo": 57, "Guache Up": 59, "Guache Down": 60, "Guache Rain": 62,
         "Guache Tremolo": 64, "Maracon Up": 65, "Maracon Down": 67, "Maracon Tremolo": 69, "Caja Fondo L": 71,
         "Caja Fondo R": 72, "Caja Quemao L": 74, "Caja Quemao R": 76, "Caja Latigazo": 77, "Guacharaca Shot": 79,
         "Guacharaca Short": 81, "Guacharaca Mid": 83, "Guacharaca Long": 84, "Guacharaca Longer": 86}
# Sonidos largos (trémolos, lluvia, guacharaca larga): necesitan más espacio para no pisarse con el siguiente.
LARGOS = {"Guache Rain", "Guache Tremolo", "Maracon Tremolo", "Guacharaca Mid", "Guacharaca Long", "Guacharaca Longer"}
VELOCITIES = (40, 70, 100, 127)
REPETICIONES = 2  # captura variaciones alternadas (round-robin) si el instrumento las tiene
BPM = 120  # 0,5 s por negra: los tiempos del plan son exactos en segundos
ESPACIO_CORTO, ESPACIO_LARGO, DURACION_NOTA = 1.5, 4.0, 0.25
LICENCIA = "Tribe Instruments — uso personal de Juan; sin redistribuir; permiso de uso en la consola pendiente"


def plan_de_muestreo() -> list[dict]:
    """Lista de golpes con su segundo de inicio, en el orden del MIDI."""
    golpes, t = [], 1.0  # un segundo de margen al principio
    for articulacion, nota in TRIBE.items():
        espacio = ESPACIO_LARGO if articulacion in LARGOS else ESPACIO_CORTO
        for vel in VELOCITIES:
            for rr in range(1, REPETICIONES + 1):
                golpes.append({"articulacion": articulacion, "nota": nota, "velocity": vel, "rr": rr,
                               "inicio": round(t, 3), "espacio": espacio})
                t += espacio
    return golpes


def escribir_midi(plan: list[dict], ruta: Path) -> None:
    import mido

    mid = mido.MidiFile(ticks_per_beat=480)
    pista = mido.MidiTrack()
    mid.tracks.append(pista)
    pista.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(BPM)))
    por_segundo = 480 * BPM / 60
    eventos = []
    for g in plan:
        eventos.append((round(g["inicio"] * por_segundo), "note_on", g["nota"], g["velocity"]))
        eventos.append((round((g["inicio"] + DURACION_NOTA) * por_segundo), "note_off", g["nota"], 0))
    eventos.sort(key=lambda e: (e[0], e[1] == "note_on"))
    previo = 0
    for tick, tipo, nota, vel in eventos:
        pista.append(mido.Message(tipo, note=nota, velocity=vel, time=tick - previo))
        previo = tick
    mid.save(ruta)


def primer_golpe(x: np.ndarray, sr: int, umbral_db: float = -45.0) -> int:
    """Muestra donde el audio supera por primera vez el umbral (relativo al pico)."""
    env = np.abs(x).max(axis=1) if x.ndim > 1 else np.abs(x)
    pico = env.max()
    if pico <= 0:
        raise ValueError("el audio está en silencio")
    sobre = np.nonzero(env >= pico * 10 ** (umbral_db / 20))[0]
    return int(sobre[0])


def cortar(audio: Path, plan_ruta: Path, out: Path, umbral_cola_db: float = -60.0,
           pico_objetivo_db: float | None = -1.0, bpm_export: float | None = None) -> dict:
    import soundfile as sf

    datos = json.loads(plan_ruta.read_text())
    plan = datos["golpes"]
    # Ableton toca el MIDI en compases: si el proyecto estaba a otro tempo, todos los tiempos se escalan igual.
    escala = 1.0 if bpm_export is None else datos.get("bpm", BPM) / bpm_export
    plan = [{**g, "inicio": g["inicio"] * escala, "espacio": g["espacio"] * escala} for g in plan]
    x, sr = sf.read(audio, always_2d=True, dtype="float32")
    if sr != 48_000:
        raise ValueError(f"exporta a 48 kHz (llegó {sr} Hz)")
    desfase = primer_golpe(x, sr) - round(plan[0]["inicio"] * sr)  # alinea el plan con el audio exportado
    pico_global = float(np.abs(x).max())
    # Una sola ganancia para todo el kit (el golpe más fuerte queda en pico_objetivo_db): recupera nivel si el export
    # salió bajo sin alterar la diferencia entre velocities.
    ganancia = 1.0 if pico_objetivo_db is None else 10 ** (pico_objetivo_db / 20) / pico_global
    x *= ganancia
    pico_global *= ganancia
    out.mkdir(parents=True, exist_ok=True)
    manifiesto, vacios = [], []
    pre = int(0.002 * sr)  # 2 ms antes del golpe para no cortar el ataque
    fundido = int(0.005 * sr)
    for g in plan:
        a = max(0, round(g["inicio"] * sr) + desfase - pre)
        b = min(len(x), a + round(g["espacio"] * sr) - pre)
        trozo = x[a:b].copy()
        if len(trozo) < int(0.05 * sr):  # el export terminó antes que el plan
            vacios.append(f'{g["articulacion"]} v{g["velocity"]} rr{g["rr"]} (fuera del export)')
            continue
        env = np.abs(trozo).max(axis=1)
        if env.max() < pico_global * 10 ** (-60 / 20):
            vacios.append(f'{g["articulacion"]} v{g["velocity"]} rr{g["rr"]}')
            continue
        vivo = np.nonzero(env >= env.max() * 10 ** (umbral_cola_db / 20))[0]
        fin = min(len(trozo), int(vivo[-1]) + fundido)
        trozo = trozo[:fin]
        trozo[-fundido:] *= np.linspace(1.0, 0.0, min(fundido, len(trozo)))[:, None]
        nombre = f'{g["articulacion"].lower().replace(" ", "_")}_v{g["velocity"]:03d}_rr{g["rr"]}.wav'
        sf.write(out / nombre, trozo, sr, subtype="PCM_24")
        manifiesto.append({**{k: g[k] for k in ("articulacion", "nota", "velocity", "rr")},
                           "archivo": nombre, "segundos": round(len(trozo) / sr, 3),
                           "pico_db": round(20 * np.log10(float(np.abs(trozo).max())), 1)})
    resumen = {"fuente": "Tribe Player 2 · Tribe Instruments Map · preset 01 Original Recording (Clean)",
               "licencia": LICENCIA, "frecuencia": sr, "desfase_muestras": int(desfase),
               "ganancia_db": round(20 * np.log10(ganancia), 1), "escala_tempo": round(escala, 5),
               "golpes": manifiesto, "vacios": vacios}
    (out / "manifiesto.json").write_text(json.dumps(resumen, ensure_ascii=False, indent=1))
    return resumen


# Rangos de velocity de cada capa (las velocities muestreadas quedan dentro de su rango).
RANGOS = {40: (1, 55), 70: (56, 85), 100: (86, 113), 127: (114, 127)}


def armar_racks(kit: Path, data_dir: Path, pico_pad_db: float | None = -3.0) -> list[str]:
    """Dos racks de 16 pads (las 32 articulaciones en el orden del mapa), con 4 capas de velocity por pad.

    Usa el formato y el almacén de samples del servicio de kits del modo DRUM (`tools/drum-kits`). Solo la primera
    repetición de cada velocity: el rack no combina capas de velocity con round-robin.
    """
    import sys
    from uuid import uuid4

    import soundfile as sf

    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "drum-kits"))
    import drum_kits as dk

    man = json.loads((kit / "manifiesto.json").read_text())
    por_art: dict[str, dict[int, str]] = {}
    for g in man["golpes"]:
        if g["rr"] == 1:
            por_art.setdefault(g["articulacion"], {})[g["velocity"]] = g["archivo"]
    fuente = dict(id=uuid4().hex, name="Tribe Colombian Percussion (muestreo)", kind="IMPORTED",
                  licence_note=man["licencia"], original_folder=str(kit))
    articulaciones = [a for a in TRIBE if a in por_art]
    rutas = []
    for n, bloque in enumerate((articulaciones[:16], articulaciones[16:32]), start=1):
        rack = dk.empty_rack(f"TRIBE {n}", source=fuente)
        for i, art in enumerate(bloque):
            capas = []
            audios = {vel: sf.read(kit / archivo, dtype="float32", always_2d=True)
                      for vel, archivo in sorted(por_art[art].items())}
            # Una ganancia por pad (su capa más fuerte a pico_pad_db): iguala pads de nivel muy distinto en Tribe
            # (Guache Rain ~ -33 dB frente a Tambora ~ -1 dB) sin cambiar la diferencia entre sus capas de velocity.
            pico = max(float(np.abs(a).max()) for a, _ in audios.values())
            g_pad = 1.0 if pico_pad_db is None or pico <= 0 else 10 ** (pico_pad_db / 20) / pico
            for vel, (pcm, sr) in audios.items():
                capa = dk.store_sample(data_dir, pcm * g_pad, sr)
                capa["velocity_min"], capa["velocity_max"] = RANGOS[vel]
                capas.append(capa)
            cat = dk.category(art)
            pad = rack["pads"][i]
            pad.update(name=art.upper(), category=cat, source_id=fuente["id"], layers=capas, layer_mode="velocity",
                       tribe_note=TRIBE[art])
            larga = max(c["frames"] for c in capas) / 48_000 * 1000
            pad["params"].update(group=dk.group_for(cat), decay=max(2000.0, larga))
        rutas.append(dk.save_rack(data_dir, rack))
    return rutas


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    m = sub.add_parser("midi", help="escribe muestreo_tribe.mid y plan.json")
    m.add_argument("--out", type=Path, required=True)
    c = sub.add_parser("cortar", help="corta el audio exportado en golpes")
    c.add_argument("--audio", type=Path, required=True)
    c.add_argument("--plan", type=Path, required=True)
    c.add_argument("--out", type=Path, required=True)
    c.add_argument("--pico-db", type=float, default=-1.0, help="pico del golpe más fuerte tras la ganancia global")
    c.add_argument("--sin-normalizar", action="store_true")
    c.add_argument("--bpm-export", type=float, help="tempo del proyecto de Ableton al exportar, si no era el del plan")
    rk = sub.add_parser("rack", help="arma los racks TRIBE 1 y TRIBE 2 en el almacén del modo DRUM")
    rk.add_argument("--kit", type=Path, required=True)
    rk.add_argument("--data-dir", type=Path, required=True)
    rk.add_argument("--pico-pad-db", type=float, default=-3.0, help="pico de la capa más fuerte de cada pad")
    a = ap.parse_args()
    if a.cmd == "midi":
        a.out.mkdir(parents=True, exist_ok=True)
        plan = plan_de_muestreo()
        escribir_midi(plan, a.out / "muestreo_tribe.mid")
        (a.out / "plan.json").write_text(json.dumps({"bpm": BPM, "velocities": VELOCITIES,
                                                     "repeticiones": REPETICIONES, "golpes": plan}, indent=1))
        fin = plan[-1]["inicio"] + plan[-1]["espacio"]
        print(f"{len(plan)} golpes, {fin / 60:.1f} min a {BPM} BPM → {a.out}/muestreo_tribe.mid")
    elif a.cmd == "rack":
        for ruta in armar_racks(a.kit, a.data_dir, a.pico_pad_db):
            print(ruta)
    else:
        r = cortar(a.audio, a.plan, a.out, pico_objetivo_db=None if a.sin_normalizar else a.pico_db,
                   bpm_export=a.bpm_export)
        print(f'{len(r["golpes"])} golpes en {a.out}; vacíos: {len(r["vacios"])}; desfase {r["desfase_muestras"]} muestras; '
              f'ganancia global {r["ganancia_db"]:+.1f} dB')


if __name__ == "__main__":
    main()

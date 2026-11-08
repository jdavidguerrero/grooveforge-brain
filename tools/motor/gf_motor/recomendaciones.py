"""Carga por sección y recomendaciones de voces y anchura. Diseño: apps/docs/produccion/motor-recomendaciones.md."""

from __future__ import annotations

from .mezcla import rol_de

BANDAS = (("grave", 0.0, 175.0), ("medio-grave", 175.0, 350.0), ("medio", 350.0, 700.0), ("agudo", 700.0, 1e9))
MELODICOS = {"BASS", "BASS SUB", "BASS MID", "LEAD", "LEAD MAIN", "LEAD AUX", "AUX LEAD", "MELODY", "CHORDS", "CHORDS PLK",
             "CHORDS 2", "CHORDS SUS", "PAD", "ATMOS", "STABS", "PLUCKS"}
LEADS = {"LEAD", "LEAD MAIN", "LEAD AUX", "AUX LEAD", "MELODY"}
PERCUSION = {"PERC", "TOP", "DRUMS", "FILL"}
FRASE_SI_CALLA = 0.6
PERC_DENSA = 8.0

ANCHURA = {
    "KICK": ("Mono, sin duplicar", "Utility con Bass Mono (~120 Hz): en mono lo ancho del grave se cancela"),
    "BASS": ("Mono, sin duplicar", "Utility con Bass Mono (~120 Hz)"),
    "BASS SUB": ("Mono, sin duplicar", "Utility con Bass Mono (~120 Hz)"),
    "BASS MID": ("Mono abajo, algo de ancho arriba", "Utility: Bass Mono + ancho leve"),
    "LEAD MAIN": ("Al centro; octava arriba bajita si el agudo está libre", "Capa o pista con el mismo MIDI +12"),
    "LEAD": ("Al centro; octava arriba bajita si el agudo está libre", "Capa o pista con el mismo MIDI +12"),
    "LEAD AUX": ("Doble con MIDI, paneado al lado opuesto", "Segunda pista: mismo MIDI corrido 10–20 ms, velocity distinta"),
    "AUX LEAD": ("Doble con MIDI, paneado al lado opuesto", "Segunda pista: mismo MIDI corrido 10–20 ms, velocity distinta"),
    "STABS": ("Haas", "Delay del rack de rol: sin sync, L 1 ms, R 15 ms, feedback 0, mezcla 100 %"),
    "PLUCKS": ("Haas", "Delay del rack de rol: sin sync, L 1 ms, R 15 ms, feedback 0, mezcla 100 %"),
    "CHORDS PLK": ("Haas", "Delay del rack de rol: sin sync, L 1 ms, R 15 ms, feedback 0, mezcla 100 %"),
    "CHORDS": ("Haas", "Delay del rack de rol: sin sync, L 1 ms, R 15 ms, feedback 0, mezcla 100 %"),
    "CHORDS SUS": ("Ancho 120–140 % o chorus", "Utility (Width) o el chorus del preset"),
    "CHORDS 2": ("Ancho 120–140 % o chorus", "Utility (Width) o el chorus del preset"),
    "PAD": ("Ancho 120–140 % o chorus", "Utility (Width) o el chorus del preset"),
    "ATMOS": ("Ancho 120–140 % o chorus", "Utility (Width) o el chorus del preset"),
}
APLICAR_VOZ = ("Llévala a La (transponer o warp), pasaaltos ~150 Hz, de-esser; envíos B Plate y F Echo. "
               "Chop: Simpler en modo Slice, cortes cortos en los huecos del lead.")


def _hz(pitch: int) -> float:
    return 440.0 * 2 ** ((pitch - 69) / 12)


def carga(seccion: dict, notas_por_pista: dict[str, list[dict]]) -> dict:
    """Bandas ocupadas (fracción del tiempo), silencio del lead y densidad de percusión en una sección."""
    a, b = seccion["inicio"], seccion["fin"]
    pasos = max(int(round((b - a) * 4)), 1)
    bandas = {n: set() for n, _, _ in BANDAS}
    lead, perc = set(), 0
    for pista, notas in notas_por_pista.items():
        rol = rol_de(pista)
        for n in notas:
            ini, fin = max(n["start_beat"], a), min(n["start_beat"] + n["duration_beats"], b)
            if fin <= ini:
                continue
            ocupa = range(int((ini - a) * 4), max(int((fin - a) * 4), int((ini - a) * 4) + 1))
            if rol in MELODICOS:
                hz = _hz(n["pitch"])
                banda = next(nombre for nombre, lo, hi in BANDAS if lo <= hz < hi)
                bandas[banda].update(ocupa)
            if rol in LEADS:
                lead.update(ocupa)
            if rol in PERCUSION:
                perc += 1
    compases = (b - a) / 4
    return {
        "bandas": {k: round(len(v) / pasos, 2) for k, v in bandas.items()},
        "lead_calla": round(1 - len(lead) / pasos, 2),
        "perc_por_compas": round(perc / compases, 1) if compases else 0.0,
    }


def voz(seccion: dict, c: dict) -> dict | None:
    if seccion["nombre"].startswith(("INTER", "OUTRO")):
        return None
    alto = c["bandas"]["medio"] + c["bandas"]["agudo"]
    bajo = c["bandas"]["medio-grave"]
    tipo = "masculina o canto grave" if alto > bajo else "femenina o chop agudo"
    razon_tipo = (f"el medio y agudo están ocupados {round(alto * 100)} % y el medio-grave {round(bajo * 100)} %: "
                  + ("una voz grave deja libre al lead" if alto > bajo else "una voz aguda va por encima de acordes y bajo"))
    if c["lead_calla"] >= FRASE_SI_CALLA:
        forma, razon_forma = "Frase o hook", f"el lead calla el {round(c['lead_calla'] * 100)} %: cabe una palabra"
    else:
        forma, razon_forma = "Chop rítmico en los huecos del lead", f"el lead suena el {round((1 - c['lead_calla']) * 100)} %: una frase competiría"
    extra = None
    if seccion["nombre"].startswith(("INTRO", "BREAK")) and c["perc_por_compas"] >= PERC_DENSA:
        extra = f"Además, coro tribal corto en llamada y respuesta (percusión densa: {c['perc_por_compas']:g} notas por compás)"
    return {"forma": forma, "tipo": tipo, "razon": f"{razon_forma}; {razon_tipo}", "extra": extra, "aplicar": APLICAR_VOZ}


def anchura(pistas: list[str]) -> list[dict]:
    salida, vistos = [], set()
    for p in pistas:
        rol = rol_de(p)
        if rol in ANCHURA and p not in vistos:
            vistos.add(p)
            que, como = ANCHURA[rol]
            salida.append({"pista": p, "rol": rol, "que": que, "como": como})
    return salida

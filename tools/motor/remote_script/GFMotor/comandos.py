"""Comandos sobre el set. Solo corren en el hilo principal de Live; sin dependencias de red para probarlos fuera."""

import re

from .buscar import buscar_item

# Solo el nombre exacto que genera el motor: un rack de Juan nunca coincide y nunca se borra.
_RACK_DEL_MOTOR = re.compile(r"^(KICK|DRUMS) \S+ \S+ \S+ [0-9a-f]{4} R\d+$")


class ErrorComando(Exception):
    pass


def _pista(song, indice):
    pistas = list(song.tracks)
    if not 0 <= indice < len(pistas):
        raise ErrorComando("No existe la pista %d" % indice)
    return pistas[indice]


def _pista_midi(song, indice):
    pista = _pista(song, indice)
    if not pista.has_midi_input:
        raise ErrorComando("La pista %r no es MIDI" % pista.name)
    return pista


def _clips(pista):
    return sorted(pista.arrangement_clips, key=lambda c: c.start_time)


def sesion(song, _m):
    return {
        "session": {
            "tempo": song.tempo,
            "file_path": song.file_path or "",
            "tracks": [{"index": i, "name": t.name} for i, t in enumerate(song.tracks)],
        }
    }


def poner_tempo(song, m):
    song.tempo = max(20.0, min(999.0, float(m["value"])))
    return {}


def crear_pista(song, m):
    indice = len(list(song.tracks))
    song.begin_undo_step()
    try:
        song.create_midi_track(indice)
        song.tracks[indice].name = m["track_name"]
    finally:
        song.end_undo_step()
    return {"track_index": indice}


def _agregar_notas(clip, notas, especificacion):
    if notas:
        clip.add_new_notes(
            tuple(
                especificacion(
                    pitch=int(n["pitch"]),
                    start_time=float(n["start_beat"]),
                    duration=float(n["duration_beats"]),
                    velocity=float(n["velocity"]),
                )
                for n in notas
            )
        )


def escribir_clip(song, m, especificacion):
    """Live no crea clips en Arrangement desde un script: se arma uno temporal en Session y se duplica.

    Nombre y color se ponen en el temporal, y la copia los hereda. Duplicar ocupa el tramo destino: decidir si la
    pista se puede escribir le corresponde al motor (regla 3).
    """
    pista = _pista_midi(song, int(m["track_index"]))
    ranura = next((s for s in pista.clip_slots if not s.has_clip), None)
    if ranura is None:
        raise ErrorComando("La pista %r no tiene una ranura de Session libre" % pista.name)
    song.begin_undo_step()
    try:
        ranura.create_clip(float(m["length_beats"]))
        try:
            clip = ranura.clip
            _agregar_notas(clip, m["notes"], especificacion)
            clip.name = m["clip_name"]
            if m.get("color") is not None:
                clip.color = int(m["color"])
            pista.duplicate_clip_to_arrangement(clip, float(m["start_beats"]))
        finally:
            ranura.delete_clip()
    finally:
        song.end_undo_step()
    return {}


def listar_clips(song, m):
    pista = _pista(song, int(m["track_index"]))
    # `length` es el largo del loop, no lo que ocupa en Arrangement: un clip partido conserva todas sus notas.
    return {"clips": [{"start": c.start_time, "length": c.length, "name": c.name, "end": c.end_time, "marker": c.start_marker} for c in _clips(pista)]}


def leer_clip(song, m):
    """Solo devuelve el clip que empieza justo en ese tiempo; nunca el más cercano."""
    pista = _pista_midi(song, int(m["track_index"]))
    inicio = float(m["start_beats"])
    clip = next((c for c in _clips(pista) if abs(c.start_time - inicio) < 1e-3), None)
    if clip is None:
        return {"clip": None}
    notas = clip.get_notes_extended(0, 128, 0.0, float(clip.length))
    return {
        "clip": {
            "start": clip.start_time,
            "length": clip.length,
            "name": clip.name,
            "notes": [
                {
                    "pitch": int(n.pitch),
                    "start_beat": round(float(n.start_time), 4),
                    "duration_beats": round(float(n.duration), 4),
                    "velocity": int(n.velocity),
                }
                for n in notas
            ],
        }
    }


def intentar_cargar(song, navegador, partes, nombre_pista):
    """Devuelve False si Live aún no ve el archivo en Places, para reintentar."""
    pistas = [t for t in song.tracks if t.name == nombre_pista]
    if len(pistas) != 1:
        raise ErrorComando("Se esperaba una pista %s y hay %d" % (nombre_pista, len(pistas)))
    item = buscar_item(navegador.user_folders, partes)
    if item is None:
        return False
    pista = pistas[0]
    song.begin_undo_step()
    try:
        song.view.selected_track = pista
        navegador.load_item(item)
        # Después de cargar: si la carga falla, el rack anterior sigue en su sitio.
        nuevo = partes[-1].rsplit(".", 1)[0]
        for i in range(len(pista.devices) - 1, -1, -1):
            nombre = pista.devices[i].name
            if nombre != nuevo and _RACK_DEL_MOTOR.match(nombre):
                pista.delete_device(i)
    finally:
        song.end_undo_step()
    return True

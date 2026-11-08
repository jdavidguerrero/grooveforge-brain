"""Comandos sobre el set. Solo corren en el hilo principal de Live; sin dependencias de red para probarlos fuera."""

import re

from .buscar import buscar_item, buscar_plugin

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


def _notas_del_clip(clip):
    return [
        {
            "pitch": int(n.pitch),
            "start_beat": round(float(n.start_time), 4),
            "duration_beats": round(float(n.duration), 4),
            "velocity": int(n.velocity),
        }
        for n in clip.get_notes_extended(0, 128, 0.0, float(clip.length))
    ]


def leer_clip(song, m):
    """Solo devuelve el clip que empieza justo en ese tiempo; nunca el más cercano."""
    pista = _pista_midi(song, int(m["track_index"]))
    inicio = float(m["start_beats"])
    clip = next((c for c in _clips(pista) if abs(c.start_time - inicio) < 1e-3), None)
    if clip is None:
        return {"clip": None}
    return {"clip": {"start": clip.start_time, "length": clip.length, "name": clip.name, "notes": _notas_del_clip(clip)}}


def leer_pista(song, m):
    """Todos los clips del arreglo con sus notas, en una sola consulta: Live atiende pocos comandos por segundo."""
    pista = _pista(song, int(m["track_index"]))
    midi = pista.has_midi_input
    return {"clips": [
        {"start": c.start_time, "length": c.length, "name": c.name, "end": c.end_time, "marker": c.start_marker,
         "notes": _notas_del_clip(c) if midi else None}
        for c in _clips(pista)
    ]}


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
        # Después de cargar: si la carga falla, el rack anterior sigue en su sitio. Solo un rack del motor
        # reemplaza a otro rack del motor; una cadena de efectos (GF ROL) se suma y no borra nada.
        nuevo = partes[-1].rsplit(".", 1)[0]
        if _RACK_DEL_MOTOR.match(nuevo):
            for i in range(len(pista.devices) - 1, -1, -1):
                nombre = pista.devices[i].name
                if nombre != nuevo and _RACK_DEL_MOTOR.match(nombre):
                    pista.delete_device(i)
    finally:
        song.end_undo_step()
    return True


def _es_instrumento(dispositivo):
    tipo = getattr(dispositivo, "type", None)
    return tipo == 1 or str(tipo).endswith("instrument")  # Live.Device.DeviceType.instrument


def cargar_plugin(song, navegador, m):
    """Pone un plugin en una pista que todavía no tiene instrumento. Si ya tiene uno (el de Juan, un rack o
    el mismo plugin), no toca nada: nunca reemplaza un sonido elegido."""
    pistas = [t for t in song.tracks if t.name == m["pista"]]
    if len(pistas) != 1:
        raise ErrorComando("Se esperaba una pista %s y hay %d" % (m["pista"], len(pistas)))
    pista = pistas[0]
    if any(_es_instrumento(d) for d in pista.devices):
        return {"cargado": False, "motivo": "ya tiene instrumento"}
    item = buscar_plugin(navegador.plugins, m["plugin"])
    if item is None:
        raise ErrorComando("Live no encontró el plugin %r en Plugins" % m["plugin"])
    song.view.selected_track = pista
    navegador.load_item(item)
    return {"cargado": True}


def borrar_clip(song, m):
    """Borra el clip del arreglo que empieza exactamente en `start_beats`. El motor solo lo pide para clips suyos
    o para el material que ya guardó."""
    pista = _pista(song, int(m["track_index"]))
    clip = next((c for c in pista.arrangement_clips if abs(c.start_time - float(m["start_beats"])) < 1e-3), None)
    if clip is None:
        return {"borrado": False}
    song.begin_undo_step()
    try:
        pista.delete_clip(clip)
    finally:
        song.end_undo_step()
    return {"borrado": True}


def _db_de(parametro, valor=None):
    """dB del texto que muestra Live («-6.0 dB», «-inf dB»)."""
    texto = parametro.str_for_value(parametro.value if valor is None else valor)
    numero = texto.replace("dB", "").strip()
    try:
        return float(numero)
    except ValueError:
        return float("-inf")


def _valor_para_db(parametro, db):
    """Live guarda el fader de 0 a 1 con una curva propia: se busca el valor cuyo texto da los dB pedidos."""
    if db == float("-inf"):
        return parametro.min
    bajo, alto = parametro.min, parametro.max
    for _ in range(40):
        medio = (bajo + alto) / 2
        if _db_de(parametro, medio) < db:
            bajo = medio
        else:
            alto = medio
    return (bajo + alto) / 2


def leer_mezcla(song, _m):
    pistas = [
        {"name": t.name, "volume_db": _db_de(t.mixer_device.volume), "pan": t.mixer_device.panning.value,
         "sends": [_db_de(s) for s in t.mixer_device.sends], "devices": [d.name for d in t.devices]}
        for t in song.tracks
    ]
    return {"tracks": pistas, "returns": [r.name for r in song.return_tracks]}


def poner_mezcla(song, m):
    pistas = [t for t in song.tracks if t.name == m["pista"]]
    if len(pistas) != 1:
        raise ErrorComando("Se esperaba una pista %s y hay %d" % (m["pista"], len(pistas)))
    mixer = pistas[0].mixer_device
    song.begin_undo_step()
    try:
        if m.get("volume_db") is not None:
            mixer.volume.value = _valor_para_db(mixer.volume, float(m["volume_db"]))
        if m.get("pan") is not None:
            mixer.panning.value = max(-1.0, min(1.0, float(m["pan"])))
        envios = list(mixer.sends)
        for indice, db in (m.get("sends") or {}).items():
            i = int(indice)
            if 0 <= i < len(envios):
                envios[i].value = _valor_para_db(envios[i], float(db))
    finally:
        song.end_undo_step()
    return {}


def duplicar_pista(song, m):
    """Duplica una pista con todo (instrumento, preset, cadena y clips) y le pone nombre. Queda justo debajo."""
    i = int(m["track_index"])
    _pista(song, i)
    song.duplicate_track(i)
    nueva = list(song.tracks)[i + 1]
    nueva.name = m["name"]
    return {"track_index": i + 1}

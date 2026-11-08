"""Cliente de la superficie GFMotor. Teoría: apps/docs/theory/motor-primera-entrega.md §2-§3."""

from __future__ import annotations

import json
import socket
import struct
import threading
import uuid

_CABECERA = struct.Struct(">I")
PUERTO = 9878


class ErrorLive(RuntimeError):
    """Live no respondió o rechazó el comando."""


class ClienteLive:
    """Una conexión persistente: Live tarda ~1 s en aceptar cada conexión nueva, y un set armado pide cientos de
    lecturas. GFMotor ya atiende varios mensajes por conexión."""

    def __init__(self, host: str = "127.0.0.1", puerto: int = PUERTO, timeout: float = 30.0) -> None:
        self.host = host
        self.puerto = puerto
        self.timeout = timeout
        self._socket: socket.socket | None = None
        self._candado = threading.Lock()

    def _conectar(self) -> socket.socket:
        s = socket.create_connection((self.host, self.puerto), timeout=min(self.timeout, 2.0))
        s.settimeout(self.timeout)
        return s

    def _cerrar(self) -> None:
        if self._socket is not None:
            try:
                self._socket.close()
            except OSError:
                pass
            self._socket = None

    def llamar(self, tipo: str, **campos: object) -> dict:
        ident = uuid.uuid4().hex
        cuerpo = json.dumps({"type": tipo, "id": ident, **campos}).encode()
        trama = _CABECERA.pack(len(cuerpo)) + cuerpo
        with self._candado:
            try:
                if self._socket is None:
                    self._socket = self._conectar()
                try:
                    self._socket.sendall(trama)
                except OSError:
                    # La conexión vieja se cerró (Live se reinició): una sola vez, por una nueva. Si el envío
                    # falló, Live no recibió el mensaje, así que repetirlo no lo duplica.
                    self._cerrar()
                    self._socket = self._conectar()
                    self._socket.sendall(trama)
                respuesta = self._recibir(self._socket)
            except (OSError, ConnectionError, ValueError) as exc:
                self._cerrar()
                raise ErrorLive(f"La superficie GFMotor no responde en {self.host}:{self.puerto}: {exc}") from exc
        if respuesta.get("id") not in (None, ident):
            with self._candado:
                self._cerrar()
            raise ErrorLive("Respuesta de Live fuera de orden: se reinicia la conexión")
        return respuesta

    @staticmethod
    def _recibir(s: socket.socket) -> dict:
        def exacto(n: int) -> bytes:
            datos = b""
            while len(datos) < n:
                parte = s.recv(n - len(datos))
                if not parte:
                    raise ConnectionError("Live cerró la conexión")
                datos += parte
            return datos

        return json.loads(exacto(_CABECERA.unpack(exacto(4))[0]))

    def _ok(self, tipo: str, **campos: object) -> dict:
        r = self.llamar(tipo, **campos)
        if not r.get("ok"):
            raise ErrorLive(f"{tipo}: {r.get('error', 'error desconocido')}")
        return r

    def sesion(self) -> dict:
        return self._ok("get_session")["session"]

    def indice_pista(self, nombre: str) -> int | None:
        coincidencias = [t["index"] for t in self.sesion()["tracks"] if t["name"] == nombre]
        if len(coincidencias) > 1:
            raise ErrorLive(f"Hay {len(coincidencias)} pistas llamadas {nombre!r}; renombra una para evitar escribir en la equivocada")
        return coincidencias[0] if coincidencias else None

    def crear_pista_midi(self, nombre: str) -> int:
        return int(self._ok("create_midi_track", track_name=nombre)["track_index"])

    def poner_tempo(self, bpm: float) -> None:
        self._ok("set_tempo", value=bpm)

    def escribir_clip(self, pista: int, inicio: float, longitud: float, notas: list[dict], nombre: str, color: int | None = None) -> None:
        self._ok("write_clip", track_index=pista, start_beats=inicio, length_beats=longitud, notes=notas, clip_name=nombre, color=color)

    def leer_clip(self, pista: int, inicio: float) -> dict | None:
        """Clip que empieza exactamente en `inicio`, o None."""
        return self._ok("read_clip", track_index=pista, start_beats=inicio)["clip"]

    def listar_clips(self, pista: int) -> list[dict]:
        return self._ok("list_clips", track_index=pista)["clips"]

    def pista_sin_clips(self, pista: int) -> bool:
        return not self._ok("list_clips", track_index=pista)["clips"]

    def version(self) -> int:
        """Versión de GFMotor que Live cargó al arrancar."""
        try:
            return int(self.llamar("ping").get("version", "1"))
        except (ValueError, TypeError):
            return 1

    def clips_con_notas(self, pista: int) -> list[dict]:
        """Clips del arreglo con sus notas. Con GFMotor 5, una consulta; antes, una por clip."""
        r = self.llamar("read_track", track_index=pista)
        if r.get("ok"):
            return r["clips"]
        clips = self.listar_clips(pista)
        for c in clips:
            try:
                leido = self.leer_clip(pista, c["start"])
            except ErrorLive:
                leido = None  # pista de audio
            c["notes"] = leido["notes"] if leido else None
        return clips

    def duplicar_pista(self, pista: int, nombre: str) -> int:
        return int(self._ok("duplicate_track", track_index=pista, name=nombre)["track_index"])

    def leer_mezcla(self) -> dict:
        return self._ok("get_mixer")

    def poner_mezcla(self, pista: str, volume_db: float | None, pan: float | None, sends: dict[int, float]) -> None:
        self._ok("set_mixer", pista=pista, volume_db=volume_db, pan=pan, sends={str(k): v for k, v in sends.items()})

    def borrar_clip(self, pista: int, inicio: float) -> bool:
        return bool(self._ok("delete_clip", track_index=pista, start_beats=inicio).get("borrado"))

    def cargar_plugin(self, plugin: str, pista: str) -> dict:
        """Solo si la pista no tiene instrumento; si ya tiene, no toca nada."""
        return self._ok("load_plugin", plugin=plugin, pista=pista)

    def cargar_rack(self, partes: list[str], pista: str) -> None:
        self._ok("load_rack", partes=partes, pista=pista)

"""Cliente de la superficie GFMotor. Teoría: apps/docs/theory/motor-primera-entrega.md §2-§3."""

from __future__ import annotations

import json
import socket
import struct
import uuid

_CABECERA = struct.Struct(">I")
PUERTO = 9878


class ErrorLive(RuntimeError):
    """Live no respondió o rechazó el comando."""


class ClienteLive:
    def __init__(self, host: str = "127.0.0.1", puerto: int = PUERTO, timeout: float = 30.0) -> None:
        self.host = host
        self.puerto = puerto
        self.timeout = timeout

    def llamar(self, tipo: str, **campos: object) -> dict:
        ident = uuid.uuid4().hex
        cuerpo = json.dumps({"type": tipo, "id": ident, **campos}).encode()
        try:
            with socket.create_connection((self.host, self.puerto), timeout=min(self.timeout, 2.0)) as s:
                s.settimeout(self.timeout)
                s.sendall(_CABECERA.pack(len(cuerpo)) + cuerpo)
                return self._recibir(s)
        except OSError as exc:
            raise ErrorLive(f"La superficie GFMotor no responde en {self.host}:{self.puerto}: {exc}") from exc

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

    def borrar_clip(self, pista: int, inicio: float) -> bool:
        return bool(self._ok("delete_clip", track_index=pista, start_beats=inicio).get("borrado"))

    def cargar_plugin(self, plugin: str, pista: str) -> dict:
        """Solo si la pista no tiene instrumento; si ya tiene, no toca nada."""
        return self._ok("load_plugin", plugin=plugin, pista=pista)

    def cargar_rack(self, partes: list[str], pista: str) -> None:
        self._ok("load_rack", partes=partes, pista=pista)

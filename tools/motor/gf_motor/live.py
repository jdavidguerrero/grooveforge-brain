"""Cliente del Remote Script de GroovePilot. Teoría: apps/docs/theory/motor-primera-entrega.md §2-§3."""

from __future__ import annotations

import json
import socket
import struct
import uuid

_CABECERA = struct.Struct(">I")
_SIN_CLIPS = "No arrangement clips"


class ErrorLive(RuntimeError):
    """Live no respondió o rechazó el comando."""


class ClienteLive:
    def __init__(self, host: str = "127.0.0.1", puerto: int = 9877, timeout: float = 20.0) -> None:
        self.host = host
        self.puerto = puerto
        self.timeout = timeout

    def llamar(self, tipo: str, **campos: object) -> dict:
        ident = uuid.uuid4().hex
        cuerpo = json.dumps({"type": tipo, "id": ident, **campos}).encode()
        try:
            with socket.create_connection((self.host, self.puerto), timeout=self.timeout) as s:
                s.settimeout(self.timeout)
                s.sendall(_CABECERA.pack(len(cuerpo)) + cuerpo)
                while True:
                    r = self._recibir(s)
                    # read_midi_clip responde primero "queued" y después el resultado con el mismo id;
                    # entre medias pueden llegar notificaciones ajenas.
                    if r.get("id") == ident and not r.get("queued"):
                        return r
        except OSError as exc:
            raise ErrorLive(f"Sin conexión con Live en {self.host}:{self.puerto}: {exc}") from exc

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

    def escribir_clip(self, pista: int, inicio: float, longitud: float, notas: list[dict], nombre: str) -> None:
        r = self._ok(
            "write_arrangement_clip", track_index=pista, start_beats=inicio, length_beats=longitud, notes=notas, clip_name=nombre
        )
        if r.get("view") != "arrangement":
            raise ErrorLive(f"El clip {nombre!r} no llegó a Arrangement")

    def leer_clip(self, pista: int, tiempo: float) -> dict | None:
        """Clip que empieza exactamente en `tiempo`, o None. El script devuelve el más cercano si no lo hay."""
        r = self.llamar("read_midi_clip", view="arrangement", track_index=pista, time_beats=tiempo)
        if not r.get("ok"):
            if _SIN_CLIPS in str(r.get("error", "")):
                return None
            raise ErrorLive(f"read_midi_clip: {r.get('error', 'error desconocido')}")
        return r if abs(float(r["clip_start_time"]) - tiempo) < 1e-3 else None

    def pista_sin_clips(self, pista: int) -> bool:
        r = self.llamar("read_midi_clip", view="arrangement", track_index=pista, time_beats=0.0)
        if r.get("ok"):
            return False
        if _SIN_CLIPS in str(r.get("error", "")):
            return True
        raise ErrorLive(f"read_midi_clip: {r.get('error', 'error desconocido')}")

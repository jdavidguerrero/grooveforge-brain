"""Puente simulado: imita el protocolo de GFMotor, incluida la sobrescritura al duplicar en Arrangement."""

from __future__ import annotations

import json
import socketserver
import struct
import threading

_H = struct.Struct(">I")


class LiveSimulado:
    def __init__(self, ruta: str = "") -> None:
        self.ruta = ruta
        self.tempo = 124.0
        self.pistas: list[dict] = [{"name": "SC GHOST", "clips": []}, {"name": "KICK", "clips": []}]
        self.comandos: list[str] = []
        self.cargas: list[tuple[list[str], str]] = []
        self.error_carga: str | None = None
        externo = self

        class Manejador(socketserver.BaseRequestHandler):
            def handle(self) -> None:
                while True:
                    try:
                        msg = json.loads(self._exacto(_H.unpack(self._exacto(4))[0]))
                    except ConnectionError:
                        return
                    b = json.dumps({"id": msg["id"], **externo.atender(msg)}).encode()
                    self.request.sendall(_H.pack(len(b)) + b)

            def _exacto(self, n: int) -> bytes:
                d = b""
                while len(d) < n:
                    p = self.request.recv(n - len(d))
                    if not p:
                        raise ConnectionError
                    d += p
                return d

        class Servidor(socketserver.ThreadingTCPServer):
            allow_reuse_address = True
            daemon_threads = True

        self._srv = Servidor(("127.0.0.1", 0), Manejador)
        self.puerto = self._srv.server_address[1]
        threading.Thread(target=self._srv.serve_forever, daemon=True).start()

    def cerrar(self) -> None:
        self._srv.shutdown()
        self._srv.server_close()

    def atender(self, m: dict) -> dict:
        t = m["type"]
        self.comandos.append(t)
        if t == "get_session":
            pistas = [{"index": i, "name": p["name"]} for i, p in enumerate(self.pistas)]
            return {"ok": True, "session": {"tempo": self.tempo, "tracks": pistas, "file_path": self.ruta}}
        if t == "set_tempo":
            self.tempo = m["value"]
            return {"ok": True}
        if t == "create_midi_track":
            self.pistas.append({"name": m["track_name"], "clips": []})
            return {"ok": True, "track_index": len(self.pistas) - 1}
        if t == "write_clip":
            pista = self.pistas[m["track_index"]]
            a, b = m["start_beats"], m["start_beats"] + m["length_beats"]
            pista["clips"] = [c for c in pista["clips"] if c["start"] + c["length"] <= a or c["start"] >= b]
            pista["clips"].append({"start": a, "length": m["length_beats"], "notes": m["notes"], "name": m["clip_name"], "color": m.get("color")})
            return {"ok": True}
        if t == "list_clips":
            return {"ok": True, "clips": [{k: c[k] for k in ("start", "length", "name")} for c in self.pistas[m["track_index"]]["clips"]]}
        if t == "read_clip":
            c = next((x for x in self.pistas[m["track_index"]]["clips"] if abs(x["start"] - m["start_beats"]) < 1e-3), None)
            return {"ok": True, "clip": c}
        if t == "load_rack":
            if self.error_carga:
                return {"ok": False, "error": self.error_carga}
            self.cargas.append((m["partes"], m["pista"]))
            return {"ok": True}
        return {"ok": False, "error": f"Comando desconocido: {t!r}"}

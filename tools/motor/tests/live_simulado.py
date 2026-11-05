"""Live simulado: imita el protocolo del Remote Script, incluidos sus comportamientos peligrosos."""

from __future__ import annotations

import json
import socket
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
        externo = self

        class Manejador(socketserver.BaseRequestHandler):
            def handle(self) -> None:
                while True:
                    try:
                        cab = self._exacto(4)
                        msg = json.loads(self._exacto(_H.unpack(cab)[0]))
                    except ConnectionError:
                        return
                    if msg["type"] == "read_midi_clip":
                        self._enviar({"type": "ack", "id": msg["id"], "ok": True, "queued": True})
                        self._enviar({"type": "room_state", "tracks": []})
                    self._enviar({"type": "ack", "id": msg["id"], **externo.atender(msg)})

            def _exacto(self, n: int) -> bytes:
                d = b""
                while len(d) < n:
                    p = self.request.recv(n - len(d))
                    if not p:
                        raise ConnectionError
                    d += p
                return d

            def _enviar(self, datos: dict) -> None:
                b = json.dumps(datos).encode()
                self.request.sendall(_H.pack(len(b)) + b)

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
        self.comandos.append(m["type"])
        t = m["type"]
        if t == "get_session":
            pistas = [{"index": i, "name": p["name"], "lom_id": 1} for i, p in enumerate(self.pistas)]
            return {"ok": True, "session": {"tempo": self.tempo, "tracks": pistas, "file_path": self.ruta}}
        if t == "set_tempo":
            self.tempo = m["value"]
            return {"ok": True}
        if t == "create_midi_track":
            self.pistas.append({"name": m["track_name"], "clips": []})
            return {"ok": True, "track_index": len(self.pistas) - 1}
        if t == "write_arrangement_clip":
            pista = self.pistas[m["track_index"]]
            a, b = m["start_beats"], m["start_beats"] + m["length_beats"]
            # Igual que duplicate_clip_to_arrangement: lo que estaba en ese tramo desaparece.
            pista["clips"] = [c for c in pista["clips"] if c["start"] + c["length"] <= a or c["start"] >= b]
            pista["clips"].append({"start": a, "length": m["length_beats"], "notes": m["notes"], "name": m["clip_name"]})
            return {"ok": True, "view": "arrangement"}
        if t == "read_midi_clip":
            pista = self.pistas[m["track_index"]]
            if not pista["clips"]:
                return {"ok": False, "error": f"No arrangement clips on track {m['track_index']}"}
            tiempo = m["time_beats"]
            dentro = [c for c in pista["clips"] if c["start"] <= tiempo < c["start"] + c["length"]]
            # Igual que el script: sin clip en ese tiempo, devuelve el más cercano.
            c = dentro[0] if dentro else min(pista["clips"], key=lambda x: abs(x["start"] - tiempo))
            return {"ok": True, "notes": c["notes"], "length_beats": c["length"], "clip_start_time": c["start"], "view": "arrangement"}
        return {"ok": False, "error": f"Unknown message type: {t!r}"}

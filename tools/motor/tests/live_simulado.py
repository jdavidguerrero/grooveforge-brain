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
        self.plugins: list[tuple[str, str]] = []
        self.version = "5"
        self.retornos = ["A Office", "B Plate", "C Hall", "D Rumble", "E Delay", "F Echo"]
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
        if t == "ping":
            return {"ok": True, "version": self.version}
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
        if t == "read_track":
            if self.version < "5":
                return {"ok": False, "error": "Comando desconocido: 'read_track'"}
            clips = sorted(self.pistas[m["track_index"]]["clips"], key=lambda c: c["start"])
            return {"ok": True, "clips": [{"start": c["start"], "length": c["length"], "name": c["name"], "notes": c["notes"]} for c in clips]}
        if t == "get_mixer":
            pistas = [{"name": p["name"], "volume_db": p.get("volume_db", 0.0), "pan": p.get("pan", 0.0),
                       "sends": p.get("sends", [float("-inf")] * len(self.retornos))} for p in self.pistas]
            return {"ok": True, "tracks": pistas, "returns": list(self.retornos)}
        if t == "set_mixer":
            p = next(x for x in self.pistas if x["name"] == m["pista"])
            if m.get("volume_db") is not None:
                p["volume_db"] = m["volume_db"]
            if m.get("pan") is not None:
                p["pan"] = m["pan"]
            envios = p.setdefault("sends", [float("-inf")] * len(self.retornos))
            for i, db in (m.get("sends") or {}).items():
                if int(i) < len(envios):
                    envios[int(i)] = db
            return {"ok": True}
        if t == "delete_clip":
            pista = self.pistas[m["track_index"]]
            antes = len(pista["clips"])
            pista["clips"] = [c for c in pista["clips"] if abs(c["start"] - m["start_beats"]) >= 1e-3]
            return {"ok": True, "borrado": len(pista["clips"]) < antes}
        if t == "load_plugin":
            pista = next(p for p in self.pistas if p["name"] == m["pista"])
            if pista.get("instrumento"):
                return {"ok": True, "cargado": False, "motivo": "ya tiene instrumento"}
            pista["instrumento"] = m["plugin"]
            self.plugins.append((m["plugin"], m["pista"]))
            return {"ok": True, "cargado": True}
        if t == "load_rack":
            if self.error_carga:
                return {"ok": False, "error": self.error_carga}
            self.cargas.append((m["partes"], m["pista"]))
            return {"ok": True}
        return {"ok": False, "error": f"Comando desconocido: {t!r}"}

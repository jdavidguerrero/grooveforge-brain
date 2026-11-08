"""Interfaz web local. Solo escucha en 127.0.0.1 y rechaza peticiones de otros orígenes, porque puede escribir en Live."""

from __future__ import annotations

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlparse

from .borrador import COLORES, PLANTILLAS
from .live import PUERTO, ClienteLive, ErrorLive
from .motor import Motor

_WEB = Path(__file__).parent / "web" / "index.html"


def crear_servidor(motor: Motor, puerto: int = 8765) -> ThreadingHTTPServer:
    acciones = {
        "partida": lambda d: motor.partida(d["genero"], d["raiz"], d["modo"], float(d["bpm"])),
        "guia": lambda d: motor.crear_guia(),
        "borrador": lambda d: motor.borrador_kick(),
        "kicks": lambda d: motor.generar_kicks(),
        "seleccionar": lambda d: motor.alternar_kick(d["id"]),
        "usar": lambda d: motor.usar_kick(d["id"]),
        "nueva_seleccion": lambda d: motor.nueva_seleccion(),
        "revisar": lambda d: motor.revisar_edicion(),
        "bajo_patrones": lambda d: motor.generar_patrones(),
        "bajo_poner": lambda d: motor.poner_patron(d["id"]),
        "bajo_capas": lambda d: motor.cargar_capas(),
        "bajo_capa": lambda d: motor.usar_capa(d["nombre"]),
        "bajo_revisar": lambda d: motor.revisar_bajo(),
        "drums": lambda d: motor.generar_drums(),
        "drum_patrones": lambda d: motor.proponer_patrones_drums(),
        "drum_poner": lambda d: motor.poner_patron_drums(d["nombre"]),
        "tops_proponer": lambda d: motor.proponer_tops(d["rol"]),
        "tops_poner": lambda d: motor.poner_top(d["rol"], d["id"]),
        "motivo_generar": lambda d: motor.generar_motivos(),
        "motivo_poner": lambda d: motor.poner_motivo(d["id"]),
        "motivo_capas": lambda d: motor.cargar_capas_motivo(),
        "motivo_capa": lambda d: motor.usar_capa_motivo(d["nombre"]),
        "acordes_proponer": lambda d: motor.proponer_acordes(),
        "acordes_poner": lambda d: motor.poner_acordes(d["id"]),
        "pads_proponer": lambda d: motor.proponer_pads(),
        "pads_poner": lambda d: motor.poner_pad(d["parte"], d["id"]),
        "detalles_proponer": lambda d: motor.proponer_detalles(),
        "detalles_poner": lambda d: motor.poner_detalle(d["parte"], d["id"]),
        "transiciones_proponer": lambda d: motor.proponer_transiciones(),
        "roll_sonido": lambda d: motor.sonido_roll(d["material"]),
        "subdrop_sonido": lambda d: motor.sonido_subdrop(d["variante"]),
        "transiciones_poner": lambda d: motor.poner_transicion(d["elemento"], d["id"]),
        "drum_elegir": lambda d: motor.elegir_drum(d["id"]),
        "tonalidad": lambda d: motor.cambiar_tonalidad(d["raiz"], d["modo"]),
        "registrar": lambda d: motor.registrar(d.get("sonido", ""), d.get("kick", ""), d.get("nota", "")),
    }

    class Manejador(BaseHTTPRequestHandler):
        def log_message(self, *_: object) -> None:
            pass

        def _enviar(self, codigo: int, cuerpo: bytes, tipo: str = "application/json") -> None:
            self.send_response(codigo)
            self.send_header("Content-Type", tipo)
            self.send_header("Content-Length", str(len(cuerpo)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(cuerpo)

        def _json(self, codigo: int, datos: object) -> None:
            self._enviar(codigo, json.dumps(datos, ensure_ascii=False).encode())

        def _origen_valido(self) -> bool:
            esperado = f"127.0.0.1:{self.server.server_address[1]}"
            origen = self.headers.get("Origin")
            return self.headers.get("Host") == esperado and (origen is None or origen == f"http://{esperado}")

        def do_GET(self) -> None:
            if not self._origen_valido():
                return self._json(403, {"error": "Origen no permitido"})
            ruta = urlparse(self.path).path
            if ruta == "/":
                return self._enviar(200, _WEB.read_bytes(), "text/html; charset=utf-8")
            if ruta == "/api/estado":
                try:
                    live = {"conectado": True, **motor.info_set()}
                except ErrorLive as exc:
                    live = {"conectado": False, "error": str(exc)}
                plantillas = {
                    g: [{"nombre": n, "compases": c, "color": f"#{COLORES[n]:06X}"} for n, c in bloques] for g, bloques in PLANTILLAS.items()
                }
                return self._json(200, {"live": live, "sesion": motor.estado, "plantillas": plantillas})
            if ruta.startswith("/drum/"):
                try:
                    _, _, ident, voz = ruta.split("/")
                    return self._enviar(200, motor.ruta_drum(unquote(ident), unquote(voz)).read_bytes(), "audio/wav")
                except (ValueError, OSError) as exc:
                    return self._json(404, {"error": str(exc)})
            if ruta.startswith("/audio/"):
                try:
                    return self._enviar(200, motor.ruta_opcion(ruta.rsplit("/", 1)[-1]).read_bytes(), "audio/wav")
                except (ValueError, OSError) as exc:
                    return self._json(404, {"error": str(exc)})
            self._json(404, {"error": "No existe"})

        def do_POST(self) -> None:
            ruta = urlparse(self.path).path
            accion = acciones.get(ruta.removeprefix("/api/"))
            if not self._origen_valido() or self.headers.get("Content-Type") != "application/json":
                return self._json(403, {"error": "Origen no permitido"})
            if accion is None:
                return self._json(404, {"error": "No existe"})
            try:
                datos = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
                self._json(200, {"sesion": accion(datos)})
            except (ErrorLive, ValueError, KeyError, OSError) as exc:
                self._json(400, {"error": str(exc)})

    return ThreadingHTTPServer(("127.0.0.1", puerto), Manejador)


def main() -> None:
    ap = argparse.ArgumentParser(description="Motor de asistencia: primera entrega")
    ap.add_argument("--puerto", type=int, default=8765)
    ap.add_argument("--puerto-live", type=int, default=PUERTO)
    a = ap.parse_args()
    servidor = crear_servidor(Motor(ClienteLive(puerto=a.puerto_live)), a.puerto)
    print(f"Motor en http://127.0.0.1:{a.puerto}  (Ctrl+C para cerrar; Live sigue igual)")
    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

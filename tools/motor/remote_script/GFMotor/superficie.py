"""Puente del motor en 127.0.0.1:9878. El script de GroovePilot sigue igual; este es independiente."""

import json
import queue
import socket
import struct
import threading
import time

import Live
from _Framework.ControlSurface import ControlSurface

from . import comandos

PUERTO = 9878
VERSION = "1"
ESPERA_COMANDO_S = 15.0
ESPERA_INDEXADO_S = 20.0  # Live tarda en ver un archivo recién creado en una carpeta de Places.
_CABECERA = struct.Struct(">I")


class _Pedido:
    def __init__(self, mensaje):
        self.mensaje = mensaje
        self.respuesta = None
        self.cancelado = False
        self.listo = threading.Event()
        self.limite = time.time() + ESPERA_INDEXADO_S
        self.proximo_intento = 0.0


def _exacto(conexion, n):
    datos = b""
    while len(datos) < n:
        parte = conexion.recv(n - len(datos))
        if not parte:
            raise ConnectionError
        datos += parte
    return datos


class SuperficieGFMotor(ControlSurface):
    def __init__(self, c_instance):
        super().__init__(c_instance)
        self._cola = queue.Queue()
        self._cargas = []
        self._vivo = True
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._socket.bind(("127.0.0.1", PUERTO))
        self._socket.listen(4)
        self._socket.settimeout(0.5)
        threading.Thread(target=self._aceptar, daemon=True).start()
        self.log_message("GFMotor %s escuchando en %d" % (VERSION, PUERTO))

    def disconnect(self):
        self._vivo = False
        try:
            self._socket.close()
        except OSError:
            pass
        super().disconnect()

    # ── Red: este hilo nunca toca el set ──
    def _aceptar(self):
        while self._vivo:
            try:
                conexion, _ = self._socket.accept()
            except (socket.timeout, TimeoutError):
                continue
            except OSError:
                return
            threading.Thread(target=self._atender, args=(conexion,), daemon=True).start()

    def _atender(self, conexion):
        with conexion:
            try:
                while self._vivo:
                    conexion.settimeout(None)
                    largo = _CABECERA.unpack(_exacto(conexion, 4))[0]
                    mensaje = json.loads(_exacto(conexion, largo))
                    cuerpo = json.dumps({"id": mensaje.get("id"), **self._resolver(mensaje)}).encode()
                    conexion.sendall(_CABECERA.pack(len(cuerpo)) + cuerpo)
            except (ConnectionError, OSError, ValueError):
                pass

    def _resolver(self, mensaje):
        if mensaje.get("type") == "ping":
            return {"ok": True, "version": VERSION}
        pedido = _Pedido(mensaje)
        self._cola.put(pedido)
        espera = ESPERA_INDEXADO_S + 5.0 if mensaje.get("type") == "load_rack" else ESPERA_COMANDO_S
        if pedido.listo.wait(espera):
            return pedido.respuesta
        # Evita que Live lo ejecute tarde, cuando el motor ya informó un fallo.
        pedido.cancelado = True
        return {"ok": False, "error": "Live no respondió a tiempo"}

    # ── Hilo principal de Live ──
    def update_display(self):
        super().update_display()
        while True:
            try:
                pedido = self._cola.get_nowait()
            except queue.Empty:
                break
            if pedido.cancelado:
                continue
            if pedido.mensaje.get("type") == "load_rack":
                self._cargas.append(pedido)
            else:
                self._terminar(pedido, self._ejecutar(pedido.mensaje))
        ahora = time.time()
        for pedido in list(self._cargas):
            if ahora >= pedido.proximo_intento:
                pedido.proximo_intento = ahora + 1.0
                resultado = self._intentar_carga(pedido, ahora)
                if resultado is not None:
                    self._cargas.remove(pedido)
                    self._terminar(pedido, resultado)

    @staticmethod
    def _terminar(pedido, respuesta):
        pedido.respuesta = respuesta
        pedido.listo.set()

    def _ejecutar(self, m):
        from Live.Clip import MidiNoteSpecification

        song = self.song()
        tabla = {
            "get_session": comandos.sesion,
            "set_tempo": comandos.poner_tempo,
            "create_midi_track": comandos.crear_pista,
            "list_clips": comandos.listar_clips,
            "read_clip": comandos.leer_clip,
            "write_clip": lambda s, x: comandos.escribir_clip(s, x, MidiNoteSpecification),
        }
        funcion = tabla.get(m.get("type"))
        if funcion is None:
            return {"ok": False, "error": "Comando desconocido: %r" % m.get("type")}
        try:
            return {"ok": True, **funcion(song, m)}
        except Exception as exc:
            self.log_message("GFMotor %s: %s" % (m.get("type"), exc))
            return {"ok": False, "error": str(exc)}

    def _intentar_carga(self, pedido, ahora):
        if pedido.cancelado:
            return {"ok": False, "error": "Cancelado"}
        m = pedido.mensaje
        try:
            navegador = Live.Application.get_application().browser
            if comandos.intentar_cargar(self.song(), navegador, m["partes"], m["pista"]):
                return {"ok": True}
        except Exception as exc:
            self.log_message("GFMotor load_rack: %s" % exc)
            return {"ok": False, "error": str(exc)}
        if ahora > pedido.limite:
            return {"ok": False, "error": "Live no encontró el rack en Places: %s" % "/".join(m["partes"])}
        return None

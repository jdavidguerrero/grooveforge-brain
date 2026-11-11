"""Lector MIDI y diagnóstico CDC de S (apps/docs/consola/08-superficie-pi.md)."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from queue import Empty, SimpleQueue
from threading import Lock
from typing import Iterator

# Console v1, doc18 §3. Unassigned notes decode to None.
BOTONES = (
    "ENCODER_1", "ENCODER_2", "ENCODER_3", "ENCODER_4",
    "ENCODER_5", "ENCODER_6", "ENCODER_7", "ENCODER_8",
    "JOG", "OCT_PLUS", "OCT_MINUS", "PAGE_RIGHT", "PAGE_LEFT", None,
    "PLAY", "STOP", "REC", "LOOP", "UNDO", "NEW", "SHIFT", "METRO",
    "BACK", "MENU", "LEFT", "RIGHT", "BROWSE", "DELETE",
)
PERILLAS = ("MASTER", "CUE", "IN")
BOTONES_EXTRA = (
    "DUPLICATE", "QUANTIZE", "CUE", "FILL", "TAP", "BUILD", "DROP", "BREAK",
    None, None, None, None, "PEDAL",
)
PUERTO = "GF Consola"  # consola_uno; --puerto 'GF Superficie' permite usar consola_s


@dataclass(frozen=True)
class Pad:
    indice: int
    presionado: bool
    velocidad: float


@dataclass(frozen=True)
class Fsr:
    indice: int
    presionado: bool
    velocidad: float


@dataclass(frozen=True)
class Boton:
    nombre: str
    presionado: bool


@dataclass(frozen=True)
class Encoder:
    indice: int
    delta: int


@dataclass(frozen=True)
class Jog:
    delta: int


@dataclass(frozen=True)
class Fader:
    indice: int
    valor: float


@dataclass(frozen=True)
class Expresion:
    indice: int
    valor: float


@dataclass(frozen=True)
class Perilla:
    nombre: str
    valor: float


@dataclass(frozen=True)
class Mano:
    indice: int
    valor: float


@dataclass(frozen=True)
class ManoPresencia:
    indice: int
    presente: bool


@dataclass(frozen=True)
class SelectorPad:
    db: int


Evento = SelectorPad | Pad | Fsr | Boton | Encoder | Jog | Fader | Expresion | Perilla | Mano | ManoPresencia


def decodificar(msg) -> Evento | None:
    if getattr(msg, "channel", None) != 0:
        return None
    if msg.type in ("note_on", "note_off"):
        presionado = msg.type == "note_on" and msg.velocity > 0
        velocidad = msg.velocity / 127 if presionado else 0.0
        if 36 <= msg.note <= 39:
            return Fsr(msg.note - 36, presionado, velocidad)
        if 48 <= msg.note <= 79:
            return Pad(msg.note - 48, presionado, velocidad)
        if 80 <= msg.note <= 107:
            name = BOTONES[msg.note - 80]
            return Boton(name, presionado) if name else None
        if 110 <= msg.note <= 122:
            name = BOTONES_EXTRA[msg.note - 110]
            return Boton(name, presionado) if name else None
        if 108 <= msg.note <= 109:
            return ManoPresencia(msg.note - 108, presionado)
    elif msg.type == "control_change":
        cc, valor = msg.control, msg.value
        if 16 <= cc <= 23:
            delta = valor if valor < 64 else valor - 128
            return Encoder(cc - 16, delta) if delta else None
        if cc == 36 and valor in (0, 64, 127):
            return SelectorPad({0: 0, 64: -10, 127: -20}[valor])
        if cc == 35:
            delta = valor if valor < 64 else valor - 128
            return Jog(delta) if delta else None
        if 24 <= cc <= 27:
            return Fader(cc - 24, valor / 127)
        if 28 <= cc <= 29:
            return Expresion(cc - 28, valor / 127)
        if 30 <= cc <= 32:
            return Perilla(PERILLAS[cc - 30], valor / 127)
        if 33 <= cc <= 34:
            return Mano(cc - 33, valor / 127)
    return None


_FIN = object()


class Superficie:
    """Un puerto, una cola y un consumidor. Usar como context manager o llamar close()."""

    def __init__(self, puerto: str = PUERTO):
        import mido

        self._cola = SimpleQueue()
        self._lock = Lock()
        self._cerrada = False
        nombres = [n for n in mido.get_input_names() if puerto in n]
        if len(nombres) != 1:
            raise ValueError(f"Se esperaba un puerto con {puerto!r}; coincidencias: {nombres}")
        self._puerto = mido.open_input(nombres[0], callback=self._recibir)

    def _recibir(self, msg) -> None:
        evento = decodificar(msg)
        if evento is not None:
            with self._lock:
                if not self._cerrada:
                    self._cola.put(evento)

    def poll(self, timeout: float | None = 0) -> Evento | None:
        """Sin espera por defecto; None espera indefinidamente. Cerrada y vacía devuelve None."""
        try:
            evento = self._cola.get(timeout=timeout)
        except Empty:
            return None
        if evento is _FIN:
            self._cola.put(_FIN)
            return None
        return evento

    def __iter__(self) -> Iterator[Evento]:
        while (evento := self.poll(timeout=None)) is not None:
            yield evento

    def close(self) -> None:
        with self._lock:
            if self._cerrada:
                return
            self._cerrada = True
            self._cola.put(_FIN)
        # El backend puede esperar al callback al cerrar: nunca cerrarlo bajo su lock.
        self._puerto.close()

    def __enter__(self) -> Superficie:
        return self

    def __exit__(self, *_exc) -> None:
        self.close()


class Stats:
    CAMPOS = (
        "loop_max_us", "fsr_hz", "scan_gap_max_us", "ads0_sps", "ads1_sps",
        "events_s", "i2c_errors", "drops",
    )

    def __init__(self, puerto: str, timeout: float = 2.0):
        if timeout <= 0:
            raise ValueError("El timeout serial debe ser positivo")
        self.puerto, self.timeout = puerto, timeout

    @staticmethod
    def parsear(respuesta: str) -> dict[str, int]:
        partes = respuesta.strip().split()
        if partes[:2] != ["@S", "PROTOTYPE"]:
            raise ValueError("Respuesta STATS sin cabecera @S PROTOTYPE")
        campos = {}
        for parte in partes[2:]:
            nombre, separador, valor = parte.partition("=")
            if not separador or nombre in campos or not valor.isascii() or not valor.isdecimal():
                raise ValueError(f"Campo STATS inválido: {parte!r}")
            campos[nombre] = int(valor)
        if not set(Stats.CAMPOS) <= campos.keys():
            raise ValueError("Respuesta STATS incompleta")
        return campos

    def _abrir(self):
        import serial

        return serial.Serial(self.puerto, baudrate=115200, timeout=self.timeout,
                             write_timeout=self.timeout)

    def consultar(self) -> dict[str, int]:
        with self._abrir() as puerto:
            puerto.reset_input_buffer()
            puerto.write(b"@S STATS\n")
            respuesta = puerto.read_until(b"\n", size=1024)
        if not respuesta.endswith(b"\n"):
            raise TimeoutError("Respuesta STATS incompleta o timeout")
        return self.parsear(respuesta.decode("ascii"))

    def reset(self) -> None:
        with self._abrir() as puerto:
            puerto.write(b"@S RESET\n")
            puerto.flush()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    modo = ap.add_mutually_exclusive_group(required=True)
    modo.add_argument("--listar", action="store_true")
    modo.add_argument("--monitor", action="store_true")
    modo.add_argument("--stats", metavar="PUERTO_CDC")
    ap.add_argument("--puerto", default=PUERTO, help="Subcadena del nombre del puerto MIDI")
    a = ap.parse_args()
    if a.listar:
        import mido

        for nombre in mido.get_input_names():
            print(nombre)
    elif a.stats:
        for nombre, valor in Stats(a.stats).consultar().items():
            print(f"{nombre}: {valor}")
    else:
        try:
            with Superficie(a.puerto) as superficie:
                for evento in superficie:
                    print(evento)
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()

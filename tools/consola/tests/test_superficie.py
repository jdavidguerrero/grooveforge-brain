"""Contrato de 08-superficie-pi: mapa completo, consumidor y CDC sin hardware."""
import sys
import subprocess
from concurrent.futures import ThreadPoolExecutor
from types import SimpleNamespace

import pytest

import superficie as s


def test_import_y_decoder_sin_librerias_midi():
    codigo = '''
import builtins
original = builtins.__import__
def importar(nombre, *args, **kwargs):
    if nombre.split('.')[0] in ('mido', 'rtmidi', 'serial'):
        raise ImportError(nombre)
    return original(nombre, *args, **kwargs)
builtins.__import__ = importar
from types import SimpleNamespace
from superficie import decodificar, Encoder
assert decodificar(SimpleNamespace(type='control_change', channel=0, control=16, value=127)) == Encoder(0, -1)
'''
    subprocess.run([sys.executable, "-c", codigo], check=True, capture_output=True)


def nota(numero, tipo="note_on", velocidad=127, canal=0):
    return SimpleNamespace(type=tipo, note=numero, velocity=velocidad, channel=canal)


def cc(numero, valor, canal=0):
    return SimpleNamespace(type="control_change", control=numero, value=valor, channel=canal)


@pytest.mark.parametrize("numero", range(36, 40))
@pytest.mark.parametrize("velocidad", [1, 64, 127])
def test_fsr(numero, velocidad):
    assert s.decodificar(nota(numero, velocidad=velocidad)) == s.Fsr(numero - 36, True, velocidad / 127)


@pytest.mark.parametrize("indice", range(32))
def test_pad_fila_columna(indice):
    fila, columna = divmod(indice, 8)
    assert s.decodificar(nota(48 + fila * 8 + columna, velocidad=100)) == s.Pad(indice, True, 100 / 127)


NOMBRES = (
    "ENCODER_1", "ENCODER_2", "ENCODER_3", "ENCODER_4",
    "ENCODER_5", "ENCODER_6", "ENCODER_7", "ENCODER_8",
    "MCP1_8", "MCP1_9", "MCP1_10", "MCP1_11", "MCP1_12", "MCP1_13",
    "PLAY", "STOP", "REC", "LOOP", "UNDO", "NEW", "SHIFT", "METRO",
    "MCP2_8", "MCP2_9", "MCP2_10", "MCP2_11", "MCP2_12", "MCP2_13",
)


@pytest.mark.parametrize("numero,nombre", enumerate(NOMBRES, 80))
def test_botones(numero, nombre):
    assert s.decodificar(nota(numero)) == s.Boton(nombre, True)


@pytest.mark.parametrize("numero", [*range(36, 40), *range(48, 110)])
@pytest.mark.parametrize("tipo,velocidad", [("note_off", 0), ("note_off", 64), ("note_on", 0)])
def test_liberaciones(numero, tipo, velocidad):
    evento = s.decodificar(nota(numero, tipo, velocidad))
    if isinstance(evento, s.ManoPresencia):
        assert not evento.presente
    else:
        assert not evento.presionado
    if isinstance(evento, (s.Fsr, s.Pad)):
        assert evento.velocidad == 0


@pytest.mark.parametrize("indice", range(8))
@pytest.mark.parametrize("valor,delta", [(0, 0), (1, 1), (127, -1), (63, 63), (64, -64), (126, -2)])
def test_encoder(indice, valor, delta):
    esperado = s.Encoder(indice, delta) if delta else None
    assert s.decodificar(cc(16 + indice, valor)) == esperado


ABSOLUTOS = [
    *[(24 + i, s.Fader, i) for i in range(4)],
    *[(28 + i, s.Expresion, i) for i in range(2)],
    (30, s.Perilla, "MASTER"), (31, s.Perilla, "CUE"), (32, s.Perilla, "IN"),
    (33, s.Mano, 0), (34, s.Mano, 1),
]


@pytest.mark.parametrize("numero,clase,indice", ABSOLUTOS)
@pytest.mark.parametrize("valor", [0, 64, 127])
def test_absolutos(numero, clase, indice, valor):
    assert s.decodificar(cc(numero, valor)) == clase(indice, valor / 127)


@pytest.mark.parametrize("indice", [0, 1])
def test_presencia_no_se_infiere_del_cc(indice):
    assert s.decodificar(nota(108 + indice)) == s.ManoPresencia(indice, True)
    assert s.decodificar(cc(33 + indice, 0)) == s.Mano(indice, 0)


@pytest.mark.parametrize("canal", range(1, 16))
def test_otros_canales(canal):
    for numero in [*range(36, 40), *range(48, 110)]:
        assert s.decodificar(nota(numero, canal=canal)) is None
    for numero in range(16, 35):
        assert s.decodificar(cc(numero, 127, canal)) is None


def test_numeros_y_tipos_desconocidos():
    for numero in range(128):
        if numero not in [*range(36, 40), *range(48, 110)]:
            assert s.decodificar(nota(numero)) is None
        if numero not in range(16, 35):
            assert s.decodificar(cc(numero, 127)) is None
    for tipo in ("clock", "sysex", "pitchwheel", "aftertouch", "program_change"):
        assert s.decodificar(SimpleNamespace(type=tipo, channel=0)) is None
    assert s.decodificar(SimpleNamespace(type="clock")) is None


class PuertoFalso:
    def __init__(self, nombre, callback):
        self.nombre, self.callback, self.cerrado = nombre, callback, False

    def close(self):
        self.cerrado = True
        self.callback(nota(94))


@pytest.fixture
def midi(monkeypatch):
    puertos = []

    def abrir(nombre, callback):
        puerto = PuertoFalso(nombre, callback)
        puertos.append(puerto)
        callback(nota(94))
        return puerto

    modulo = SimpleNamespace(get_input_names=lambda: ["Teensy MIDI:Teensy MIDI MIDI 1 28:0", "GF Superficie:GF Superficie MIDI 1 32:0", "Otro"], open_input=abrir)
    monkeypatch.setitem(sys.modules, "mido", modulo)
    return modulo, puertos


def test_cola_callback_y_consumidor(midi):
    _, puertos = midi
    with s.Superficie() as superficie:
        puerto = puertos[0]
        assert puerto.nombre == "GF Superficie:GF Superficie MIDI 1 32:0"  # elige la S aunque la A también esté conectada
        assert superficie.poll() == s.Boton("PLAY", True)
        assert superficie.poll() is None
        with ThreadPoolExecutor() as hilos:
            consumidor = hilos.submit(lambda: superficie.poll(timeout=1))
            puerto.callback(cc(16, 127))
            assert consumidor.result(timeout=2) == s.Encoder(0, -1)
        puerto.callback(nota(94, "note_on", 0))
        puerto.callback(cc(35, 1))
        puerto.callback(cc(24, 127))
    assert puerto.cerrado
    assert list(superficie) == [s.Boton("PLAY", False), s.Fader(0, 1)]
    assert superficie.poll() is None
    superficie.close()


def test_cierre_despierta_iterador(midi):
    superficie = s.Superficie("GF Superficie")
    superficie.poll()
    with ThreadPoolExecutor() as hilos:
        consumidor = hilos.submit(lambda: list(superficie))
        superficie.close()
        assert consumidor.result(timeout=2) == []


def test_seleccion_no_ambigua(midi):
    modulo, puertos = midi
    with s.Superficie("Otro"):
        assert puertos[-1].nombre == "Otro"
    with pytest.raises(ValueError, match="coincidencias"):
        s.Superficie("Ausente")
    modulo.get_input_names = lambda: ["GF Superficie 1", "GF Superficie 2"]
    with pytest.raises(ValueError, match="coincidencias"):
        s.Superficie()


RESPUESTA = ("@S PROTOTYPE loop_max_us=71 fsr_hz=2500 scan_gap_max_us=401 "
             "ads0_sps=380 ads1_sps=379 events_s=52 i2c_errors=0 drops=0\n")
ESPERADOS = dict(loop_max_us=71, fsr_hz=2500, scan_gap_max_us=401,
                 ads0_sps=380, ads1_sps=379, events_s=52, i2c_errors=0, drops=0)


def test_parsear_stats():
    assert s.Stats.parsear(RESPUESTA) == ESPERADOS


@pytest.mark.parametrize("respuesta", ["", "@S OK", "@S PROTOTYPE drops=0",
    RESPUESTA.replace("fsr_hz=2500", "fsr_hz=nan"), RESPUESTA + " drops=1",
    RESPUESTA.replace("drops=0", "drops=-1")])
def test_stats_invalido(respuesta):
    with pytest.raises(ValueError):
        s.Stats.parsear(respuesta)


def test_cliente_stats_serial_perezoso(monkeypatch):
    class SerialFalso:
        respuesta = RESPUESTA.encode("ascii")

        def __init__(self, puerto, **opciones):
            assert puerto == "/dev/ttyACM1"
            assert opciones["timeout"] == opciones["write_timeout"] == 2
            self.escritos = []
            conexiones.append(self)

        def __enter__(self):
            return self

        def __exit__(self, *_exc):
            self.cerrado = True

        def reset_input_buffer(self):
            self.limpio = True

        def write(self, datos):
            self.escritos.append(datos)

        def read_until(self, terminador, size):
            assert terminador == b"\n" and size == 1024 and self.limpio
            return self.respuesta

        def flush(self):
            self.flushed = True

    conexiones = []
    monkeypatch.setitem(sys.modules, "serial", SimpleNamespace(Serial=SerialFalso))
    cliente = s.Stats("/dev/ttyACM1")
    assert cliente.consultar() == ESPERADOS
    assert conexiones[-1].escritos == [b"@S STATS\n"] and conexiones[-1].cerrado
    cliente.reset()
    assert conexiones[-1].escritos == [b"@S RESET\n"] and conexiones[-1].flushed
    SerialFalso.respuesta = b"@S PROTOTYPE drops=0"
    with pytest.raises(TimeoutError):
        cliente.consultar()

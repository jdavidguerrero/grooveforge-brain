#!/usr/bin/env python3
"""
send_frame.py — GrooveForge Bridge Protocol demo sender

Envía frames binarios al ESP32 vía USB-UART para validar el parser
de BridgeSlave sin necesitar un Teensy físico.

Uso:
    python3 send_frame.py --port /dev/cu.usbmodem* --cmd ENGINE_CHANGED --engine 0 --name "MOOG MODEL D"
    python3 send_frame.py --port /dev/cu.usbmodem* --cmd HEARTBEAT --count 5 --interval 1.0
    python3 send_frame.py --port /dev/cu.usbmodem* --cmd PARAM_CHANGED --param 5 --value 0.75
    python3 send_frame.py --list-ports

Requisitos: pip install pyserial

Spec de referencia: apps/docs/02-bridge-protocol.md
"""

import argparse
import struct
import sys
import time
import glob

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERROR: pyserial no instalado. Instalar con: pip install pyserial")
    sys.exit(1)


# ── CMD IDs (mirror de protocol.h) ─────────────────────────────────────────

CMD = {
    "HEARTBEAT":        0x00,
    "ACK":              0x01,
    "NACK":             0x02,
    "GET_VERSION":      0x03,
    "VERSION":          0x04,
    "SET_ENGINE":       0x10,
    "ENGINE_CHANGED":   0x12,
    "SET_PARAM":        0x13,
    "PARAM_CHANGED":    0x14,
    "CLOUD_STATUS":     0x50,
}

ENGINE_NAMES = {
    0: "MOOG MODEL D",
    1: "JUNO-106",
    2: "PROPHET-5",
}


# ── CRC-8 poly 0x07 ─────────────────────────────────────────────────────────

def crc8(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    return crc


# ── Frame builder ─────────────────────────────────────────────────────────────

def build_frame(cmd: int, seq: int, payload: bytes = b"") -> bytes:
    assert len(payload) <= 255, "Payload máximo 255 bytes"
    header = bytes([cmd, len(payload), seq])
    body   = header + payload
    return body + bytes([crc8(body)])


# ── Helpers de payload por CMD ───────────────────────────────────────────────

def payload_heartbeat() -> bytes:
    return b""

def payload_engine_changed(engine_id: int, name: str) -> bytes:
    return bytes([engine_id]) + name.encode("ascii") + b"\x00"

def payload_param_changed(param_id: int, value: float) -> bytes:
    return struct.pack("<Hf", param_id, value)

def payload_cloud_status(connected: bool, latency_ms: int) -> bytes:
    return struct.pack("<BH", int(connected), latency_ms)

def payload_get_version() -> bytes:
    return b""

def payload_set_engine(engine_id: int) -> bytes:
    return bytes([engine_id])


# ── Serial port discovery ─────────────────────────────────────────────────────

def find_esp32_port() -> str | None:
    """Busca el puerto USB del ESP32-S3 automáticamente."""
    for port_info in serial.tools.list_ports.comports():
        desc = (port_info.description or "").lower()
        if "cp210" in desc or "ch340" in desc or "silicon" in desc or "esp32" in desc:
            return port_info.device
    # macOS fallback
    candidates = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.SLAB*")
    return candidates[0] if candidates else None


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="GrooveForge Bridge Protocol demo sender",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument("--port",     help="Puerto serie (e.g. /dev/cu.usbmodem14101)")
    parser.add_argument("--baud",     type=int, default=115200,
                        help="Baud rate del monitor serial (default 115200 para USB-CDC)")
    parser.add_argument("--cmd",      choices=list(CMD.keys()), default="HEARTBEAT",
                        help="Comando a enviar")
    parser.add_argument("--count",    type=int, default=1,
                        help="Número de frames a enviar")
    parser.add_argument("--interval", type=float, default=0.0,
                        help="Intervalo entre frames en segundos (0=inmediato)")
    parser.add_argument("--engine",   type=int, default=0,
                        help="Engine ID para ENGINE_CHANGED / SET_ENGINE")
    parser.add_argument("--name",     default=None,
                        help="Nombre del engine para ENGINE_CHANGED")
    parser.add_argument("--param",    type=int, default=0,
                        help="Param ID para PARAM_CHANGED / SET_PARAM")
    parser.add_argument("--value",    type=float, default=0.5,
                        help="Valor float para PARAM_CHANGED / SET_PARAM")
    parser.add_argument("--latency",  type=int, default=0,
                        help="Latency ms para CLOUD_STATUS")
    parser.add_argument("--list-ports", action="store_true",
                        help="Listar puertos disponibles y salir")
    args = parser.parse_args()

    if args.list_ports:
        ports = list(serial.tools.list_ports.comports())
        if not ports:
            print("No se encontraron puertos serie.")
        for p in ports:
            print(f"  {p.device:30s}  {p.description}")
        return

    # Resolver puerto
    port = args.port or find_esp32_port()
    if not port:
        print("ERROR: No se encontró ningún ESP32. Usa --port /dev/cu.usbmodem*")
        sys.exit(1)

    print(f"Conectando a {port} @ {args.baud} baud...")
    try:
        ser = serial.Serial(port, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"ERROR: {e}")
        sys.exit(1)

    time.sleep(0.5)  # dar tiempo al CDC de enumerar

    # Construir payload según CMD
    cmd_id = CMD[args.cmd]
    engine_name = args.name or ENGINE_NAMES.get(args.engine, f"ENGINE_{args.engine:02X}")

    def make_payload() -> bytes:
        if args.cmd == "HEARTBEAT":
            return payload_heartbeat()
        elif args.cmd == "ENGINE_CHANGED":
            return payload_engine_changed(args.engine, engine_name)
        elif args.cmd == "SET_ENGINE":
            return payload_set_engine(args.engine)
        elif args.cmd in ("PARAM_CHANGED", "SET_PARAM"):
            return payload_param_changed(args.param, args.value)
        elif args.cmd == "GET_VERSION":
            return payload_get_version()
        elif args.cmd == "CLOUD_STATUS":
            return payload_cloud_status(True, args.latency)
        else:
            return b""

    seq = 0
    for i in range(args.count):
        payload = make_payload()
        frame   = build_frame(cmd_id, seq & 0xFF, payload)
        ser.write(frame)

        print(f"[{i+1}/{args.count}] CMD={args.cmd}(0x{cmd_id:02X}) "
              f"SEQ={seq} LEN={len(payload)} "
              f"CRC=0x{frame[-1]:02X}  wire={frame.hex()}")

        seq = (seq + 1) & 0xFF

        if args.interval > 0 and i < args.count - 1:
            time.sleep(args.interval)

    # Leer respuesta(s) del ESP32 por 2s
    print("\nEsperando respuesta del ESP32 (2s)...")
    deadline = time.time() + 2.0
    rx_buf   = bytearray()

    while time.time() < deadline:
        n = ser.in_waiting
        if n > 0:
            rx_buf += ser.read(n)
        time.sleep(0.05)

    if rx_buf:
        print(f"Respuesta raw ({len(rx_buf)} bytes): {rx_buf.hex()}")
        # Parsear ACK/NACK básico
        i = 0
        while i + 3 < len(rx_buf):
            r_cmd = rx_buf[i]
            r_len = rx_buf[i + 1]
            r_seq = rx_buf[i + 2]
            if r_cmd == 0x01:
                print(f"  → ACK (SEQ={r_seq}, acked_seq={rx_buf[i+3] if r_len >= 1 else '?'})")
            elif r_cmd == 0x02:
                reason = rx_buf[i + 4] if r_len >= 2 else 0
                print(f"  → NACK (SEQ={r_seq}, reason=0x{reason:02X})")
            elif r_cmd == 0x04:
                if r_len >= 3:
                    print(f"  → VERSION {rx_buf[i+3]}.{rx_buf[i+4]}.{rx_buf[i+5]}")
            i += 3 + r_len + 1  # header + payload + CRC
    else:
        print("(sin respuesta binaria — ver monitor serial del ESP32)")

    ser.close()
    print("Done.")


if __name__ == "__main__":
    main()

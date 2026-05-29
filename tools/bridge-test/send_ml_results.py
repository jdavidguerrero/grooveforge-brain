#!/usr/bin/env python3
"""
send_ml_results.py — GrooveForge Sprint 32 demo sender

Simula los frames de inferencia ML que normalmente enviaría el Teensy,
permitiendo validar view_10 (AI PROC) y view_25 (AI MODELS) en el ESP32
sin necesitar hardware Teensy conectado.

Uso rápido — demo canónico del sprint (C key, Am chord, 120 BPM):
    python3 send_ml_results.py --port /dev/cu.usbmodem*

Loop interactivo (cicla por diferentes tonalidades y acordes):
    python3 send_ml_results.py --port /dev/cu.usbmodem* --loop

Comandos individuales:
    python3 send_ml_results.py --port /dev/cu.usbmodem* --cmd KEY   --key 9 --conf 90
    python3 send_ml_results.py --port /dev/cu.usbmodem* --cmd CHORD --root 0 --qual 0 --conf 80
    python3 send_ml_results.py --port /dev/cu.usbmodem* --cmd BEAT  --bpm 128

Navegar a view_25 (AI MODELS):
    python3 send_ml_results.py --port /dev/cu.usbmodem* --view models
    python3 send_ml_results.py --port /dev/cu.usbmodem* --view aiproc

Toggle bypass desde CLI:
    python3 send_ml_results.py --port /dev/cu.usbmodem* --bypass beat off
    python3 send_ml_results.py --port /dev/cu.usbmodem* --bypass scale on

Requisitos: pip install pyserial
Spec de referencia: apps/docs/02-bridge-protocol.md
Sprint doc:         apps/docs/sprints/32-ml-engine-integration.md
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


# ── Tablas de nombres (mirror de bridge_handlers.cpp) ───────────────────────

NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# chord_type encoding: ChordRecognizer nativo
# 0=maj, 1=min, 2=7, 3=m7, 4=maj7, 5=N(no chord)
QUAL_NAMES = ["", "m", "7", "m7", "M7", "N"]

# Secuencia de demo: tonalidades + acordes + BPM representativos
DEMO_SEQUENCE = [
    # (key_idx, key_conf, root, qual, chord_conf, bpm)
    (0,  85,  9, 1,  72, 120),   # C key → Am chord → 120 BPM
    (7,  90,  4, 0,  88, 96),    # G key → E maj   → 96 BPM
    (2,  78,  2, 1,  65, 140),   # D key → Dm       → 140 BPM
    (5,  83,  0, 4,  79, 110),   # F key → CM7      → 110 BPM
    (9,  91, 11, 2,  70, 75),    # A key → B7        → 75 BPM
]

# CMD IDs (mirror de protocol.h)
CMD_HEARTBEAT      = 0x00
CMD_ACK            = 0x01
CMD_NACK           = 0x02
CMD_PARAM_CHANGED  = 0x14
CMD_KEY_DETECTED   = 0x80
CMD_CHORD_DETECTED = 0x81
CMD_BEAT_DETECTED  = 0x82

# param_id especiales del Sprint 31/32 (bridge_handlers.cpp)
PARAM_AI_SUBVIEW        = 0x00F1  # 0.0 → AI PROC (view_10), 1.0 → AI MODELS (view_25)
PARAM_SCALE_LOCK_BYPASS = 0x00F2  # 1.0 = bypass ON (scale lock OFF)
PARAM_BEAT_BYPASS       = 0x00F3  # 1.0 = bypass ON (beat follower OFF)


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
    """Serializa [CMD LEN SEQ PAYLOAD... CRC8] según wire format del spec."""
    assert len(payload) <= 255, "Payload máximo 255 bytes"
    header = bytes([cmd, len(payload), seq])
    body   = header + payload
    return body + bytes([crc8(body)])


# ── Builders de payload por CMD ──────────────────────────────────────────────

def payload_heartbeat() -> bytes:
    return b""


def payload_param_changed(param_id: int, value: float) -> bytes:
    """PARAM_CHANGED: [param_id: 2B LE][value: 4B LE float]"""
    return struct.pack("<Hf", param_id, value)


def payload_key_detected(key_idx: int, confidence: int) -> bytes:
    """KEY_DETECTED (0x80): [key_idx: 1B][confidence: 1B]
    key_idx: 0=C, 1=C#, 2=D, ..., 11=B
    confidence: 0-100 (porcentaje)
    """
    assert 0 <= key_idx <= 11, "key_idx debe ser 0-11"
    assert 0 <= confidence <= 100, "confidence debe ser 0-100"
    return bytes([key_idx, confidence])


def payload_chord_detected(root: int, qual: int, confidence: int) -> bytes:
    """CHORD_DETECTED (0x81): [root: 1B][chord_type: 1B][confidence: 1B]
    root:       0-11 (nota raíz, mismo encoding que key_idx)
    chord_type: 0=maj, 1=min, 2=7, 3=m7, 4=maj7, 5=N(no chord)
    confidence: 0-100
    """
    assert 0 <= root <= 11, "root debe ser 0-11"
    assert 0 <= qual <= 5,  "qual debe ser 0-5"
    assert 0 <= confidence <= 100
    return bytes([root, qual, confidence])


def payload_beat_detected(bpm: float) -> bytes:
    """BEAT_DETECTED (0x82): [bpm_x10: 2B LE]
    bpm_x10 = round(bpm * 10), e.g. 120.0 BPM → 1200
    Rango: 20-300 BPM
    """
    bpm_x10 = round(bpm * 10)
    assert 200 <= bpm_x10 <= 3000, f"BPM {bpm} fuera de rango (20-300)"
    return struct.pack("<H", bpm_x10)


# ── Helpers de descripción humana ────────────────────────────────────────────

def key_name(key_idx: int) -> str:
    return NOTES[key_idx % 12]


def chord_name(root: int, qual: int) -> str:
    if qual == 5:
        return "---"
    return f"{NOTES[root % 12]}{QUAL_NAMES[qual]}"


# ── Port discovery ───────────────────────────────────────────────────────────

def find_esp32_port() -> str | None:
    for port_info in serial.tools.list_ports.comports():
        desc = (port_info.description or "").lower()
        if any(x in desc for x in ("cp210", "ch340", "silicon", "esp32")):
            return port_info.device
    candidates = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.SLAB*")
    return candidates[0] if candidates else None


# ── Sender wrapper ───────────────────────────────────────────────────────────

class Sender:
    def __init__(self, port: str, baud: int = 115200):
        print(f"Conectando a {port} @ {baud} baud...")
        self.ser  = serial.Serial(port, baud, timeout=1)
        self.seq  = 0
        time.sleep(0.5)  # dar tiempo al CDC de enumerar

    def send(self, cmd: int, payload: bytes, label: str = "") -> bytes:
        frame = build_frame(cmd, self.seq & 0xFF, payload)
        self.ser.write(frame)
        tag = f"CMD=0x{cmd:02X}"
        if label:
            tag += f" ({label})"
        print(f"  → SEQ={self.seq:3d} {tag:30s} LEN={len(payload):3d}  "
              f"CRC=0x{frame[-1]:02X}  {frame.hex()}")
        self.seq = (self.seq + 1) & 0xFF
        return frame

    def heartbeat(self):
        self.send(CMD_HEARTBEAT, payload_heartbeat(), "HEARTBEAT")

    def navigate(self, view: str):
        """Navegar a view_10 (aiproc) o view_25 (models) vía param_id 0x00F1."""
        value = 0.0 if view == "aiproc" else 1.0
        name  = "AI PROC (view_10)" if view == "aiproc" else "AI MODELS (view_25)"
        self.send(CMD_PARAM_CHANGED,
                  payload_param_changed(PARAM_AI_SUBVIEW, value),
                  f"PARAM AI_SUBVIEW→{name}")

    def send_key(self, key_idx: int, confidence: int = 80):
        label = f"KEY_DETECTED {key_name(key_idx)} conf={confidence}%"
        self.send(CMD_KEY_DETECTED,
                  payload_key_detected(key_idx, confidence),
                  label)

    def send_chord(self, root: int, qual: int, confidence: int = 80):
        label = f"CHORD_DETECTED {chord_name(root, qual)} conf={confidence}%"
        self.send(CMD_CHORD_DETECTED,
                  payload_chord_detected(root, qual, confidence),
                  label)

    def send_beat(self, bpm: float):
        label = f"BEAT_DETECTED {bpm:.1f} BPM"
        self.send(CMD_BEAT_DETECTED, payload_beat_detected(bpm), label)

    def set_bypass(self, model: str, bypass: bool):
        if model == "beat":
            param_id = PARAM_BEAT_BYPASS
            name     = "BEAT_FOLLOWER bypass"
        else:
            param_id = PARAM_SCALE_LOCK_BYPASS
            name     = "SCALE_LOCK bypass"
        state = "ON (OFF)" if bypass else "OFF (ON)"
        self.send(CMD_PARAM_CHANGED,
                  payload_param_changed(param_id, 1.0 if bypass else 0.0),
                  f"PARAM {name}={state}")

    def read_response(self, timeout_s: float = 1.5):
        """Lee y parsea respuesta del ESP32 durante timeout_s segundos."""
        deadline = time.time() + timeout_s
        rx_buf   = bytearray()
        while time.time() < deadline:
            n = self.ser.in_waiting
            if n > 0:
                rx_buf += self.ser.read(n)
            time.sleep(0.05)
        if not rx_buf:
            return
        print(f"\n  ← Respuesta raw ({len(rx_buf)} bytes): {rx_buf.hex()}")
        i = 0
        while i + 3 <= len(rx_buf):
            r_cmd = rx_buf[i]
            r_len = rx_buf[i + 1]
            r_seq = rx_buf[i + 2]
            if r_cmd == CMD_ACK:
                acked = rx_buf[i + 3] if r_len >= 1 else "?"
                print(f"  ← ACK  seq={r_seq} acked_seq={acked}")
            elif r_cmd == CMD_NACK:
                reason = rx_buf[i + 4] if r_len >= 2 else 0
                print(f"  ← NACK seq={r_seq} reason=0x{reason:02X}")
            i += 3 + r_len + 1

    def close(self):
        self.ser.close()


# ── Modos de operación ───────────────────────────────────────────────────────

def run_demo(s: Sender):
    """Demo canónico del Sprint 32: C key + Am chord + 120 BPM en view_10."""
    print("\n══ Demo Sprint 32 — view_10 AI PROC ══")
    print("Esperado en display: key='C'  resultado='Am · 120'\n")

    s.heartbeat()
    time.sleep(0.05)

    s.navigate("aiproc")
    time.sleep(0.1)

    s.send_key(key_idx=0, confidence=85)    # C
    time.sleep(0.05)

    s.send_chord(root=9, qual=1, confidence=72)  # Am
    time.sleep(0.05)

    s.send_beat(bpm=120.0)

    s.read_response(timeout_s=2.0)
    print("\n✓ Demo completo — verificar view_10 en display GC9A01")


def run_loop(s: Sender):
    """Cicla por la secuencia de demo cada 3s, bucle indefinido (Ctrl+C para salir)."""
    print("\n══ Loop demo — Ctrl+C para salir ══\n")
    s.heartbeat()
    time.sleep(0.05)
    s.navigate("aiproc")
    time.sleep(0.1)

    idx = 0
    try:
        while True:
            key_i, k_conf, root, qual, c_conf, bpm = DEMO_SEQUENCE[idx]
            print(f"\n[{idx+1}/{len(DEMO_SEQUENCE)}] "
                  f"{key_name(key_i)} key | {chord_name(root, qual)} | {bpm} BPM")
            s.send_key(key_i, k_conf)
            time.sleep(0.05)
            s.send_chord(root, qual, c_conf)
            time.sleep(0.05)
            s.send_beat(bpm)
            s.read_response(timeout_s=0.3)
            idx = (idx + 1) % len(DEMO_SEQUENCE)
            time.sleep(3.0)
    except KeyboardInterrupt:
        print("\n\nLoop detenido.")


# ── CLI ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="GrooveForge Sprint 32 — demo sender ML results",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--port",  help="Puerto serie (auto-detecta si se omite)")
    parser.add_argument("--baud",  type=int, default=115200,
                        help="Baud rate USB-CDC monitor (default 115200)")
    parser.add_argument("--loop",  action="store_true",
                        help="Modo loop: cicla por secuencia demo indefinidamente")
    parser.add_argument("--view",  choices=["aiproc", "models"],
                        help="Navegar a view_10 (aiproc) o view_25 (models)")
    parser.add_argument("--bypass", nargs=2, metavar=("MODEL", "STATE"),
                        help="Toggle bypass: --bypass beat off | --bypass scale on")
    parser.add_argument("--cmd",   choices=["KEY", "CHORD", "BEAT"],
                        help="Enviar un comando ML individual")
    parser.add_argument("--key",   type=int, default=0,
                        help="key_idx 0-11 (C=0, C#=1, ... B=11) para --cmd KEY")
    parser.add_argument("--root",  type=int, default=9,
                        help="root 0-11 para --cmd CHORD")
    parser.add_argument("--qual",  type=int, default=1,
                        help="chord_type: 0=maj 1=min 2=7 3=m7 4=maj7 5=N")
    parser.add_argument("--conf",  type=int, default=80,
                        help="Confidence 0-100 para KEY/CHORD")
    parser.add_argument("--bpm",   type=float, default=120.0,
                        help="BPM (20-300) para --cmd BEAT")
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

    port = args.port or find_esp32_port()
    if not port:
        print("ERROR: No se encontró ningún ESP32. Usa --port /dev/cu.usbmodem*")
        sys.exit(1)

    try:
        s = Sender(port, args.baud)
    except serial.SerialException as e:
        print(f"ERROR: {e}")
        sys.exit(1)

    try:
        # ── Navegación explícita ─────────────────────────────────────────────
        if args.view:
            print(f"\n══ Navegando a {args.view} ══")
            s.heartbeat()
            time.sleep(0.05)
            s.navigate(args.view)
            s.read_response()

        # ── Toggle bypass ────────────────────────────────────────────────────
        elif args.bypass:
            model, state = args.bypass
            if model not in ("beat", "scale"):
                print("ERROR: MODEL debe ser 'beat' o 'scale'")
                sys.exit(1)
            if state not in ("on", "off"):
                print("ERROR: STATE debe ser 'on' o 'off'")
                sys.exit(1)
            bypass = (state == "on")
            print(f"\n══ Bypass {model} = {state} ══")
            s.heartbeat()
            time.sleep(0.05)
            s.navigate("models")
            time.sleep(0.1)
            s.set_bypass(model, bypass)
            s.read_response()

        # ── Comando ML individual ────────────────────────────────────────────
        elif args.cmd:
            print(f"\n══ Comando ML individual: {args.cmd} ══")
            s.heartbeat()
            time.sleep(0.05)
            if args.cmd == "KEY":
                s.send_key(args.key, args.conf)
            elif args.cmd == "CHORD":
                s.send_chord(args.root, args.qual, args.conf)
            elif args.cmd == "BEAT":
                s.send_beat(args.bpm)
            s.read_response()

        # ── Modo loop ────────────────────────────────────────────────────────
        elif args.loop:
            run_loop(s)

        # ── Demo canónico (default) ──────────────────────────────────────────
        else:
            run_demo(s)

    finally:
        s.close()
        print("Conexión cerrada.")


if __name__ == "__main__":
    main()

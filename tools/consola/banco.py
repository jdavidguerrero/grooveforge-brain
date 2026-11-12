#!/usr/bin/env python3
"""Banco CDC por placas; contrato y límites en apps/docs/consola/29-banco-pruebas.md."""
from __future__ import annotations

import argparse
from collections import deque
import csv
from dataclasses import dataclass
from datetime import datetime
import math
from pathlib import Path
import re
import time

ROOT = Path(__file__).resolve().parents[2]
BOARDS = ("pcb-m", "pcb-k-izq", "pcb-k-der", "pcb-a", "pcb-f", "pcb-p")
EXPECTED = {
    "pcb-m": {0x0A, 0x48, 0x49, 0x60, 0x61}, "pcb-k-izq": {0x22, 0x23},
    "pcb-k-der": {0x21}, "pcb-a": {0x20}, "pcb-f": {0x48}, "pcb-p": {0x49},
}


@dataclass(frozen=True)
class Control:
    name: str
    kind: str
    index: int


@dataclass(frozen=True)
class Result:
    board: str
    name: str
    passed: bool
    detail: str


def note_map(root=ROOT):
    text = (root / "apps/consola/surface/include/consola_s/ConsolaMap.h").read_text()
    body = re.search(r"notes\[4\]\[14\]\s*=\s*\{(.*?)\};", text, re.S)
    if not body:
        raise ValueError("ConsolaMap.h: no se encontró notes[4][14]")
    rows = re.findall(r"\{([^{}]+)\}", body[1])
    mapping = [[int(n) for n in row.split(",")] for row in rows]
    if len(mapping) != 4 or any(len(row) != 14 for row in mapping):
        raise ValueError("ConsolaMap.h: dimensiones inesperadas")
    return mapping


def checklist(root=ROOT):
    """Names from panel CSV/doc28; numeric MCP contract from production map."""
    with (root / "apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv").open(newline="") as f:
        panel = {r["ref"]: r for r in csv.DictReader(f)}
    mapping = note_map(root)
    result = {board: [] for board in BOARDS}
    doc = (root / "apps/docs/consola/28-pcb-k.md").read_text().split("## 4.")[1].split("## 5.")[0]
    for line in doc.splitlines():
        match = re.match(r"\| 0x(2[123]) \| GP([AB])([0-6]) \| ([^|]+) \|[^|]*\| (\d+) \|", line)
        if not match:
            continue
        addr, port, pin, name, note = match.groups()
        chip = int(addr, 16) - 0x20
        index = int(pin) + (7 if port == "B" else 0)
        if mapping[chip][index] != int(note):
            raise ValueError(f"Doc28/ConsolaMap discrepan: {name}")
        if int(note) in (88, 122):
            continue  # Routed through K-left, operated on M.
        name = name.strip().replace("−", "-")
        if name not in panel:
            raise ValueError(f"Tecla ausente del CSV: {name}")
        result["pcb-k-der" if chip == 1 else "pcb-k-izq"].append(Control(name, "KEY", int(note)))
    if len(result["pcb-k-izq"]) != 17 or len(result["pcb-k-der"]) != 9:
        raise ValueError("Doc28: se esperaban 17+9 teclas")
    for i in range(8):
        name = f"E{i+1}"
        if panel[name]["kind"] != "encoder":
            raise ValueError(f"CSV: {name} no es encoder")
        result["pcb-a"] += [Control(name, "ENC", i), Control(f"{name}_CLICK", "KEY", 80+i)]
        if 80+i not in mapping[0]:
            raise ValueError("ConsolaMap: falta clic de encoder")
    result["pcb-f"] = [Control(f"F{i+1}", "ADS", i) for i in range(4)]
    result["pcb-p"] = [Control(f"FSR_V{i+1}", "KEY", 36+i) for i in range(4)]
    result["pcb-p"] += [Control(f"FSR_X{i+1}", "ADS", 4+i) for i in range(2)]
    result["pcb-m"] = [Control("JOG", "ENC", 8), Control("JOG_CLICK", "KEY", 88), Control("PEDAL_BTN", "KEY", 122)]
    result["pcb-m"] += [Control(f"POT_{name}", "POT", i) for i, name in enumerate(("MASTER", "CUE", "IN"))]
    result["pcb-m"] += [Control("SEL_PAD", "SEL", 0), Control("SHARP_L", "ADS", 6), Control("SHARP_R", "ADS", 7)]
    for controls in result.values():
        for c in controls:
            if c.name.endswith("_CLICK") or c.name == "PEDAL_BTN":
                continue
            if c.name not in panel:
                raise ValueError(f"Control ausente del CSV: {c.name}")
    return result


def discover_port(ports=None):
    if ports is None:
        from serial.tools import list_ports
        ports = list_ports.comports()
    matches = [p.device for p in ports if "gf consola" in
               " ".join(str(getattr(p, key, "") or "") for key in ("product", "description")).lower()]
    if len(matches) != 1:
        raise RuntimeError(f"Se esperaba un puerto GF Consola; encontrados {matches}. Usa --puerto.")
    return matches[0]


def parse_inputs(line):
    if not line.startswith("@GF OK INPUTS "):
        raise ValueError("Respuesta INPUTS inválida")
    fields = dict(item.split("=", 1) for item in line.split()[3:])
    required = {"MCP", "ENC", "ADS", "FSR", "POT", "MCP_VALID", "ENC_MASK", "ADS_VALID", "FSR_VALID", "POT_VALID", "SEL", "SEL_VALID", "M4", "M4_BITS", "DROPS"}
    if not required.issubset(fields):
        raise ValueError("INPUTS: faltan campos obligatorios")
    result = {}
    for key in ("MCP", "ENC", "ADS", "FSR", "POT"):
        result[key] = [int(n, 16 if key == "MCP" else 10) for n in fields[key].split(",")]
    for key, size in (("MCP", 4), ("ENC", 9), ("ADS", 8), ("FSR", 4), ("POT", 3)):
        if len(result[key]) != size:
            raise ValueError(f"INPUTS: tamaño {key} inválido")
    for key in ("MCP_VALID", "ENC_MASK", "ADS_VALID", "FSR_VALID", "POT_VALID", "SEL", "SEL_VALID", "M4", "DROPS"):
        result[key] = int(fields[key])
    result["M4_BITS"] = int(fields["M4_BITS"], 16)
    return result


class Client:
    def __init__(self, port, clock=time.monotonic):
        self.port, self.clock = port, clock

    def line(self):
        raw = self.port.readline()
        if len(raw) > 2048 or (raw and not raw.endswith(b"\n")):
            raise RuntimeError("CDC: línea incompleta o demasiado larga")
        return raw.decode("utf-8", errors="replace").strip()

    def command(self, command, timeout=4.0):
        data = ("@GF " + command + "\n").encode("ascii")
        if self.port.write(data) != len(data):
            raise RuntimeError("CDC: escritura incompleta")
        verb = command.split()[0]
        deadline = self.clock() + timeout
        while self.clock() < deadline:
            line = self.line()
            if line.startswith("@GF ERR "):
                raise RuntimeError(line)
            if line == f"@GF OK {command}" or (verb in ("SCAN", "INPUTS") and line.startswith(f"@GF OK {verb} ")):
                return line
        raise TimeoutError(f"Sin respuesta a {command}")

    def snapshot(self):
        return parse_inputs(self.command("INPUTS"))


def valid(control, s, mapping):
    if control.kind == "KEY":
        if 36 <= control.index <= 39:
            return bool(s["FSR_VALID"] & (1 << (control.index-36)))
        return any(control.index in row and s["MCP_VALID"] & (1 << chip) for chip, row in enumerate(mapping))
    mask = {"ENC": "ENC_MASK", "ADS": "ADS_VALID", "POT": "POT_VALID", "SEL": "SEL_VALID"}[control.kind]
    return bool(s[mask] & (1 << control.index))


def initial_value(control, s):
    if control.kind == "KEY":
        return 0
    if control.kind == "SEL":
        return s["SEL"]
    return s[control.kind][control.index]


def exercise(client, control, timeout, mapping, output=print):
    client.command("WATCH 0")
    baseline = client.snapshot()
    client.command("WATCH 1")  # Ordered ACK drains all earlier events before the next prompt.
    action = {"KEY": "pulsa y suelta", "ENC": "gira en ambos sentidos", "SEL": "recorre las tres posiciones"}.get(control.kind, "mueve/presiona al menos un tramo")
    output(f"{control.name}: {action} ({timeout:g} s)")
    if not valid(control, baseline, mapping):
        return False, "Sin muestra válida/habilitada; revisa perfil full, cableado y dependencias"
    start_value = initial_value(control, baseline)
    deadline = client.clock() + timeout
    renew = client.clock() + 90
    pressed = False
    directions = set()
    positions = {0 if start_value < 1365 else 1 if start_value < 2730 else 2}
    previous = start_value
    # A fake port generates input only after the operator prompt, never by guessing from a command.
    if isinstance(client.port, SimulatedPort):
        client.port.exercise(control, start_value)
    while client.clock() < deadline:
        if client.clock() >= renew:
            client.command("WATCH 1")
            renew = client.clock() + 90
        line = client.line()
        match = re.fullmatch(r"@GF OK EVENT (KEY|ENC|ADS|FSR|POT|SEL) (\d+) (-?\d+)", line)
        if not match or (match[1], int(match[2])) != (control.kind, control.index):
            continue
        value = int(match[3])
        if control.kind == "KEY":
            if value == 1:
                pressed = True
            elif value == 0 and pressed:
                return True, "Pulsación y liberación recibidas"
        elif control.kind == "ENC":
            # Firmware counts wrap at int32; retain direction across that wrap.
            delta = (value - previous + 2**31) % 2**32 - 2**31
            if delta:
                directions.add(1 if delta > 0 else -1)
            previous = value
            if len(directions) == 2:
                return True, "Cuenta varió en ambos sentidos"
        elif control.kind == "SEL":
            positions.add(0 if value < 1365 else 1 if value < 2730 else 2)
            if len(positions) == 3:
                return True, "Tres posiciones ADC observadas"
        elif abs(value - start_value) >= (512 if control.kind == "ADS" else 64):
            return True, f"Crudo {start_value} → {value}"
    return False, "Tiempo agotado sin completar el movimiento solicitado"


class SimulatedPort:
    """Serial-compatible fake; explicit fault injection keeps desktop tests honest."""
    def __init__(self, addresses=None, silent=(), wrong=(), disconnect=None, drops=0):
        self.addresses = set().union(*EXPECTED.values()) if addresses is None else set(addresses)
        self.silent, self.wrong = set(silent), set(wrong)
        self.disconnect, self.drops = disconnect, drops
        self.rx, self.writes = deque(), []
        self.closed = False
        self.watch = False
        self.now = 0.0
        self.snap = dict(MCP=[0xFFFF]*4, MCP_VALID=15, ENC=[0]*9, ENC_MASK=511,
                         ADS=[1000]*8, ADS_VALID=255, FSR=[0]*4, FSR_VALID=15,
                         POT=[1000]*3, POT_VALID=7, SEL=0, SEL_VALID=1, M4=0, M4_BITS=0, DROPS=0)

    def clock(self):
        return self.now

    def enqueue(self, line):
        self.rx.append((line + "\n").encode())

    def write(self, data):
        if self.closed:
            raise OSError("Puerto cerrado")
        command = data.decode().strip()
        self.writes.append(command)
        if command == "@GF SCAN":
            self.enqueue("@GF OK SCAN " + (",".join(f"{a:02X}" for a in sorted(self.addresses)) or "-"))
        elif command == "@GF INPUTS":
            fields = []
            for key, value in self.snap.items():
                if isinstance(value, list):
                    text = ",".join(f"{n:04X}" if key == "MCP" else str(n) for n in value)
                else:
                    text = f"{value:08X}" if key == "M4_BITS" else str(value)
                fields.append(f"{key}={text}")
            self.enqueue("@GF OK INPUTS " + " ".join(fields))
        elif command in ("@GF WATCH 0", "@GF WATCH 1"):
            self.watch = command.endswith("1")
            self.enqueue("@GF OK WATCH " + command[-1])
        elif re.fullmatch(r"@GF KEYLEDS (?:[0-9]|1[0-9]|2[0-6])", command):
            self.enqueue("@GF OK " + command[4:])
        else:
            self.enqueue("@GF ERR COMANDO")
        return len(data)

    def readline(self):
        self.now += 0.01
        return self.rx.popleft() if self.rx else b""

    def exercise(self, control, baseline):
        if control.name == self.disconnect:
            raise OSError("Desconexión simulada")
        if not self.watch or control.name in self.silent:
            return
        index = control.index + (100 if control.name in self.wrong else 0)
        values = [1, 0] if control.kind == "KEY" else [baseline+4, baseline] if control.kind == "ENC" else [2048, 4095, 0] if control.kind == "SEL" else [baseline+1024]
        for value in values:
            self.enqueue(f"@GF OK EVENT {control.kind} {index} {value}")
        self.snap["DROPS"] += self.drops

    def close(self):
        self.closed = True


def run(client, board, timeout=15, confirm=input, output=print, results=None):
    results = [] if results is None else results
    plan, mapping = checklist(), note_map()
    for selected in BOARDS if board == "todo" else (board,):
        scan = client.command("SCAN").split(maxsplit=3)[3]
        found = set() if scan == "-" else {int(a, 16) for a in scan.split(",")}
        missing = EXPECTED[selected] - found
        results.append(Result(selected, "SCAN", not missing,
                              f"Responden: {scan}; faltan: {','.join(f'{a:02X}' for a in sorted(missing)) or '-'}"))
        before = client.snapshot()
        results.append(Result(selected, "INPUTS", True, str(before)))
        for control in plan[selected]:
            try:
                passed, detail = exercise(client, control, timeout, mapping, output)
                client.command("WATCH 0")
                current = client.snapshot()
            except (Exception, KeyboardInterrupt) as exc:
                results.append(Result(selected, control.name, False, str(exc) or type(exc).__name__))
                raise
            if passed and not valid(control, current, mapping):
                passed, detail = False, "Entrada desconectada al terminar el movimiento"
            results.append(Result(selected, control.name, passed, detail))
            output(f"  {'PASA' if passed else 'FALLA'}: {detail}")
        client.command("WATCH 0")
        after = client.snapshot()
        lost = after["DROPS"] - before["DROPS"]
        results.append(Result(selected, "Integridad WATCH", lost == 0, f"DROPS {before['DROPS']} → {after['DROPS']}"))
        if selected.startswith("pcb-k-"):
            try:
                client.command("KEYLEDS 26")
                yes = confirm(f"{selected}: ¿se iluminaron todas las teclas del tramo y cambiaron de color? [s/n] ").strip().lower() == "s"
                detail = "Confirmación simulada" if isinstance(client.port, SimulatedPort) else "Confirmación visual del operador"
                results.append(Result(selected, "LEDs", yes, detail))
            finally:
                client.command("KEYLEDS 0")
    return results


def report(path, board, port, simulated, results, error=None):
    def safe(text):
        return str(text).replace("|", "\\|").replace("\n", " ")
    lines = [f"# Banco {board}", "", f"Fecha: {datetime.now().astimezone().isoformat()}",
             f"Modo: {'SIMULADO — no valida hardware' if simulated else 'HARDWARE'}", f"Puerto: {safe(port)}", "",
             f"Resultado: {'FALLA / INCOMPLETO' if error or not results or any(not r.passed for r in results) else 'PASA'}", "",
             "| Placa | Prueba | Resultado | Evidencia |", "|---|---|---|---|"]
    lines += [f"| {r.board} | {safe(r.name)} | {'PASA' if r.passed else 'FALLA'} | {safe(r.detail)} |" for r in results]
    if error:
        lines += ["", f"Ejecución interrumpida: {safe(error)}"]
    lines += ["", "Límites: puesta en marcha funcional; no certifica alimentación, consumo, calibración, CV, audio,",
              "latencia, NeoTrellis, ERC/DRC ni 60 min dúplex. Ver [spec](../29-banco-pruebas.md).", ""]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--placa", choices=(*BOARDS, "todo"), required=True)
    parser.add_argument("--puerto", help="Puerto CDC explícito, por ejemplo /dev/cu.usbmodem...")
    parser.add_argument("--timeout", type=float, default=15, help="Segundos por control (default: 15)")
    parser.add_argument("--simular", action="store_true", help="Sin hardware; informe marcado SIMULADO")
    args = parser.parse_args(argv)
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout debe ser finito y mayor que cero")
    results, error, port = [], None, None
    name = "SIMULADO" if args.simular else args.puerto or "GF Consola (descubrimiento)"
    stamp = datetime.now().astimezone().strftime("%Y%m%d-%H%M%S-%f")
    path = ROOT / f"apps/docs/consola/mediciones/banco-{args.placa}-{stamp}.md"
    client = None
    try:
        if args.simular:
            port = SimulatedPort()
            client = Client(port, port.clock)
        else:
            import serial
            name = args.puerto or discover_port()
            port = serial.Serial(name, 115200, timeout=0.1, write_timeout=1)
            client = Client(port)
        run(client, args.placa, args.timeout,
            confirm=(lambda _: "s") if args.simular else input, results=results)
    except (Exception, KeyboardInterrupt) as exc:
        error = str(exc) or type(exc).__name__
    finally:
        if client:
            try:
                client.command("WATCH 0")
            except Exception as exc:
                error = error or f"No se pudo apagar WATCH: {exc}"
        if port:
            try:
                port.close()
            except Exception as exc:
                error = error or f"Cierre del puerto: {exc}"
        report(path, args.placa, name, args.simular, results, error)
    print(f"Informe: {path}")
    if error:
        print(f"Error: {error}")
    return int(bool(error or not results or any(not r.passed for r in results)))


if __name__ == "__main__":
    raise SystemExit(main())

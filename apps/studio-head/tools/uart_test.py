#!/usr/bin/env python3
"""Prueba de la UART Pi 3B ↔ Teensy (sketch47) sin dependencias: termios en modo raw a 921600.

  python3 uart_test.py [/dev/ttyAMA0] [segundos]

Manda "ping <n> <t_ns>" cada 200 ms; la Teensy contesta "eco ping ..." y se mide el ida y vuelta.
También muestra las líneas "teensy <n>" que la Teensy manda cada segundo.
"""
import os, sys, termios, time, select

dev = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyAMA0"
dur = float(sys.argv[2]) if len(sys.argv) > 2 else 15.0
fd = os.open(dev, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attr = termios.tcgetattr(fd)
attr[0] = attr[1] = attr[3] = 0                                   # iflag, oflag, lflag: raw
attr[2] = termios.CS8 | termios.CREAD | termios.CLOCAL           # 8N1, sin control de flujo
attr[4] = attr[5] = termios.B921600
termios.tcsetattr(fd, termios.TCSANOW, attr)
termios.tcflush(fd, termios.TCIOFLUSH)

buf, n, rtts, teensy_lines, end, next_ping = b"", 0, [], 0, time.monotonic() + dur, 0.0
while time.monotonic() < end:
    if time.monotonic() >= next_ping:
        os.write(fd, b"ping %d %d\n" % (n, time.monotonic_ns())); n += 1; next_ping = time.monotonic() + 0.2
    r, _, _ = select.select([fd], [], [], 0.05)
    if r:
        buf += os.read(fd, 4096)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode(errors="replace").strip()
            if text.startswith("eco ping"):
                rtts.append((time.monotonic_ns() - int(text.split()[3])) / 1e6)
            elif text.startswith("teensy"):
                teensy_lines += 1; print("[teensy]", text)
            elif text:
                print("[?]", text)
os.close(fd)
print("\npings enviados %d · ecos %d · líneas de la Teensy %d" % (n, len(rtts), teensy_lines))
if rtts:
    rtts.sort()
    print("ida y vuelta: mín %.2f ms · mediana %.2f ms · p95 %.2f ms" % (rtts[0], rtts[len(rtts) // 2], rtts[int(len(rtts) * 0.95) - 1]))
print("OK" if rtts and teensy_lines else "SIN COMUNICACIÓN: revisar cableado (TX↔RX cruzados, GND común) y config de la UART")

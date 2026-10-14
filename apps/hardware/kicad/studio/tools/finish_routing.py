"""Cierra las conexiones que quedaron abiertas después de Freerouting.

  $KPY apps/hardware/kicad/studio/tools/finish_routing.py portadora

Corre el DRC, toma cada par de pads sin conectar (salvo GND, que lo resuelven los planos), borra las
pistas y vías NO bloqueadas de esa red y la rutea de punta a punta con grid_route.py. Repite hasta que
no queden pares o hasta que una red no tenga camino.
"""
import os
import re
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
KPY = sys.executable


def unconnected_pairs(path):
    rpt = path + ".drc.rpt"
    subprocess.run([KICAD_CLI, "pcb", "drc", "-o", rpt, path], capture_output=True)
    text = open(rpt).read()
    os.remove(rpt)
    pairs, partial = [], set()
    for block in text.split("[unconnected_items]")[1:]:
        block = block.split("\n[")[0]
        pads = re.findall(r"pad (\S+) \[(/[^\]]+)\] of (\S+)", block)
        nets = set(re.findall(r"\[(/[^\]]+)\]", block)) - {"/GND"}
        if len(pads) >= 2 and pads[0][1] == pads[1][1] and pads[0][1] != "/GND":
            pairs.append((pads[0][1], f"{pads[0][2]}.{pads[0][0]}", f"{pads[1][2]}.{pads[1][0]}"))
        else:
            partial |= nets            # pista suelta ↔ pad: se levanta y en la vuelta siguiente es pad ↔ pad
    return pairs, partial


def rip_up(path, net):
    board = pcbnew.LoadBoard(path)
    gone = 0
    for t in list(board.GetTracks()):
        if t.GetNetname() == net and not t.IsLocked():
            board.Delete(t)
            gone += 1
    pcbnew.SaveBoard(path, board)
    return gone


def route(name, a, b, width):
    out = subprocess.run([KPY, os.path.join(HERE, "grid_route.py"), name, a, b, width],
                         capture_output=True, text=True)
    lines = [l for l in (out.stdout + out.stderr).splitlines() if "ruteado" in l or "camino" in l or "ya están" in l]
    return (lines[-1] if lines else out.stderr.strip()[-200:]), bool(lines and ("ruteado" in lines[-1] or "ya están" in lines[-1]))


def net_pads(path, net):
    board = pcbnew.LoadBoard(path)
    return [f"{fp.GetReference()}.{p.GetNumber()}" for fp in board.GetFootprints() for p in fp.Pads()
            if p.GetNetname() == net]


def blockers(path, a, b, net, margin=4.0):
    """Redes (no bloqueadas, de señal) con pistas dentro de la caja entre los dos pads, más largas primero."""
    board = pcbnew.LoadBoard(path)
    def pos(spec):
        ref, num = spec.split(".")
        p = next(p for p in board.FindFootprintByReference(ref).Pads() if p.GetNumber() == num)
        return pcbnew.ToMM(p.GetPosition())
    (ax, ay), (bx, by) = pos(a), pos(b)
    x0, x1 = min(ax, bx) - margin, max(ax, bx) + margin
    y0, y1 = min(ay, by) - margin, max(ay, by) + margin
    length = {}
    for t in board.GetTracks():
        n = t.GetNetname()
        if t.IsLocked() or n in (net, "/GND", "/+5V", "/VBUS_IN") or t.GetClass() != "PCB_TRACK":
            continue
        (sx, sy), (ex, ey) = pcbnew.ToMM(t.GetStart()), pcbnew.ToMM(t.GetEnd())
        if x0 <= (sx + ex) / 2 <= x1 and y0 <= (sy + ey) / 2 <= y1:
            length[n] = length.get(n, 0) + pcbnew.ToMM(t.GetLength())
    return sorted(length, key=lambda n: -length[n])


def reroute_net(path, name, net, width):
    """Rutea una red de dos o más pads como cadena por cercanía (pad i → pad i+1)."""
    pads = net_pads(path, net)
    ok = True
    for p, q in zip(pads, pads[1:]):
        msg, good = route(name, p, q, width)
        ok &= good
    return ok


def main():
    name = sys.argv[1]
    path = os.path.abspath(os.path.join(HERE, "..", name, f"{name}.kicad_pcb"))
    done = set()
    for _ in range(40):
        pairs, partial = unconnected_pairs(path)
        for net in partial - done:
            print(f"{net}: {rip_up(path, net)} tramos sueltos borrados")
            done.add(net)
        if partial - {p[0] for p in pairs}:
            pairs, _ = unconnected_pairs(path)
        pairs = [p for p in pairs if (p[0], p[1], p[2]) not in done]
        if not pairs:
            left = len(unconnected_pairs(path)[0])
            print(f"{name}: " + ("sin conexiones abiertas de señal" if not left else
                                  f"quedan {left} conexiones de señal sin camino"))
            return
        net, a, b = pairs[0]
        done.add((net, a, b))
        width = "0.8" if net in ("/+5V", "/VBUS_IN") else "0.25" if net.startswith(("/PAD", "/BUF", "/PROT", "/LINE", "/HP", "/OUT", "/IN_", "/VREF", "/CV")) else "0.2"
        gone = rip_up(path, net)
        msg, ok = route(name, a, b, width)
        print(f"{net}: {gone} tramos borrados; {msg}")
        if ok:
            continue
        # Rip-up and reroute: levantar de a una las redes que estorban, rutear esta y volver a rutear la otra.
        backup = open(path).read()
        for other in blockers(path, a, b, net)[:8]:
            rip_up(path, other)
            msg, ok = route(name, a, b, width)
            if ok and reroute_net(path, name, other, "0.2"):
                print(f"  {net}: cerrada levantando y re-ruteando {other}")
                break
            open(path, "w").write(backup)
        else:
            print(f"  {net}: sin solución levantando redes vecinas")
    print(f"{name}: se cortó tras 40 vueltas")


main()

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
        pads = re.findall(r"@\((\S+) mm, (\S+) mm\): (?:PTH |SMD )?pad (\S+) \[(/[^\]]+)\] of (\S+)", block, re.I)
        nets = set(re.findall(r"\[(/[^\]]+)\]", block)) - {"/GND"}
        if len(pads) >= 2 and pads[0][3] == pads[1][3] and pads[0][3] != "/GND":
            (ax, ay, an, net, ar), (bx, by, bn, _, br) = pads[0], pads[1]
            dist = abs(float(ax) - float(bx)) + abs(float(ay) - float(by))
            pairs.append((net, f"{ar}.{an}", f"{br}.{bn}", dist))
        else:
            partial |= nets            # pista suelta ↔ pad: se levanta y en la vuelta siguiente es pad ↔ pad
    # Las cortas primero: no dejar que una larga les tape el paso. ROUTE_FIRST (redes separadas por coma)
    # adelanta las que necesitan carril propio de punta a punta, como el +5V de 0.5 mm de PCB-A.
    first = [n for n in os.environ.get("ROUTE_FIRST", "").split(",") if n]
    # ROUTE_ORDER=longest: en una placa que es un bus (PCB-A: todo sale del IDC de la pestaña hacia la
    # tira), las largas necesitan los carriles de punta a punta y las cortas se acomodan cerca de su pad.
    sign = -1 if os.environ.get("ROUTE_ORDER") == "longest" else 1
    pairs.sort(key=lambda p: (p[0] not in first, sign * p[3]))
    return [p[:3] for p in pairs], partial


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
    length, near = {}, {}
    for t in board.GetTracks():
        n = t.GetNetname()
        if t.IsLocked() or n in (net, "/GND", "/VBUS_IN") or t.GetClass() != "PCB_TRACK":
            continue
        (sx, sy), (ex, ey) = pcbnew.ToMM(t.GetStart()), pcbnew.ToMM(t.GetEnd())
        # Lo que encierra un extremo (abanico de un conector o de un SSOP) va primero.
        for px, py in ((ax, ay), (bx, by)):
            d = min(_seg_dist(px, py, sx, sy, ex, ey), 99)
            if d < 2.5:
                near[n] = min(near.get(n, 99), d)
        if n != "/+5V" and x0 <= (sx + ex) / 2 <= x1 and y0 <= (sy + ey) / 2 <= y1:
            length[n] = length.get(n, 0) + pcbnew.ToMM(t.GetLength())
    first = sorted(near, key=lambda n: near[n])
    return first + [n for n in sorted(length, key=lambda n: -length[n]) if n not in near]


def _seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy or 1e-12
    t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / L2))
    return ((px - ax - t * dx) ** 2 + (py - ay - t * dy) ** 2) ** 0.5


def reroute_net(path, name, net, width):
    """Rutea una red de dos o más pads como cadena por cercanía (pad i → pad i+1)."""
    board = pcbnew.LoadBoard(path)
    pos = {f"{fp.GetReference()}.{p.GetNumber()}": pcbnew.ToMM(p.GetPosition())
           for fp in board.GetFootprints() for p in fp.Pads() if p.GetNetname() == net}
    left = list(pos)
    chain = [left.pop(0)]
    while left:                       # cadena por vecino más cercano
        lx, ly = pos[chain[-1]]
        nxt = min(left, key=lambda k: abs(pos[k][0] - lx) + abs(pos[k][1] - ly))
        left.remove(nxt)
        chain.append(nxt)
    if net in ("/+5V", "/VBUS_IN"):
        width = "0.5"
    ok = True
    for p, q in zip(chain, chain[1:]):
        msg, good = route(name, p, q, width)
        ok &= good
    return ok


def clusters(path, net):
    """Pads de la red agrupados por cobre (pistas y vías; sin planos)."""
    board = pcbnew.LoadBoard(path)
    board.BuildConnectivity()
    conn = board.GetConnectivity()
    pads = [p for fp in board.GetFootprints() for p in fp.Pads() if p.GetNetname() == net]
    spec = {p.m_Uuid.AsString(): (f"{p.GetParentFootprint().GetReference()}.{p.GetNumber()}",
                                   pcbnew.ToMM(p.GetPosition())) for p in pads}
    groups, assigned = [], set()
    for p in pads:
        k = p.m_Uuid.AsString()
        if k in assigned:
            continue
        seen, todo, g = set(), [p], []
        while todo:
            item = todo.pop()
            u = item.m_Uuid.AsString()
            if u in seen:
                continue
            seen.add(u)
            if u in spec:
                g.append(spec[u])
                assigned.add(u)
            todo.extend(i for i in conn.GetConnectedItems(item) if i.GetClass() != "ZONE")
        groups.append(g)
    return groups


def join_clusters(path, name, net, width):
    """Une los grupos sueltos de una red de varios pads por el par de pads más cercano."""
    tried = set()
    while True:
        groups = clusters(path, net)
        if len(groups) <= 1:
            return True
        best = None
        for i, g in enumerate(groups):
            for h in groups[i + 1:]:
                for a, (ax, ay) in g:
                    for b, (bx, by) in h:
                        d = abs(ax - bx) + abs(ay - by)
                        if (a, b) not in tried and (best is None or d < best[0]):
                            best = (d, a, b)
        if best is None:
            return False
        _, a, b = best
        tried.add((a, b))
        msg, ok = route(name, a, b, width)
        print(f"  {net}: unir grupos {a} → {b}: {msg}")
        if ok or len(tried) < 3:
            continue
        # Tres intentos sin camino: levantar de a una las redes que estorban (como en main()).
        backup = open(path).read()
        for other in blockers(path, a, b, net)[:8]:
            rip_up(path, other)
            msg, ok = route(name, a, b, width)
            if ok and reroute_net(path, name, other, "0.2"):
                print(f"  {net}: grupos unidos levantando y re-ruteando {other}")
                break
            open(path, "w").write(backup)
        else:
            return False


def main():
    name = sys.argv[1]
    path = os.path.abspath(os.path.join(HERE, "..", name, f"{name}.kicad_pcb"))
    done = set()
    for _ in range(400):
        pairs, partial = unconnected_pairs(path)
        for net in partial - done:
            print(f"{net}: {rip_up(path, net)} tramos sueltos borrados")
            done.add(net)
        if partial - {p[0] for p in pairs}:
            pairs, _ = unconnected_pairs(path)
        pairs = [p for p in pairs if (p[0], p[1], p[2]) not in done]
        if not pairs:
            # Redes de varios pads que quedaron en pedazos: unir grupo con grupo.
            for n in sorted(partial | {p[0] for p in unconnected_pairs(path)[0]}):
                if len(net_pads(path, n)) > 2:
                    join_clusters(path, name, n, "0.5" if n in ("/+5V", "/VBUS_IN") else "0.2")
            left = len(unconnected_pairs(path)[0])
            print(f"{name}: " + ("sin conexiones abiertas de señal" if not left else
                                  f"quedan {left} conexiones de señal sin camino"))
            return
        net, a, b = pairs[0]
        done.add((net, a, b))
        width = "0.5" if net in ("/+5V", "/VBUS_IN") else "0.25" if net.startswith(("/PAD", "/BUF", "/PROT", "/LINE", "/HP", "/OUT", "/IN_", "/VREF", "/CV")) else "0.2"
        # Multi-pad nets (power, I2C) are routed pair by pair: keep what's already there.
        gone = rip_up(path, net) if len(net_pads(path, net)) <= 2 else 0
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
    print(f"{name}: se cortó tras 400 vueltas")


if __name__ == "__main__":
    main()

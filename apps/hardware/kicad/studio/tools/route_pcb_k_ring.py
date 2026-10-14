"""Rutea el anillo de NAV de PCB-K (D1–D12 + C1–C12) con un patrón regular, y deja las pistas bloqueadas.

Correr después de place_pcb_k.py y antes de route_board.py (Freerouting respeta lo bloqueado).

Con los LEDs a 270 − i·30° (place_pcb_k.py), en cada hueco k entre D(k) y D(k+1) quedan:
  adentro (r ≈ 10.3): DOUT de D(k) y VDD de D(k+1)      afuera (r ≈ 15.7): GND de D(k) y DIN de D(k+1)
Por hueco:
  - datos: DOUT(k) → vía (r 8.6) → F.Cu → vía (r 17.0) → DIN(k+1). Va por F.Cu porque en B.Cu cruzaría
    al capacitor, que une VDD (adentro) con GND (afuera) en la diagonal opuesta.
  - +5V: anillo en B.Cu (r 7.2) con radios al VDD de cada LED y al pad 1 de cada capacitor.
  - GND: vía al plano de F.Cu (r 16.9, centro del hueco), unida al GND de D(k) y al pad 2 del capacitor.
El hueco D12→D1 no lleva datos: LED_12 sale por F.Cu hacia las teclas y LED_DIN llega desde J1.
El anillo de +5V se alimenta por un radio en F.Cu que sale a r 20 (Freerouting lo une al resto).
"""
import math
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, "..", "pcb-k", "pcb-k.kicad_pcb")

CX, CY = 140.0, 168.0          # centro de NAV en la placa (panel (90, 32))
R_VDD = 7.2
R_VIA_IN = 8.6
R_VIA_OUT = 17.0
R_GND_VIA = 16.9
R_ESCAPE = 20.0
# Cada LED tiene su ventana de luz cortada en la placa (r 11.4–14.6, ±1.5 mm del eje): en F.Cu las
# pistas cruzan esa banda solo por los huecos. Ángulos en grados desde el eje de D(k).
DATA_IN_OFFSET = 7.0           # vía interior del salto de datos
DATA_OUT_OFFSET = 23.0         # vía exterior (el DIN de D(k+1) está a +27.3)
FEED_OFFSET = 349.0 - 330.0    # radio de alimentación, dentro del hueco D12→D1 (desde el eje de D12)
ESCAPE_OFFSET = 341.0 - 330.0  # salida de LED_12, mismo hueco
W_SIG, W_PWR = 0.2, 0.3
VIA_D, VIA_DRILL = 0.6, 0.3


def polar(r, a_deg):
    a = math.radians(a_deg)
    return CX + r * math.sin(a), CY - r * math.cos(a)


def angle_of(p):
    return math.degrees(math.atan2(p[0] - CX, -(p[1] - CY))) % 360


class Router:
    def __init__(self, board):
        self.board = board

    def net(self, name):
        return self.board.FindNet(name)

    def track(self, net, a, b, layer, width):
        t = pcbnew.PCB_TRACK(self.board)
        t.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(a[0]), pcbnew.FromMM(a[1])))
        t.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(b[0]), pcbnew.FromMM(b[1])))
        t.SetWidth(pcbnew.FromMM(width))
        t.SetLayer(layer)
        t.SetNet(self.net(net))
        t.SetLocked(True)
        self.board.Add(t)

    def via(self, net, p):
        v = pcbnew.PCB_VIA(self.board)
        v.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(p[0]), pcbnew.FromMM(p[1])))
        v.SetWidth(pcbnew.FromMM(VIA_D))
        v.SetDrill(pcbnew.FromMM(VIA_DRILL))
        v.SetNet(self.net(net))
        v.SetLocked(True)
        self.board.Add(v)


def axis(board, ref):
    return angle_of(pcbnew.ToMM(board.FindFootprintByReference(ref).GetPosition()))


def pad(board, ref, number):
    p = next(p for p in board.FindFootprintByReference(ref).Pads() if p.GetNumber() == number)
    return pcbnew.ToMM(p.GetPosition()), p.GetNetname()


def main():
    board = pcbnew.LoadBoard(BOARD)
    rt = Router(board)
    B, F = pcbnew.B_Cu, pcbnew.F_Cu

    # Re-ejecutable: borra lo que este script dejó antes (pistas bloqueadas dentro del anillo).
    for t in list(board.GetTracks()):
        x, y = pcbnew.ToMM(t.GetPosition() if t.GetClass() == "PCB_VIA" else t.GetStart())
        if t.IsLocked() and math.hypot(x - CX, y - CY) <= R_ESCAPE + 0.5:
            board.Delete(t)

    # Anillo de +5V (polígono de 48 lados; el error de cuerda a r 7.2 es de 0.015 mm).
    n = 48
    for i in range(n):
        rt.track("/+5V", polar(R_VDD, i * 360 / n), polar(R_VDD, (i + 1) * 360 / n), B, W_PWR)

    for k in range(1, 13):
        d_k, d_next, cap = f"D{k}", f"D{k % 12 + 1}", f"C{k}"
        (dout, dout_net), (gnd_k, _) = pad(board, d_k, "4"), pad(board, d_k, "1")
        (din, din_net), (vdd_next, _) = pad(board, d_next, "2"), pad(board, d_next, "3")
        (c_vdd, _), (c_gnd, _) = pad(board, cap, "1"), pad(board, cap, "2")
        a_gap = angle_of(c_vdd)

        # +5V: radios al anillo desde el VDD del LED siguiente y desde el capacitor.
        rt.track("/+5V", vdd_next, polar(R_VDD, angle_of(vdd_next)), B, W_PWR)
        rt.track("/+5V", c_vdd, polar(R_VDD, a_gap), B, W_PWR)

        # GND: vía al plano en el centro exterior del hueco.
        g = polar(R_GND_VIA, a_gap)
        rt.via("/GND", g)
        rt.track("/GND", c_gnd, g, B, W_PWR)
        rt.track("/GND", gnd_k, g, B, W_PWR)

        if k == 12:
            continue
        assert dout_net == din_net, f"{d_k}.DOUT ({dout_net}) no va a {d_next}.DIN ({din_net})"
        a_k = axis(board, d_k)
        v_in, v_out = polar(R_VIA_IN, a_k + DATA_IN_OFFSET), polar(R_VIA_OUT, a_k + DATA_OUT_OFFSET)
        rt.track(dout_net, dout, v_in, B, W_SIG)
        rt.via(dout_net, v_in)
        rt.track(dout_net, v_in, v_out, F, W_SIG)
        rt.via(dout_net, v_out)
        rt.track(dout_net, v_out, din, B, W_SIG)

    # Salida de LED_12 (DOUT de D12, adentro) hacia las teclas: por F.Cu, hasta el hueco D12→D1 y afuera.
    (dout12, net12) = pad(board, "D12", "4")
    a12 = axis(board, "D12")
    v_in = polar(R_VIA_IN, a12 + DATA_IN_OFFSET)
    bend, v_out = polar(R_VIA_IN, a12 + ESCAPE_OFFSET), polar(R_ESCAPE, a12 + ESCAPE_OFFSET)
    rt.track(net12, dout12, v_in, B, W_SIG)
    rt.via(net12, v_in)
    rt.track(net12, v_in, bend, F, W_SIG)
    rt.track(net12, bend, v_out, F, W_SIG)   # sigue por F.Cu en route_pcb_k_keys.py

    # Alimentación del anillo de +5V.
    f_in, f_out = polar(R_VDD, a12 + FEED_OFFSET), polar(R_ESCAPE, a12 + FEED_OFFSET)
    rt.via("/+5V", f_in)
    rt.track("/+5V", f_in, f_out, F, W_PWR)
    rt.via("/+5V", f_out)

    pcbnew.SaveBoard(BOARD, board)
    print("PCB-K: anillo de NAV ruteado (bloqueado)")


main()

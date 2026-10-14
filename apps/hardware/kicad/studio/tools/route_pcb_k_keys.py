"""Rutea la zona de teclas de PCB-K (J1, 6 Choc, D13–D18, C13–C19, TP1/TP2) con rutas explícitas.

Correr después de place_pcb_k.py y route_pcb_k_ring.py, y antes de add_gnd_pours.py.

Capas:
  B.Cu (cara de los SMD): cadena de datos de los LEDs y +5V.
  F.Cu (cara del panel, plano de GND): las 6 líneas de botón y LED_DIN, que salen de J1 por vías y se
  abren hacia su lado sin cruzarse. El orden de pines de J1 está elegido para eso (02-studio-pcb.md §6.1).

Canales en B.Cu:
  y 130.9   saltos de datos entre LEDs de la fila superior (por encima de los LEDs)
  y 135.1   bus de +5V bajo la fila superior (VDD de cada LED, capacitores y C19)
  y 145.7   LED_12 (del anillo a D13)
  y 146.6   D16 → D17      y 147.4   D17 → D18      y 148.1   +5V hacia el anillo y la fila inferior
GND: cada pad SMD de GND tiene su vía al plano; los switches son THT y tocan los dos planos.
Coordenadas de la placa en mm; cada ruta se revisó contra agujeros, ventanas de LED y pines.
"""
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, "..", "pcb-k", "pcb-k.kicad_pcb")

GROUP = "rutas_teclas"         # todo lo de este script queda en este grupo de KiCad
W_SIG, W_PWR = 0.2, 0.3
VIA_D, VIA_DRILL = 0.6, 0.3
B, F = pcbnew.B_Cu, pcbnew.F_Cu

TOP_Y, BOT_Y = 133.07, 163.07  # centro de los LEDs de tecla
TOP_KEYS = [110.0, 130.0, 150.0, 170.0]
BOT_KEYS = [110.0, 170.0]
J1_Y, J1_VIA_Y = 125.5, 127.4
# Los buses de +5V pasan entre la ventana del LED y el agujero central del Choc: 1.03 mm libres. Con
# 0.25 mm de ancho quedan ≥0.28 mm a cada lado (reglas: 0.25 al borde y al agujero). Llevan ≤0.3 A.
BUS_Y, BOT_BUS_Y, W_BUS = 135.1, 165.08, 0.25


def routes():
    """Devuelve (pistas, vías): pistas = (red, capa, ancho, [puntos]); vías = (red, punto)."""
    T, V = [], []

    def t(net, layer, width, *pts):
        T.append((net, layer, width, list(pts)))

    def via(net, p):
        V.append((net, p))

    # --- J1: cada pin baja a su vía (las señales siguen por F.Cu) ---------------------------------
    j1 = {135.5: "B1", 136.5: "B5", 137.5: "B2", 138.5: "LED_DIN", 139.5: "GND", 140.5: "+5V",
          141.5: "GND", 142.5: "B3", 143.5: "B6", 144.5: "B4"}
    for x, net in j1.items():
        w = W_PWR if net in ("GND", "+5V") else W_SIG
        t(net, B, w, (x, J1_Y), (x, J1_VIA_Y))
        via(net, (x, J1_VIA_Y))

    # --- Botones por F.Cu (oeste: B1, B5, B2 · este: B4, B6, B3) ----------------------------------
    t("B1", F, W_SIG, (135.5, 127.4), (135.5, 128.3), (113.0, 128.3), (113.0, 142.5), (110.0, 143.9))
    t("B5", F, W_SIG, (136.5, 127.4), (136.5, 129.2), (118.5, 129.2), (118.5, 146.0), (115.5, 146.0),
      (115.5, 159.0), (116.6, 160.1), (116.6, 172.8), (115.5, 173.9), (110.0, 173.9))
    t("B2", F, W_SIG, (137.5, 127.4), (137.5, 130.1), (133.2, 130.1), (133.2, 142.5), (130.0, 143.9))
    t("B4", F, W_SIG, (144.5, 127.4), (144.5, 128.3), (166.8, 128.3), (166.8, 142.5), (170.0, 143.9))
    t("B6", F, W_SIG, (143.5, 127.4), (143.5, 129.2), (161.5, 129.2), (161.5, 146.0), (164.3, 146.0),
      (164.3, 160.0), (166.75, 162.45), (166.75, 172.5), (168.15, 173.9), (170.0, 173.9))
    t("B3", F, W_SIG, (142.5, 127.4), (142.5, 130.1), (146.6, 130.1), (146.6, 142.5), (150.0, 143.9))

    # --- LED_DIN: J1 → F.Cu → vía junto a D1 (DIN, borde exterior del anillo); TP1 en el camino ----
    t("LED_DIN", F, W_SIG, (138.5, 127.4), (138.5, 150.9))
    via("LED_DIN", (138.5, 150.9))
    t("LED_DIN", B, W_SIG, (138.5, 150.9), (139.25, 151.6), (139.25, 152.28))
    via("LED_DIN", (138.5, 140.0))
    t("LED_DIN", B, W_SIG, (138.5, 140.0), (139.7, 140.0))

    # --- +5V: J1 → F.Cu (salta los datos en y 130.9) → bus en y 135.0 ------------------------------
    t("+5V", F, W_PWR, (140.5, 127.4), (140.5, 132.5))
    via("+5V", (140.5, 132.5))
    t("+5V", B, W_PWR, (140.5, 132.5), (140.5, BUS_Y))
    t("+5V", B, W_BUS, (112.72, BUS_Y), (176.72, BUS_Y))
    for x in TOP_KEYS:
        t("+5V", B, W_PWR, (x + 2.72, TOP_Y + 0.75), (x + 2.72, BUS_Y))      # VDD del LED
        t("+5V", B, W_PWR, (x + 6.72, TOP_Y), (x + 6.72, BUS_Y))             # pad 1 del capacitor
    t("+5V", B, W_PWR, (157.53, 136.8), (157.53, BUS_Y))                     # C19
    # Bus → anillo y fila inferior: baja por x 136.18 y cruza los datos por F.Cu hasta la vía de
    # alimentación del anillo (r 20); de ahí sale el ramal de y 148.1.
    t("+5V", B, W_PWR, (136.18, BUS_Y), (136.18, 145.9))
    via("+5V", (136.18, 145.9))
    t("+5V", F, W_PWR, (136.18, 145.9), (136.18, 148.37))
    t("+5V", B, W_PWR, (114.8, 148.1), (164.3, 148.1))
    t("+5V", B, W_PWR, (136.18, 148.1), (136.18, 148.37))
    t("+5V", B, W_PWR, (114.8, 148.1), (114.8, 160.5), (116.72, 160.5), (116.72, BOT_Y))       # C17
    t("+5V", B, W_PWR, (112.72, BOT_Y + 0.75), (112.72, 164.6), (116.72, 164.6), (116.72, BOT_Y))  # D17
    t("+5V", B, W_PWR, (164.3, 148.1), (164.3, BOT_BUS_Y))
    t("+5V", B, W_BUS, (164.3, BOT_BUS_Y), (176.72, BOT_BUS_Y), (176.72, BOT_Y))        # C18
    t("+5V", B, W_PWR, (172.72, BOT_Y + 0.75), (172.72, BOT_BUS_Y))                                 # D18

    # --- Cadena de datos ----------------------------------------------------------------------------
    for x, net in zip(TOP_KEYS[:3], ("LED_13", "LED_14", "LED_15")):
        t(net, B, W_SIG, (x + 2.72, TOP_Y - 0.75), (x + 2.72, 130.9), (x + 15.4, 130.9),
          (x + 15.4, TOP_Y + 0.75), (x + 17.28, TOP_Y + 0.75))
    t("LED_16", B, W_SIG, (172.72, TOP_Y - 0.75), (172.72, 130.9), (181.5, 130.9), (181.5, 146.6),
      (104.0, 146.6), (104.0, BOT_Y + 0.75), (107.28, BOT_Y + 0.75))
    t("LED_17", B, W_SIG, (112.72, BOT_Y - 0.75), (112.72, 147.4), (165.5, 147.4),
      (165.5, BOT_Y + 0.75), (167.28, BOT_Y + 0.75))
    t("LED_DOUT", B, W_SIG, (172.72, BOT_Y - 0.75), (172.72, 160.9), (178.0, 160.9))
    # LED_12: de la vía de salida del anillo (F.Cu) a una vía en el canal y 145.7, y por B.Cu a D13.
    t("LED_12", F, W_SIG, (133.49, 149.09), (135.0, 147.58), (135.0, 144.0))
    via("LED_12", (135.0, 144.0))
    t("LED_12", B, W_SIG, (135.0, 144.0), (135.0, 145.7), (103.6, 145.7), (103.6, TOP_Y + 0.75),
      (107.28, TOP_Y + 0.75))

    # --- GND: vía al plano por cada pad SMD ---------------------------------------------------------
    for keys, y in ((TOP_KEYS, TOP_Y), (BOT_KEYS, BOT_Y)):
        for x in keys:
            g_led = (x - 2.0, y - 2.32)
            t("GND", B, W_PWR, (x - 2.72, y - 0.75), g_led)
            via("GND", g_led)
            g_cap = (x + 9.4, y)
            t("GND", B, W_PWR, (x + 8.28, y), g_cap)
            via("GND", g_cap)
    t("GND", B, W_PWR, (160.47, 136.8), (160.47, 138.4))                     # C19
    via("GND", (160.47, 138.4))
    return T, V


def main():
    board = pcbnew.LoadBoard(BOARD)

    # Re-ejecutable: borra lo que dejó la corrida anterior (su grupo) y arma uno nuevo.
    for g in list(board.Groups()):
        if g.GetName() == GROUP:
            for item in list(g.GetItems()):
                board.Delete(item)
            board.Delete(g)
    group = pcbnew.PCB_GROUP(board)
    group.SetName(GROUP)
    board.Add(group)

    def vec(p):
        return pcbnew.VECTOR2I(pcbnew.FromMM(p[0]), pcbnew.FromMM(p[1]))

    tracks, vias = routes()
    for net, layer, width, pts in tracks:
        n = board.FindNet("/" + net)
        assert n, net
        for a, b in zip(pts, pts[1:]):
            tr = pcbnew.PCB_TRACK(board)
            tr.SetStart(vec(a))
            tr.SetEnd(vec(b))
            tr.SetWidth(pcbnew.FromMM(width))
            tr.SetLayer(layer)
            tr.SetNet(n)
            tr.SetLocked(True)
            board.Add(tr)
            group.AddItem(tr)
    for net, p in vias:
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(vec(p))
        v.SetWidth(pcbnew.FromMM(VIA_D))
        v.SetDrill(pcbnew.FromMM(VIA_DRILL))
        v.SetNet(board.FindNet("/" + net))
        v.SetLocked(True)
        board.Add(v)
        group.AddItem(v)

    pcbnew.SaveBoard(BOARD, board)
    print(f"PCB-K: zona de teclas ruteada ({len(tracks)} rutas, {len(vias)} vías)")


main()

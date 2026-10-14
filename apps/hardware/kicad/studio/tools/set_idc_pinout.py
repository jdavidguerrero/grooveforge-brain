"""Aplica en las placas el orden de pines del cable IDC 2×15 PCB-A (J2) ↔ portadora (J5).

  $KPY apps/hardware/kicad/studio/tools/set_idc_pinout.py

El orden (rev. 2026-10-14) sigue la posición x de cada pin de la Teensy, para que el bus baje casi derecho
en la portadora (theory/02 §6.2). Debe coincidir con los esquemáticos de las dos placas.
"""
import os

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
ORDER = {1: "+5V", 2: "+5V", 3: "GND", 4: "GND", 5: "B1", 6: "GND", 7: "B2", 8: "POT_VOL", 9: "B3", 10: "B4",
         11: "NAV_B", 12: "NAV_A", 13: "P2_SW", 14: "ENCR_SW", 15: "ENCL_A", 16: "ENCR_B", 17: "ENCL_B",
         18: "ENCR_A", 19: "ENCL_SW", 20: "B6", 21: "POT_IN", 22: "B5", 23: "NAV_SW", 24: "P2_B", 25: "P1_A",
         26: "LED_DIN", 27: "P2_A", 28: "P1_B", 29: "P1_SW", 30: "+3V3"}

for board_name, ref in (("portadora", "J5"), ("pcb-a", "J2")):
    path = os.path.join(HERE, "..", board_name, f"{board_name}.kicad_pcb")
    board = pcbnew.LoadBoard(path)
    fp = board.FindFootprintByReference(ref)
    for pad in fp.Pads():
        if pad.GetNumber().isdigit():
            pad.SetNet(board.FindNet("/" + ORDER[int(pad.GetNumber())]))
    pcbnew.SaveBoard(path, board)
    print(f"{board_name}: {ref} con el orden nuevo")

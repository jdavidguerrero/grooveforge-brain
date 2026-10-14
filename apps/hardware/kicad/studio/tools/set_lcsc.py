"""Escribe el campo LCSC en los símbolos de los tres esquemáticos (para el BOM de ensamble de JLCPCB).

  python3 apps/hardware/kicad/studio/tools/set_lcsc.py

Los SMD, y en la portadora también los jacks y los IDC. El resto de los THT (JST-XH, zócalos de la
Teensy y el shield, headers, el IDC de PCB-A, pots, encoders, Choc) se sueldan a mano y quedan sin LCSC. Los números se verificaron con el buscador
de piezas de JLC el 2026-10-14: todos admiten ensamble Economic, y se eligieron piezas Basic cuando
había una.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))

# Valor del símbolo → LCSC. El valor alcanza para distinguir porque cada valor usa un solo encapsulado.
LCSC = {
    # Resistencias 0603 1 % (UNI-ROYAL, todas Basic; la escalera del pad pasó de E96 a 33k/12k/5.1k)
    "330": "C23138", "1k": "C21190", "4.7k": "C23162", "10k": "C25804", "47k": "C25819",
    "100k": "C25803", "33k": "C4216", "12k": "C22790", "5.1k": "C23186",
    # MLCC
    "10nF": "C57112", "100nF": "C14663", "1uF": "C28323", "4.7uF": "C1779", "10uF": "C15850",
    "47uF": "C96123",
    # Electrolítico 6.3×7.7 (el footprint actual), 16 V
    "100uF": "C2992605",
    # Semiconductores y protección
    "SMBJ5.0CA": "C113975", "BAT54S": "C7420333", "3A hold": "C69689",
    "74AHCT1G125": "C842287", "MCP6004": "C7378",
    # SK6812MINI-EA: mismo pinout y cuerpo que el MINI-E, pero entra en ensamble Economic
    "SK6812MINI-EA": "C5378731",
}
# Conectores SMD, por referencia (su valor es descriptivo)
BY_REF = {("pcb-k", "J1"): "C160409", ("pcb-a", "J1"): "C160409",
          # Portadora: jacks e IDC los suelda JLC (THT, cargo por unión)
          **{("portadora", j): "C22419846" for j in ("J7", "J8", "J11", "J12")},    # NRJ3HF-1
          **{("portadora", j): "C4992459" for j in ("J13", "J14")},                 # SJ1-3533NG
          ("portadora", "J5"): "C48687630",     # IDC 2×15 al panel (XFCN BH254V-30P)
          ("portadora", "J6"): "C492442"}       # IDC 2×5 a la Pi (XFCN BH254V-10P)
# El resto de los THT (JST-XH, zócalos, headers, el IDC de PCB-A) se compra suelto en LCSC:
# fab/lcsc-piezas-sueltas.csv.
RENAME = {("pcb-k", "SK6812MINI-E"): "SK6812MINI-EA"}

PROP = ('\t\t(property "LCSC" "{}"\n\t\t\t(at {} {} 0)\n\t\t\t(hide yes)\n\t\t\t(show_name no)\n'
        '\t\t\t(do_not_autoplace no)\n\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n'
        '\t\t\t\t)\n\t\t\t)\n\t\t)\n')


def process(board):
    path = os.path.join(HERE, "..", board, f"{board}.kicad_sch")
    text = open(path).read()
    out, n = [], 0
    # Instancias de símbolo: bloques "\t(symbol\n" de primer nivel
    parts = re.split(r"(?m)^(?=\t\(symbol\n)", text)
    for part in parts:
        ref = re.search(r'\(property "Reference" "([^"]+)"', part)
        val = re.search(r'\(property "Value" "([^"]+)"', part)
        if part.startswith("\t(symbol\n") and ref and val:
            ref, value = ref.group(1), val.group(1)
            new_value = RENAME.get((board, value))
            if new_value:
                part = part.replace(f'(property "Value" "{value}"', f'(property "Value" "{new_value}"', 1)
                value = new_value
            code = BY_REF.get((board, ref)) or LCSC.get(value)
            part = re.sub(r'\t\t\(property "LCSC" "[^"]*"\n.*?\n\t\t\)\n', "", part, flags=re.S)
            if code:
                at = re.search(r"^\t\t\(at ([-\d.]+) ([-\d.]+)", part, re.M)
                end = part.index('\t\t(property "Description"')
                part = part[:end] + PROP.format(code, at.group(1), at.group(2)) + part[end:]
                n += 1
        out.append(part)
    open(path, "w").write("".join(out))
    print(f"{board}: LCSC en {n} símbolos")


for b in ("pcb-k", "pcb-a", "portadora"):
    process(b)

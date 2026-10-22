"""Write the LCSC field on every SMD symbol of the four panel boards (JLCPCB assembly BOM).
Adapted from the GrooveForge Brain tool of the same name.

  python3 apps/hardware/kicad/studio/tools/set_lcsc.py

Los SMD (solo la portadora se ensambla; PCB-A va sin ensamble desde 2026-10-15). Los THT (jacks, IDC,
JST-XH, zócalos, headers, pots, encoders, Choc) se sueldan a mano y quedan sin LCSC. Los números se verificaron con el buscador
de piezas de JLC el 2026-10-14: todos admiten ensamble Economic, y se eligieron piezas Basic cuando
había una.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))

# Valor del símbolo → LCSC. El valor alcanza para distinguir porque cada valor usa un solo encapsulado.
LCSC = {
    # GroovePush panel boards (checked with the JLCPCB MCP 2026-10-19: in stock, Economic assembly)
    "2.2k": "C4190", "4.7k": "C23162", "10k": "C25804",
    "10nF": "C57112", "100nF": "C14663", "10uF": "C15850", "47uF": "C96123",
    "MCP23017-E/SO": "C47023", "MCP23017T-E/SS": "C558584", "SK6812MINI-EA": "C5378731",
}
# JST-SH connectors, by reference (their value is descriptive)
BY_REF = {
    ("pcb-a", "J2"): "C160404", ("pcb-a", "J3"): "C160405", ("pcb-a", "J4"): "C160409",
    ("pcb-f", "J1"): "C160405",
    ("pcb-k1", "J1"): "C160409", ("pcb-k1", "J2"): "C160409",
    ("pcb-k2", "J1"): "C160409",
    ("pcb-s", "J1"): "C160390",   # BM04B-SRSS-TB (vertical); J2 (JST-PH, THT) is hand-soldered
}
RENAME = {}


def lookup(value):
    """Exact value, or the value's first word (e.g. 'MCP23017-E/SO @0x21', 'SK6812MINI-EA (PLAY, idx 11)')."""
    return LCSC.get(value) or LCSC.get(value.split(" ")[0])

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
            code = BY_REF.get((board, ref)) or lookup(value)
            part = re.sub(r'\t\t\(property "LCSC" "[^"]*"\n.*?\n\t\t\)\n', "", part, flags=re.S)
            if code:
                at = re.search(r"^\t\t\(at ([-\d.]+) ([-\d.]+)", part, re.M)
                end = part.index('\t\t(property "Description"')
                part = part[:end] + PROP.format(code, at.group(1), at.group(2)) + part[end:]
                n += 1
        out.append(part)
    open(path, "w").write("".join(out))
    print(f"{board}: LCSC en {n} símbolos")


for b in ("pcb-a", "pcb-f", "pcb-k1", "pcb-k2", "pcb-s"):
    process(b)

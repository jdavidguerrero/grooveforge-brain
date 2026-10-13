"""Simulación de la etapa de entrada y del CV OUT de la portadora de Studio (ngspice de KiCad).

Criterios C1–C6: apps/docs/variants/studio/theory/03-studio-input-stage.md §4.
Valores y topología: se leen del esquemático (portadora/portadora.kicad_sch) exportando su netlist con
kicad-cli. Si el esquemático deja de coincidir con el circuito simulado, el script se detiene y dice qué
red cambió. Correrlo antes de cada pedido.

Correr con cualquier Python 3 que tenga numpy:
  python3 apps/hardware/kicad/studio/sim/input_stage_sim.py
Escribe los resultados en sim/results.md.
"""
import ctypes as C
import math
import os
import re
import subprocess
import sys
import tempfile

import numpy as np

LIBNGSPICE = "/Applications/KiCad/KiCad.app/Contents/Frameworks/libngspice.0.dylib"
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMATIC = os.path.join(HERE, "..", "portadora", "portadora.kicad_sch")

# Circuito simulado = estas conexiones del esquemático (ref.pin por red). Canal L; el R se deriva
# cambiando las referencias. Si alguna falta, el netlist de la simulación ya no representa la placa.
TOPOLOGY_L = {
    "IN_L": ["R4.1", "SW1.1"],
    "TAP10_L": ["R4.2", "R5.1", "SW1.2"],
    "TAP20_L": ["R5.2", "R6.1", "SW1.4"],
    "PAD_L": ["SW1.3", "C5.1"],
    "AC_L": ["C5.2", "R10.1"],
    "PROT_L": ["R10.2", "R12.1", "D2.3", "U2.3"],
    "BUF_L": ["U2.1", "U2.2", "C8.1"],
    "LINEIN_L": ["C8.2", "J9.1"],
}
CHANNEL_R = {"_L": "_R", "R4.": "R7.", "R5.": "R8.", "R6.": "R9.", "R10.": "R11.", "R12.": "R13.",
             "C5.": "C6.", "C8.": "C9.", "D2.": "D3.", "SW1.1": "SW1.5", "SW1.2": "SW1.6",
             "SW1.3": "SW1.7", "SW1.4": "SW1.8", "U2.1": "U2.7", "U2.2": "U2.6", "U2.3": "U2.5",
             "J9.1": "J9.2"}
TOPOLOGY_COMMON = {
    "VREF": ["R12.2", "R13.2", "U2.8", "U2.9"],
    "VREF_DIV": ["R14.2", "R15.1", "C10.1", "U2.10"],
    "+3V3": ["R14.1", "D2.2", "D3.2", "U2.4"],
    "GND": ["R6.2", "R9.2", "D2.1", "D3.1", "R15.2", "C10.2", "U2.11", "C13.2", "C14.2"],
    "CV_PWM": ["R18.1"],
    "CVF1": ["R18.2", "C13.1", "R19.1"],
    "CV_FILT": ["R19.2", "C14.1", "U2.12"],
    "CV_BUF": ["U2.13", "U2.14", "R20.1"],
    "CV_OUT": ["R20.2", "J14.T"],
}
# Plan B (theory/03 §6): deben existir y quedar abiertos por defecto.
BYPASS = {"JP2": ("PROT_L", "BUF_L"), "JP3": ("PROT_R", "BUF_R"),
          "JP4": ("VREF_DIV", "VREF"), "JP5": ("CV_FILT", "CV_BUF")}

P = {}                             # valores leídos del esquemático (ref → float)
RIN_CODEC = [10e3, 29e3, 100e3]   # impedancia de LINE IN del SGTL5000: sin confirmar, se barre
F_PWM = 36621.09                   # analogWriteFrequency ideal para 12 bits en la Teensy 4.x

# MCP6004 por parámetros del datasheet DS20001733: GBW 1 MHz, A_OL 112 dB, SR 0.6 V/µs, salida a 25 mV
# de los rieles. Las ESD de entrada (diodos de silicio a los rieles) dejan ver cuánta corriente se
# llevaría la entrada si los BAT54S no estuvieran.
A_OL = 10 ** (112 / 20)
GBW = 1e6
CP = 1 / (2 * math.pi * (GBW / A_OL) * A_OL)   # gm = 1 S → polo dominante en GBW/A_OL
IMAX = 0.6e6 * CP                               # slew rate = IMAX / CP
OPAMP = f"""
.subckt mcp6004 inp inn vdd vss out
Desd1 inp vdd DESD
Desd2 vss inp DESD
Desd3 inn vdd DESD
Desd4 vss inn DESD
Bgm 0 n1 I = {IMAX:.6g} * tanh(V(inp, inn) / {IMAX:.6g})
Rp n1 0 {A_OL:.6g}
Cp n1 0 {CP:.6g}
Dcl1 n1 vdd DCL
Dcl2 vss n1 DCL
Bo oi 0 V = min(max(V(n1), V(vss) + 0.025), V(vdd) - 0.025)
Ro oi out 50
.model DESD D(IS=1e-14 N=1)
.model DCL D(IS=1e-12 N=0.3)
.ends
"""
BAT54 = ".model BAT54 D(IS=9e-9 N=1 RS=2 CJO=10p BV=30 IBV=2e-6)"


class _VecInfo(C.Structure):
    _fields_ = [("v_name", C.c_char_p), ("v_type", C.c_int), ("v_flags", C.c_short),
                ("v_realdata", C.POINTER(C.c_double)), ("v_compdata", C.c_void_p), ("v_length", C.c_int)]


class _Complex(C.Structure):
    _fields_ = [("re", C.c_double), ("im", C.c_double)]


class Spice:
    def __init__(self):
        self.lib = C.CDLL(LIBNGSPICE)
        self.log = []
        send = C.CFUNCTYPE(C.c_int, C.c_char_p, C.c_int, C.c_void_p)
        stat = C.CFUNCTYPE(C.c_int, C.c_int, C.c_bool, C.c_bool, C.c_int, C.c_void_p)
        # Los callbacks se guardan en el objeto: si el GC los libera, ngspice salta a memoria inválida.
        self._send = send(lambda s, i, u: self.log.append(s.decode(errors="replace")) or 0)
        self._exit = stat(lambda *a: 0)
        self.lib.ngSpice_Init(self._send, None, self._exit, None, None, None, None)
        self.lib.ngGet_Vec_Info.restype = C.POINTER(_VecInfo)

    def run(self, netlist):
        self.log.clear()
        self.lib.ngSpice_Command(b"destroy all")
        self.lib.ngSpice_Command(b"remcirc")
        for line in netlist.strip().splitlines():
            self.lib.ngSpice_Command(("circbyline " + line).encode())
        self.lib.ngSpice_Command(b"run")
        errors = [l for l in self.log if "rror" in l]
        if errors:
            raise RuntimeError("\n".join(errors[:5]))

    def vec(self, name):
        p = self.lib.ngGet_Vec_Info(name.encode())
        if not p:
            raise KeyError(name)
        v = p.contents
        if v.v_realdata:
            return np.ctypeslib.as_array(v.v_realdata, shape=(v.v_length,)).copy()
        arr = C.cast(v.v_compdata, C.POINTER(_Complex))
        return np.array([complex(arr[i].re, arr[i].im) for i in range(v.v_length)])


_UNITS = {"p": 1e-12, "n": 1e-9, "u": 1e-6, "µ": 1e-6, "m": 1e-3, "k": 1e3, "M": 1e6}


def parse_value(text):
    """'12.4k' → 12400, '2.2uF' → 2.2e-6, '4k7' → 4700."""
    m = re.fullmatch(r"\s*([0-9.]+)\s*([pnuµmkM]?)([0-9]*)\s*(F|Ω|ohm|R)?\s*", text)
    if not m:
        raise ValueError(f"valor no reconocido: {text!r}")
    num, unit, tail = m.group(1), m.group(2), m.group(3)
    if tail:                                     # notación 4k7
        num = f"{num}.{tail}"
    return float(num) * _UNITS.get(unit, 1.0)


def load_schematic():
    """Exporta el netlist de la portadora, verifica la topología y devuelve {ref: valor}."""
    with tempfile.TemporaryDirectory() as tmp:
        net_path = os.path.join(tmp, "portadora.net")
        subprocess.run([KICAD_CLI, "sch", "export", "netlist", "--format", "kicadsexpr",
                        "-o", net_path, SCHEMATIC], check=True, capture_output=True)
        text = open(net_path).read()

    values = {m.group(1): m.group(2)
              for m in re.finditer(r'\(comp\s*\(ref "([^"]+)"\)\s*\(value "([^"]*)"\)', text)}
    nets = {}
    for chunk in re.split(r"\(net\s*\(code", text)[1:]:
        name = re.search(r'\(name "([^"]+)"', chunk).group(1).lstrip("/")
        nets[name] = {f"{r}.{p}" for r, p in re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', chunk)}

    def to_r(item):
        """Referencia del canal L → la equivalente del R (pin exacto primero, luego prefijo de ref)."""
        if item in CHANNEL_R:
            return CHANNEL_R[item]
        for a, b in CHANNEL_R.items():
            if a.endswith(".") and item.startswith(a):
                return b + item[len(a):]
        return item

    expected = dict(TOPOLOGY_COMMON)
    for net, nodes in TOPOLOGY_L.items():
        expected[net] = nodes
        expected[net.replace("_L", "_R")] = [to_r(n) for n in nodes]
    problems = []
    for net, nodes in expected.items():
        missing = set(nodes) - nets.get(net, set())
        if missing:
            problems.append(f"  red {net}: falta {', '.join(sorted(missing))}")
    for jp, (a, b) in BYPASS.items():
        if f"{jp}.1" not in nets.get(a, set()) or f"{jp}.2" not in nets.get(b, set()):
            problems.append(f"  {jp}: debería unir {a} con {b}")
    if problems:
        raise SystemExit("El esquemático ya no coincide con el circuito simulado:\n" + "\n".join(problems))

    refs = ["R4", "R5", "R6", "C5", "R10", "R12", "C8", "R14", "R15", "C10",
            "R18", "R19", "C13", "C14", "R20"]
    vals = {r: parse_value(values[r]) for r in refs}
    # El canal R se simula en reposo con los valores del L: si difieren, avisar en vez de suponer.
    for l, r in [("R4", "R7"), ("R5", "R8"), ("R6", "R9"), ("C5", "C6"), ("R10", "R11"),
                 ("R12", "R13"), ("C8", "C9")]:
        if parse_value(values[r]) != vals[l]:
            problems.append(f"  {r} ({values[r]}) ≠ {l} ({values[l]})")
    if problems:
        raise SystemExit("Los canales L y R difieren:\n" + "\n".join(problems))
    return vals, values


def input_netlist(pos, rin, src, analysis):
    """Un canal completo (L) + canal R en reposo + VREF. pos: '0', '10' o '20'."""
    tap = {"0": "in", "10": "tap10", "20": "tap20"}[pos]
    return f"""studio input stage
VDD vdd 0 3.3
Vsrc src 0 {src}
Rsrc src in 100
R4 in tap10 {P['R4']}
R5 tap10 tap20 {P['R5']}
R6 tap20 0 {P['R6']}
Rsw {tap} pad 0.05
C5 pad ac {P['C5']}
R10 ac prot {P['R10']}
R12 prot vref {P['R12']}
Dlo 0 prot BAT54
Dhi prot dhi BAT54
Vdhi dhi vdd 0
Vin_m prot prot_u 0
XU2A prot_u buf vdd 0 buf mcp6004
C8 buf lin {P['C8']}
Rcodec lin 0 {rin}
* canal R en reposo: carga VREF igual que el L
C6 0 acr {P['C5']}
R11 acr protr {P['R10']}
R13 protr vref {P['R12']}
XU2B protr bufr vdd 0 bufr mcp6004
C9 bufr linr {P['C8']}
Rcodecr linr 0 {rin}
R14 vdd vdiv {P['R14']}
R15 vdiv 0 {P['R15']}
C10 vdiv 0 {P['C10']}
XU2C vdiv vref vdd 0 vref mcp6004
{OPAMP}
{BAT54}
{analysis}
.end
"""


def cv_netlist(duty, t_stop):
    period = 1 / F_PWM
    return f"""studio cv out
VDD vdd 0 3.3
Vpwm pwm 0 PULSE(0 3.3 0 5n 5n {duty * period - 10e-9:.6g} {period:.6g})
R18 pwm cvf1 {P['R18']}
C13 cvf1 0 {P['C13']}
R19 cvf1 cvfilt {P['R19']}
C14 cvfilt 0 {P['C14']}
XU2D cvfilt cvbuf vdd 0 cvbuf mcp6004
R20 cvbuf cvout {P['R20']}
Rload cvout 0 100k
{OPAMP}
.tran 0.2u {t_stop} 0 0.2u uic
.end
"""


def db(x):
    return 20 * np.log10(np.abs(x))


def thd(t, v, f0, periods=10, harmonics=10):
    """THD por FFT sobre un número entero de períodos, remuestreado uniforme."""
    t_end = t[-1]
    t0 = t_end - periods / f0
    n = 4096
    tu = np.linspace(t0, t_end, n, endpoint=False)
    vu = np.interp(tu, t, v)
    spec = np.abs(np.fft.rfft(vu - vu.mean())) / (n / 2)
    fund = spec[periods]
    harm = np.sqrt(sum(spec[periods * k] ** 2 for k in range(2, harmonics + 1)))
    return harm / fund, fund


def main():
    vals, raw = load_schematic()
    P.update(vals)
    sp = Spice()
    out = []
    ok_all = True

    def verdict(ok):
        nonlocal ok_all
        ok_all &= bool(ok)
        return "✅" if ok else "❌"

    # --- C1 + C2: respuesta AC por posición y por R_in del codec -------------------------------------
    out.append("## C1 · Niveles del pad (1 kHz) y C2 · Respuesta 20 Hz–20 kHz\n")
    out.append("| Pad | R_in codec | Ganancia @1 kHz | Error vs nominal | 20 Hz | 20 kHz | Plano ±0.5 dB |")
    out.append("|---|---|---|---|---|---|---|")
    c1_ok = True
    c2_ok = True
    for pos in ("0", "10", "20"):
        for rin in RIN_CODEC:
            sp.run(input_netlist(pos, rin, "DC 0 AC 1", ".ac dec 100 10 100k"))
            f = np.real(sp.vec("frequency"))
            g = db(sp.vec("lin"))
            g1k = np.interp(1e3, f, g)
            g20, g20k = np.interp(20, f, g), np.interp(20e3, f, g)
            err = g1k - (-float(pos))
            flat = max(abs(g20 - g1k), abs(g20k - g1k))
            if pos != "0":
                c1_ok &= abs(err) <= 0.5
            if rin == 29e3:
                c2_ok &= flat <= 0.5
            out.append(f"| {pos} dB | {rin / 1e3:.0f} k | {g1k:.2f} dB | {err:+.2f} dB | {g20 - g1k:+.2f} dB "
                       f"| {g20k - g1k:+.2f} dB | {'sí' if flat <= 0.5 else 'no'} |")
    out.append(f"\n**C1 {verdict(c1_ok)}** (tomas −10/−20 a ±0.5 dB) · **C2 {verdict(c2_ok)}** "
               "(±0.5 dB con R_in = 29 k)\n")

    # --- C3: techo en la posición 0 dB ----------------------------------------------------------------
    out.append("## C3 · Techo (pad en 0 dB, 1 kHz, R_in = 29 k)\n")
    out.append("| Entrada (Vpp) | Salida en LINE IN (Vpp) | THD |")
    out.append("|---|---|---|")
    c3_ok = None
    for vpp in (2.0, 2.6, 3.0, 3.12, 3.2, 3.3, 3.5, 4.0):
        sp.run(input_netlist("0", 29e3, f"SIN(0 {vpp / 2} 1k)", ".tran 1u 30m 15m 1u"))
        t, v = sp.vec("time"), sp.vec("lin")
        d, fund = thd(t, v, 1e3)
        out_vpp = v[t > 20e-3].max() - v[t > 20e-3].min()
        if vpp == 3.0:   # con el pad en 0 dB la ganancia es ~1: 3.0 Vpp de entrada ≈ 3.0 Vpp en LINE IN
            c3_ok = d < 1e-3
        out.append(f"| {vpp:.2f} | {out_vpp:.2f} | {d * 100:.3f} % |")
    out.append(f"\n**C3 {verdict(c3_ok)}** (3.0 Vpp sin recorte, THD < 0.1 %)\n")

    # --- C4: sobretensión -----------------------------------------------------------------------------
    out.append("## C4 · Sobretensión (±12 V pico, 1 kHz, pad en 0 dB)\n")
    sp.run(input_netlist("0", 29e3, "SIN(0 12 1k)", ".tran 0.5u 20m 10m 0.5u"))
    t = sp.vec("time")
    v_in = sp.vec("prot_u")
    i_in = sp.vec("vin_m#branch")
    i_hi = sp.vec("vdhi#branch")
    v_lin = sp.vec("lin")
    i_r10 = (sp.vec("ac") - sp.vec("prot")) / 1e3
    rows = [
        ("Entrada del MCP6004 (min / max)", f"{v_in.min():.2f} V / {v_in.max():.2f} V",
         v_in.min() >= -1.0 and v_in.max() <= 4.3, "[−1.0, 4.3] V"),
        ("Corriente en la entrada del MCP6004 (pico)", f"{np.abs(i_in).max() * 1e6:.1f} µA",
         np.abs(i_in).max() <= 2e-3, "≤ 2 mA"),
        ("Corriente por R10 / BAT54S (pico)", f"{np.abs(i_r10).max() * 1e3:.2f} mA",
         np.abs(i_r10).max() < 0.2, "< 200 mA"),
        ("Corriente inyectada al +3V3 (pico)", f"{np.abs(i_hi).max() * 1e3:.2f} mA", True, "informativo"),
        ("Salida en LINE IN (min / max)", f"{v_lin.min():.2f} V / {v_lin.max():.2f} V", True, "informativo"),
    ]
    out.append("| Qué | Valor | Límite | |")
    out.append("|---|---|---|---|")
    c4_ok = True
    for name, val, ok, lim in rows:
        c4_ok &= bool(ok)
        out.append(f"| {name} | {val} | {lim} | {'✅' if ok else '❌'} |")
    out.append(f"\n**C4 {verdict(c4_ok)}**\n")

    # --- C5: VREF -------------------------------------------------------------------------------------
    out.append("## C5 · VREF\n")
    sp.run(input_netlist("0", 29e3, "DC 0", ".op"))
    vref_op = float(sp.vec("vref")[0])
    vprot_op = float(sp.vec("prot")[0])
    sp.run(input_netlist("0", 29e3, "SIN(0 1.5 1k)", ".tran 1u 30m 15m 1u"))
    vr = sp.vec("vref")
    c5_ok = abs(vref_op - 1.65) <= 0.033 and abs(vr.max() - 1.65) <= 0.033 and abs(vr.min() - 1.65) <= 0.033
    out.append(f"- En reposo: VREF = {vref_op:.4f} V; nodo PROT = {vprot_op:.4f} V")
    out.append(f"- Con 3 Vpp en L: VREF entre {vr.min():.4f} y {vr.max():.4f} V "
               f"(ripple {(vr.max() - vr.min()) * 1e3:.2f} mVpp)")
    out.append(f"\n**C5 {verdict(c5_ok)}** (1.65 V ± 2 %)\n")

    # --- C6: CV OUT -----------------------------------------------------------------------------------
    out.append(f"## C6 · CV OUT (PWM {F_PWM / 1e3:.1f} kHz → RC {raw['R18']}/{raw['C13']} ×2 → seguidor)\n")
    sp.run(cv_netlist(1.0, 40e-3))
    t, v = sp.vec("time"), sp.vec("cvout")
    final = v[-1]
    settle = t[np.where(np.abs(v - final) > 0.01 * final)[0][-1]]
    sp.run(cv_netlist(0.5, 40e-3))
    t2, v2 = sp.vec("time"), sp.vec("cvout")
    tail = v2[t2 > 35e-3]
    ripple = tail.max() - tail.min()
    c6a, c6b = settle < 10e-3, ripple < 1e-3
    out.append(f"- Escalón de 0 → 100 % de duty: final {final:.3f} V; al 1 % en **{settle * 1e3:.1f} ms** "
               f"{'✅' if c6a else '❌'} (< 10 ms)")
    out.append(f"- Ripple con 50 % de duty: **{ripple * 1e6:.0f} µVpp** {'✅' if c6b else '❌'} (< 1 mVpp); "
               f"media {tail.mean():.3f} V")
    out.append(f"\n**C6 {verdict(c6a and c6b)}**\n")

    report = "\n".join(out)
    header = ("# Resultados · etapa de entrada y CV de la portadora\n\n"
              "Generado por `input_stage_sim.py` (ngspice de KiCad; MCP6004 por parámetros del datasheet, "
              "no el macro oficial).\n"
              "Valores leídos del esquemático de la portadora (topología verificada): "
              f"escalera {raw['R4']} / {raw['R5']} / {raw['R6']} · C5 {raw['C5']} · R10 {raw['R10']} · "
              f"R12 {raw['R12']} · C8 {raw['C8']} · RC del CV {raw['R18']} / {raw['C13']} ×2.\n\n")
    with open(os.path.join(HERE, "results.md"), "w") as fh:
        fh.write(header + report)
    print(header + report)
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main())

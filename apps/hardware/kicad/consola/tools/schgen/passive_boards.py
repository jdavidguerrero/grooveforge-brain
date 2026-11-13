"""Console faders and passive six-sensor FSR carrier."""

from .generator import Board
from .bom import populate_fields
from .pcb_k import capacitor, packed, part

FADER_FP = 'groovepush:ALPS_RS6011P_60mm'
FSR_FP = 'gf_consola:FSR402_DirectSolder_1x02_P2.54mm'
FSR_NAMES = ('V1', 'V2', 'V3', 'V4', 'X1', 'X2')
FSR_TARGETS = ('J_FSR1', 'J_FSR2', 'J_FSR3', 'J_FSR4', 'J7_EXP1', 'J8_EXP2')


def pcb_f():
    """Preserve GroovePush's passive fader circuit and JST-SH J1 pinout."""
    symbols = [part('Connector_Generic:Conn_01x06', 'J1', 'PCB-A J3 / 1:1',
                    'Connector_JST:JST_SH_SM06B-SRSS-TB_1x06-1MP_P1.00mm_Horizontal',
                    dict(zip(map(str, range(1, 7)), ('+3V3', 'FADER1', 'FADER2', 'FADER3', 'FADER4', 'GND')))),
               capacitor('C5', '100nF', '+3V3')]
    for i in range(1, 5):
        symbols += [part('Device:R_Potentiometer_MountingPin', f'RV{i}', 'RS6011 B50K', FADER_FP,
                         {'1': '+3V3', '2': f'FADER{i}', '3': 'GND', 'MP': 'GND'}),
                    capacitor(f'C{i}', '100nF', f'FADER{i}')]
    sheets = packed('faders', symbols, [
        'J1 vers PCB-A J3. MP a GND. Contorno/agujeros: mechanical.csv (v5).',
        'RV1-4: paso 20 mm, recorrido 60 mm. Comprobar pieza RS6011 exacta antes de layout.'])
    populate_fields(sheets)
    return Board('pcb-f', sheets)


def pcb_p():
    """Six isolated two-wire sensors, without shared rails or conditioning on this board."""
    symbols = []
    for i, (name, target) in enumerate(zip(FSR_NAMES, FSR_TARGETS), 1):
        # Separate supply nets deliberately preserve one independent 1:1 circuit per cable.
        supply, signal = f'+3V3_{name}', f'FSR_{name}'
        symbols += [part('Device:R_Variable', f'FSR{i}', f'Interlink 402 {name}', FSR_FP,
                         {'1': supply, '2': signal}),
                    part('Connector_Generic:Conn_01x02', f'J{i}', f'PCB-M {target}',
                         'Connector_JST:JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal',
                         {'1': supply, '2': signal})]
    sheets = packed('fsr-passive', symbols, [
        '6 circuitos independientes. Pin 1: +3V3; pin 2: senal. Sin divisor ni filtro aqui.',
        'D43: soldar solo pines metalicos prensados, sin socket; envolvente superior <=1mm; no calor en pelicula.'])
    populate_fields(sheets)
    return Board('pcb-p', sheets)

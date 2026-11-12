"""Reviewable physical cable contracts; never derive these from schgen pin maps."""

# Aliases are board-local, including the service jumpers' downstream names.
ALIASES = {
    'pcb-m': {'I2C_SDA_A': 'I2C_SDA_B_A', 'I2C_SDA_K': 'I2C_SDA_B_K',
              'I2C_SCL_A': 'I2C_SCL_B_A', 'I2C_SCL_K': 'I2C_SCL_B_K',
              '+5V_LED_K': '+5V_LED', '+5V_LED_SHARP': '+5V_LED'},
    'pcb-k-left': {'+3V3': '+3.3V', 'SDA': 'I2C_SDA_B_K', 'SCL': 'I2C_SCL_B_K'},
    'pcb-k-right': {'+3V3': '+3.3V', 'SDA': 'I2C_SDA_B_K', 'SCL': 'I2C_SCL_B_K'},
    'pcb-a': {'+3V3': '+3.3V', '+5V': '+5V_LED', 'SDA': 'I2C_SDA_B_A', 'SCL': 'I2C_SCL_B_A',
              **{f'FADER{i}': f'FADER{i}_SIG' for i in range(1, 5)}},
    'pcb-f': {'+3V3': '+3.3V', **{f'FADER{i}': f'FADER{i}_SIG' for i in range(1, 5)}},
    'pcb-p': {**{f'+3V3_{n}': '+3.3V' for n in ('V1', 'V2', 'V3', 'V4', 'X1', 'X2')},
              **{f'FSR_V{i}': f'FSR{i}_ADC' for i in range(1, 5)},
              'FSR_X1': 'EXP1_ADC', 'FSR_X2': 'EXP2_ADC'},
}


def cable(name, a, b, ways, kind, **extra):
    """A pair of (board, reference) endpoints; mapping defaults to physical pin 1:1."""
    return dict(name=name, a=a, b=b, ways=ways, kind=kind, **extra)


CABLES = [
    cable('M-K', ('pcb-m', 'J_PCB_K'), ('pcb-k-left', 'J1'), 10, 'JST-PH'),
    cable('K-left-right', ('pcb-k-left', 'J2'), ('pcb-k-right', 'J1'), 6, 'JST-PH'),
    cable('M-A-IDC', ('pcb-m', 'J1_PCBA'), ('pcb-a', 'J1'), 30, 'IDC',
          # Exact endpoint expectations: None = explicit NC, not a wildcard.
          exceptions={'18': (None, None), '20': (None, 'INT_K'),
                      '22': (None, 'INT_A'), '26': (None, 'LED_DIN')},
          note='D8: M.18/20/22/26 NC; MCP polling, LED data only via M-K. A J4 legacy unused.'),
    cable('M-A-I2C', ('pcb-m', 'J2_I2C'), ('pcb-a', 'J2'), 4, 'JST-SH'),
    cable('A-F', ('pcb-a', 'J3'), ('pcb-f', 'J1'), 6, 'JST-SH'),
]
for i, ref in enumerate(('J_FSR1', 'J_FSR2', 'J_FSR3', 'J_FSR4', 'J7_EXP1', 'J8_EXP2'), 1):
    CABLES.append(cable(f'M-P-{i}', ('pcb-m', ref), ('pcb-p', f'J{i}'), 2, 'JST-PH'))

# External parts have contact names, not invented manufacturer pin numbers.
# Replace with purchased part terminal numbers after continuity testing.
PANEL = {}


def panel(ref, nets, contacts, description, kind='JST-PH'):
    """Document an expected panel counterpart and append its cable."""
    if len(nets) != len(contacts):
        raise ValueError(f'{ref}: panel contacts/net count differs')
    PANEL[ref] = dict(pins={str(i): n for i, n in enumerate(nets, 1)},
                      contacts=dict(zip(map(str, range(1, len(nets)+1)), contacts)),
                      description=description)
    CABLES.append(cable(f'panel-{ref}', ('pcb-m', ref), ('panel', ref), len(nets), kind))


for side in ('L', 'R'):
    panel(f'J_LINE_IN_{side}', [f'LINE_IN_{side}_T', f'LINE_IN_{side}_R', 'AGND'],
          ['tip', 'ring', 'sleeve'], 'Jack LINE IN TRS; ring sin terminación definida en PCB-M.')
    panel(f'J_LINE_OUT_{side}', [f'LINE_OUT_{side}_T', None, 'AGND'],
          ['tip', 'ring NC', 'sleeve'], 'Jack LINE OUT TRS; dejar ring sin conexión.')
for i in (1, 2):
    panel(f'J_CV{i}', [f'CV{i}_PROTECTED', 'AGND'], ['tip', 'sleeve'], 'Jack CV TS.')
panel('J_PEDAL', ['PEDAL_BTN', None, 'GND'], ['tip', 'ring NC', 'sleeve'],
      'Pedal: cierre de contacto entre tip y sleeve; no asignar expresión al ring.')
panel('J_HP', ['HP_L', 'HP_R', 'HP_VGND'], ['tip L', 'ring R', 'sleeve virtual'],
      'Audífonos TRS: sleeve exclusivamente HP_VGND, aislado del chasis/masas.')
panel('J_JOG', ['GND', 'JOG_A', 'JOG_B', 'JOG_CLICK'],
      ['común encoder + común pulsador', 'fase A', 'fase B', 'pulsador'],
      'Jog: unir comunes localmente; verificar fases con pieza comprada.')
for name in ('MASTER', 'CUE', 'IN'):
    panel(f'J_POT_{name}', ['+3.3V', f'POT_{name}_WIPER', 'GND'],
          ['extremo horario', 'cursor', 'extremo antihorario'], 'Potenciómetro lineal 10k.')
panel('J_PAD_SEL', ['+3.3V', 'PAD_SEL_ADC', 'GND'],
      ['extremo alto polo C', 'común polo C', 'extremo bajo polo C'],
      'Selector 3 posiciones: contacto central polo C SIN cable (D13).')
panel('SW_PAD', ['LINE_IN_L_T', 'TAP10_L', 'PAD_L', 'TAP20_L',
                 'LINE_IN_R_T', 'TAP10_R', 'PAD_R', 'TAP20_R'],
      ['A 0dB', 'A -10dB', 'A común', 'A -20dB',
       'B 0dB', 'B -10dB', 'B común', 'B -20dB'],
      'Mismo selector: polos A/B audio, header 2x4 2.54mm (no IDC con llave).', kind='header-2x4')
for i in (1, 2):
    panel(f'J{i+4}_SHARP{i}', ['+5V_LED', f'SHARP{i}_ADC', 'GND'],
          ['VCC', 'VO', 'GND'],
          'Sharp GP2Y0A41: mapear por FUNCIÓN, no asumir orden físico del sensor; VO ≤3.3V.')
panel('J3_M4', ['GND', None, 'M4_TX7', 'M4_RX7'],
      ['CONN1 GND', 'CONN1 3V3 sin conectar', 'CONN1 SDA = RX', 'CONN1 SCL = TX'],
      'NeoTrellis M4 por USB propio; señales UART cruzadas por función.')
panel('J_5V_LED_IN', ['+5V_LED_RAW', 'GND'], ['USB-A VBUS', 'USB-A GND'],
      'USB-A Pi solo alimentación. D+/D- sin cable; no unir VBUS con +5V_TEENSY.')

# Every remaining M connector must be deliberately excluded, rather than silently forgotten.
EXCLUDED = {
    ('pcb-m', 'J_MIDI_IN'): 'D20: MIDI USB en Pi; DNP, sin mazo ni agujero en cuerpo.',
    ('pcb-m', 'J_MIDI_OUT'): 'D20: MIDI USB en Pi; DNP, sin mazo ni agujero en cuerpo.',
    ('pcb-m', 'J_T41L'): 'Socket del módulo Teensy, no mazo.',
    ('pcb-m', 'J_T41R'): 'Socket del módulo Teensy, no mazo.',
    ('pcb-m', 'J_SHIELD_AUDIO'): 'Headers internos Audio Shield, revisar módulo físicamente.',
    ('pcb-m', 'J_SHIELD_HP'): 'Headers internos Audio Shield, revisar módulo físicamente.',
    ('pcb-m', 'J_EDRUM'): 'Reservado DNP, sin mazo autorizado.',
    ('pcb-m', 'J_5V_BKUP'): 'Backup DNP; no conectar segunda fuente simultánea.',
    ('pcb-a', 'J4'): 'Ruta histórica K1/K2; NO conectar en integración PCB-M (D8).',
}
BOARDS = ('pcb-m', 'pcb-k-left', 'pcb-k-right', 'pcb-f', 'pcb-p', 'pcb-a')

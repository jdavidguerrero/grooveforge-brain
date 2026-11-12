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
panel('J_PEDAL', ['PEDAL_RAW', None, 'GND'], ['tip', 'ring NC', 'sleeve'],
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
    panel(f'J{i+4}_SHARP{i}', ['+5V_LED', f'SHARP{i}_RAW', 'GND'],
          ['VCC', 'VO', 'GND'],
          'Sharp GP2Y0A41: mapear por FUNCIÓN, no asumir orden físico del sensor; VO ≤3.3V.')
panel('J3_M4', ['GND', None, 'M4_RX7', 'M4_TX7'],
      ['CONN1.1 GND', 'CONN1.2 3V3 NC', 'CONN1.3 SDA = TX21 M4 -> RX28 Teensy', 'CONN1.4 SCL = RX22 M4 <- TX29 Teensy'],
      'NeoTrellis M4 por USB propio; señales UART cruzadas por función.')
panel('J_5V_LED_IN', ['+5V_LED_RAW', 'GND'], ['USB-A VBUS', 'USB-A GND'],
      'USB-A Pi solo alimentación. D+/D- sin cable; no unir VBUS con +5V_TEENSY.')

# Every remaining M connector must be deliberately excluded, rather than silently forgotten.
EXCLUDED = {
    ('pcb-m', 'J_MIDI_IN'): 'D20: MIDI USB en Pi; DNP, sin mazo ni agujero en cuerpo.',
    ('pcb-m', 'J_MIDI_OUT'): 'D20: MIDI USB en Pi; DNP, sin mazo ni agujero en cuerpo.',
    ('pcb-m', 'J_T41L'): 'Socket del módulo Teensy, no mazo.',
    ('pcb-m', 'J_T41R'): 'Socket del módulo Teensy, no mazo.',
    ('pcb-m', 'J_EDRUM'): 'Reservado DNP, sin mazo autorizado.',
    ('pcb-m', 'J_5V_BKUP'): 'Backup DNP; no conectar segunda fuente simultánea.',
    ('pcb-a', 'J4'): 'Ruta histórica K1/K2; NO conectar en integración PCB-M (D8).',
}
BOARDS = ('pcb-m', 'pcb-k-left', 'pcb-k-right', 'pcb-f', 'pcb-p', 'pcb-a')

# Rev D viewed from its reverse, photos in docs/fotos. These are soldered module
# pads, not an invented consecutive header. Upper unlabeled pads stay NC.
panel('J_SHIELD_IN', ['SHIELD_LINE_IN_L', 'SHIELD_LINE_IN_R', 'GND'],
      ['lower LINE IN L (rightmost)', 'lower LINE IN R', 'upper G above LINE IN L'],
      'Rev D reverse: lower Lout/G/Rout/Rin/Lin; upper G over Rin/Lin. 2.54 mm pitch.')
panel('J_SHIELD_OUT', ['SHIELD_LINE_OUT_L', 'SHIELD_LINE_OUT_R', 'GND'],
      ['lower LINE OUT L (leftmost)', 'lower LINE OUT R', 'lower LINE OUT G'],
      'Rev D reverse. Two unlabeled upper pads over Lout/Rout MUST stay NC.')
panel('J_SHIELD_HP', ['HP_L', 'HP_R', 'HP_VGND'],
      ['bottom right L', 'bottom right R', 'bottom right VGND'],
      'Rev D: CAUTION do not short VGND to GND; isolated headphone return.')

# Explicit physical reference changes for annotated PCB-M connector labels.
M_REFERENCES = {
    'J_T41L': 'J_T41L1', 'J_T41R': 'J_T41R1', 'J1_PCBA': 'J1_PCBA1',
    'J2_I2C': 'J2_I2C1', 'J_PCB_K': 'J_PCB_K1', 'J_JOG': 'J_JOG1',
    'J_POT_MASTER': 'J_POT_MASTER1', 'J_POT_CUE': 'J_POT_CUE1', 'J_POT_IN': 'J_POT_IN1',
    'J_PAD_SEL': 'J_PAD_SEL1', 'SW_PAD': 'SW_PAD1', 'J_HP': 'J_HP1',
    'J_LINE_IN_L': 'J_LINE_IN_L1', 'J_LINE_IN_R': 'J_LINE_IN_R1',
    'J_LINE_OUT_L': 'J_LINE_OUT_L1', 'J_LINE_OUT_R': 'J_LINE_OUT_R1',
    'J_PEDAL': 'J_PEDAL1', 'J_EDRUM': 'J_EDRUM1', 'J_5V_LED_IN': 'J_5V_LED_IN1',
    'J_5V_BKUP': 'J_5V_BKUP1', 'J_MIDI_IN': 'J_MIDI_IN1', 'J_MIDI_OUT': 'J_MIDI_OUT1',
    'J_SHIELD_IN': 'J_SHIELD_IN1', 'J_SHIELD_OUT': 'J_SHIELD_OUT1', 'J_SHIELD_HP': 'J_SHIELD_HP1',
}
for c in CABLES:
    for end in ('a', 'b'):
        board, ref = c[end]
        if board == 'pcb-m':
            c[end] = board, M_REFERENCES.get(ref, ref)
EXCLUDED = {(board, M_REFERENCES.get(ref, ref) if board == 'pcb-m' else ref): reason
            for (board, ref), reason in EXCLUDED.items()}

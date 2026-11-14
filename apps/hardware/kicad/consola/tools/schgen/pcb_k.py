"""Console split key boards: doc 28 GPIO map and Juan's split LED order."""

from .generator import Board, Sheet, Symbol

CHOC = 'gf_consola:Kailh_Choc_V2_PG1353_Hotswap'
LED = 'gf_consola:SK6812MINI-EA_ReverseMount'
R0805 = 'Resistor_SMD:R_0805_2012Metric'
C0805 = 'Capacitor_SMD:C_0805_2012Metric'
TP = 'TestPoint:TestPoint_Pad_D1.5mm'
LED_ORDER = ('BACK', 'MENU', 'NAV_L', 'NAV_R', 'BROWSE', 'TAP', 'FILL', 'CUE_ENG',
             'BUILD', 'DROP', 'BREAK', 'SHIFT', 'UNDO', 'DELETE', 'DUPLICATE',
             'QUANTIZE', 'NEW', 'PLAY', 'STOP', 'REC', 'LOOP', 'METRO',
             'SCENE4_PAGE_R', 'SCENE3_PAGE_L', 'SCENE2_OCT-', 'SCENE1_OCT+')
GPIO = {
    0x21: {'GPA0': 'SCENE1_OCT+', 'GPA1': 'SCENE2_OCT-', 'GPA2': 'SCENE4_PAGE_R',
           'GPA3': 'SCENE3_PAGE_L', 'GPB0': 'METRO', 'GPB1': 'LOOP', 'GPB2': 'REC',
           'GPB3': 'STOP', 'GPB4': 'PLAY'},
    0x22: {'GPA0': 'UNDO', 'GPA1': 'BACK', 'GPA2': 'MENU', 'GPA3': 'NAV_L',
           'GPA4': 'NAV_R', 'GPA5': 'BROWSE', 'GPA6': 'SHIFT', 'GPB0': 'DELETE',
           'GPB1': 'DUPLICATE', 'GPB2': 'QUANTIZE', 'GPB3': 'NEW'},
    0x23: {'GPA0': 'CUE_ENG', 'GPA1': 'FILL', 'GPA2': 'TAP', 'GPA3': 'BUILD',
           'GPA4': 'DROP', 'GPA5': 'BREAK', 'GPA6': 'JOG_CLICK', 'GPB0': 'PEDAL_BTN'},
}
GPIO_PINS = {**{f'GPA{i}': str(21+i) for i in range(8)},
             **{f'GPB{i}': str(1+i) for i in range(8)}}
LINK = ('+3V3', 'GND', 'SDA', 'SCL', '+5V_LED', 'LED_DATA')
MAIN = ('+3V3', 'SDA', 'GND', 'SCL', 'GND', 'LED_DIN_5V', 'GND', '+5V_LED', 'JOG_CLICK', 'PEDAL_BTN')


def part(lib_id, reference, value, footprint, pins, dnp=False, lcsc=None):
    """Create an unplaced declarative part for the sheet packer."""
    return Symbol(lib_id, reference, value, footprint, (0, 0), pins, dnp=dnp,
                  fields={"LCSC": lcsc} if lcsc else {})


def packed(name, symbols, notes=()):
    """Pack small support parts with room for labels; validated by schgen."""
    sheets = []
    for start in range(0, len(symbols), 12):
        chunk = symbols[start:start+12]
        for i, symbol in enumerate(chunk):
            symbol.position = (76.2 + (i % 3) * 114.3, 63.5 + (i // 3) * 50.8)
        sheets.append(Sheet(f'{name}-{start//12+1}', chunk, notes=list(notes)))
    return sheets


def connector(ref, nets, value):
    """Use PH connectors with physical pin numbers in cable order."""
    n = len(nets)
    return part(f'Connector_Generic:Conn_01x{n:02}', ref, value,
                f'Connector_JST:JST_PH_B{n}B-PH-K_1x{n:02}_P2.00mm_Vertical',
                {str(i): net for i, net in enumerate(nets, 1)})


def capacitor(ref, value, rail, ground='GND'):
    return part('Device:C', ref, value, C0805 if value != '22uF' else
                'Capacitor_SMD:C_1206_3216Metric', {'1': rail, '2': ground},
                lcsc={'100nF': 'C49678', '1uF': 'C5137478', '22uF': 'C12891'}[value])


def testpoint(ref, net):
    return part('Connector:TestPoint', ref, net, TP, {'1': net})


def mcp(address):
    """One MCP, its required support and all available expansion/interrupt pads."""
    tag = f'{address:02X}'
    pins = {'8': None, '9': '+3V3', '10': 'GND', '11': None, '12': 'SCL',
            '13': 'SDA', '14': None, '15': '+3V3' if address & 1 else 'GND',
            '16': '+3V3' if address & 2 else 'GND', '17': 'GND',
            '18': f'RESET_{tag}', '19': f'INTB_{tag}', '20': f'INTA_{tag}', '28': None}
    support = [part('Device:R', f'R{tag}', '10k', R0805,
                    {'1': '+3V3', '2': f'RESET_{tag}'}, lcsc='C17414'),
               capacitor(f'C{tag}1', '100nF', '+3V3'),
               capacitor(f'C{tag}2', '1uF', '+3V3'),
               testpoint(f'TP{tag}20', f'INTA_{tag}'),
               testpoint(f'TP{tag}19', f'INTB_{tag}')]
    for bank in ('A', 'B'):
        for i in range(7):
            gpio = f'GP{bank}{i}'
            net = GPIO[address].get(gpio, f'SPARE_{tag}_{gpio}')
            pins[GPIO_PINS[gpio]] = net
            if gpio not in GPIO[address]:
                support.append(testpoint(f'TP{tag}{GPIO_PINS[gpio]}', net))
    chip = part('Interface_Expansion:MCP23017x-x-SS', f'U{tag}',
                f'MCP23017-E/SS 0x{tag}', 'Package_SO:SSOP-28_5.3x10.2mm_P0.65mm', pins,
                lcsc='C506653')
    chip.position = (190.5, 127)
    return [Sheet(f'mcp-{tag}', [chip], notes=[
        f'0x{tag}: GPA7 / GPB7 NC (errata). Pull-ups internos por firmware.',
        'INTA/INTB y entradas libres a pads. Consultar hoja support.'])] + packed(f'support-{tag}', support)


def pcb_k(side):
    """Return left (17 keys) or right (9 keys), preserving GPIO and split-chain contracts."""
    if side not in ('left', 'right'):
        raise ValueError('side must be left or right')
    left = side == 'left'
    indices = range(17) if left else range(17, 26)
    symbols = [connector('J1', MAIN if left else LINK, 'PCB-M' if left else 'DESDE IZQUIERDA')]
    if left:
        symbols[0].fields['LCSC'] = 'C158038'
        symbols.append(connector('J2', LINK, 'HACIA DERECHA'))
    symbols += [part('Device:R', 'R1', '33R', R0805,
                     {'1': 'LED_DIN_5V' if left else 'LED_DATA', '2': f'LED_{indices.start}_DIN'}, lcsc='C17634'),
                capacitor('C1', '22uF', '+5V_LED')]
    for index, signal in enumerate(('SDA', 'SCL'), 2):
        symbols.append(part('Device:R', f'R{index}', '2.2k DNP', R0805,
                            {'1': '+3V3', '2': signal}, dnp=True, lcsc='C17520'))
    for i, rail in enumerate(('+3V3', '+5V_LED', 'GND')):
        symbols.append(part('power:PWR_FLAG', f'#FLG{i+1:02}', 'PWR_FLAG', '', {'1': rail}))
    # Off-board SCL/DIN sources are electrically asserted; SDA is bidirectional on MCP.
    for i, signal in enumerate(('SCL', 'LED_DIN_5V' if left else 'LED_DATA'), 4):
        symbols.append(part('power:PWR_FLAG', f'#FLG{i:02}', 'PWR_FLAG', '', {'1': signal}))
    sheets = packed('cables-power', symbols, [
        'PH 1:1. R2/R3 DNP: pull-ups activos solo en PCB-M.',
        'PWR_FLAG: alimentacion y senales conducidas por PCB-M / placa izquierda.'])
    for address in ((0x22, 0x23) if left else (0x21,)):
        sheets += mcp(address)
    # Four keys per page: left to right SW, LED, local decoupling; rows follow chain order.
    for start in range(indices.start, indices.stop, 4):
        keys = []
        for row, i in enumerate(range(start, min(start+4, indices.stop))):
            key = LED_ORDER[i]
            output = ('LED_DATA' if i == 16 else None if i == 25 else f'LED_{i+1}_DIN')
            triple = [part('Switch:SW_Push', f'SW{i+1}', key, CHOC, {'1': key, '2': 'GND'}),
                      part('LED:SK6812MINI-E', f'D{i+1}', 'SK6812MINI-EA', LED,
                           {'1': 'GND', '2': f'LED_{i}_DIN', '3': '+5V_LED', '4': output}, lcsc='C5378731'),
                      capacitor(f'C{100+i}', '100nF', '+5V_LED')]
            for col, symbol in enumerate(triple):
                symbol.position = (76.2 + col*114.3, 63.5 + row*50.8)
            keys += triple
        sheets.append(Sheet(f'keys-{start:02}', keys, notes=[
            'SW arriba, zocalo integrado abajo. LED inverso abajo. Orden nuevo PCB-K partida.']))
    from .bom import populate_fields
    populate_fields(sheets)
    return Board(f'pcb-k-{side}', sheets)


def pcb_k_left():
    return pcb_k('left')


def pcb_k_right():
    return pcb_k('right')

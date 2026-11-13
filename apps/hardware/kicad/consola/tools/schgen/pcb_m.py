"""PCB-M electrical contract: Juan-approved D8-D37 (2026-11-12) and captured analog front.

None means intentional NC. Cell positions are on a 1.27 mm grid. Remaining source
contradictions are listed in ../REPORT.md; this is schematic capture, not fabrication approval.
"""

from .generator import Board, Sheet, Symbol

PARTS = {'J_T41L': {'lib_id': 'Connector_Generic:Conn_01x24',
            'reference': 'J_T41L',
            'value': 'Teensy41-LeftRow',
            'footprint': 'Connector_PinSocket_2.54mm:PinSocket_1x24_P2.54mm_Vertical'},
 'J_T41R': {'lib_id': 'Connector_Generic:Conn_01x24',
            'reference': 'J_T41R',
            'value': 'Teensy41-RightRow',
            'footprint': 'Connector_PinSocket_2.54mm:PinSocket_1x24_P2.54mm_Vertical'},
 'R_PAD_L1': {'lib_id': 'Device:R',
              'reference': 'R_PAD_L1',
              'value': '33k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_PAD_L2': {'lib_id': 'Device:R',
              'reference': 'R_PAD_L2',
              'value': '12k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_PAD_L3': {'lib_id': 'Device:R',
              'reference': 'R_PAD_L3',
              'value': '5.1k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'SW_PAD': {'lib_id': 'Switch:SW_DP3T',
            'reference': 'SW_PAD',
            'value': 'PAD 3P3T A/B',
            'footprint': 'Connector_PinHeader_2.54mm:PinHeader_2x04_P2.54mm_Vertical'},
 'C_AC_L': {'lib_id': 'Device:C',
            'reference': 'C_AC_L',
            'value': '1uF',
            'footprint': 'Capacitor_SMD:C_0805_2012Metric'},
 'R_SER_L': {'lib_id': 'Device:R',
             'reference': 'R_SER_L',
             'value': '1k',
             'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_BIAS_L': {'lib_id': 'Device:R',
              'reference': 'R_BIAS_L',
              'value': '100k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'D_CLAMP_L': {'lib_id': 'Diode:BAT54S',
               'reference': 'D_CLAMP_L',
               'value': 'BAT54S',
               'footprint': 'Package_TO_SOT_SMD:SOT-23'},
 'JP_BYP_L': {'lib_id': 'Jumper:SolderJumper_2_Open',
              'reference': 'JP_BYP_L',
              'value': 'BYP L (sin U_OA_IN)',
              'footprint': 'Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm'},
 'U_OA_IN': {'lib_id': 'Amplifier_Operational:MCP6004',
             'reference': 'U_OA_IN',
             'value': 'MCP6004',
             'footprint': 'Package_SO:SOIC-14_3.9x8.7mm_P1.27mm'},
 'TP_BUF_L': {'lib_id': 'Connector:TestPoint',
              'reference': 'TP_BUF_L',
              'value': 'BUF_L',
              'footprint': 'TestPoint:TestPoint_Pad_D1.5mm'},
 'C_LIN_L': {'lib_id': 'Device:C',
             'reference': 'C_LIN_L',
             'value': '4.7uF',
             'footprint': 'Capacitor_SMD:C_0805_2012Metric'},
 'R_PAD_R1': {'lib_id': 'Device:R',
              'reference': 'R_PAD_R1',
              'value': '33k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_PAD_R2': {'lib_id': 'Device:R',
              'reference': 'R_PAD_R2',
              'value': '12k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_PAD_R3': {'lib_id': 'Device:R',
              'reference': 'R_PAD_R3',
              'value': '5.1k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'C_AC_R': {'lib_id': 'Device:C',
            'reference': 'C_AC_R',
            'value': '1uF',
            'footprint': 'Capacitor_SMD:C_0805_2012Metric'},
 'R_SER_R': {'lib_id': 'Device:R',
             'reference': 'R_SER_R',
             'value': '1k',
             'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_BIAS_R': {'lib_id': 'Device:R',
              'reference': 'R_BIAS_R',
              'value': '100k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'D_CLAMP_R': {'lib_id': 'Diode:BAT54S',
               'reference': 'D_CLAMP_R',
               'value': 'BAT54S',
               'footprint': 'Package_TO_SOT_SMD:SOT-23'},
 'JP_BYP_R': {'lib_id': 'Jumper:SolderJumper_2_Open',
              'reference': 'JP_BYP_R',
              'value': 'BYP R (sin U_OA_IN)',
              'footprint': 'Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm'},
 'TP_BUF_R': {'lib_id': 'Connector:TestPoint',
              'reference': 'TP_BUF_R',
              'value': 'BUF_R',
              'footprint': 'TestPoint:TestPoint_Pad_D1.5mm'},
 'C_LIN_R': {'lib_id': 'Device:C',
             'reference': 'C_LIN_R',
             'value': '4.7uF',
             'footprint': 'Capacitor_SMD:C_0805_2012Metric'},
 'R_VREF_T': {'lib_id': 'Device:R',
              'reference': 'R_VREF_T',
              'value': '10k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_VREF_B': {'lib_id': 'Device:R',
              'reference': 'R_VREF_B',
              'value': '10k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'C_VREF': {'lib_id': 'Device:C',
            'reference': 'C_VREF',
            'value': '10uF',
            'footprint': 'Capacitor_SMD:C_0805_2012Metric'},
 'JP_BYP_VREF': {'lib_id': 'Jumper:SolderJumper_2_Open',
                 'reference': 'JP_BYP_VREF',
                 'value': 'BYP VREF (sin U_OA_IN)',
                 'footprint': 'Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm'},
 'TP_VREF': {'lib_id': 'Connector:TestPoint',
             'reference': 'TP_VREF',
             'value': 'VREF',
             'footprint': 'TestPoint:TestPoint_Pad_D1.5mm'},
 'C_OA_IN': {'lib_id': 'Device:C',
             'reference': 'C_OA_IN',
             'value': '100nF',
             'footprint': 'Capacitor_SMD:C_0603_1608Metric'},
 'C_LOUT_L': {'lib_id': 'Device:C',
              'reference': 'C_LOUT_L',
              'value': '10uF',
              'footprint': 'Capacitor_SMD:C_0805_2012Metric'},
 'R_LOUT_L': {'lib_id': 'Device:R',
              'reference': 'R_LOUT_L',
              'value': '47k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'C_LOUT_R': {'lib_id': 'Device:C',
              'reference': 'C_LOUT_R',
              'value': '10uF',
              'footprint': 'Capacitor_SMD:C_0805_2012Metric'},
 'R_LOUT_R': {'lib_id': 'Device:R',
              'reference': 'R_LOUT_R',
              'value': '47k',
              'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_SEL_T': {'lib_id': 'Device:R',
             'reference': 'R_SEL_T',
             'value': '10k',
             'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'R_SEL_B': {'lib_id': 'Device:R',
             'reference': 'R_SEL_B',
             'value': '10k',
             'footprint': 'Resistor_SMD:R_0603_1608Metric'},
 'U_ADS1': {'lib_id': 'Analog_ADC:ADS1115IDGS',
            'reference': 'U_ADS1',
            'value': 'ADS1115IDGSR',
            'footprint': 'Package_SO:TSSOP-10_3x3mm_P0.5mm'},
 'U_ADS2': {'lib_id': 'Analog_ADC:ADS1115IDGS',
            'reference': 'U_ADS2',
            'value': 'ADS1115IDGSR',
            'footprint': 'Package_SO:TSSOP-10_3x3mm_P0.5mm'},
 'U_DAC1': {'lib_id': 'Analog_DAC:MCP4725xxx-xCH',
            'reference': 'U_DAC1',
            'value': 'MCP4725A0T-E/CH',
            'footprint': ''},
 'U_DAC2': {'lib_id': 'Analog_DAC:MCP4725xxx-xCH',
            'reference': 'U_DAC2',
            'value': 'MCP4725A0T-E/CH',
            'footprint': ''},
 'U_OA_CV': {'lib_id': 'Amplifier_Operational:MCP6004',
             'reference': 'U_OA_CV',
             'value': 'MCP6004',
             'footprint': ''},
 'J_FSR1': {'lib_id': 'Connector_Generic:Conn_01x02',
            'reference': 'J_FSR1',
            'value': 'JST-PH_2pin_FSR1',
            'footprint': ''},
 'J_FSR2': {'lib_id': 'Connector_Generic:Conn_01x02',
            'reference': 'J_FSR2',
            'value': 'JST-PH_2pin_FSR2',
            'footprint': ''},
 'J_FSR3': {'lib_id': 'Connector_Generic:Conn_01x02',
            'reference': 'J_FSR3',
            'value': 'JST-PH_2pin_FSR3',
            'footprint': ''},
 'J_FSR4': {'lib_id': 'Connector_Generic:Conn_01x02',
            'reference': 'J_FSR4',
            'value': 'JST-PH_2pin_FSR4',
            'footprint': ''},
 'R_FSR1': {'lib_id': 'Device:R', 'reference': 'R_FSR1', 'value': '10k', 'footprint': ''},
 'R_FSR2': {'lib_id': 'Device:R', 'reference': 'R_FSR2', 'value': '10k', 'footprint': ''},
 'R_FSR3': {'lib_id': 'Device:R', 'reference': 'R_FSR3', 'value': '10k', 'footprint': ''},
 'R_FSR4': {'lib_id': 'Device:R', 'reference': 'R_FSR4', 'value': '10k', 'footprint': ''},
 'J_JOG': {'lib_id': 'Connector_Generic:Conn_01x04',
           'reference': 'J_JOG',
           'value': 'JOG_CABLE',
           'footprint': ''},
 'J_POT_MASTER': {'lib_id': 'Connector_Generic:Conn_01x03',
                  'reference': 'J_POT_MASTER',
                  'value': 'POT_MASTER_CABLE',
                  'footprint': ''},
 'J_POT_CUE': {'lib_id': 'Connector_Generic:Conn_01x03',
               'reference': 'J_POT_CUE',
               'value': 'POT_CUE_CABLE',
               'footprint': ''},
 'J_POT_IN': {'lib_id': 'Connector_Generic:Conn_01x03',
              'reference': 'J_POT_IN',
              'value': 'POT_IN_CABLE',
              'footprint': ''},
 'J_PAD_SEL': {'lib_id': 'Connector_Generic:Conn_01x03',
               'reference': 'J_PAD_SEL',
               'value': 'PAD_SEL_CABLE',
               'footprint': ''},
 'J_HP': {'lib_id': 'Connector_Generic:Conn_01x03',
          'reference': 'J_HP',
          'value': 'HP_CABLE',
          'footprint': ''},
 'U_OPT_MIDI': {'lib_id': 'Isolator:6N138',
                'reference': 'U_OPT_MIDI',
                'value': '6N138',
                'footprint': ''},
 'J_MIDI_IN': {'lib_id': 'Connector_Generic:Conn_01x03',
                'reference': 'J_MIDI_IN', 'value': 'MIDI_IN_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical'},
 'J_MIDI_OUT': {'lib_id': 'Connector_Generic:Conn_01x03',
                'reference': 'J_MIDI_OUT', 'value': 'MIDI_OUT_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical'},
 'J1_PCBA': {'lib_id': 'Connector_Generic:Conn_02x15_Odd_Even',
             'reference': 'J1_PCBA',
             'value': 'IDC_2x15_PCB-A',
             'footprint': ''},
 'J2_I2C': {'lib_id': 'Connector_Generic:Conn_01x04',
            'reference': 'J2_I2C',
            'value': 'JST-SH_4pin_I2C',
            'footprint': ''},
 'J3_M4': {'lib_id': 'Connector_Generic:Conn_01x04',
           'reference': 'J3_M4',
           'value': 'JST-PH_4pin_M4',
           'footprint': ''},
 'J5_SHARP1': {'lib_id': 'Connector_Generic:Conn_01x03',
               'reference': 'J5_SHARP1',
               'value': 'JST-PH_3pin_Sharp1',
               'footprint': ''},
 'J6_SHARP2': {'lib_id': 'Connector_Generic:Conn_01x03',
               'reference': 'J6_SHARP2',
               'value': 'JST-PH_3pin_Sharp2',
               'footprint': ''},
 'J7_EXP1': {'lib_id': 'Connector_Generic:Conn_01x02',
             'reference': 'J7_EXP1',
             'value': 'JST-PH_2pin_EXP1',
             'footprint': ''},
 'J8_EXP2': {'lib_id': 'Connector_Generic:Conn_01x02',
             'reference': 'J8_EXP2',
             'value': 'JST-PH_2pin_EXP2',
             'footprint': ''},
 'J_PCB_K': {'lib_id': 'Connector_Generic:Conn_01x10',
             'reference': 'J_PCB_K',
             'value': 'PCB_K_KEY_CABLE',
             'footprint': ''},
 'J_LINE_IN_L': {'lib_id': 'Connector_Generic:Conn_01x03',
                'reference': 'J_LINE_IN_L', 'value': 'LINE_IN_L_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical'},
 'J_LINE_IN_R': {'lib_id': 'Connector_Generic:Conn_01x03',
                'reference': 'J_LINE_IN_R', 'value': 'LINE_IN_R_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical'},
 'J_LINE_OUT_L': {'lib_id': 'Connector_Generic:Conn_01x03',
                'reference': 'J_LINE_OUT_L', 'value': 'LINE_OUT_L_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical'},
 'J_LINE_OUT_R': {'lib_id': 'Connector_Generic:Conn_01x03',
                'reference': 'J_LINE_OUT_R', 'value': 'LINE_OUT_R_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical'},
 'J_CV1': {'lib_id': 'Connector_Generic:Conn_01x02',
                'reference': 'J_CV1', 'value': 'CV1_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical'},
 'J_CV2': {'lib_id': 'Connector_Generic:Conn_01x02',
                'reference': 'J_CV2', 'value': 'CV2_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical'},
 'J_PEDAL': {'lib_id': 'Connector_Generic:Conn_01x03',
                'reference': 'J_PEDAL', 'value': 'PEDAL_PANEL_CABLE',
                'footprint': 'Connector_JST:JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical'},
 'J_EDRUM': {'lib_id': 'Connector_Generic:Conn_01x05',
             'reference': 'J_EDRUM',
             'value': 'EDRUM_RESERVED_DNP',
             'footprint': ''},
 'U_LDO1': {'lib_id': 'Regulator_Linear:TLV75533PDBV',
            'reference': 'U_LDO1',
            'value': 'TLV75533PDBV',
            'footprint': 'Package_TO_SOT_SMD:SOT-23-5'},
 'F1': {'lib_id': 'Device:Polyfuse',
        'reference': 'F1',
        'value': '0.75A MF-MSMF075/24-2',
        'footprint': 'Fuse:Fuse_1812_4532Metric'},
 'J_5V_BKUP': {'lib_id': 'Connector_Generic:Conn_01x02',
               'reference': 'J_5V_BKUP',
               'value': '5V_BACKUP_DNP',
               'footprint': 'Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical'},
 'C_LDO_IN': {'lib_id': 'Device:C',
              'reference': 'C_LDO_IN',
              'value': '10uF',
              'footprint': 'Capacitor_SMD:C_0805_2012Metric'},
 'C_LDO_OUT': {'lib_id': 'Device:C',
               'reference': 'C_LDO_OUT',
               'value': '1uF',
               'footprint': 'Capacitor_SMD:C_0603_1608Metric'},
 'U_LED_BUF': {'lib_id': '74xGxx:74AHCT1G125',
               'reference': 'U_LED_BUF',
               'value': '74AHCT1G125',
               'footprint': ''},
 'R_SDA': {'lib_id': 'Device:R', 'reference': 'R_SDA', 'value': '2.2k', 'footprint': ''},
 'R_SCL': {'lib_id': 'Device:R', 'reference': 'R_SCL', 'value': '2.2k', 'footprint': ''}}

FRONT = [('R_PAD_L1', 1, {'1': 'LINE_IN_L_T', '2': 'TAP10_L'}),
 ('R_PAD_L2', 1, {'1': 'TAP10_L', '2': 'TAP20_L'}),
 ('R_PAD_L3', 1, {'1': 'TAP20_L', '2': 'AGND'}),
 ('SW_PAD', 1, {'1': 'LINE_IN_L_T', '2': 'TAP10_L', '3': 'PAD_L', '4': 'TAP20_L'}),
 ('C_AC_L', 1, {'1': 'PAD_L', '2': 'AC_L'}),
 ('R_SER_L', 1, {'1': 'AC_L', '2': 'PROT_L'}),
 ('R_BIAS_L', 1, {'1': 'PROT_L', '2': 'VREF'}),
 ('D_CLAMP_L', 1, {'1': 'AGND', '2': '+3.3VA', '3': 'PROT_L'}),
 ('JP_BYP_L', 1, {'1': 'PROT_L', '2': 'BUF_L'}),
 ('U_OA_IN', 1, {'1': 'BUF_L', '2': 'BUF_L', '3': 'PROT_L'}),
 ('TP_BUF_L', 1, {'1': 'BUF_L'}),
 ('C_LIN_L', 1, {'1': 'BUF_L', '2': 'SHIELD_LINE_IN_L'}),
 ('R_PAD_R1', 1, {'1': 'LINE_IN_R_T', '2': 'TAP10_R'}),
 ('R_PAD_R2', 1, {'1': 'TAP10_R', '2': 'TAP20_R'}),
 ('R_PAD_R3', 1, {'1': 'TAP20_R', '2': 'AGND'}),
 ('SW_PAD', 2, {'5': 'LINE_IN_R_T', '6': 'TAP10_R', '7': 'PAD_R', '8': 'TAP20_R'}),
 ('C_AC_R', 1, {'1': 'PAD_R', '2': 'AC_R'}),
 ('R_SER_R', 1, {'1': 'AC_R', '2': 'PROT_R'}),
 ('R_BIAS_R', 1, {'1': 'PROT_R', '2': 'VREF'}),
 ('D_CLAMP_R', 1, {'1': 'AGND', '2': '+3.3VA', '3': 'PROT_R'}),
 ('JP_BYP_R', 1, {'1': 'PROT_R', '2': 'BUF_R'}),
 ('U_OA_IN', 2, {'5': 'PROT_R', '6': 'BUF_R', '7': 'BUF_R'}),
 ('TP_BUF_R', 1, {'1': 'BUF_R'}),
 ('C_LIN_R', 1, {'1': 'BUF_R', '2': 'SHIELD_LINE_IN_R'}),
 ('R_VREF_T', 1, {'1': '+3.3VA', '2': 'VREF_DIV'}),
 ('R_VREF_B', 1, {'1': 'VREF_DIV', '2': 'AGND'}),
 ('C_VREF', 1, {'1': 'VREF_DIV', '2': 'AGND'}),
 ('U_OA_IN', 3, {'10': 'VREF_DIV', '8': 'VREF', '9': 'VREF'}),
 ('JP_BYP_VREF', 1, {'1': 'VREF_DIV', '2': 'VREF'}),
 ('TP_VREF', 1, {'1': 'VREF'}),
 ('U_OA_IN', 4, {'12': 'VREF', '13': 'OA_IN_D', '14': 'OA_IN_D'}),
 ('U_OA_IN', 5, {'11': 'AGND', '4': '+3.3VA'}),
 ('C_OA_IN', 1, {'1': '+3.3VA', '2': 'AGND'}),
 ('C_LOUT_L', 1, {'1': 'SHIELD_LINE_OUT_L', '2': 'LINE_OUT_L_T'}),
 ('R_LOUT_L', 1, {'1': 'LINE_OUT_L_T', '2': 'AGND'}),
 ('C_LOUT_R', 1, {'1': 'SHIELD_LINE_OUT_R', '2': 'LINE_OUT_R_T'}),
 ('R_LOUT_R', 1, {'1': 'LINE_OUT_R_T', '2': 'AGND'}),
 ('R_SEL_T', 1, {'1': '+3.3V', '2': 'PAD_SEL_ADC'}),
 ('R_SEL_B', 1, {'1': 'PAD_SEL_ADC', '2': 'GND'})]

RESISTOR = 'Resistor_SMD:R_0402_1005Metric'
CAPACITOR = 'Capacitor_SMD:C_0402_1005Metric'
PH = 'Connector_JST:JST_PH_B{n}B-PH-K_1x{n:02}_P2.00mm_Vertical'


def annotated_reference(reference):
    """KiCad needs a final numeric annotation; retain the function in its own field."""
    return reference if reference[-1].isdigit() else reference + '1'


def pcb_m():
    """Build PCB-M with service/I2C sheets and the intentionally empty reserved sheet 04."""
    sheets = [Sheet(name) for name in (
        '01-teensy', '02-analog-front', '03-adc', '04-mcp-io', '05-cv-dac',
        '06-surface', '07-midi', '08-connectors-panel', '09-connectors-rear', '10-power', '11-diagnostics', '12-i2c-branches')]

    def part(sheet, ref, pins, cell=None, unit=1, lib_id=None, value=None, footprint=None, dnp=False, fields=None):
        data = PARTS.get(ref, {})
        index = len(sheet.symbols)
        column, row = cell if cell is not None else (index % 4, index // 4)
        sheet.symbols.append(Symbol(
            lib_id or data['lib_id'], ref, value or data['value'],
            footprint if footprint is not None else data.get('footprint', ''),
            (round(63.5 + column * 88.9, 2), round(63.5 + row * 63.5, 2)),
            {str(pin): net for pin, net in pins.items()}, unit=unit, dnp=dnp, fields=fields or {}))

    def connector(sheet, ref, nets, cell=None, value=None, footprint=None, dnp=False):
        part(sheet, ref, dict(enumerate(nets, 1)), cell,
             lib_id=PARTS.get(ref, {}).get('lib_id', f'Connector_Generic:Conn_01x{len(nets):02}'),
             value=value, footprint=footprint if footprint is not None else PH.format(n=len(nets)), dnp=dnp)

    def resistor(sheet, ref, value, a, b, cell=None, dnp=False):
        part(sheet, ref, {1: a, 2: b}, cell, lib_id='Device:R', value=value, footprint=RESISTOR, dnp=dnp)

    def capacitor(sheet, ref, value, rail, ground, cell=None):
        part(sheet, ref, {1: rail, 2: ground}, cell, lib_id='Device:C', value=value,
             footprint=CAPACITOR if value == '100nF' else 'Capacitor_SMD:C_0805_2012Metric')

    s = sheets[0]
    connector(s, 'J_T41L', [
        'GND', 'EN_K_GPIO', 'EN_A_GPIO', 'ENC1_A', 'ENC1_B', 'ENC2_A', 'ENC2_B', 'ENC3_A',
        'I2S_TX', 'I2S_RX', 'ENC3_B', 'ENC4_A', 'ENC4_B', 'ENC5_A', '+3.3V', 'POT_CUE',
        None, 'POT_IN', None, 'M4_RX7', 'M4_TX7', 'ENC5_B', 'ENC6_A', 'ENC6_B'], (0, 1),
        footprint=PARTS['J_T41L']['footprint'])
    connector(s, 'J_T41R', [
        '+5V_TEENSY', 'GND', '+3.3V', 'I2S_MCLK', 'POT_MASTER', 'I2S_BCLK', 'I2S_LRCLK',
        'I2C_SCL', 'I2C_SDA', 'FSR4_ADC', 'FSR3_ADC', 'FSR2_ADC', 'FSR1_ADC', 'LED_DATA',
        'GND', 'PAD_SEL_ADC', 'JOG_B', None, None, 'JOG_A', 'ENC8_B', 'ENC8_A',
        'ENC7_B', 'ENC7_A'], (2, 1), footprint=PARTS['J_T41R']['footprint'])
    connector(s, 'J_SHIELD_IN', ['SHIELD_LINE_IN_L', 'SHIELD_LINE_IN_R', 'GND'], (0, 2),
              value='SHIELD pads LINE IN L/R/G')
    connector(s, 'J_SHIELD_OUT', ['SHIELD_LINE_OUT_L', 'SHIELD_LINE_OUT_R', 'GND'], (1, 2),
              value='SHIELD pads LINE OUT L/R/G')
    connector(s, 'J_SHIELD_HP', ['HP_L', 'HP_R', 'HP_VGND'], (2, 2),
              value='HP L/R/VGND - NEVER GND')
    s.notes = ['Teensy + Audio Shield: I2S interno al modulo; no microSD ni pot opcional del shield.',
               'NC: T25/T27/T38/T39 reservados e-drum. HP_VGND nunca se une a GND/AGND.',
               'Filas de sockets conservadas; validar mecanica del montaje antes de fabricar.']

    s = sheets[1]
    s.paper = 'A1'
    for index, (ref, unit, pins) in enumerate(FRONT):
        part(s, ref, {pin: '+3.3VA_FRONT' if net == '+3.3VA' else
                          f'FRONT_LINE_IN_{net[-1]}' if ref in ('C_LIN_L', 'C_LIN_R') and pin == '2' else net
                      for pin, net in pins.items()}, (index % 6, index // 6), unit)
    for col, channel in enumerate(('L', 'R')):
        part(s, f'R_ISO_IN_{channel}', {1: f'FRONT_LINE_IN_{channel}', 2: f'SHIELD_LINE_IN_{channel}'},
             (col, 7), lib_id='Device:R', value='0', footprint='Resistor_SMD:R_0805_2012Metric')
    s.notes = ['Frente analogico de la portadora Studio conservado (doc25 seccion 6).',
               'D13: selector en panel; escalera 10k/10k a PAD_SEL_ADC. Polo C via J_PAD_SEL (3 hilos).',
               'SW_PAD representa polos A/B en panel, cableados al header 2x4; no montar switch en PCB-M.',
               'D17: R_ISO_IN_L/R 0 ohm 0805 separate front from codec; remove both if input fails.']
    sheets[3].notes = ['RESERVADO: MCP23017 0x23 vive en PCB-K. Hoja vacia conservada en la raiz.']

    s = sheets[2]
    for index, inputs in enumerate((['FADER1_SIG', 'FADER2_SIG', 'FADER3_SIG', 'FADER4_SIG'],
                                    ['EXP1_ADC', 'EXP2_ADC', 'SHARP1_ADC', 'SHARP2_ADC']), 1):
        part(s, f'U_ADS{index}', {1: 'GND' if index == 1 else '+3.3VA', 2: None,
             3: 'GND', 4: inputs[0], 5: inputs[1], 6: inputs[2], 7: inputs[3],
             8: '+3.3VA', 9: 'I2C_SDA', 10: 'I2C_SCL'}, (0 if index == 1 else 2, 0))
        capacitor(s, f'C_ADS{index}_VCC', '100nF', '+3.3VA', 'GND', (index - 1, 1))
        capacitor(s, f'C_ADS{index}_BULK', '10uF', '+3.3VA', 'GND', (index + 1, 2))
    s.notes = ['ADS1=0x48 (ADDR=GND); ADS2=0x49 (ADDR=VDD); ALERT/RDY no usado.']

    s = sheets[4]
    s.paper = 'A2'
    opamp_pins = [(1, 2, 3), (7, 6, 5), (8, 9, 10), (14, 13, 12)]
    for channel in (1, 2):
        part(s, f'U_DAC{channel}', {1: f'DAC{channel}_RAW', 2: 'GND', 3: '+3.3V', 4: 'I2C_SDA',
             5: 'I2C_SCL', 6: 'GND' if channel == 1 else '+3.3V'}, (0, channel - 1),
             footprint='Package_TO_SOT_SMD:SOT-23-6')
        resistor(s, f'R_DAC_LIMIT{channel}', '2.2k', f'DAC{channel}_RAW', f'DAC{channel}_LIMITED', (channel + 3, 2))
        out, negative, positive = opamp_pins[channel - 1]
        part(s, 'U_OA_CV', {out: f'CV{channel}_OUT', negative: f'CV{channel}_FB',
             positive: f'DAC{channel}_LIMITED'}, (1, channel - 1), unit=channel,
             footprint='Package_SO:SOIC-14_3.9x8.7mm_P1.27mm')
        resistor(s, f'R_CV_RF{channel}', '10k', f'CV{channel}_OUT', f'CV{channel}_FB', (2, channel - 1))
        resistor(s, f'R_CV_RI{channel}', '20k', f'CV{channel}_FB', 'GND', (3, channel - 1))
        resistor(s, f'R_CV_SER{channel}', '1k', f'CV{channel}_OUT', f'CV{channel}_PROTECTED',
                 (4, channel - 1))
        part(s, f'D_CV_CLAMP{channel}', {1: 'GND', 2: '+5V_CV', 3: f'CV{channel}_PROTECTED'},
             (5, channel - 1), lib_id='Diode:BAT54S', value='BAT54S',
             footprint='Package_TO_SOT_SMD:SOT-23')
    for unit in (3, 4):
        out, negative, positive = opamp_pins[unit - 1]
        part(s, 'U_OA_CV', {out: f'CV_UNUSED_{unit}', negative: f'CV_UNUSED_{unit}', positive: 'GND'},
             (unit - 3, 2), unit=unit, footprint='Package_SO:SOIC-14_3.9x8.7mm_P1.27mm')
    part(s, 'U_OA_CV', {4: '+5V_CV', 11: 'GND'}, (2, 2), unit=5,
         footprint='Package_SO:SOIC-14_3.9x8.7mm_P1.27mm')
    for index, ref in enumerate(('DAC1', 'DAC2', 'OA_CV')):
        rail = '+5V_CV' if ref == 'OA_CV' else '+3.3V'
        capacitor(s, f'C_{ref}_VCC', '100nF', rail, 'GND', (index, 3))
        capacitor(s, f'C_{ref}_BULK', '10uF', rail, 'GND', (index, 4))
    part(s, 'R_CV_FILTER', {1: '+5V_CV_IN', 2: '+5V_CV'}, (3, 3),
         lib_id='Device:R', value='10', footprint='Resistor_SMD:R_0603_1608Metric')
    part(s, 'C_CV_FILTER', {1: '+5V_CV', 2: 'GND'}, (3, 4),
         lib_id='Device:C_Polarized', value='47uF 16V',
         footprint='Capacitor_SMD:CP_Elec_5x5.4')
    s.notes = ['CV: 1 + 10k/20k = 1.5; A0 DAC1=GND (0x60), DAC2=3V3 (0x61).',
               'D11: realimentacion antes de 1k serie; BAT54S en nodo protegido hacia jack, a GND/+5V_CV (D38).',
               'D38: +5V_TEENSY -> SJ_CV + RC 10 ohm / 47uF -> +5V_CV; swing 4.95V ideal, medir margen/carga.']

    s = sheets[5]
    s.paper = 'A1'
    for channel in range(1, 5):
        connector(s, f'J_FSR{channel}', ['+3.3V', f'FSR{channel}_ADC'], (channel - 1, 0))
        resistor(s, f'R_FSR{channel}', '10k', f'FSR{channel}_ADC', 'GND', (channel - 1, 1))
    connector(s, 'J_JOG', ['GND', 'JOG_A', 'JOG_B', 'JOG_CLICK'], (0, 2))
    for index, name in enumerate(('MASTER', 'CUE', 'IN')):
        connector(s, f'J_POT_{name}', ['+3.3V', f'POT_{name}_WIPER', 'GND'], (index + 1, 2))
        resistor(s, f'R_POT_{name}', '1k', f'POT_{name}_WIPER', f'POT_{name}', (index + 1, 4))
        capacitor(s, f'C_POT_{name}', '100nF', f'POT_{name}', 'GND', (index + 1, 5))
        resistor(s, f'R_POT_BIAS_{name}', '100k', f'POT_{name}', 'GND', (index + 1, 6))
    connector(s, 'J_PAD_SEL', ['+3.3V', 'PAD_SEL_ADC', 'GND'], (0, 3))
    connector(s, 'J_HP', ['HP_L', 'HP_R', 'HP_VGND'], (2, 3))
    s.notes = ['Componentes de panel por cable. JOG_CLICK via J_PCB_K hacia MCP 0x23.',
               'D13: polo C comun a J_PAD_SEL.2; extremos a .1/.3; contacto central sin cable (1.65V).',
               'D11: pots con RC 1k/100nF del lado ADC. J_HP unico jack HP; HP_VGND aislado de masa.']

    s = sheets[6]
    s.paper = 'A2'
    part(s, 'U_OPT_MIDI', {1: None, 2: 'MIDI_OPTO_A', 3: 'MIDI_IN_TIP', 4: None,
         5: 'GND', 6: 'MIDI_RX', 7: 'MIDI_OPTO_BASE', 8: '+5V_TEENSY'}, (1, 0),
         footprint='Package_DIP:DIP-8_W7.62mm')
    connector(s, 'J_MIDI_IN', ['MIDI_IN_TIP', 'MIDI_IN_RING', None], (0, 0))
    connector(s, 'J_MIDI_OUT', ['MIDI_OUT_TIP', 'MIDI_OUT_RING', 'GND'], (0, 2))
    resistor(s, 'R_MIDI_IN', '220', 'MIDI_IN_RING', 'MIDI_OPTO_A', (2, 0))
    resistor(s, 'R_MIDI_RX', '4.7k', '+3.3V', 'MIDI_RX', (3, 0))
    resistor(s, 'R_MIDI_BASE', '10k', 'MIDI_OPTO_BASE', 'GND', (2, 1))
    part(s, 'D_MIDI_IN', {1: 'MIDI_OPTO_A', 2: 'MIDI_IN_TIP'}, (3, 1),
         lib_id='Device:D', value='1N4148W', footprint='Diode_SMD:D_SOD-123')
    part(s, 'R_MIDI_T', {1: 'MIDI_TX', 2: 'MIDI_OUT_TIP'}, (2, 2),
         lib_id='Device:R', value='10', footprint='Resistor_SMD:R_1206_3216Metric')
    part(s, 'R_MIDI_R', {1: '+3.3V', 2: 'MIDI_OUT_RING'}, (3, 2),
         lib_id='Device:R', value='33', footprint='Resistor_SMD:R_1210_3225Metric')
    capacitor(s, 'C_MIDI_VCC', '100nF', '+5V_TEENSY', 'GND', (1, 1))
    s.notes = ['MIDI TRS Type A: ring=fuente, tip=sumidero; entrada aislada, sleeve IN NC.',
               'D12 CA-033 (2014): R_MIDI_R=33 ohm >=0.5W; R_MIDI_T=10 ohm >=0.25W; sin 100 ohm extra.',
               'JST-PH hacia jacks de panel: 1=tip, 2=ring, 3=sleeve. Sin deteccion de insercion.']

    for symbol in s.symbols:
        symbol.dnp = True
        symbol.in_bom = False
    s.notes.append('D20: ALL DNP / excluded assembly BOM; USB MIDI on Pi, no MIDI panel holes. Teensy 0=EN_K; 1=EN_A.')

    s = sheets[7]
    part(s, 'J1_PCBA', dict(enumerate([
        '+5V_LED', '+5V_LED', 'GND', 'GND', 'ENC1_A', 'GND', 'ENC1_B', 'ENC5_A',
        'ENC2_A', 'ENC2_B', 'FADER4_SIG', 'FADER3_SIG', 'ENC3_A', 'FADER2_SIG',
        'ENC3_B', 'FADER1_SIG', 'ENC4_A', None, 'ENC4_B', None, 'ENC5_B', None,
        'ENC6_A', 'ENC8_B', 'ENC6_B', None, 'ENC8_A', 'ENC7_A', 'ENC7_B', '+3.3V'], 1)),
        (0, 0), footprint='Connector_IDC:IDC-Header_2x15_P2.54mm_Vertical')
    connector(s, 'J2_I2C', ['GND', '+3.3V', 'I2C_SDA_A', 'I2C_SCL_A'], (1, 0),
              footprint='Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal')
    connector(s, 'J3_M4', ['GND', None, 'M4_RX7_CABLE', 'M4_TX7_CABLE'], (2, 0))
    for index, direction in enumerate(('RX', 'TX')):
        part(s, f'R_M4_{direction}', {1: f'M4_{direction}7', 2: f'M4_{direction}7_CABLE'},
             (index + 2, 3), lib_id='Device:R', value='1k',
             footprint='Resistor_SMD:R_0805_2012Metric',
             fields={'Placement': 'D39: place next to J3_M4; limits current, not Ioff isolation'})
    connector(s, 'J_PCB_K', ['+3.3V', 'I2C_SDA_K', 'GND', 'I2C_SCL_K', 'GND',
              'LED_DIN_5V', 'GND', '+5V_LED_K', 'JOG_CLICK', 'PEDAL_BTN'], (3, 0))
    s.symbols[-1].fields['LCSC'] = 'C158038'
    for index in (1, 2):
        connector(s, f'J{index+4}_SHARP{index}', ['+5V_LED_SHARP', f'SHARP{index}_RAW', 'GND'], (index - 1, 1))
        connector(s, f'J{index+6}_EXP{index}', ['+3.3V', f'EXP{index}_ADC'], (index + 1, 1))
        resistor(s, f'R_EXP{index}', '10k', f'EXP{index}_ADC', 'GND', (index + 1, 2))
        capacitor(s, f'C_SHARP{index}', '10uF', '+5V_LED_SHARP', 'GND', (index - 1, 2))
        resistor(s, f'R_SHARP_LIMIT{index}', '10k', f'SHARP{index}_RAW', f'SHARP{index}_ADC', (index - 1, 3))
    s.notes = ['D8: J1 segun PCB-A fabricada; NC 18(spare)/20(INT_K)/22(INT_A)/26(LED_DIN).',
               'Encoders a Teensy segun firmware; faders al ADS0x48 AIN0-3. LED solo via J_PCB_K.',
               'I2C hacia PCB-A por J2; J1 no lleva SDA/SCL. Retirar pull-ups PCB-A.']

    s = sheets[8]
    for channel, row in (('L', 0), ('R', 1)):
        connector(s, f'J_LINE_IN_{channel}', [f'LINE_IN_{channel}_T', f'LINE_IN_{channel}_R', 'AGND'],
                  (0, row))
        connector(s, f'J_LINE_OUT_{channel}', [f'LINE_OUT_{channel}_T', None, 'AGND'], (1, row))
        number = 1 if channel == 'L' else 2
        connector(s, f'J_CV{number}', [f'CV{number}_PROTECTED', 'AGND'], (2, row))
    connector(s, 'J_PEDAL', ['PEDAL_RAW', None, 'GND'], (1, 2))
    resistor(s, 'R_PEDAL_SER', '1k', 'PEDAL_RAW', 'PEDAL_BTN', (0, 3))
    resistor(s, 'R_PEDAL_PU', '10k', '+3.3V', 'PEDAL_BTN', (1, 3))
    capacitor(s, 'C_PEDAL', '10nF', 'PEDAL_BTN', 'GND', (2, 3))
    part(s, 'D_PEDAL_ESD', {1: 'PEDAL_BTN', 2: 'GND'}, (3, 3),
         lib_id='Device:D_Zener', value='PESD3V3U1UL,315', footprint='gf_consola:D_SOD-882_K1')
    connector(s, 'J_EDRUM', [None] * 5, (2, 2), dnp=True)
    s.notes = ['D15: jacks con tuerca en pared trasera; JST-PH en borde PCB-M, uno por jack.',
               'TRS: 1=tip/2=ring/3=sleeve; TS CV: 1=tip/2=sleeve. HP solo por J_HP (06).',
               'M1: TVS unidirectional K=1 at PEDAL_BTN / A=2 at GND; cathode K marked on footprint.',
               'Captura anterior sin contactos de deteccion; no se agrega deteccion de insercion.',
               'Anillos LINE IN expuestos como redes locales; terminacion balanceada sin definir.']

    s = sheets[9]
    s.paper = 'A2'
    part(s, 'U_LDO1', {1: '+5V_TEENSY', 2: 'AGND', 3: '+5V_TEENSY', 4: None, 5: '+3.3VA'}, (1, 0))
    part(s, 'F1', {1: '+5V_LED_RAW', 2: '+5V_LED_FUSED'}, (0, 0))
    part(s, 'Q_LED_REVERSE', {1: 'GND', 2: '+5V_LED', 3: '+5V_LED_FUSED'}, (4, 0),
         lib_id='Transistor_FET:AO3401A', value='AO3401A', footprint='Package_TO_SOT_SMD:SOT-23')
    part(s, 'D_LED_TVS', {1: '+5V_LED', 2: 'GND'}, (4, 1),
         lib_id='Device:D_Zener', value='SMAJ5.0A', footprint='gf_consola:D_SMA_K1')
    resistor(s, 'R_LED_BIAS', '100k', 'LED_DATA', 'GND', (4, 2))
    connector(s, 'J_5V_LED_IN', ['+5V_LED_RAW', 'GND'], (0, 2), value='USB-A_PI_5V_LED_IN',
              footprint='Connector_JST:JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal')
    part(s, 'NT1', {1: 'AGND', 2: 'GND'}, (1, 2), lib_id='Device:NetTie_2', value='NetTie_2',
         footprint='NetTie:NetTie-2_SMD_Pad0.5mm')
    connector(s, 'J_5V_BKUP', ['+5V_LED_RAW', 'GND'], (0, 1), dnp=True,
              footprint=PARTS['J_5V_BKUP']['footprint'])
    capacitor(s, 'C_LDO_IN', '10uF', '+5V_TEENSY', 'AGND', (2, 0))
    part(s, 'C_LDO_OUT', {1: '+3.3VA', 2: 'AGND'}, (3, 0), footprint=CAPACITOR)
    part(s, 'U_LED_BUF', {1: 'GND', 2: 'LED_DATA', 3: 'GND', 4: 'LED_BUF_OUT', 5: '+5V_LED'}, (1, 1),
         footprint='Package_TO_SOT_SMD:SOT-353_SC-70-5')
    resistor(s, 'R_SDA', '2.2k', 'PU_SDA_22', 'I2C_SDA', (2, 1))
    resistor(s, 'R_SCL', '2.2k', 'PU_SCL_22', 'I2C_SCL', (3, 1))
    capacitor(s, 'C_LED_BUF', '100nF', '+5V_LED', 'GND', (2, 2))
    for index, rail in enumerate(('+5V_LED_RAW', '+5V_LED', '+5V_TEENSY', '+3.3V', 'GND', 'AGND')):
        part(s, f'#FLG{index+1:03}', {1: rail}, (index % 4, 3 + index // 4),
             lib_id='power:PWR_FLAG', value='PWR_FLAG', footprint='')
    s.notes = ['LDO from +5V_TEENSY -> +3.3VA; +3.3V desde Teensy; +5V_LED separado de +5V_TEENSY. D10: NT1 junto a LDO, HP_VGND aislada.',
               'D33: USB Pi -> F1 1812 -> PMOS D/S -> +5V_LED; SMAJ5.0A K1=rail/A2=GND; cathode K marked. Backup DNP.',
               'D14: pull-ups 2.2k en PCB-M; desoldar R1/R2 de PCB-A. Unica union AGND/GND por NT1.']
    s = sheets[10]
    s.paper = 'A1'

    def service(ref, pins, lib_id, value, footprint, dnp=False):
        index = len(s.symbols)
        part(s, ref, pins, (index % 6, index // 6), lib_id=lib_id, value=value,
             footprint=footprint, dnp=dnp)

    def jumper(ref, a, b, closed=True):
        state = 'Bridged' if closed else 'Open'
        service(ref, {1: a, 2: b}, f'Jumper:SolderJumper_2_{state}', ref,
                f'Jumper:SolderJumper-2_P1.3mm_{state}_RoundedPad1.0x1.5mm')

    for ref, a, b in (
            ('SJ_FRONT', '+3.3VA', '+3.3VA_FRONT'),
            ('SJ_CV', '+5V_TEENSY', '+5V_CV_IN'),
            ('SJ_LED_K', '+5V_LED', '+5V_LED_K'),
            ('SJ_SHARP', '+5V_LED', '+5V_LED_SHARP'),
            ('SJ_SDA_A', 'I2C_SDA_B_A', 'I2C_SDA_A_SER'),
            ('SJ_SCL_A', 'I2C_SCL_B_A', 'I2C_SCL_A_SER'),
            ('SJ_SDA_K', 'I2C_SDA_B_K', 'I2C_SDA_K_SER'),
            ('SJ_SCL_K', 'I2C_SCL_B_K', 'I2C_SCL_K_SER'),
            ('SJ_LED_DATA', 'LED_BUF_OUT', 'LED_DIN_5V')):
        jumper(ref, a, b)
    for signal in ('SDA', 'SCL'):
        jumper(f'SJ_PU_{signal}_22', '+3.3V', f'PU_{signal}_22')
        jumper(f'SJ_PU_{signal}_47', '+3.3V', f'PU_{signal}_47', closed=False)
        service(f'R_{signal}_47', {1: f'PU_{signal}_47', 2: f'I2C_{signal}'},
                'Device:R', '4.7k', 'Resistor_SMD:R_0603_1608Metric')
    for name, rail, ground, resistance in (
            ('3V3', '+3.3V', 'GND', '1.5k'),
            ('3V3A', '+3.3VA', 'AGND', '1.5k'),
            ('5V_LED', '+5V_LED', 'GND', '3.3k')):
        service(f'R_IND_{name}', {1: rail, 2: f'IND_{name}_A'}, 'Device:R', resistance,
                'Resistor_SMD:R_0603_1608Metric')
        service(f'D_IND_{name}', {1: ground, 2: f'IND_{name}_A'}, 'Device:LED', 'RED ~1mA',
                'LED_SMD:LED_0603_1608Metric')
    for ref, net in (
            ('TP_5V_TEENSY', '+5V_TEENSY'), ('TP_5V_RAW', '+5V_LED_RAW'),
            ('TP_5V_LED', '+5V_LED'), ('TP_3V3', '+3.3V'), ('TP_3V3A', '+3.3VA'),
            ('TP_FRONT', '+3.3VA_FRONT'), ('TP_5V_CV', '+5V_CV'),
            ('TP_AGND', 'AGND'), ('TP_GND', 'GND'), ('TP_SDA', 'I2C_SDA'),
            ('TP_SCL', 'I2C_SCL'), ('TP_LED_DIN', 'LED_DIN_5V'),
            ('TP_CV1', 'CV1_OUT'), ('TP_CV2', 'CV2_OUT'),
            ('TP_CV1_JACK', 'CV1_PROTECTED'), ('TP_CV2_JACK', 'CV2_PROTECTED')):
        service(ref, {1: net}, 'Connector:TestPoint', net, 'TestPoint:TestPoint_Pad_D1.5mm')
    for target, a, b in (
            ('BIAS_L', 'PROT_L', 'VREF'), ('BIAS_R', 'PROT_R', 'VREF'),
            ('MIDI_RX', '+3.3V', 'MIDI_RX'), ('MIDI_BASE', 'MIDI_OPTO_BASE', 'GND')):
        service(f'R_OPT_{target}', {1: a, 2: b}, 'Device:R', 'DNP tune parallel',
                'Resistor_SMD:R_0603_1608Metric', dnp=True)
        if target.startswith('MIDI'):
            s.symbols[-1].in_bom = False
    for index, rail in enumerate(('+3.3VA_FRONT', '+5V_CV', '+5V_LED_K', '+5V_LED_SHARP')):
        service(f'#DFT{index+1:03}', {1: rail}, 'power:PWR_FLAG', 'PWR_FLAG', '')
    s.notes = [
        'DFT doc24 section 10. Bridges closed by copper: open copper then resolder to restore.',
        'J1_PCBA has no I2C; A branch is J2_I2C. Branch VCC isolation does not prevent signal backfeed.',
        'Select one pull-up pair: 2.2k CLOSED default / 4.7k OPEN. Remove PCB-A R1/R2; PCB-K pull-ups DNP.',
        'TP_CV1/2 before 1k; TP_CV1/2_JACK after 1k. HP_VGND must remain separate.',
        'R_OPT footprints DNP parallel only; MIDI reserved DNP. No change to MIDI OUT CA-033.']
    s = sheets[11]
    s.paper = 'A1'
    for branch in ('K', 'A'):
        part(s, f'U_I2C_{branch}', {1: '+3.3V', 2: 'I2C_SCL', 3: 'I2C_SDA', 4: 'GND',
             5: f'EN_{branch}', 6: f'I2C_SDA_B_{branch}', 7: f'I2C_SCL_B_{branch}', 8: '+3.3V'},
             lib_id='Logic_LevelTranslator:TCA9517ADGK', value='PCA9517ADP,118',
             footprint='Package_SO:MSOP-8_3x3mm_P0.65mm',
             fields={'LCSC': 'C80799', 'Datasheet': 'https://www.nxp.com/docs/en/data-sheet/PCA9517A.pdf'})
        for port in ('A', 'B'):
            capacitor(s, f'C_I2C_{branch}_{port}', '100nF', '+3.3V', 'GND')
        for signal in ('SDA', 'SCL'):
            part(s, f'R_I2C_{signal}_{branch}', {1: f'I2C_{signal}_{branch}_SER', 2: f'I2C_{signal}_{branch}'},
                 lib_id='Device:R', value='47', footprint='Resistor_SMD:R_0805_2012Metric',
                 fields={'Service': '0 ohm if series damping not required'})
            for suffix, value in (('22', '2.2k'), ('47', '4.7k')):
                jumper(f'SJ_PU_{signal}_{branch}_{suffix}', '+3.3V', f'PU_{signal}_{branch}_{suffix}',
                       closed=suffix == '22')
                service(f'R_PU_{signal}_{branch}_{suffix}',
                        {1: f'PU_{signal}_{branch}_{suffix}', 2: f'I2C_{signal}_B_{branch}'},
                        'Device:R', value, 'Resistor_SMD:R_0805_2012Metric')
            for tag, net in ((f'B_{branch}', f'I2C_{signal}_B_{branch}'),
                             (branch, f'I2C_{signal}_{branch}')):
                service(f'TP_{signal}_{tag}', {1: net}, 'Connector:TestPoint', net,
                        'TestPoint:TestPoint_Pad_D1.5mm')
        part(s, f'D_ESD_I2C_{branch}', {1: f'I2C_SDA_{branch}', 6: f'I2C_SDA_{branch}',
             3: f'I2C_SCL_{branch}', 4: f'I2C_SCL_{branch}', 2: 'GND', 5: '+3.3V'},
             lib_id='Power_Protection:USBLC6-2SC6', value='USBLC6-2SC6',
             footprint='Package_TO_SOT_SMD:SOT-23-6', fields={'LCSC': 'C7519',
             'Datasheet': 'https://www.st.com/resource/en/datasheet/usblc6-2.pdf'})
        capacitor(s, f'C_ESD_I2C_{branch}', '100nF', '+3.3V', 'GND')
    for branch in ('K', 'A'):
        resistor(s, f'R_EN_{branch}', '10k', '+3.3V', f'EN_{branch}')
        jumper(f'SJ_EN_{branch}_GPIO', f'EN_{branch}_GPIO', f'EN_{branch}')
    for name in ('RX', 'TX'):
        service(f'TP_MIDI_{name}', {1: f'MIDI_{name}'}, 'Connector:TestPoint', f'MIDI_{name}',
                'TestPoint:TestPoint_Pad_D1.5mm', dnp=True)
        s.symbols[-1].in_bom = False
    for index, symbol in enumerate(s.symbols):
        symbol.position = (63.5 + (index % 6) * 114.3, 63.5 + (index // 6) * 63.5)
    s.notes = [
        'D21: TCA9517ADGK library graphic/pins = NXP PCA9517ADP C80799 (MSOP8). VCCA=VCCB=3.3V.',
        'A=local bus shared; B_K and B_A separate cables. NEVER connect two B sides. EN changes only while idle.',
        'D30: EN_K=Teensy0, EN_A=Teensy1; separate 10k pull-ups / GPIO jumpers. Change EN only with bus idle.',
        '47 ohm 0805 series per cable line; replace with 0 ohm only after measuring VOL/rise time.',
        'USBLC6-2SC6 C7519: 3.5pF max per line; short GND return near cable connector. PCB-A I2C only via J2.',
        'Pull-ups per segment: 2.2k closed / 4.7k open. Remote pull-ups DNP. No MIDI harness or panel holes.']
    from .bom import populate_fields
    populate_fields(sheets)
    for sheet in sheets:
        for symbol in sheet.symbols:
            if not symbol.reference.startswith('#'):
                symbol.fields['Function'] = symbol.reference
                symbol.reference = annotated_reference(symbol.reference)
    return Board('pcb-m', sheets)

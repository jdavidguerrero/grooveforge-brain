"""PCB-M electrical contract transcribed from README tables and the captured analog front.

None means intentional NC. Cell positions are on a 1.27 mm grid. Unresolved source
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
 'J_SHIELD_AUDIO': {'lib_id': 'Connector_Generic:Conn_01x04',
                    'reference': 'J_SHIELD_AUDIO',
                    'value': 'AudioShieldRevD-LineInOut',
                    'footprint': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical'},
 'J_SHIELD_HP': {'lib_id': 'Connector_Generic:Conn_01x03',
                 'reference': 'J_SHIELD_HP',
                 'value': 'AudioShieldRevD-HP',
                 'footprint': 'Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical'},
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
 'SW_PAD_SEL': {'lib_id': 'Switch:SW_SP3T',
                'reference': 'SW_PAD_SEL',
                'value': 'PAD 3P3T C',
                'footprint': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical'},
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
 'J_MIDI_IN': {'lib_id': 'Connector_Audio:AudioJack3',
               'reference': 'J_MIDI_IN',
               'value': '3.5mm_TRS_MIDI_IN',
               'footprint': ''},
 'J_MIDI_OUT': {'lib_id': 'Connector_Audio:AudioJack3',
                'reference': 'J_MIDI_OUT',
                'value': '3.5mm_TRS_MIDI_OUT',
                'footprint': ''},
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
 'J_PCB_K': {'lib_id': 'Connector_Generic:Conn_01x08',
             'reference': 'J_PCB_K',
             'value': 'PCB_K_KEY_CABLE',
             'footprint': ''},
 'J_LINE_IN_L': {'lib_id': 'Connector_Audio:AudioJack3',
                 'reference': 'J_LINE_IN_L',
                 'value': 'NRJ3HF-1_LINE_IN_L',
                 'footprint': ''},
 'J_LINE_IN_R': {'lib_id': 'Connector_Audio:AudioJack3',
                 'reference': 'J_LINE_IN_R',
                 'value': 'NRJ3HF-1_LINE_IN_R',
                 'footprint': ''},
 'J_LINE_OUT_L': {'lib_id': 'Connector_Audio:AudioJack3',
                  'reference': 'J_LINE_OUT_L',
                  'value': '6.35mm_LINE_OUT_L',
                  'footprint': ''},
 'J_LINE_OUT_R': {'lib_id': 'Connector_Audio:AudioJack3',
                  'reference': 'J_LINE_OUT_R',
                  'value': '6.35mm_LINE_OUT_R',
                  'footprint': ''},
 'J_HP_CUE': {'lib_id': 'Connector_Audio:AudioJack3',
              'reference': 'J_HP_CUE',
              'value': '6.35mm_HP_CUE_front',
              'footprint': ''},
 'J_CV1': {'lib_id': 'Connector_Audio:AudioJack2',
           'reference': 'J_CV1',
           'value': '3.5mm_CV1_OUT',
           'footprint': ''},
 'J_CV2': {'lib_id': 'Connector_Audio:AudioJack2',
           'reference': 'J_CV2',
           'value': '3.5mm_CV2_OUT',
           'footprint': ''},
 'J_PEDAL': {'lib_id': 'Connector_Audio:AudioJack3',
             'reference': 'J_PEDAL',
             'value': '6.35mm_PEDAL_TRS',
             'footprint': ''},
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
        'value': '0.75A MF-MSMF075',
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
 ('SW_PAD_SEL', 1, {'1': 'GND', '2': 'PAD_SEL_MID', '3': 'PAD_SEL_ADC', '4': '+3.3V'}),
 ('R_SEL_T', 1, {'1': '+3.3V', '2': 'PAD_SEL_MID'}),
 ('R_SEL_B', 1, {'1': 'PAD_SEL_MID', '2': 'GND'})]

RESISTOR = 'Resistor_SMD:R_0402_1005Metric'
CAPACITOR = 'Capacitor_SMD:C_0402_1005Metric'
PH = 'Connector_JST:JST_PH_B{n}B-PH-K_1x0{n}_P2.00mm_Vertical'


def pcb_m():
    """Build the nine populated PCB-M sheets plus the intentionally empty reserved sheet 04."""
    sheets = [Sheet(name) for name in (
        '01-teensy', '02-analog-front', '03-adc', '04-mcp-io', '05-cv-dac',
        '06-surface', '07-midi', '08-connectors-panel', '09-connectors-rear', '10-power')]

    def part(sheet, ref, pins, cell=None, unit=1, lib_id=None, value=None, footprint=None, dnp=False):
        data = PARTS.get(ref, {})
        index = len(sheet.symbols)
        column, row = cell if cell is not None else (index % 4, index // 4)
        sheet.symbols.append(Symbol(
            lib_id or data['lib_id'], ref, value or data['value'],
            footprint if footprint is not None else data.get('footprint', ''),
            (round(63.5 + column * 88.9, 2), round(63.5 + row * 63.5, 2)),
            {str(pin): net for pin, net in pins.items()}, unit=unit, dnp=dnp))

    def connector(sheet, ref, nets, cell=None, value=None, footprint=None, dnp=False):
        part(sheet, ref, dict(enumerate(nets, 1)), cell,
             lib_id=PARTS.get(ref, {}).get('lib_id', f'Connector_Generic:Conn_01x{len(nets):02}'),
             value=value, footprint=footprint if footprint is not None else PH.format(n=len(nets)), dnp=dnp)

    def resistor(sheet, ref, value, a, b, cell=None):
        part(sheet, ref, {1: a, 2: b}, cell, lib_id='Device:R', value=value, footprint=RESISTOR)

    def capacitor(sheet, ref, value, rail, ground, cell=None):
        part(sheet, ref, {1: rail, 2: ground}, cell, lib_id='Device:C', value=value,
             footprint=CAPACITOR if value == '100nF' else 'Capacitor_SMD:C_0805_2012Metric')

    s = sheets[0]
    connector(s, 'J_T41L', [
        'GND', 'MIDI_RX', 'MIDI_TX', 'ENC1_A', 'ENC1_B', 'ENC2_A', 'ENC2_B', 'ENC3_A',
        'I2S_TX', 'I2S_RX', 'ENC3_B', 'ENC4_A', 'ENC4_B', 'ENC5_A', '+3.3V', 'POT_CUE',
        None, 'POT_IN', None, 'M4_RX7', 'M4_TX7', 'ENC5_B', 'ENC6_A', 'ENC6_B'], (0, 1),
        footprint=PARTS['J_T41L']['footprint'])
    connector(s, 'J_T41R', [
        '+5V_TEENSY', 'GND', '+3.3V', 'I2S_MCLK', 'POT_MASTER', 'I2S_BCLK', 'I2S_LRCLK',
        'I2C_SCL', 'I2C_SDA', 'FSR4_ADC', 'FSR3_ADC', 'FSR2_ADC', 'FSR1_ADC', 'LED_DATA',
        'GND', 'PAD_SEL_ADC', 'JOG_B', None, None, 'JOG_A', 'ENC8_B', 'ENC8_A',
        'ENC7_B', 'ENC7_A'], (2, 1), footprint=PARTS['J_T41R']['footprint'])
    connector(s, 'J_SHIELD_AUDIO', ['SHIELD_LINE_IN_L', 'SHIELD_LINE_IN_R', 'SHIELD_LINE_OUT_L',
              'SHIELD_LINE_OUT_R'], (0, 2), footprint=PARTS['J_SHIELD_AUDIO']['footprint'])
    connector(s, 'J_SHIELD_HP', ['HP_L', 'HP_R', 'HP_VGND'], (2, 2),
              footprint=PARTS['J_SHIELD_HP']['footprint'])
    s.notes = ['Teensy + Audio Shield: I2S interno al modulo; no microSD ni pot opcional del shield.',
               'NC: T25/T27/T38/T39 reservados e-drum. HP_VGND nunca se une a GND/AGND.',
               'Filas de sockets conservadas; validar mecanica del montaje antes de fabricar.']

    s = sheets[1]
    s.paper = 'A1'
    for index, (ref, unit, pins) in enumerate(FRONT):
        part(s, ref, pins, (index % 6, index // 6), unit)
    s.notes = ['Frente analogico de la portadora Studio, contrato README D4.',
               'D10 abierto: se conservan SW_PAD_SEL + escalera y J_PAD_SEL; no poblar ambos sin decision.']
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
        out, negative, positive = opamp_pins[channel - 1]
        part(s, 'U_OA_CV', {out: f'CV{channel}_OUT', negative: f'CV{channel}_FB',
             positive: f'DAC{channel}_RAW'}, (1, channel - 1), unit=channel,
             footprint='Package_SO:SOIC-14_3.9x8.7mm_P1.27mm')
        resistor(s, f'R_CV_RF{channel}', '10k', f'CV{channel}_OUT', f'CV{channel}_FB', (2, channel - 1))
        resistor(s, f'R_CV_RI{channel}', '20k', f'CV{channel}_FB', 'GND', (3, channel - 1))
    for unit in (3, 4):
        out, negative, positive = opamp_pins[unit - 1]
        part(s, 'U_OA_CV', {out: f'CV_UNUSED_{unit}', negative: f'CV_UNUSED_{unit}', positive: 'GND'},
             (unit - 3, 2), unit=unit, footprint='Package_SO:SOIC-14_3.9x8.7mm_P1.27mm')
    part(s, 'U_OA_CV', {4: '+5V_LED', 11: 'GND'}, (2, 2), unit=5,
         footprint='Package_SO:SOIC-14_3.9x8.7mm_P1.27mm')
    for index, ref in enumerate(('DAC1', 'DAC2', 'OA_CV')):
        rail = '+5V_LED' if ref == 'OA_CV' else '+3.3V'
        capacitor(s, f'C_{ref}_VCC', '100nF', rail, 'GND', (index, 3))
        capacitor(s, f'C_{ref}_BULK', '10uF', rail, 'GND', (index, 4))
    s.notes = ['CV: 1 + 10k/20k = 1.5; A0 DAC1=GND (0x60), DAC2=3V3 (0x61).',
               'Doc24 pide 1k + proteccion de salida; no definidos en README, pendiente de decision.']

    s = sheets[5]
    for channel in range(1, 5):
        connector(s, f'J_FSR{channel}', ['+3.3V', f'FSR{channel}_ADC'], (channel - 1, 0))
        resistor(s, f'R_FSR{channel}', '10k', f'FSR{channel}_ADC', 'GND', (channel - 1, 1))
    connector(s, 'J_JOG', ['GND', 'JOG_A', 'JOG_B', 'JOG_CLICK'], (0, 2))
    for index, name in enumerate(('MASTER', 'CUE', 'IN')):
        connector(s, f'J_POT_{name}', ['+3.3V', f'POT_{name}', 'GND'], (index + 1, 2))
    connector(s, 'J_PAD_SEL', ['+3.3V', 'PAD_SEL_ADC', 'GND'], (0, 3))
    connector(s, 'J_HP', ['HP_L', 'HP_R', 'HP_VGND'], (2, 3))
    s.notes = ['Componentes de panel por cable. JOG_CLICK via J_PCB_K hacia MCP 0x23.',
               'J_PAD_SEL: duplicacion D10 pendiente; HP_VGND aislado de masa.']

    s = sheets[6]
    s.paper = 'A2'
    part(s, 'U_OPT_MIDI', {1: None, 2: 'MIDI_OPTO_A', 3: 'MIDI_IN_TIP', 4: None,
         5: 'GND', 6: 'MIDI_RX', 7: 'MIDI_OPTO_BASE', 8: '+5V_TEENSY'}, (1, 0),
         footprint='Package_DIP:DIP-8_W7.62mm')
    jack_fp = 'Connector_Audio:Jack_3.5mm_CUI_SJ-3523-SMT_Horizontal'
    part(s, 'J_MIDI_IN', {'T': 'MIDI_IN_TIP', 'R': 'MIDI_IN_RING', 'S': None}, (0, 0), footprint=jack_fp)
    part(s, 'J_MIDI_OUT', {'T': 'MIDI_OUT_TIP', 'R': 'MIDI_OUT_RING', 'S': 'GND'}, (0, 2), footprint=jack_fp)
    resistor(s, 'R_MIDI_IN', '220', 'MIDI_IN_RING', 'MIDI_OPTO_A', (2, 0))
    resistor(s, 'R_MIDI_RX', '4.7k', '+3.3V', 'MIDI_RX', (3, 0))
    resistor(s, 'R_MIDI_BASE', '10k', 'MIDI_OPTO_BASE', 'GND', (2, 1))
    part(s, 'D_MIDI_IN', {1: 'MIDI_OPTO_A', 2: 'MIDI_IN_TIP'}, (3, 1),
         lib_id='Device:D', value='1N4148W', footprint='Diode_SMD:D_SOD-123')
    resistor(s, 'R_MIDI_S', '100', 'MIDI_TX', 'MIDI_TX_PROTECTED', (1, 2))
    resistor(s, 'R_MIDI_T', '47', 'MIDI_TX_PROTECTED', 'MIDI_OUT_TIP', (2, 2))
    resistor(s, 'R_MIDI_R', '47', '+3.3V', 'MIDI_OUT_RING', (3, 2))
    capacitor(s, 'C_MIDI_VCC', '100nF', '+5V_TEENSY', 'GND', (1, 1))
    s.notes = ['MIDI TRS Type A: ring=fuente, tip=sumidero; entrada aislada, sleeve IN NC.',
               'R_MIDI_S/T/R materializan D3; polarizacion opto explicita, validar en banco.']

    s = sheets[7]
    part(s, 'J1_PCBA', dict(enumerate([
        '+5V_LED', '+5V_LED', 'GND', 'GND', 'ENC1_A', 'GND', 'ENC1_B', 'ENC5_A',
        'ENC2_A', 'ENC2_B', 'FADER4_SIG', 'FADER3_SIG', 'ENC3_A', 'FADER2_SIG',
        'ENC3_B', 'FADER1_SIG', 'ENC4_A', 'LED_DATA', 'ENC4_B', None, 'ENC5_B', None,
        'ENC6_A', 'ENC8_B', 'ENC6_B', 'LED_DIN_5V', 'ENC8_A', 'ENC7_A', 'ENC7_B', '+3.3V'], 1)),
        (0, 0), footprint='Connector_IDC:IDC-Header_2x15_P2.54mm_Vertical')
    connector(s, 'J2_I2C', ['GND', '+3.3V', 'I2C_SDA', 'I2C_SCL'], (1, 0),
              footprint='Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal')
    connector(s, 'J3_M4', ['GND', None, 'M4_TX7', 'M4_RX7'], (2, 0))
    connector(s, 'J_PCB_K', ['GND', '+3.3V', 'I2C_SDA', 'I2C_SCL', 'LED_DIN_5V',
              '+5V_LED', 'JOG_CLICK', 'PEDAL_BTN'], (3, 0))
    for index in (1, 2):
        connector(s, f'J{index+4}_SHARP{index}', ['+5V_LED', f'SHARP{index}_ADC', 'GND'], (index - 1, 1))
        connector(s, f'J{index+6}_EXP{index}', ['+3.3V', f'EXP{index}_ADC'], (index + 1, 1))
        resistor(s, f'R_EXP{index}', '10k', f'EXP{index}_ADC', 'GND', (index + 1, 2))
        capacitor(s, f'C_SHARP{index}', '10uF', '+5V_LED', 'GND', (index - 1, 2))
    s.notes = ['J1: pinout de captura conservado; NC 20/22 (D1). Tabla README contradictoria: ver REPORT.md.',
               'I2C hacia PCB-A por J2; J1 no lleva SDA/SCL. Retirar pull-ups PCB-A.']

    s = sheets[8]
    for channel, row in (('L', 0), ('R', 1)):
        part(s, f'J_LINE_IN_{channel}', {'T': f'LINE_IN_{channel}_T', 'R': f'LINE_IN_{channel}_R', 'S': 'AGND'},
             (0, row), footprint='Connector_Audio:Jack_6.35mm_Neutrik_NRJ3HF-1_Horizontal')
        part(s, f'J_LINE_OUT_{channel}', {'T': f'LINE_OUT_{channel}_T', 'R': None, 'S': 'AGND'}, (1, row))
        part(s, f'J_CV{1 if channel == "L" else 2}', {'T': f'CV{1 if channel == "L" else 2}_OUT', 'S': 'AGND'},
             (2, row))
    part(s, 'J_HP_CUE', {'T': 'HP_L', 'R': 'HP_R', 'S': 'HP_VGND'}, (0, 2))
    part(s, 'J_PEDAL', {'T': 'PEDAL_BTN', 'R': None, 'S': 'GND'}, (1, 2))
    connector(s, 'J_EDRUM', [None] * 5, (2, 2), dnp=True)
    s.notes = ['D9: LINE OUT despues del capacitor; sleeve HP = HP_VGND, nunca AGND.',
               'Footprints jacks salida/HP/CV/pedal no definidos por README: se conservan vacios.',
               'Anillos LINE IN expuestos como redes locales; terminacion balanceada sin definir.']

    s = sheets[9]
    s.paper = 'A2'
    part(s, 'U_LDO1', {1: '+5V_LED', 2: 'AGND', 3: '+5V_LED', 4: None, 5: '+3.3VA'}, (1, 0))
    part(s, 'F1', {1: '+5V_LED_RAW', 2: '+5V_LED'}, (0, 0), footprint='Fuse:Fuse_1206_3216Metric')
    connector(s, 'J_5V_BKUP', ['+5V_LED_RAW', 'GND'], (0, 1), dnp=True,
              footprint=PARTS['J_5V_BKUP']['footprint'])
    capacitor(s, 'C_LDO_IN', '10uF', '+5V_LED', 'AGND', (2, 0))
    part(s, 'C_LDO_OUT', {1: '+3.3VA', 2: 'AGND'}, (3, 0), footprint=CAPACITOR)
    part(s, 'U_LED_BUF', {1: 'GND', 2: 'LED_DATA', 3: 'GND', 4: 'LED_DIN_5V', 5: '+5V_LED'}, (1, 1),
         footprint='Package_TO_SOT_SMD:SOT-353_SC-70-5')
    resistor(s, 'R_SDA', '2.2k', '+3.3V', 'I2C_SDA', (2, 1))
    resistor(s, 'R_SCL', '2.2k', '+3.3V', 'I2C_SCL', (3, 1))
    capacitor(s, 'C_LED_BUF', '100nF', '+5V_LED', 'GND', (0, 2))
    for index, rail in enumerate(('+5V_LED_RAW', '+5V_LED', '+5V_TEENSY', '+3.3V', 'GND', 'AGND')):
        part(s, f'#FLG{index+1:03}', {1: rail}, (index % 4, 3 + index // 4),
             lib_id='power:PWR_FLAG', value='PWR_FLAG', footprint='')
    s.notes = ['LDO -> +3.3VA (D4). +3.3V desde Teensy; rieles LED y Teensy separados.',
               'PWR_FLAG: fuentes externas/pasivas. AGND/GND sin punto estrella definido (REPORT.md).',
               'USB-A3 alimenta +5V_LED_RAW; conector primario sin definir, J_5V_BKUP DNP conservado.']
    return Board('pcb-m', sheets)

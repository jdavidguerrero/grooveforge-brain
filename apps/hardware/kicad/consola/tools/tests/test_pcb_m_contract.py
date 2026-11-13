"""Approved D8-D15 wiring, checked against independent board/firmware contracts."""

from pathlib import Path
import re

import pytest

from schgen.pcb_m import pcb_m

ROOT = Path(__file__).resolve().parents[6]


@pytest.fixture
def parts():
    return {s.fields.get('Function', s.reference): s for sheet in pcb_m().sheets for s in sheet.symbols}


def test_fabricated_pcba_pinout_and_current_firmware(parts):
    contract = (ROOT / 'apps/hardware/kicad/consola/boards/CONNECTORS.md').read_text()
    table = contract.split('## PCB-A J1 —')[1].split('## PCB-A J2')[0]
    for line in table.splitlines():
        cells = [cell.strip() for cell in line.strip('|').split('|')]
        if len(cells) != 6 or not cells[0].isdigit():
            continue
        for number, name in ((cells[0], cells[1]), (cells[3], cells[4])):
            expected = {'+5V': '+5V_LED', '+3V3': '+3.3V'}.get(name, name)
            if name.startswith('FADER'):
                expected += '_SIG'
            if name.startswith(('—', 'INT_', 'LED_DIN')):
                expected = None
            assert parts['J1_PCBA'].pins[number] == expected, (number, name)

    firmware = (ROOT / 'apps/consola/surface/include/consola_s/ConsolaMap.h').read_text()
    pairs = re.search(r'ENCODER_PINS\[\]\[2\] = \{(.*?)\};', firmware)[1]
    encoder_pins = [tuple(map(int, pair)) for pair in re.findall(r'\{(\d+),(\d+)\}', pairs)]
    # Physical socket order from the Teensy module, independent of logical net names.
    left = [None, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, None,
            24, 25, 26, 27, 28, 29, 30, 31, 32]
    right = [None, None, None, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13,
             None, 41, 40, 39, 38, 37, 36, 35, 34, 33]
    by_teensy = {pin: parts[ref].pins[str(index)]
                 for ref, row in [('J_T41L', left), ('J_T41R', right)]
                 for index, pin in enumerate(row, 1) if pin is not None}
    for encoder, pair in enumerate(encoder_pins, 1):
        for phase, pin in zip('AB', pair):
            assert by_teensy[pin] == (f'ENC{encoder}_{phase}' if encoder <= 8 else f'JOG_{phase}')
    assert parts['U_ADS1'].pins['1'] == 'GND'  # 0x48
    for channel, idc in enumerate((16, 14, 12, 11), 1):
        assert parts['U_ADS1'].pins[str(channel + 3)] == parts['J1_PCBA'].pins[str(idc)] == f'FADER{channel}_SIG'
    assert parts['U_LED_BUF'].pins['2'] == by_teensy[13] == 'LED_DATA'
    assert parts['U_LED_BUF'].pins['4'] == parts['SJ_LED_DATA'].pins['1'] == 'LED_BUF_OUT'
    assert parts['J_PCB_K'].pins['6'] == parts['SJ_LED_DATA'].pins['2'] == 'LED_DIN_5V'
    assert all(parts['J1_PCBA'].pins[str(n)] is None for n in (18, 20, 22, 26))


def test_fused_usb_power_and_unique_ground_tie(parts):
    assert parts['J_5V_LED_IN'].pins == {'1': '+5V_LED_RAW', '2': 'GND'}
    assert 'JST_PH_S2B-PH-K' in parts['J_5V_LED_IN'].footprint
    assert not parts['J_5V_LED_IN'].dnp
    assert parts['F1'].pins == {'1': '+5V_LED_RAW', '2': '+5V_LED_FUSED'}
    assert '0.75A' in parts['F1'].value
    assert parts['J_5V_BKUP'].dnp
    assert parts['U_LDO1'].pins['5'] == '+3.3VA'
    assert parts['NT1'].lib_id == 'Device:NetTie_2'
    assert parts['NT1'].footprint == 'NetTie:NetTie-2_SMD_Pad0.5mm'
    assert parts['NT1'].pins == {'1': 'AGND', '2': 'GND'}
    # Two distinct nets may meet only through the specified passive bridge.
    assert [ref for ref, symbol in parts.items() if {'AGND', 'GND'} <= set(symbol.pins.values())] == ['NT1']
    assert not any({'HP_VGND', 'AGND'} <= set(s.pins.values()) or
                   {'HP_VGND', 'GND'} <= set(s.pins.values()) for s in parts.values())
    for ref, signal in [('R_SDA', 'I2C_SDA'), ('R_SCL', 'I2C_SCL')]:
        assert parts[ref].value == '2.2k'
        assert parts[ref].pins == {'1': f'PU_{signal[4:]}_22', '2': signal}
        assert parts[f'SJ_PU_{signal[4:]}_22'].pins == {'1': '+3.3V', '2': parts[ref].pins['1']}


@pytest.mark.parametrize('channel', (1, 2))
def test_cv_clamps_after_series_resistor(parts, channel):
    raw, protected = f'CV{channel}_OUT', f'CV{channel}_PROTECTED'
    series = parts[f'R_CV_SER{channel}']
    assert series.value == '1k'
    assert series.pins == {'1': raw, '2': protected}
    assert parts[f'R_CV_RF{channel}'].pins['1'] == raw
    clamp = parts[f'D_CV_CLAMP{channel}']
    assert clamp.lib_id == 'Diode:BAT54S'
    assert clamp.pins == {'1': 'GND', '2': '+5V_CV', '3': protected}
    assert parts[f'J_CV{channel}'].pins == {'1': protected, '2': 'AGND'}


@pytest.mark.parametrize('name', ('MASTER', 'CUE', 'IN'))
def test_pot_rc_is_between_cursor_and_adc(parts, name):
    cursor, adc = f'POT_{name}_WIPER', f'POT_{name}'
    assert parts[f'J_POT_{name}'].pins == {'1': '+3.3V', '2': cursor, '3': 'GND'}
    assert parts[f'R_POT_{name}'].value == '1k'
    assert parts[f'R_POT_{name}'].pins == {'1': cursor, '2': adc}
    assert parts[f'C_POT_{name}'].value == '100nF'
    assert parts[f'C_POT_{name}'].pins == {'1': adc, '2': 'GND'}
    ref, pin = {'MASTER': ('J_T41R', '5'), 'CUE': ('J_T41L', '16'), 'IN': ('J_T41L', '18')}[name]
    assert parts[ref].pins[pin] == adc


def test_three_wire_panel_selector_and_midi_type_a(parts):
    assert 'SW_PAD_SEL' not in parts
    assert parts['J_PAD_SEL'].pins == {'1': '+3.3V', '2': 'PAD_SEL_ADC', '3': 'GND'}
    assert parts['R_SEL_T'].pins == {'1': '+3.3V', '2': 'PAD_SEL_ADC'}
    assert parts['R_SEL_B'].pins == {'1': 'PAD_SEL_ADC', '2': 'GND'}
    assert parts['R_SEL_T'].value == parts['R_SEL_B'].value == '10k'
    assert parts['J_T41R'].pins['16'] == 'PAD_SEL_ADC'
    assert parts['R_MIDI_R'].value == '33'
    assert parts['R_MIDI_R'].pins == {'1': '+3.3V', '2': 'MIDI_OUT_RING'}
    assert parts['R_MIDI_T'].value == '10'
    assert parts['R_MIDI_T'].pins == {'1': 'MIDI_TX', '2': 'MIDI_OUT_TIP'}
    assert '1210' in parts['R_MIDI_R'].footprint
    assert '1206' in parts['R_MIDI_T'].footprint
    assert 'R_MIDI_S' not in parts
    assert parts['J_MIDI_IN'].pins == {'1': 'MIDI_IN_TIP', '2': 'MIDI_IN_RING', '3': None}
    assert parts['J_MIDI_OUT'].pins == {'1': 'MIDI_OUT_TIP', '2': 'MIDI_OUT_RING', '3': 'GND'}


def test_one_jst_per_panel_jack_with_existing_contacts(parts):
    expected = {
        'J_LINE_IN_L': ['LINE_IN_L_T', 'LINE_IN_L_R', 'AGND'],
        'J_LINE_IN_R': ['LINE_IN_R_T', 'LINE_IN_R_R', 'AGND'],
        'J_LINE_OUT_L': ['LINE_OUT_L_T', None, 'AGND'],
        'J_LINE_OUT_R': ['LINE_OUT_R_T', None, 'AGND'],
        'J_PEDAL': ['PEDAL_RAW', None, 'GND'],
        'J_CV1': ['CV1_PROTECTED', 'AGND'], 'J_CV2': ['CV2_PROTECTED', 'AGND'],
        'J_MIDI_IN': ['MIDI_IN_TIP', 'MIDI_IN_RING', None],
        'J_MIDI_OUT': ['MIDI_OUT_TIP', 'MIDI_OUT_RING', 'GND'],
        'J_HP': ['HP_L', 'HP_R', 'HP_VGND'],
    }
    assert 'J_HP_CUE' not in parts  # J_HP is already the headphone cable, no parallel outlet.
    assert not any(s.lib_id.startswith('Connector_Audio:AudioJack') for s in parts.values())
    for ref, nets in expected.items():
        assert parts[ref].pins == {str(n): net for n, net in enumerate(nets, 1)}
        assert parts[ref].footprint.startswith('Connector_JST:JST_PH_')

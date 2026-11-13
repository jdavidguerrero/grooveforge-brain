"""Review regressions: physical UART crossing, power-loss current paths and sourcing failures.

Theory: ../THEORY-fixrev.md. These checks prove topology/identity, not powered-off
silicon behavior, ESD immunity or mechanical mating.
"""
import json

from check_bom import CATALOG, check_components
from harness import PANEL, CABLES
from schgen.pcb_m import pcb_m


def parts():
    return {s.fields.get('Function', s.reference): s for sh in pcb_m().sheets for s in sh.symbols}


def test_uart_matches_adafruit_function_with_soldered_pads():
    p = parts()
    # Module socket physical contact20 = T28/RX7, contact21 = T29/TX7.
    for direction, contact, socket in [('RX', '3', '20'), ('TX', '4', '21')]:
        resistor = p[f'R_M4_{direction}']
        assert p['J_T41L'].pins[socket] == resistor.pins['1'] == f'M4_{direction}7'
        assert resistor.pins['2'] == p['J3_M4'].pins[contact] == f'M4_{direction}7_CABLE'
    assert p['J3_M4'].pins['2'] is None
    assert 'SDA = TX21' in PANEL['J3_M4']['contacts']['SDA']
    assert 'SCL = RX22' in PANEL['J3_M4']['contacts']['SCL']


def test_power_loss_has_series_limiting_and_common_sensor_origin():
    p = parts()
    assert p['U_LDO1'].pins['1'] == p['U_LDO1'].pins['3'] == p['C_LDO_IN'].pins['1'] == '+5V_TEENSY'
    for i in (1, 2):
        assert p[f'U_ADS{i}'].pins['8'] == '+3.3VA'
        assert p[f'R_SHARP_LIMIT{i}'].pins == {'1': p[f'J{i+4}_SHARP{i}'].pins['2'], '2': f'SHARP{i}_ADC'}
        assert p[f'R_SHARP_LIMIT{i}'].value == '10k'
        assert p[f'R_DAC_LIMIT{i}'].pins == {'1': f'DAC{i}_RAW', '2': f'DAC{i}_LIMITED'}
        assert p[f'R_DAC_LIMIT{i}'].value == '2.2k'
    assert 5/10000 < .010 and 3.3/2200 < .002
    assert p['Q_LED_REVERSE'].pins == {'1': 'GND', '2': '+5V_LED', '3': '+5V_LED_FUSED'}
    assert p['D_LED_TVS'].pins == {'1': '+5V_LED', '2': 'GND'}
    assert p['F1'].footprint == 'Fuse:Fuse_1812_4532Metric'


def test_external_pedal_and_unplugged_pots_have_defined_state():
    p = parts()
    assert p['J_PEDAL'].pins['1'] == p['R_PEDAL_SER'].pins['1'] == 'PEDAL_RAW'
    assert p['R_PEDAL_SER'].value == '1k'
    assert p['R_PEDAL_SER'].pins['2'] == p['J_PCB_K'].pins['10'] == 'PEDAL_BTN'
    assert p['R_PEDAL_PU'].pins == {'1': '+3.3V', '2': 'PEDAL_BTN'}
    assert p['C_PEDAL'].value == '10nF' and p['C_PEDAL'].pins == {'1': 'PEDAL_BTN', '2': 'GND'}
    assert p['D_PEDAL_ESD'].pins == {'1': 'PEDAL_BTN', '2': 'GND'}
    assert p['R_LED_BIAS'].value == '100k' and p['R_LED_BIAS'].pins == {'1': 'LED_DATA', '2': 'GND'}
    for n in ('MASTER', 'CUE', 'IN'):
        assert p[f'R_POT_BIAS_{n}'].value == '100k'
        assert p[f'R_POT_BIAS_{n}'].pins == p[f'C_POT_{n}'].pins == {'1': f'POT_{n}', '2': 'GND'}
    err = max(x/10000 - x/10000*1e5/(1e5+1000+10000*(x/10000)*(1-x/10000)) for x in range(10001))
    assert .02108 < err < .02109


def test_audio_module_is_three_soldered_pad_cables_and_hp_return_isolated():
    p=parts()
    for ref in ('J_SHIELD_IN', 'J_SHIELD_OUT', 'J_SHIELD_HP'):
        assert any(c['a'] == ('pcb-m', p[ref].reference) and c['b'] == ('panel', ref) for c in CABLES)
        assert 'JST_PH_B3B' in p[ref].footprint
    assert p['J_SHIELD_IN'].pins == {'1': 'SHIELD_LINE_IN_L', '2': 'SHIELD_LINE_IN_R', '3': 'GND'}
    assert p['J_SHIELD_OUT'].pins == {'1': 'SHIELD_LINE_OUT_L', '2': 'SHIELD_LINE_OUT_R', '3': 'GND'}
    assert p['J_SHIELD_HP'].pins['3'] == p['J_HP'].pins['3'] == 'HP_VGND'


def test_bom_rejects_wrong_part_package_dnp_and_annotation():
    catalog=json.loads(CATALOG.read_text())
    valid=dict(Reference='U1', Value='74AHCT1G125', Footprint='Package_TO_SOT_SMD:SOT-353_SC-70-5', LCSC='C12495', DNP='')
    assert check_components([valid],catalog) == ([], [])
    for change in [dict(LCSC='C7466'),dict(Footprint='Package_SO:SOIC-8'),dict(Value='inverter'),dict(Reference='U?'),dict(MPN='wrong')]:
        bad=dict(valid,**change)
        assert check_components([bad],catalog)[0]
    assert check_components([dict(valid,LCSC='')],catalog)[1]
    assert check_components([dict(valid,LCSC='',DNP='DNP')],catalog) == ([], [])
    assert all(s.reference[-1].isdigit() and '?' not in s.reference for sh in pcb_m().sheets for s in sh.symbols)

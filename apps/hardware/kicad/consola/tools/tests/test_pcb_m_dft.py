"""Observable service access and isolation paths required by doc24 section 10."""

import pytest

from schgen.pcb_m import pcb_m


@pytest.fixture
def parts():
    return {s.reference: s for sheet in pcb_m().sheets for s in sheet.symbols}


def test_instrument_access_to_service_nets(parts):
    wanted = {'+5V_TEENSY', '+5V_LED_RAW', '+5V_LED', '+3.3V', '+3.3VA',
              '+3.3VA_FRONT', '+5V_CV', 'VREF', 'AGND', 'GND', 'I2C_SDA', 'I2C_SCL',
              'LED_DIN_5V', 'MIDI_RX', 'MIDI_TX', 'CV1_OUT', 'CV2_OUT',
              'CV1_PROTECTED', 'CV2_PROTECTED'}
    available = {s.pins['1'] for s in parts.values() if s.lib_id == 'Connector:TestPoint'
                 and s.footprint == 'TestPoint:TestPoint_Pad_D1.5mm'}
    wanted |= {f'I2C_{signal}{suffix}' for signal in ('SDA', 'SCL')
               for suffix in ('_B_A', '_B_K', '_A', '_K')}
    assert wanted <= available


def conductive_reach(parts, start, opened=()):
    # Only service paths conduct here, never model an IC as a short between its pins.
    edges = []
    for ref, symbol in parts.items():
        if ref in opened:
            continue
        if symbol.lib_id == 'Jumper:SolderJumper_2_Bridged' or ref in ('R_CV_FILTER', 'F1') or ref.startswith(('R_I2C_SDA_', 'R_I2C_SCL_', 'R_ISO_IN_')):
            edges.append(set(symbol.pins.values()))
    reachable = {start}
    while True:
        updated = reachable | set().union(*(e for e in edges if e & reachable))
        if updated == reachable:
            return reachable
        reachable = updated


@pytest.mark.parametrize('bridge,source,load,pin', [
    ('SJ_FRONT', '+3.3VA', 'C_OA_IN', '1'),
    ('SJ_CV', '+5V_LED', 'C_OA_CV_VCC', '1'),
    ('SJ_LED_K', '+5V_LED', 'J_PCB_K', '8'),
    ('SJ_SHARP', '+5V_LED', 'J5_SHARP1', '1'),
    ('SJ_SHARP', '+5V_LED', 'J6_SHARP2', '1'),
    ('SJ_SDA_A', 'I2C_SDA_B_A', 'J2_I2C', '3'),
    ('SJ_SCL_A', 'I2C_SCL_B_A', 'J2_I2C', '4'),
    ('SJ_SDA_K', 'I2C_SDA_B_K', 'J_PCB_K', '2'),
    ('SJ_SCL_K', 'I2C_SCL_B_K', 'J_PCB_K', '4'),
    ('SJ_LED_DATA', 'LED_BUF_OUT', 'J_PCB_K', '6'),
])
def test_bridge_opens_only_named_service_path(parts, bridge, source, load, pin):
    target = parts[load].pins[pin]
    assert target != source
    assert target in conductive_reach(parts, source)
    assert target not in conductive_reach(parts, source, [bridge])
    assert 'Bridged' in parts[bridge].footprint


def test_front_power_and_cv_rc_preserve_other_domains(parts):
    units = [s for sheet in pcb_m().sheets for s in sheet.symbols if s.reference == 'U_OA_IN']
    assert next(s for s in units if s.unit == 5).pins['4'] == '+3.3VA_FRONT'
    assert parts['D_CLAMP_L'].pins['2'] == parts['R_VREF_T'].pins['1'] == '+3.3VA_FRONT'
    assert parts['U_ADS1'].pins['8'] == parts['U_ADS2'].pins['8'] == '+3.3VA'
    assert parts['R_CV_FILTER'].pins == {'1': '+5V_CV_IN', '2': '+5V_CV'}
    assert parts['R_CV_FILTER'].value == '10'
    assert parts['C_CV_FILTER'].pins == {'1': '+5V_CV', '2': 'GND'}
    assert parts['C_CV_FILTER'].value == '47uF 10V'
    assert parts['U_OA_CV'].pins['4'] == '+5V_CV'
    assert parts['C_OA_CV_VCC'].pins['1'] == parts['C_OA_CV_BULK'].pins['1'] == '+5V_CV'
    assert '+5V_TEENSY' not in conductive_reach(parts, '+5V_LED')
    assert 'GND' not in conductive_reach(parts, 'HP_VGND')


def test_pullup_pair_is_selectable_without_unsoldering_resistors(parts):
    for signal in ('SDA', 'SCL'):
        for suffix, value, state in [('22', '2.2k', 'Bridged'), ('47', '4.7k', 'Open')]:
            bridge = parts[f'SJ_PU_{signal}_{suffix}']
            resistor = parts[f'R_{signal}' + ('_47' if suffix == '47' else '')]
            assert resistor.value == value
            assert resistor.pins['1'] == bridge.pins['2']
            assert resistor.pins['2'] == f'I2C_{signal}'
            assert bridge.pins['1'] == '+3.3V'
            assert bridge.lib_id == f'Jumper:SolderJumper_2_{state}'
        assert f'PU_{signal}_22' in conductive_reach(parts, '+3.3V')
        assert f'PU_{signal}_47' not in conductive_reach(parts, '+3.3V')


def test_led_current_and_conservative_dnp_adjustments(parts):
    for name, voltage, resistance in [('3V3', 3.3, 1500), ('3V3A', 3.3, 1500), ('5V_LED', 5, 3300)]:
        assert 0.0008 <= (voltage - 1.8) / resistance <= 0.0012
        assert '0603' in parts[f'R_IND_{name}'].footprint
        assert '0603' in parts[f'D_IND_{name}'].footprint
        assert parts[f'R_IND_{name}'].pins['2'] == parts[f'D_IND_{name}'].pins['2']
    assert parts['D_IND_3V3A'].pins['1'] == 'AGND'
    for target in ('BIAS_L', 'BIAS_R', 'MIDI_RX', 'MIDI_BASE'):
        optional = parts[f'R_OPT_{target}']
        assert optional.dnp
        assert optional.pins == parts[f'R_{target}'].pins
    assert parts['R_MIDI_R'].dnp and parts['R_MIDI_R'].value == '33'
    assert parts['R_MIDI_T'].dnp and parts['R_MIDI_T'].value == '10'


@pytest.mark.parametrize('channel,shield_pin', [('L', '1'), ('R', '2')])
def test_codec_input_can_be_disconnected_without_removing_shield(parts, channel, shield_pin):
    source = parts[f'C_LIN_{channel}'].pins['2']
    target = parts['J_SHIELD_AUDIO'].pins[shield_pin]
    link = f'R_ISO_IN_{channel}'
    assert parts[link].value == '0' and '0805' in parts[link].footprint
    assert target in conductive_reach(parts, source)
    assert target not in conductive_reach(parts, source, [link])
    assert parts[f'JP_BYP_{channel}'].lib_id == 'Jumper:SolderJumper_2_Open'


def test_midi_reserve_has_no_active_teensy_or_assembly_connection(parts, tmp_path):
    from schgen import generate, Library
    from schgen.sexp import parse, children, child
    board = pcb_m()
    midi = next(sheet for sheet in board.sheets if sheet.name == '07-midi')
    assert all(symbol.dnp for symbol in midi.symbols)
    assert parts['J_T41L'].pins['2'] == 'I2C_BUF_EN_GPIO'
    assert parts['J_T41L'].pins['3'] is None
    generate(board, tmp_path, Library())
    tree = parse((tmp_path / 'sheets/07-midi.kicad_sch').read_text())
    for symbol in children(tree, 'symbol'):
        assert child(symbol, 'dnp')[1] == 'yes'
        assert child(symbol, 'in_bom')[1] == 'no'


    from check_netlist import find_kicad_cli, run_kicad
    import csv
    cli = find_kicad_cli()
    if cli:
        output = tmp_path / 'bom.csv'
        run_kicad([cli, 'sch', 'export', 'bom', '--fields', 'Reference,Value,DNP',
                   '--labels', 'Reference,Value,DNP', '--output', str(output),
                   str(tmp_path / 'pcb-m.kicad_sch')], tmp_path)
        with output.open() as stream:
            refs = {row['Reference'].rstrip('?') for row in csv.DictReader(stream)}
        assert not refs & {symbol.reference for symbol in midi.symbols}
        assert not {ref for ref in refs if 'MIDI' in ref}
        assert {'U_I2C_A', 'U_I2C_K', 'J_PCB_K'} <= refs


def test_buffer_pinout_domains_protection_and_service_enable(parts):
    for branch, connector, sda_pin, scl_pin in [('K', 'J_PCB_K', '2', '4'), ('A', 'J2_I2C', '3', '4')]:
        chip = parts[f'U_I2C_{branch}']
        assert chip.value == 'PCA9517ADP,118' and chip.fields['LCSC'] == 'C80799'
        assert '3x3mm_P0.65mm' in chip.footprint
        assert chip.pins == {'1': '+3.3V', '2': 'I2C_SCL', '3': 'I2C_SDA', '4': 'GND',
                             '5': 'I2C_BUF_EN', '6': f'I2C_SDA_B_{branch}',
                             '7': f'I2C_SCL_B_{branch}', '8': '+3.3V'}
        for signal, pin in [('SDA', sda_pin), ('SCL', scl_pin)]:
            local, remote = f'I2C_{signal}', parts[connector].pins[pin]
            assert remote not in conductive_reach(parts, local)
            assert f'I2C_{signal}_B_{branch}' in conductive_reach(parts, remote)
            for suffix, value, state in [('22', '2.2k', 'Bridged'), ('47', '4.7k', 'Open')]:
                jumper = parts[f'SJ_PU_{signal}_{branch}_{suffix}']
                resistor = parts[f'R_PU_{signal}_{branch}_{suffix}']
                assert jumper.lib_id == f'Jumper:SolderJumper_2_{state}'
                assert resistor.value == value
                assert resistor.pins == {'1': jumper.pins['2'], '2': f'I2C_{signal}_B_{branch}'}
            assert parts[f'R_I2C_{signal}_{branch}'].value == '47'
            assert '0805' in parts[f'R_I2C_{signal}_{branch}'].footprint
        esd = parts[f'D_ESD_I2C_{branch}']
        assert esd.value == 'USBLC6-2SC6' and esd.fields['LCSC'] == 'C7519'
        assert esd.pins == {'1': f'I2C_SDA_{branch}', '6': f'I2C_SDA_{branch}', '2': 'GND',
                            '3': f'I2C_SCL_{branch}', '4': f'I2C_SCL_{branch}', '5': '+3.3V'}
    assert parts['R_I2C_EN'].value == '10k'
    assert parts['R_I2C_EN'].pins == {'1': '+3.3V', '2': 'I2C_BUF_EN'}
    assert 'I2C_BUF_EN' in conductive_reach(parts, 'I2C_BUF_EN_GPIO')
    assert 'I2C_BUF_EN' not in conductive_reach(parts, 'I2C_BUF_EN_GPIO', ['SJ_EN_GPIO'])
    assert parts['SJ_EN_HIGH'].lib_id == 'Jumper:SolderJumper_2_Open'


def test_m_k_ten_way_contract_independent_on_both_boards(parts):
    from schgen.pcb_k import pcb_k_left
    left = {s.reference: s for sheet in pcb_k_left().sheets for s in sheet.symbols}
    assert list(parts['J_PCB_K'].pins.values()) == [
        '+3.3V', 'I2C_SDA_K', 'GND', 'I2C_SCL_K', 'GND', 'LED_DIN_5V',
        'GND', '+5V_LED_K', 'JOG_CLICK', 'PEDAL_BTN']
    assert list(left['J1'].pins.values()) == [
        '+3V3', 'SDA', 'GND', 'SCL', 'GND', 'LED_DIN_5V',
        'GND', '+5V_LED', 'JOG_CLICK', 'PEDAL_BTN']
    assert parts['J_PCB_K'].footprint == left['J1'].footprint
    assert parts['J_PCB_K'].fields['LCSC'] == left['J1'].fields['LCSC'] == 'C158038'

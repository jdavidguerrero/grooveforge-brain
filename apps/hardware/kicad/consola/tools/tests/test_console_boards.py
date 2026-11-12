"""Console acceptance: independent GPIO/cable/chain expectations and physical handoff."""

import csv
import pytest

from check_netlist import check, compare_netlist, find_kicad_cli, run_kicad
from schgen import Library, generate
from schgen.boards import BOARDS, NEW_BOARDS
from schgen.console_artifacts import CONSOLE, placement_rows, write_artifacts
from schgen.footprints import DEFAULT_FOOTPRINTS, write_footprints
from schgen.generator import DEFAULT_LIBRARIES, validate
from schgen.sexp import parse, child, children


def by_ref(name):
    return {s.reference: s for sheet in BOARDS[name]().sheets for s in sheet.symbols}


def test_gpio_contract_doc_28_independent_of_description():
    expected = {
        'pcb-k-right': {'U21': {21: 'SCENE1_OCT+', 22: 'SCENE2_OCT-', 23: 'SCENE4_PAGE_R',
            24: 'SCENE3_PAGE_L', 1: 'METRO', 2: 'LOOP', 3: 'REC', 4: 'STOP', 5: 'PLAY'}},
        'pcb-k-left': {
            'U22': {21: 'UNDO', 22: 'BACK', 23: 'MENU', 24: 'NAV_L', 25: 'NAV_R',
                    26: 'BROWSE', 27: 'SHIFT', 1: 'DELETE', 2: 'DUPLICATE', 3: 'QUANTIZE', 4: 'NEW'},
            'U23': {21: 'CUE_ENG', 22: 'FILL', 23: 'TAP', 24: 'BUILD', 25: 'DROP',
                    26: 'BREAK', 27: 'JOG_CLICK', 1: 'PEDAL_BTN'}}}
    for name, chips in expected.items():
        parts = by_ref(name)
        for ref, pins in chips.items():
            chip = parts[ref]
            assert {p: chip.pins[str(p)] for p in pins} == pins
            assert chip.pins['8'] is chip.pins['28'] is None
            assert chip.pins['11'] is chip.pins['14'] is None
            address = int(ref[1:]) - 20
            assert [chip.pins[str(p)] for p in (15, 16, 17)] == [
                '+3V3' if address & bit else 'GND' for bit in (1, 2, 4)]
            assert parts[f'R{ref[1:]}'].pins == {'1': '+3V3', '2': chip.pins['18']}
            assert parts[f'R{ref[1:]}'].value == '10k'
            for suffix, value in [('1', '100nF'), ('2', '1uF')]:
                c = parts[f'C{ref[1:]}{suffix}']
                assert c.value == value and c.pins == {'1': '+3V3', '2': 'GND'}
            attached = {s.pins['1'] for s in parts.values() if s.lib_id == 'Connector:TestPoint'}
            assert chip.pins['19'] in attached and chip.pins['20'] in attached
            for p in list(range(1, 8)) + list(range(21, 28)):
                if p not in pins:
                    assert chip.pins[str(p)] in attached


def test_connectors_and_split_led_chain():
    left, right = by_ref('pcb-k-left'), by_ref('pcb-k-right')
    assert list(left['J1'].pins.values()) == ['+3V3', 'SDA', 'GND', 'SCL', 'GND', 'LED_DIN_5V',
                                            'GND', '+5V_LED', 'JOG_CLICK', 'PEDAL_BTN']
    link = ['+3V3', 'GND', 'SDA', 'SCL', '+5V_LED', 'LED_DATA']
    assert list(left['J2'].pins.values()) == list(right['J1'].pins.values()) == link
    expected = ('BACK MENU NAV_L NAV_R BROWSE TAP FILL CUE_ENG BUILD DROP BREAK SHIFT UNDO '
                'DELETE DUPLICATE QUANTIZE NEW PLAY STOP REC LOOP METRO SCENE4_PAGE_R '
                'SCENE3_PAGE_L SCENE2_OCT- SCENE1_OCT+').split()
    parts = {**left, **{k: v for k, v in right.items() if k.startswith(('D', 'SW', 'C1'))}}
    for i, name in enumerate(expected):
        sw, led = parts[f'SW{i+1}'], parts[f'D{i+1}']
        assert sw.value == name and sw.pins == {'1': name, '2': 'GND'}
        assert led.pins['1'] == 'GND' and led.pins['3'] == '+5V_LED'
        assert led.pins['2'] == f'LED_{i}_DIN'
        if i not in (16, 25):
            assert led.pins['4'] == parts[f'D{i+2}'].pins['2']
        assert parts[f'C{100+i}'].value == '100nF'
        assert parts[f'C{100+i}'].pins == {'1': '+5V_LED', '2': 'GND'}
    assert left['D17'].pins['4'] == left['J2'].pins['6'] == right['R1'].pins['1']
    assert right['D26'].pins['4'] is None
    for parts, incoming, first in [(left, 'LED_DIN_5V', 0), (right, 'LED_DATA', 17)]:
        assert parts['R1'].value == '33R' and '0805' in parts['R1'].footprint
        assert parts['R1'].pins == {'1': incoming, '2': f'LED_{first}_DIN'}
        assert parts['C1'].value == '22uF' and parts['C1'].pins['1'] == '+5V_LED'
        assert [(parts[r].value, parts[r].dnp) for r in ('R2', 'R3')] == [('2.2k DNP', True)] * 2


def test_passive_cable_contracts():
    parts = by_ref('pcb-f')
    assert list(parts['J1'].pins.values()) == ['+3V3', 'FADER1', 'FADER2', 'FADER3', 'FADER4', 'GND']
    for i in range(1, 5):
        assert parts[f'RV{i}'].pins == {'1': '+3V3', '2': f'FADER{i}', '3': 'GND', 'MP': 'GND'}
        assert parts[f'RV{i}'].footprint == 'groovepush:ALPS_RS6011P_60mm'
    parts = by_ref('pcb-p')
    assert len(parts) == 12
    for i, (name, target) in enumerate(zip(('V1', 'V2', 'V3', 'V4', 'X1', 'X2'),
            ('J_FSR1', 'J_FSR2', 'J_FSR3', 'J_FSR4', 'J7_EXP1', 'J8_EXP2')), 1):
        assert parts[f'J{i}'].pins == parts[f'FSR{i}'].pins == {'1': f'+3V3_{name}', '2': f'FSR_{name}'}
        assert parts[f'J{i}'].value == f'PCB-M {target}'
        assert 'S2B-PH-K' in parts[f'J{i}'].footprint
    assert len({net for p in parts.values() for net in p.pins.values()}) == 12


@pytest.mark.parametrize('name', NEW_BOARDS)
def test_placement_panel_coordinates_and_projects(name, tmp_path):
    rows = placement_rows(name)
    if name.startswith('pcb-k-'):
        count = 17 if name.endswith('left') else 9
        assert len(rows) == 3 * count
        with (CONSOLE / 'panel/panel-coordinates-console-v1.csv').open() as f:
            source = {r['ref']: r for r in csv.DictReader(f)}
        for key, socket, led in zip(rows[0::3], rows[1::3], rows[2::3]):
            assert key['x_mm'] == float(source[key['name']]['x_mm'])
            assert key['y_mm'] == float(source[key['name']]['y_mm'])
            assert key['x_mm'] == socket['x_mm'] == led['x_mm']
            assert key['y_mm'] == socket['y_mm'] == pytest.approx(led['y_mm'] - 4.93)
            assert key['side'] == 'F' and socket['side'] == led['side'] == 'B'
    elif name == 'pcb-f':
        assert [r['x_mm'] for r in rows] == [264.5, 284.5, 304.5, 324.5]
        assert {r['y_mm'] for r in rows} == {129.5}
        assert {r['rotation_deg'] for r in rows} == {90}
    else:
        assert [r['x_mm'] for r in rows] == [123.5, 154.5, 185.5, 216.5, 24.5, 55.5]
        assert {r['y_mm'] for r in rows} == {84.5}
    write_artifacts(name, tmp_path)
    assert (tmp_path / f'{name}.kicad_pro').is_file()
    assert (tmp_path / 'placement.csv').is_file()
    if name in ('pcb-f', 'pcb-p'):
        with (tmp_path / 'mechanical.csv').open() as f:
            mechanical = list(csv.DictReader(f))
        assert sum(r['item'] == 'agujero' for r in mechanical) == (4 if name == 'pcb-f' else 10)


def test_footprint_geometry_and_provisional_marking(tmp_path):
    if not DEFAULT_FOOTPRINTS.exists():
        pytest.skip('KiCad footprints not installed')
    write_footprints(tmp_path)
    switch = parse((tmp_path / 'Kailh_Choc_V2_PG1353_Hotswap.kicad_mod').read_text())
    pads = children(switch, 'pad')
    holes = {tuple(map(float, child(p, 'at')[1:3])): float(child(p, 'drill')[1])
             for p in pads if p[2] == 'np_thru_hole'}
    assert holes == {(0, 0): 5, (-5, -5.15): 1.5}
    contacts = [p for p in pads if p[2] == 'smd']
    assert {p[1] for p in contacts} == {'1', '2'}
    assert all(child(p, 'layers')[1:] == ['B.Cu', 'B.Paste', 'B.Mask'] for p in contacts)
    assert 'VERIFICAR CON ZÓCALO REAL' in (tmp_path / 'Kailh_Choc_V2_PG1353_Hotswap.kicad_mod').read_text()
    window = next(n for n in children(switch, 'fp_rect') if child(n, 'layer')[1] == 'Dwgs.User')
    assert (float(child(window, 'start')[2]) + float(child(window, 'end')[2])) / 2 == -4.93
    fsr = parse((tmp_path / 'FSR402_SolderTail_Provisional.kicad_mod').read_text())
    assert {p[1] for p in children(fsr, 'pad')} == {'1', '2'}
    assert all('F.Paste' not in child(p, 'layers') for p in children(fsr, 'pad'))
    led = parse((tmp_path / 'SK6812MINI-EA_ReverseMount.kicad_mod').read_text())
    assert {p[1] for p in children(led, 'pad')} == {'1', '2', '3', '4'}
    assert any(child(n, 'layer')[1] == 'Edge.Cuts' for n in children(led, 'fp_line'))
    for path in tmp_path.glob('*.kicad_mod'):
        assert path.read_bytes() == (CONSOLE / 'lib/gf_consola.pretty' / path.name).read_bytes()


def test_kicad_can_load_custom_footprints(tmp_path):
    if not DEFAULT_FOOTPRINTS.exists() or not find_kicad_cli():
        pytest.skip('KiCad footprints/CLI not installed')
    library = tmp_path / 'gf_consola.pretty'
    write_footprints(library)
    output = tmp_path / 'svg'
    output.mkdir()
    run_kicad([find_kicad_cli(), 'fp', 'export', 'svg', '--output', str(output), str(library)], tmp_path)
    assert {p.stem for p in output.glob('*.svg')} == {p.stem for p in library.glob('*.kicad_mod')}


@pytest.mark.parametrize('name', NEW_BOARDS)
def test_new_board_deterministic_kicad_acceptance(name, tmp_path):
    if not DEFAULT_LIBRARIES.exists() or not find_kicad_cli():
        pytest.skip('KiCad not installed')
    board, library = BOARDS[name](), Library()
    validate(board, library)
    generate(board, tmp_path, library)
    first = {str(p.relative_to(tmp_path)): p.read_bytes() for p in tmp_path.rglob('*.kicad_sch')}
    generate(board, tmp_path, library)
    assert first == {str(p.relative_to(tmp_path)): p.read_bytes() for p in tmp_path.rglob('*.kicad_sch')}
    write_artifacts(name, tmp_path)
    check(board, tmp_path, find_kicad_cli(), library=library)
    assert (tmp_path / 'docs' / f'{name}-schematic.pdf').stat().st_size > 1000
    # Prove a wrong GPIO binding is rejected against an actual exported schematic.
    if name == 'pcb-k-left':
        chip = next(s for sheet in board.sheets for s in sheet.symbols if s.reference == 'U22')
        chip.pins['21'], chip.pins['22'] = chip.pins['22'], chip.pins['21']
        with pytest.raises(ValueError):
            compare_netlist(board, tmp_path / 'docs' / f'{name}-netlist.xml')

"""Observable geometry, deterministic generation and real KiCad connectivity regression tests."""

import copy
from pathlib import Path
import xml.etree.ElementTree as ET

import pytest

from check_netlist import compare_netlist, export_netlist, find_kicad_cli
from schgen import Board, Library, Sheet, Symbol, check_collisions, generate, pin_position, stable_uuid
from schgen.generator import DEFAULT_LIBRARIES, validate
from schgen.pcb_m import pcb_m


@pytest.mark.parametrize('angle,expected', [
    (0, (12.7, 17.78)), (90, (7.62, 17.78)),
    (180, (7.62, 22.86)), (270, (12.7, 22.86))])
def test_pin_position_rotation(angle, expected):
    assert pin_position((10.16, 20.32), (2.54, 2.54), angle) == pytest.approx(expected)


def test_collisions():
    check_collisions([('a', (0, 0, 5, 5)), ('b', (5, 0, 10, 5))])
    with pytest.raises(ValueError, match='Collision: b with a'):
        check_collisions([('a', (0, 0, 5, 5)), ('b', (4, 4, 10, 10))])


def test_stable_uuids():
    assert stable_uuid('board/sheet/R1/1') == '8193b7a2-45b0-58e6-b65d-339ee66ef1b1'
    assert stable_uuid('board/sheet/R1/1') != stable_uuid('board/sheet/R1/2')


@pytest.fixture
def library():
    if not DEFAULT_LIBRARIES.is_dir():
        pytest.skip('KiCad standard symbol libraries are not installed')
    return Library()


def small_board():
    """Use standard parts at all four rotations, including one explicit NC."""
    symbols = [Symbol('Device:R', f'R{index+1}', '10k', 'Resistor_SMD:R_0402_1005Metric',
                      (50.8 + index * 50.8, 50.8), {'1': '+3.3V', '2': f'SIGNAL{index}'},
                      rotation=angle)
               for index, angle in enumerate((0, 90, 180, 270))]
    symbols.append(Symbol('Connector_Generic:Conn_01x02', 'J1', 'Test', '', (50.8, 101.6),
                          {'1': 'SIGNAL0', '2': None}))
    return Board('fixture', [Sheet('test', symbols)])


def test_deterministic_generation_and_stable_object_ids(tmp_path, library):
    board = small_board()
    generate(board, tmp_path, library)
    first = {str(p.relative_to(tmp_path)): p.read_bytes() for p in tmp_path.rglob('*.kicad_sch')}
    generate(board, tmp_path, library)
    assert first == {str(p.relative_to(tmp_path)): p.read_bytes() for p in tmp_path.rglob('*.kicad_sch')}
    board.sheets[0].symbols[0].value = '20k'
    generate(board, tmp_path, library)
    from schgen.sexp import child, children, parse
    old_tree = parse(first['sheets/test.kicad_sch'].decode())
    new_tree = parse((tmp_path / 'sheets/test.kicad_sch').read_text())
    assert [child(s, 'uuid')[1] for s in children(old_tree, 'symbol')] == [
        child(s, 'uuid')[1] for s in children(new_tree, 'symbol')]


def test_geometry_collision_before_any_write(tmp_path, library):
    board = small_board()
    board.sheets[0].symbols[1].position = board.sheets[0].symbols[0].position
    with pytest.raises(ValueError, match='Collision'):
        generate(board, tmp_path / 'invalid', library)
    assert not (tmp_path / 'invalid').exists()


def test_rotation_changes_collision_envelope(library):
    board = small_board()
    board.sheets[0].symbols = [board.sheets[0].symbols[0], board.sheets[0].symbols[1]]
    board.sheets[0].symbols[1].position = (78.74, 50.8)
    validate(board, library)
    board.sheets[0].symbols[0].rotation = 90
    with pytest.raises(ValueError, match='Collision'):
        validate(board, library)


@pytest.mark.parametrize('change,match', [('missing', 'missing pins'), ('unknown', 'unknown pins'),
                                         ('grid', 'off 1.27'), ('duplicate', 'Duplicate reference')])
def test_invalid_description(change, match, library):
    board = small_board()
    first = board.sheets[0].symbols[0]
    if change == 'missing':
        del first.pins['2']
    elif change == 'unknown':
        first.pins['99'] = 'BAD'
    elif change == 'grid':
        first.position = (50, 50)
    else:
        board.sheets[0].symbols.append(copy.deepcopy(first))
    with pytest.raises(ValueError, match=match):
        validate(board, library)


def test_pcb_m_complete_description(library):
    board = pcb_m()
    validate(board, library)
    assert len(board.sheets) == 10
    assert not board.sheets[3].symbols
    assert all(s.symbols for s in board.sheets if s.name != '04-mcp-io')
    by_ref = {s.reference: s for sheet in board.sheets for s in sheet.symbols}
    assert by_ref['J_HP'].pins['3'] == 'HP_VGND'
    assert by_ref['J_LINE_OUT_L'].pins['1'] == 'LINE_OUT_L_T'
    assert by_ref['J1_PCBA'].pins['20'] is None
    assert by_ref['J1_PCBA'].pins['22'] is None


def test_inherited_library_symbol(library):
    from schgen.sexp import children
    definition = library.definition('Amplifier_Operational:MCP6004')
    assert not children(definition, 'extends')
    assert len(children(definition, 'symbol')) >= 5
    assert definition[1] == 'Amplifier_Operational:MCP6004'


@pytest.fixture
def exported(tmp_path, library):
    cli = find_kicad_cli()
    if not cli:
        pytest.skip('kicad-cli is not available')
    board = small_board()
    root = generate(board, tmp_path, library)
    netlist = tmp_path / 'netlist.xml'
    export_netlist(cli, root, netlist)
    return board, netlist


def test_kicad_end_to_end_rotations_and_nc(exported):
    board, netlist = exported
    stats = compare_netlist(board, netlist)
    assert stats == {'components': 5, 'pins': 10, 'nc': 1, 'nets': 5}


@pytest.mark.parametrize('damage', ['merge', 'rename', 'missing', 'extra', 'nc_connected', 'nc_flag', 'split'])
def test_checker_rejects_corrupt_export(exported, damage):
    board, path = exported
    tree = ET.parse(path)
    nets = tree.find('nets')
    first = next(n for n in nets if n.attrib['name'] == '+3.3V')
    other = next(n for n in nets if n.attrib['name'].endswith('/SIGNAL0'))
    nc = next(n for n in nets if n.attrib['name'].startswith('unconnected-('))
    if damage == 'merge':
        for node in list(other):
            first.append(node)
        nets.remove(other)
    elif damage == 'rename':
        first.set('name', 'WRONG_RAIL')
    elif damage == 'missing':
        first.remove(first.find('node'))
    elif damage == 'extra':
        first.append(ET.Element('node', {'ref': 'UNDECLARED', 'pin': '1'}))
    elif damage == 'nc_connected':
        first.append(nc.find('node'))
        nets.remove(nc)
    elif damage == 'nc_flag':
        nc.find('node').set('pintype', 'passive')
    else:
        node = first.find('node')
        first.remove(node)
        split = ET.SubElement(nets, 'net', {'code': '999', 'name': '+3.3V'})
        split.append(node)
    tree.write(path)
    with pytest.raises(ValueError):
        compare_netlist(board, path)


def test_page_frame_and_safe_sheet_names(library):
    board = small_board()
    board.sheets[0].symbols[0].position = (0, 0)
    with pytest.raises(ValueError, match='outside drawing frame'):
        validate(board, library)
    board = small_board()
    board.sheets[0].name = '../outside'
    with pytest.raises(ValueError, match='safe filename'):
        validate(board, library)


def test_root_all_ten_sheets_inside_page(tmp_path, library):
    from schgen.sexp import child, children, parse
    root = generate(pcb_m(), tmp_path, library)
    tree = parse(root.read_text())
    assert child(tree, 'paper')[1] == 'A4'
    sheets = children(tree, 'sheet')
    assert len(sheets) == 10
    for sheet in sheets:
        x, y = map(float, child(sheet, 'at')[1:3])
        width, height = map(float, child(sheet, 'size')[1:3])
        assert x >= 10 and y >= 10
        assert x + width <= 287 and y + height <= 200
        assert float(child(children(sheet, 'property')[1], 'at')[2]) <= 200

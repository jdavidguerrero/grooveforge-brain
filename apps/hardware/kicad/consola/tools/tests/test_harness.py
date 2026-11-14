"""Small independent KiCad XML examples, including physical pin-count metadata."""

import copy
import xml.etree.ElementTree as ET

import pytest

from check_harness import check_cables, read_netlist
from harness import cable


def example(path, signals, physical=None):
    tree = ET.Element('export')
    components = ET.SubElement(tree, 'components')
    comp = ET.SubElement(components, 'comp', ref='J1')
    ET.SubElement(comp, 'libsource', lib='Connector_Generic', part='example')
    libpart = ET.SubElement(ET.SubElement(tree, 'libparts'), 'libpart', lib='Connector_Generic', part='example')
    pins = ET.SubElement(libpart, 'pins')
    for pin in physical or map(str, range(1, len(signals)+1)):
        ET.SubElement(pins, 'pin', num=pin)
    nets = ET.SubElement(tree, 'nets')
    # Group duplicate names exactly as a real exporter does.
    by_name = {}
    for i, signal in enumerate(signals, 1):
        name = signal if signal is not None else f'unconnected-(J1-Pin_{i}-Pad{i})'
        if name not in by_name:
            by_name[name] = ET.SubElement(nets, 'net', code=str(len(by_name)+1), name=name)
        ET.SubElement(by_name[name], 'node', ref='J1', pin=str(i),
                      pintype='passive+no_connect' if signal is None else 'passive')
    ET.ElementTree(tree).write(path)
    return read_netlist(path)


def run_pair(tmp_path, a, b, **kwargs):
    return check_cables({'a': example(tmp_path/'a.xml', a), 'b': example(tmp_path/'b.xml', b)},
                        [cable('example', ('a', 'J1'), ('b', 'J1'), len(a), 'JST-PH')], **kwargs)


def test_good_xml_and_assembly_pin_rows(tmp_path):
    results, errors = run_pair(tmp_path, ['GND', '+3.3V', 'SDA', 'SCL'],
                              ['/GND', '/+3V3', '/SDA', '/SCL'], aliases={'b': {'+3V3': '+3.3V'}})
    assert not errors
    assert len(results[0]['rows']) == 4
    assert all(row[-1] == 'PASS' for row in results[0]['rows'])


@pytest.mark.parametrize('a,b', [('+5V_LED', '+5V_TEENSY'), ('+5V_LED', 'VUSB'),
                                 ('+3.3VA', '+3.3V'), ('HP_VGND', 'GND'), ('HP_VGND', 'AGND')])
def test_power_rail_crossing(tmp_path, a, b):
    _, errors = run_pair(tmp_path, [a, 'DATA'], [b, 'DATA'])
    assert any('RAIL_CROSS pin 1->1' in e for e in errors)


def test_pin_shift_cannot_hide_behind_matching_count(tmp_path):
    _, errors = run_pair(tmp_path, ['GND', 'SDA', 'SCL', '+3.3V'], ['+3.3V', 'GND', 'SDA', 'SCL'])
    assert len(errors) == 4


def test_bad_alias_cannot_hide_power_crossing(tmp_path):
    _, errors = run_pair(tmp_path, ['+5V_LED'], ['+5V_TEENSY'], aliases={'b': {'+5V_TEENSY': '+5V_LED'}})
    assert any('unsafe power alias' in e for e in errors)


def test_missing_exported_pin_and_library_count_are_independent(tmp_path):
    a = example(tmp_path/'a.xml', ['GND', 'SDA'], physical=['1', '2', '3'])
    b = example(tmp_path/'b.xml', ['GND', 'SDA', 'SCL'])
    results, errors = check_cables({'a': a, 'b': b}, [cable('ex', ('a', 'J1'), ('b', 'J1'), 3, 'JST-SH')])
    assert any('pin not exported' in e for e in errors)
    _, errors = check_cables({'a': a, 'b': b}, [cable('ex', ('a', 'J1'), ('b', 'J1'), 2, 'JST-SH')])
    assert any('expected 2 pins' in e for e in errors)


def test_non_bijective_mapping_and_real_pin_remap(tmp_path):
    a = example(tmp_path/'a.xml', ['GND', 'SDA'])
    b = example(tmp_path/'b.xml', ['SDA', 'GND'])
    c = cable('ex', ('a', 'J1'), ('b', 'J1'), 2, 'JST-PH', mapping={'1': '2', '2': '2'})
    _, errors = check_cables({'a': a, 'b': b}, [c])
    assert any('bijection' in e for e in errors)
    c['mapping'] = {'1': '2', '2': '1'}
    _, errors = check_cables({'a': a, 'b': b}, [c])
    assert not errors


def test_nc_exception_is_exact_not_wildcard(tmp_path):
    a = example(tmp_path/'a.xml', [None, 'GND'])
    b = example(tmp_path/'b.xml', ['INT_A', 'GND'])
    c = cable('ex', ('a', 'J1'), ('b', 'J1'), 2, 'IDC', exceptions={'1': (None, 'INT_A')})
    _, errors = check_cables({'a': a, 'b': b}, [c])
    assert not errors
    b.nets['J1']['1'] = '+5V_LED'
    _, errors = check_cables({'a': a, 'b': b}, [c])
    assert any('exception expects' in e for e in errors)


def test_panel_pair_documented_and_unlisted_jack_fails(tmp_path):
    a = example(tmp_path/'a.xml', ['HP_L', 'HP_R', 'HP_VGND'])
    c = cable('hp', ('a', 'J1'), ('panel', 'HP'), 3, 'JST-PH')
    _, errors = check_cables({'a': a}, [c])
    assert any('panel counterpart undocumented' in e for e in errors)
    _, errors = check_cables({'a': a}, [c], panel={'HP': {'pins': {'1': 'HP_L', '2': 'HP_R', '3': 'HP_VGND'}}})
    assert not errors
    _, errors = check_cables({'a': a}, [])
    assert any('UNDOCUMENTED' in e for e in errors)


def test_missing_board_never_passes(tmp_path):
    a = example(tmp_path/'a.xml', ['GND'])
    results, errors = check_cables({'a': a}, [cable('ex', ('a', 'J1'), ('b', 'J1'), 1, 'JST-PH')])
    assert errors and results[0]['pending']


def test_hierarchical_ambiguity_requires_explicit_alias(tmp_path):
    a = example(tmp_path/'a.xml', ['/first/SDA', '/second/SDA'])
    b = example(tmp_path/'b.xml', ['SDA_1', 'SDA_2'])
    c = cable('ex', ('a', 'J1'), ('b', 'J1'), 2, 'JST-SH')
    _, errors = check_cables({'a': a, 'b': b}, [c])
    assert any('ambiguous local net' in e for e in errors)
    _, errors = check_cables({'a': a, 'b': b}, [c], aliases={'a': {'/first/SDA': 'SDA_1', '/second/SDA': 'SDA_2'}})
    assert not errors


def test_duplicate_node_is_rejected(tmp_path):
    example(tmp_path/'a.xml', ['GND'])
    tree = ET.parse(tmp_path/'a.xml')
    tree.find('./nets/net').append(copy.deepcopy(tree.find('./nets/net/node')))
    tree.write(tmp_path/'a.xml')
    with pytest.raises(ValueError, match='multiple nets'):
        read_netlist(tmp_path/'a.xml')


@pytest.mark.parametrize('a,b', [('I2C_SDA', 'I2C_SDA_B_K'), ('I2C_SCL', 'I2C_SCL_B_A'),
                                ('I2C_SDA_B_K', 'I2C_SDA_B_A')])
def test_i2c_segment_crossing_cannot_be_hidden_by_alias(tmp_path, a, b):
    _, errors = run_pair(tmp_path, [a], [b])
    assert any('I2C_DOMAIN_CROSS' in e for e in errors)
    _, errors = run_pair(tmp_path, [a], [b], aliases={'b': {b: a}})
    assert any('unsafe I2C alias' in e for e in errors)


def test_real_remote_names_cannot_mask_local_side_on_cable(tmp_path):
    a = example(tmp_path/'a.xml', ['I2C_SDA', 'I2C_SCL'])
    b = example(tmp_path/'b.xml', ['SDA', 'SCL'])
    c = cable('M-K', ('pcb-m', 'J1'), ('pcb-k-left', 'J1'), 2, 'JST-PH')
    from harness import ALIASES
    _, errors = check_cables({'pcb-m': a, 'pcb-k-left': b}, [c], ALIASES)
    assert len(errors) == 2 and all('I2C_DOMAIN_CROSS' in e for e in errors)


def test_report_paths_do_not_escape_clone(tmp_path):
    from check_harness import markdown, Netlist, TOOLS
    source = TOOLS.parent / 'boards/pcb-m/docs/pcb-m-netlist.xml'
    report = markdown([], [], {'pcb-m': Netlist({}, {}, str(source))}, {})
    assert 'apps/hardware/kicad/consola/boards/pcb-m/docs/pcb-m-netlist.xml' in report

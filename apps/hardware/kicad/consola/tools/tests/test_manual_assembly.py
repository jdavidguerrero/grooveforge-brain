"""Observable hand-soldering contract; theory and limits in THEORY-mano.md."""

import csv
import math
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

import pytest

from check_netlist import (check_legacy, compare_legacy_netlist, find_kicad_cli,
                           run_kicad)
from schgen import Library
from schgen.boards import BOARDS
from schgen.console_artifacts import CONSOLE
from schgen.sexp import child, children, parse


def test_dual_switch_plated_contacts_and_clearance():
    switch = parse((CONSOLE / 'lib/gf_consola.pretty/Kailh_Choc_V2_PG1353_Hotswap.kicad_mod').read_text())
    pads = children(switch, 'pad')
    pth = [p for p in pads if p[2] == 'thru_hole']
    smd = [p for p in pads if p[2] == 'smd']
    assert {p[1]: tuple(map(float, child(p, 'at')[1:])) for p in pth} == {
        '1': (0, 5.9), '2': (5, 3.8)}
    assert {p[1] for p in pth} == {p[1] for p in smd} == {'1', '2'}
    gaps = []
    for p in pth:
        drill = float(child(p, 'drill')[1])
        width, height = map(float, child(p, 'size')[1:])
        assert drill == 1.3 and width == height == 1.9
        radius = width / 2
        assert (width - drill) / 2 >= 0.15
        assert child(p, 'layers')[1:] == ['*.Cu', '*.Mask']
        x, y = map(float, child(p, 'at')[1:])
        assert math.hypot(x, y) - radius - 2.5 >= 0.2
        for s in smd:
            sx, sy = map(float, child(s, 'at')[1:])
            w, h = map(float, child(s, 'size')[1:])
            # Bounding rectangle is conservative for rounded SMD corners.
            gap = math.hypot(max(abs(x-sx)-w/2, 0), max(abs(y-sy)-h/2, 0))-radius
            assert gap >= 0.2
            gaps.append(gap)
    assert min(gaps) == pytest.approx(1.025)


@pytest.mark.parametrize('name', ['pcb-k-left', 'pcb-k-right'])
def test_manual_parts_match_purchase_packages(name):
    parts = [s for sheet in BOARDS[name]().sheets for s in sheet.symbols]
    expected = {'100nF': ('C49678', 'C_0805_2012Metric'),
                '1uF': ('C5137478', 'C_0805_2012Metric'),
                '22uF': ('C12891', 'C_1206_3216Metric'),
                '10k': ('C17414', 'R_0805_2012Metric'),
                '33R': ('C17634', 'R_0805_2012Metric'),
                '2.2k DNP': ('C17520', 'R_0805_2012Metric')}
    for s in parts:
        if s.reference.startswith(('R', 'C')):
            lcsc, footprint = expected[s.value]
            assert s.fields['LCSC'] == lcsc and s.footprint.endswith(footprint)
        elif s.reference.startswith('U'):
            assert s.fields['LCSC'] == 'C506653'
            assert s.footprint == 'Package_SO:SSOP-28_5.3x10.2mm_P0.65mm'
        elif s.reference.startswith('D'):
            assert s.value == 'SK6812MINI-EA' and s.fields['LCSC'] == 'C5378731'
            assert s.footprint == 'gf_consola:SK6812MINI-EA_ReverseMount'
            assert s.pins['1'] == 'GND' and s.pins['3'] == '+5V_LED'


def test_pcb_a_dnp_and_passive_sourcing():
    path = CONSOLE / 'boards/pcb-a/pcb-a'
    sch = parse(path.with_suffix('.kicad_sch').read_text())
    pcb = parse(path.with_suffix('.kicad_pcb').read_text())
    for kind, tree in [('symbol', sch), ('footprint', pcb)]:
        dnp = set()
        for node in children(tree, kind):
            props = {p[1]: p[2] for p in children(node, 'property')}
            ref = props['Reference']
            marked = child(node, 'dnp')[1] == 'yes' if kind == 'symbol' else 'dnp' in child(node, 'attr')
            if marked:
                dnp.add(ref)
            if ref == 'U1':
                assert props['LCSC'] == 'C506653'
            if ref[0] in 'RC' and ref[1:].isdigit():
                assert props['LCSC'] == {'100nF': 'C14663', '10nF': 'C1589',
                                         '10uF': 'C15850', '2.2k': 'C4190'}[props['Value']]
        assert dnp == {'R1', 'R2', 'J4'}


def test_pcb_a_legacy_connectivity_and_mutation(tmp_path):
    cli = find_kicad_cli()
    if not cli:
        pytest.skip('KiCad CLI not installed')
    source = CONSOLE / 'boards/pcb-a'
    shutil.copy(source / 'pcb-a.kicad_sch', tmp_path)
    check_legacy('pcb-a', tmp_path, cli, Library())
    xml = tmp_path / 'docs/pcb-a-netlist.xml'
    tree = ET.parse(xml)
    node = tree.find('./nets/net/node')
    node.set('pin', '999')
    tree.write(xml)
    with pytest.raises(ValueError, match='legacy connectivity changed'):
        compare_legacy_netlist('pcb-a', xml)


@pytest.mark.parametrize('name', ['pcb-k-left', 'pcb-k-right', 'pcb-a'])
def test_real_bom_export_matches_checked_in_csv(name, tmp_path):
    cli = find_kicad_cli()
    if not cli:
        pytest.skip('KiCad CLI not installed')
    output = tmp_path / 'bom.csv'
    fields = 'Reference,Value,Footprint,LCSC,Supply,SupplyDescription,DNP'
    run_kicad([cli, 'sch', 'export', 'bom', '--fields', fields, '--labels', fields,
               '--output', str(output), str(CONSOLE / f'boards/{name}/{name}.kicad_sch')], tmp_path)
    assert output.read_bytes() == (CONSOLE / f'fab/bom-{name}.csv').read_bytes()
    with output.open() as f:
        rows = {r['Reference']: r for r in csv.DictReader(f)}
    if name == 'pcb-a':
        assert {ref for ref, row in rows.items() if row['DNP']} == {'R1', 'R2', 'J4'}
    else:
        assert {ref for ref, row in rows.items() if row['DNP']} == {'R2', 'R3'}
    for ref, row in rows.items():
        if ref[0] in 'UCRD' and ref[1:].isdigit():
            assert row['LCSC']

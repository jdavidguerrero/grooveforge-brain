"""Procurement closure and mutation checks; rationale/limits in THEORY-lcsc.md."""
import csv
import json

import pytest

from check_bom import BOARDS, CATALOG, CONSOLE, check_components, read_components
from schgen import DEFAULT_LIBRARIES
from schgen.boards import BOARDS as FACTORIES
from schgen.sexp import child, children, parse


@pytest.fixture
def catalog():
    return json.loads(CATALOG.read_text())


def external_row(profile):
    return dict(Reference='J1', Value=profile['values'][0], Footprint=profile['footprints'][0],
                LCSC='', Supply=profile['supply'], SupplyDescription=profile['description'])


def test_external_supplies_require_exact_declaration(catalog):
    for profile in catalog['external_supplies']:
        row = external_row(profile)
        assert check_components([row], catalog) == ([], [])
        for change in (dict(Supply='pending'), dict(Supply=''), dict(SupplyDescription=''),
                       dict(SupplyDescription='   '), dict(SupplyDescription='unspecified'),
                       dict(Footprint='Package_SO:SOIC-8'), dict(Value='different part'),
                       dict(LCSC='C41376163')):
            assert check_components([dict(row, **change)], catalog)[0]
        unresolved = dict(row, Supply='', SupplyDescription='')
        assert check_components([unresolved], catalog)[1]
        assert check_components([dict(unresolved, DNP='DNP')], catalog) == ([], [])


@pytest.mark.parametrize('board,external_count', tuple(zip(BOARDS, (1, 17, 9, 9))))
def test_exported_procurement_is_closed(board, external_count, catalog):
    rows = read_components(CONSOLE / f'boards/{board}/docs/{board}-netlist.xml')
    assert check_components(rows, catalog) == ([], [])
    assert sum(bool(r.get('Supply')) for r in rows) == external_count
    with (CONSOLE / f'fab/bom-{board}.csv').open() as f:
        bom = list(csv.DictReader(f))
    assert check_components(bom, catalog) == ([], [])
    assert sum(bool(r['Supply']) for r in bom) == external_count


def test_pcb_m_bom_is_real_kicad_export(tmp_path):
    from check_netlist import find_kicad_cli, run_kicad
    cli = find_kicad_cli()
    if not cli:
        pytest.skip('KiCad CLI not installed')
    output = tmp_path / 'bom.csv'
    fields = 'Reference,Value,Footprint,LCSC,Supply,SupplyDescription,DNP'
    run_kicad([cli, 'sch', 'export', 'bom', '--fields', fields, '--labels', fields,
               '--output', str(output), str(CONSOLE / 'boards/pcb-m/pcb-m.kicad_sch')], tmp_path)
    assert output.read_bytes() == (CONSOLE / 'fab/bom-pcb-m.csv').read_bytes()


def test_capacitor_identity_and_positive_pad(catalog):
    parts = {s.reference: s for sheet in FACTORIES['pcb-m']().sheets for s in sheet.symbols}
    cap = parts['C_CV_FILTER1']
    assert cap.lib_id == 'Device:C_Polarized'
    assert cap.pins == {'1': '+5V_CV', '2': 'GND'}
    assert cap.fields['LCSC'] == 'C7469972' and cap.value == '47uF 16V'
    fp_path = DEFAULT_LIBRARIES.parent / 'footprints/Capacitor_SMD.pretty/CP_Elec_5x5.4.kicad_mod'
    if not fp_path.exists():
        pytest.skip('KiCad footprint library not installed')
    fp = parse(fp_path.read_text())
    pads = {p[1]: child(p, 'at')[1:] for p in children(fp, 'pad')}
    assert float(pads['1'][0]) < 0 < float(pads['2'][0])
    assert cap.footprint == 'Capacitor_SMD:CP_Elec_5x5.4'
    assert check_components([dict(Reference=cap.reference, Value=cap.value,
                                  Footprint=cap.footprint, **cap.fields)], catalog) == ([], [])


def test_manual_order_does_not_duplicate_pcb_m_smd():
    with (CONSOLE / 'fab/pedido-lcsc-v1.csv').open() as f:
        order = {r['LCSC Part Number']: r for r in csv.DictReader(f)}
    assert order['C41376163']['Quantity'] == '2'
    assert order['C225519']['Quantity'] == '1'
    assert 'C7469972' not in order

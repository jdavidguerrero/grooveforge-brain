"""Revision 2 regressions; THEORY-fixrev2.md defines physical limits not proved here."""
import copy
import csv
import json
import math

import pytest

from check_bom import (CATALOG, CONSOLE, check_components, compare_representations,
                       read_components, read_schematic)
from harness import CABLES, PANEL
from schgen.boards import BOARDS
from schgen.console_artifacts import placement_rows
from schgen.sexp import parse, child, children


def parts():
    return {s.fields.get('Function', s.reference): s for sh in BOARDS['pcb-m']().sheets for s in sh.symbols}


def test_cv_source_and_uart_current_limit():
    p = parts()
    assert p['SJ_CV'].pins == {'1': '+5V_TEENSY', '2': '+5V_CV_IN'}
    assert p['R_CV_FILTER'].pins == {'1': '+5V_CV_IN', '2': '+5V_CV'}
    assert p['R_CV_FILTER'].value == '10' and p['C_CV_FILTER'].value == '47uF 16V'
    assert p['U_OA_CV'].pins['4'] == '+5V_CV'
    for channel in (1, 2):
        assert p[f'D_CV_CLAMP{channel}'].pins['2'] == '+5V_CV'
    assert p['U_LED_BUF'].pins['5'] == '+5V_LED'
    assert p['SJ_LED_K'].pins['1'] == p['SJ_SHARP'].pins['1'] == '+5V_LED'
    for name in ('RX', 'TX'):
        r = p[f'R_M4_{name}']
        assert r.value == '1k' and r.footprint == 'Resistor_SMD:R_0805_2012Metric'
        assert r.fields['LCSC'] == 'C17513'
    tau = 1010 * 100e-12
    assert -math.log(.3)*tau < 123e-9 and -math.log(.01)*tau < .5e-6


def test_tvs_are_unidirectional_with_unchanged_polarity_and_marked_landing():
    p = parts()
    for name, net, footprint in [('D_PEDAL_ESD', 'PEDAL_BTN', 'D_SOD-882_K1'),
                                  ('D_LED_TVS', '+5V_LED', 'D_SMA_K1')]:
        d = p[name]
        assert d.lib_id == 'Device:D_Zener'
        assert d.pins == {'1': net, '2': 'GND'}
        fp = parse((CONSOLE / f'lib/gf_consola.pretty/{footprint}.kicad_mod').read_text())
        assert any(n[2] == 'K' and child(n, 'layer')[1] == 'F.SilkS' for n in children(fp, 'fp_text'))
    import xml.etree.ElementTree as ET
    xml = ET.parse(CONSOLE / 'boards/pcb-m/docs/pcb-m-netlist.xml')
    pins = {n.get('num'): n.get('name') for n in xml.findall('./libparts/libpart[@part="D_Zener"]/pins/pin')}
    assert pins == {'1': 'K', '2': 'A'}


@pytest.fixture
def representations():
    name = 'pcb-m'
    scheme = read_schematic(CONSOLE / f'boards/{name}/{name}.kicad_sch')
    xml = read_components(CONSOLE / f'boards/{name}/docs/{name}-netlist.xml')
    with (CONSOLE / f'fab/bom-{name}.csv').open() as stream:
        bom = list(csv.DictReader(stream))
    return scheme, xml, bom


@pytest.mark.parametrize('target', (1, 2))
@pytest.mark.parametrize('mutation', ('missing', 'extra', 'duplicate'))
def test_reference_integrity_rejects_missing_extra_and_duplicate_rows(representations, target, mutation):
    tables = copy.deepcopy(representations)
    assert not compare_representations(*tables)
    rows = tables[target]
    if mutation == 'missing':
        rows[:] = [r for r in rows if r['Reference'] != 'U_ADS1']
    elif mutation == 'extra':
        rows.append(dict(rows[0], Reference='U_EXTRA1'))
    else:
        rows.append(dict(rows[0]))
    assert compare_representations(*tables)


def test_testpoint_footprint_cannot_disguise_bought_ic(representations):
    _, xml, _ = representations
    row = next(r for r in xml if r['Reference'] == 'U_ADS1')
    disguised = dict(row, Footprint='TestPoint:TestPoint_Pad_D1.5mm', LCSC='',
                     MPN='', Manufacturer='', Package='', Supply='', SupplyDescription='')
    assert disguised['Symbol'] == 'Analog_ADC:ADS1115IDGS'
    assert check_components([disguised], json.loads(CATALOG.read_text()))[1]
    # Changing XML libsource too still disagrees with independent schematic.
    broken = copy.deepcopy(representations)
    broken[1][:] = [dict(disguised, Symbol='Connector:TestPoint') if r['Reference'] == row['Reference'] else r
                    for r in broken[1]]
    assert compare_representations(*broken)


@pytest.mark.parametrize('source', ('buscador LCSC', 'http://www.lcsc.com/product-detail/C17513.html',
                                     'https://www.lcsc.com/product-detail/C999.html'))
def test_catalog_requires_exact_https_product_url(source):
    catalog = json.loads(CATALOG.read_text())
    catalog['parts'][-1]['source'] = source
    with pytest.raises(ValueError, match='HTTPS'):
        check_components([], catalog)


def test_open_pot_timing_includes_leakage_and_component_tolerances():
    p = parts()
    for name in ('MASTER', 'CUE', 'IN'):
        assert p[f'R_POT_BIAS_{name}'].value == '100k'
        assert p[f'R_POT_BIAS_{name}'].fields['LCSC'] == 'C25741'
    r, c, leakage = 101000, 120e-9, 1e-6
    assert leakage*r+(3.3-leakage*r)*math.exp(-.1/(r*c)) < .15
    error = max(x/10000-x/10000*100000/(101000+10000*x/10000*(1-x/10000)) for x in range(10001))
    assert .02108 < error < .02109


def test_fsr_overlap_from_panel_outline_and_direct_solder_handoff():
    with (CONSOLE / 'panel/pcb-outlines-console-v5.csv').open() as stream:
        outline = list(csv.DictReader(stream))
    vertices = [(float(r['x_mm']), float(r['y_mm'])) for r in outline
                if r['board'] == 'PCB-K' and r['item'].startswith('vértice')]

    def inside(x, y):
        return sum((a[1] > y) != (b[1] > y) and
                   x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]
                   for a, b in zip(vertices, vertices[1:]+vertices[:1])) % 2 == 1

    rows = placement_rows('pcb-p')
    assert [r['name'] for r in rows if inside(r['x_mm'], r['y_mm']-11)] == ['V1','V2','V3','V4']
    assert 6.5-3.4-1.6 == pytest.approx(1.5)
    assert all('SIN SOCKET' in r['notes'] and 'superior <=1mm' in r['notes'] for r in rows)
    sensors = [s for sh in BOARDS['pcb-p']().sheets for s in sh.symbols if s.reference.startswith('FSR')]
    assert len(sensors) == 6
    assert all(s.footprint == 'gf_consola:FSR402_DirectSolder_1x02_P2.54mm' and
               s.fields['Supply'] == 'owner' for s in sensors)


def test_m4_cable_uses_functional_solder_pad_labels():
    from check_harness import check_cables, read_netlist, markdown
    c = next(c for c in CABLES if c['name'] == 'panel-J3_M4')
    assert c['mapping'] == {'1': 'GND', '2': '3V', '3': 'SDA', '4': 'SCL'}
    assert PANEL['J3_M4']['pins'] == {'GND': 'GND', '3V': None,
                                     'SDA': 'M4_RX7_CABLE', 'SCL': 'M4_TX7_CABLE'}
    source = read_netlist(CONSOLE / 'boards/pcb-m/docs/pcb-m-netlist.xml')
    excluded = {('pcb-m', ref): 'outside this cable test' for ref in source.pins if ref != 'J3_M4'}
    results, errors = check_cables({'pcb-m': source}, [c], panel=PANEL, excluded=excluded)
    assert not errors
    report = markdown(results, errors, {'pcb-m': source}, excluded)
    assert '3 → SDA' in report and '4 → SCL' in report
    assert 'Juan confirmará los rótulos impresos' in report
    assert 'CONN1.' not in report and 'AVISO' not in report
    crossed = copy.deepcopy(c)
    crossed['mapping'].update({'3': 'SCL', '4': 'SDA'})
    assert check_cables({'pcb-m': source}, [crossed], panel=PANEL, excluded=excluded)[1]
    numbered = dict(c, mapping={str(i): str(i) for i in range(1, 5)})
    assert check_cables({'pcb-m': source}, [numbered], panel=PANEL, excluded=excluded)[1]

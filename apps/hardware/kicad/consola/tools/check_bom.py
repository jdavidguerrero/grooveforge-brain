#!/usr/bin/env python3
"""Validate exported LCSC identities or declared external supplies; fail closed."""
import argparse
import csv
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET
from urllib.parse import urlparse

from schgen.sexp import parse, child, children

TOOLS = Path(__file__).resolve().parent
CONSOLE = TOOLS.parent
CATALOG = TOOLS / 'lcsc_catalog.json'
BOARDS = ('pcb-m', 'pcb-k-left', 'pcb-k-right', 'pcb-a', 'pcb-f', 'pcb-p')


COPPER_SYMBOLS = {
    'Connector:TestPoint', 'Connector:TestPoint_2Pole', 'Device:NetTie_2',
    'Jumper:SolderJumper_2_Bridged', 'Jumper:SolderJumper_2_Open',
    'Mechanical:MountingHole', 'Mechanical:MountingHole_Pad',
}
BOM_FIELDS = ('Value', 'Footprint', 'LCSC', 'Supply', 'SupplyDescription', 'DNP')


def check_components(components, catalog):
    """Return identity errors and unresolved purchase references separately."""
    by_code = {p['lcsc']: p for p in catalog['parts']}
    if len(by_code) != len(catalog['parts']):
        raise ValueError('Duplicate LCSC in verified catalog')
    required = ('lcsc', 'mpn', 'fabricante', 'encapsulado', 'fecha_verificacion', 'source', 'footprints')
    for p in by_code.values():
        if any(not p.get(k) for k in required) or not re.fullmatch(r'C\d+', p['lcsc']):
            raise ValueError(f'Incomplete verified catalog entry: {p}')
        url = urlparse(p['source'])
        if (url.scheme != 'https' or url.netloc != 'www.lcsc.com' or
                not re.fullmatch(r'/(?:[a-z]{2}/)?product-detail/' + p['lcsc'] + r'\.html', url.path)
                or url.query or url.fragment):
            raise ValueError(f'{p["lcsc"]}: exact HTTPS LCSC product URL required')
    external = catalog.get('external_supplies', [])
    for p in external:
        if (p.get('supply') not in ('owner', 'local') or
                any(not p.get(k) for k in ('description', 'footprints', 'values',
                                          'fecha_verificacion', 'source'))):
            raise ValueError(f'Incomplete external supply entry: {p}')
    errors, pending = [], []
    for c in components:
        ref, value, footprint, code = (c.get(k, '') for k in ('Reference', 'Value', 'Footprint', 'LCSC'))
        if '?' in ref or not re.fullmatch(r'.*\d', ref):
            errors.append(f'{ref}: incomplete annotation')
        if c.get('DNP'):
            continue
        # Only physical copper/features are exempt, never bought connectors or ICs.
        if c.get('Symbol') in COPPER_SYMBOLS:
            if code or c.get('Supply') or c.get('SupplyDescription'):
                errors.append(f'{ref}: copper-only feature unexpectedly assigned purchase supply')
            continue
        supply, description = c.get('Supply', ''), c.get('SupplyDescription', '')
        if supply or description:
            if code:
                errors.append(f'{ref}: LCSC conflicts with external Supply')
            matched = [p for p in external if supply == p['supply'] and
                       description == p['description'] and footprint in p['footprints'] and
                       value in p['values']]
            if supply not in ('owner', 'local') or not description.strip() or len(matched) != 1:
                errors.append(f'{ref}: invalid or undeclared external Supply/description/value/footprint')
            continue
        if not code:
            pending.append(f'{ref}: {value} | {footprint}')
            continue
        p = by_code.get(code)
        if p is None:
            errors.append(f'{ref}: {code} not in verified catalog')
            continue
        if footprint not in p['footprints']:
            errors.append(f'{ref}: {footprint} incompatible with {code} {p["encapsulado"]}')
        if p.get('values') and value not in p['values']:
            errors.append(f'{ref}: value {value} incompatible with {code} {p["mpn"]}')
        for key, catalog_key in [('MPN', 'mpn'), ('Manufacturer', 'fabricante'), ('Package', 'encapsulado')]:
            if c.get(key) and c[key] != p[catalog_key]:
                errors.append(f'{ref}: {key} disagrees with verified {code}')
    return errors, pending


def read_components(path):
    """Read identities from KiCad XML, independent of expected generator model."""
    rows = []
    for c in ET.parse(path).findall('./components/comp'):
        fields = {f.get('name'): f.text or '' for f in c.findall('./fields/field')}
        properties = {p.get('name'): p.get('value', '') for p in c.findall('./property')}
        rows.append(dict(fields, Reference=c.get('ref'), Value=c.findtext('value', ''),
                         Footprint=c.findtext('footprint', ''),
                         Symbol=f'{c.find("libsource").get("lib")}:{c.find("libsource").get("part")}',
                         InBOM='no' if 'exclude_from_bom' in properties else 'yes',
                         DNP='DNP' if 'dnp' in properties else ''))
    return rows


def read_schematic(path):
    """Read actual instances and hierarchy, not the expected schgen description."""
    rows, visited = {}, set()

    def visit(filename):
        filename = Path(filename).resolve()
        if filename in visited:
            raise ValueError(f'Repeated/cyclic sheet: {filename}')
        visited.add(filename)
        tree = parse(filename.read_text())
        for symbol in children(tree, 'symbol'):
            fields = {p[1]: p[2] for p in children(symbol, 'property')}
            ref = fields.get('Reference', '')
            if ref.startswith('#'):
                continue
            row = dict(fields, Symbol=child(symbol, 'lib_id')[1],
                       InBOM=child(symbol, 'in_bom')[1],
                       DNP='DNP' if children(symbol, 'dnp') and child(symbol, 'dnp')[1] == 'yes' else '')
            if ref in rows:
                if any(rows[ref].get(k, '') != row.get(k, '')
                       for k in (*BOM_FIELDS, 'Symbol', 'InBOM')):
                    raise ValueError(f'Conflicting units for {ref}')
            else:
                rows[ref] = row
        for sheet in children(tree, 'sheet'):
            fields = {p[1]: p[2] for p in children(sheet, 'property')}
            visit(filename.parent / fields['Sheetfile'])

    visit(path)
    return list(rows.values())


def compare_representations(schematic, xml, bom):
    """Detect missing/extra/duplicate refs and metadata drift, including excluded/DNP parts."""
    errors = []

    def indexed(rows, label):
        index = {}
        for row in rows:
            ref = row.get('Reference', '')
            if ref in index:
                errors.append(f'{label}: duplicate reference {ref}')
            index[ref] = row
        return index

    source = indexed(schematic, 'schematic')
    actual_xml = indexed(xml, 'XML')
    actual_bom = indexed(bom, 'BOM')
    for label, actual, expected, fields in (
            ('XML', actual_xml, source, (*BOM_FIELDS, 'Symbol', 'InBOM')),
            ('BOM', actual_bom, {r: c for r, c in source.items() if c['InBOM'] == 'yes'}, BOM_FIELDS)):
        missing, extra = set(expected) - set(actual), set(actual) - set(expected)
        if missing or extra:
            errors.append(f'{label}: missing={sorted(missing)} extra={sorted(extra)}')
        for ref in actual.keys() & expected.keys():
            changed = [k for k in fields if actual[ref].get(k, '') != expected[ref].get(k, '')]
            if changed:
                errors.append(f'{label} {ref}: metadata mismatch {changed}')
    return errors


def check_board(schematic_path, xml_path, bom_path, catalog):
    """Identity closure plus independent integrity checks for one complete board."""
    source, xml = read_schematic(schematic_path), read_components(xml_path)
    with Path(bom_path).open() as stream:
        bom = list(csv.DictReader(stream))
    errors = compare_representations(source, xml, bom)
    trusted = {r['Reference']: r for r in source}
    pending = []
    for label, rows in (('schematic', source), ('XML', xml), ('BOM', bom)):
        # CSV has no lib_id: borrow only from the independently read schematic.
        if label == 'BOM':
            rows = [dict(r, Symbol=trusted.get(r['Reference'], {}).get('Symbol', '')) for r in rows]
        identity, unresolved = check_components(rows, catalog)
        errors.extend(f'{label}: {e}' for e in identity)
        pending.extend(f'{label}: {e}' for e in unresolved)
    return errors, pending, xml


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=BOARDS)
    parser.add_argument('--directory', type=Path, default=CONSOLE / 'boards')
    parser.add_argument('--bom-directory', type=Path, default=CONSOLE / 'fab')
    args = parser.parse_args()
    catalog = json.loads(CATALOG.read_text())
    failed = False
    for name in ([args.board] if args.board else BOARDS):
        errors, pending, rows = check_board(
            args.directory / name / f'{name}.kicad_sch',
            args.directory / name / 'docs' / f'{name}-netlist.xml',
            args.bom_directory / f'bom-{name}.csv', catalog)
        supplied = sum(bool(c.get('Supply')) and not c.get('DNP') for c in rows)
        print(f'{name}: {len(rows)} components; {len(errors)} identity errors; '
              f'{len(pending)} unverified purchases; {supplied} external supplies')
        for message in errors:
            print('ERROR ' + message)
        for message in pending:
            print('PENDING ' + message)
        failed |= bool(errors or pending)
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())

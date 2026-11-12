#!/usr/bin/env python3
"""Validate real exported component identities; missing procurement codes fail closed."""
import argparse
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

TOOLS = Path(__file__).resolve().parent
CONSOLE = TOOLS.parent
CATALOG = TOOLS / 'lcsc_catalog.json'
BOARDS = ('pcb-m', 'pcb-k-left', 'pcb-k-right', 'pcb-a')


def check_components(components, catalog):
    """Return identity errors and unresolved purchase references separately."""
    by_code = {p['lcsc']: p for p in catalog['parts']}
    if len(by_code) != len(catalog['parts']):
        raise ValueError('Duplicate LCSC in verified catalog')
    required = ('lcsc', 'mpn', 'fabricante', 'encapsulado', 'fecha_verificacion', 'source', 'footprints')
    for p in by_code.values():
        if any(not p.get(k) for k in required) or not re.fullmatch(r'C\d+', p['lcsc']):
            raise ValueError(f'Incomplete verified catalog entry: {p}')
    errors, pending = [], []
    for c in components:
        ref, value, footprint, code = (c.get(k, '') for k in ('Reference', 'Value', 'Footprint', 'LCSC'))
        if '?' in ref or not re.fullmatch(r'.*\d', ref):
            errors.append(f'{ref}: incomplete annotation')
        if c.get('DNP'):
            continue
        # Only physical copper/features are exempt, never bought connectors or ICs.
        if footprint.startswith(('TestPoint:', 'NetTie:', 'Jumper:', 'MountingHole:')):
            if code:
                errors.append(f'{ref}: copper-only feature unexpectedly assigned purchase code')
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
                         DNP='DNP' if 'dnp' in properties else ''))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=BOARDS)
    parser.add_argument('--directory', type=Path, default=CONSOLE / 'boards')
    args = parser.parse_args()
    catalog = json.loads(CATALOG.read_text())
    failed = False
    for name in ([args.board] if args.board else BOARDS):
        rows = read_components(args.directory / name / 'docs' / f'{name}-netlist.xml')
        errors, pending = check_components(rows, catalog)
        print(f'{name}: {len(rows)} components; {len(errors)} identity errors; {len(pending)} unverified purchases')
        for message in errors:
            print('ERROR ' + message)
        for message in pending:
            print('PENDING ' + message)
        failed |= bool(errors or pending)
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())

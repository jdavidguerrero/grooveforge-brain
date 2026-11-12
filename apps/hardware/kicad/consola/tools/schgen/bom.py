"""Attach only catalogue identities verified for an exact value and footprint."""
import json
from pathlib import Path

CATALOG = Path(__file__).resolve().parents[1] / 'lcsc_catalog.json'


def populate_fields(sheets):
    data = json.loads(CATALOG.read_text())
    catalog = data['parts']
    for sheet in sheets:
        for symbol in sheet.symbols:
            if symbol.reference.startswith('#'):
                continue
            if symbol.lib_id.startswith(('Jumper:', 'Connector:TestPoint')) or symbol.lib_id == 'Device:NetTie_2':
                symbol.fields.update(LCSC='', Procurement='PCB copper; no purchased component')
                continue
            external = [p for p in data.get('external_supplies', [])
                        if symbol.footprint in p['footprints'] and symbol.value in p['values']]
            if len(external) == 1:
                p = external[0]
                symbol.fields.update(LCSC='', Supply=p['supply'], SupplyDescription=p['description'])
                continue
            matched = [p for p in catalog if symbol.footprint in p['footprints'] and
                       (symbol.value in p.get('values', []) or (symbol.lib_id.startswith('Connector_Generic:') and not p.get('values')))]
            # Existing identities also need a verified compatible landing.
            existing = symbol.fields.get('LCSC')
            if not existing and symbol.value == '1uF' and 'C_0805' in symbol.footprint:
                existing = 'C28323'
            if existing:
                matched = [p for p in catalog if p['lcsc'] == existing and symbol.footprint in p['footprints']]
            if len(matched) == 1:
                p = matched[0]
                symbol.fields.update(LCSC=p['lcsc'], MPN=p['mpn'], Manufacturer=p['fabricante'],
                                     Package=p['encapsulado'])
            else:
                symbol.fields['LCSC'] = ''

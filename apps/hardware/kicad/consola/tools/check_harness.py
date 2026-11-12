#!/usr/bin/env python3
"""Compare independent KiCad XML netlists against declarative cable/contact contracts."""

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

import harness

TOOLS = Path(__file__).resolve().parent
MISSING = '<PIN NOT EXPORTED>'


@dataclass
class Netlist:
    """Physical library pin sets and their independently exported electrical nets."""

    pins: dict
    nets: dict
    source: str


def read_netlist(path):
    """Count pins from libparts, so a missing exported pin cannot shrink a connector."""
    tree = ET.parse(path)
    libraries = {(p.get('lib'), p.get('part')): {n.get('num') for n in p.findall('./pins/pin')}
                 for p in tree.findall('./libparts/libpart')}
    pins, nets = {}, {}
    for component in tree.findall('./components/comp'):
        ref = component.get('ref')
        source = component.find('libsource')
        if source is None or (source.get('lib'), source.get('part')) not in libraries:
            raise ValueError(f'{path}: {ref}: missing libpart pin definition')
        if ref in pins:
            raise ValueError(f'{path}: duplicate component {ref}')
        pins[ref] = libraries[(source.get('lib'), source.get('part'))]
        nets[ref] = {}
    for net in tree.findall('./nets/net'):
        name = net.get('name')
        nodes = net.findall('node')
        for node in nodes:
            ref, pin = node.get('ref'), node.get('pin')
            if ref not in pins or pin not in pins[ref]:
                raise ValueError(f'{path}: undeclared physical pin {ref}.{pin}')
            if pin in nets[ref]:
                raise ValueError(f'{path}: pin {ref}.{pin} appears in multiple nets')
            if not name:
                raise ValueError(f'{path}: empty net at {ref}.{pin}')
            if name.startswith('unconnected-('):
                if len(nodes) != 1 or 'no_connect' not in node.get('pintype', ''):
                    raise ValueError(f'{path}: {ref}.{pin}: NC name without isolated no_connect pin')
                nets[ref][pin] = None
            else:
                nets[ref][pin] = name
    return Netlist(pins, nets, str(path))


def base_name(net):
    """Remove KiCad sheet path; ambiguous basenames are rejected by canonical()."""
    return net.rsplit('/', 1)[-1] if net is not None else None


def rail_family(net):
    """Classify original rail names BEFORE any alias can conceal a dangerous crossing."""
    name = base_name(net)
    if name is None or name == MISSING:
        return None
    if name == 'HP_VGND':
        return 'HP_VGND'
    if name in ('GND', 'AGND'):
        return 'GROUND'
    if name in ('VUSB', '+5V_TEENSY', '+5V_VUSB'):
        return '5V_TEENSY'
    if name.startswith('+5V_LED'):
        return '5V_LED'
    if name.startswith('+5V_CV'):
        return '5V_CV'
    if name.startswith(('+3.3VA', '+3V3A')):
        return '3V3_ANALOG'
    if name.startswith(('+3.3V', '+3V3')):
        return '3V3_DIGITAL'
    return None



def i2c_domain(board, net):
    """Identify local/A-side versus each B-side BEFORE aliases are applied."""
    name = base_name(net)
    if name in ('SDA', 'SCL') and board in ('pcb-a', 'pcb-k-left', 'pcb-k-right'):
        return 'B_A' if board == 'pcb-a' else 'B_K'
    if name in ('I2C_SDA', 'I2C_SCL'):
        return 'LOCAL'
    match = re.fullmatch(r'I2C_(?:SDA|SCL)_(?:B_)?([AK])(?:_SER)?', name or '')
    return f'B_{match[1]}' if match else None


def canonical(board, net, aliases, netlist=None):
    """Resolve only board-local aliases, rejecting hazardous or ambiguous equivalences."""
    if net is None or net == MISSING:
        return net
    local = aliases.get(board, {})
    name = base_name(net)
    if net not in local and netlist is not None:
        variants = {n for values in netlist.nets.values() for n in values.values()
                    if n is not None and base_name(n) == name}
        if len(variants) > 1:
            raise ValueError(f'{board}: ambiguous local net {net}; require full-path alias')
    result = local.get(net, local.get(name, name))
    if not isinstance(result, str) or not result or '/' in result:
        raise ValueError(f'{board}: invalid alias {net} -> {result}')
    original, target = rail_family(net), rail_family(result)
    if original != target and (original is not None or target is not None):
        # +5V on fabricated PCB-A has a documented board-local supply identity (D9).
        if not (board == 'pcb-a' and name == '+5V' and result == '+5V_LED'):
            raise ValueError(f'{board}: unsafe power alias {net} -> {result}')
    original_domain, target_domain = i2c_domain(board, net), i2c_domain(board, result)
    if original_domain != target_domain and (original_domain is not None or target_domain is not None):
        raise ValueError(f'{board}: unsafe I2C alias {net} -> {result}')
    return result


def suggested_color(net):
    """Functional colors, independent of commercial pre-crimped cable color order."""
    name = base_name(net)
    if name is None:
        return 'sin hilo / aislar'
    if name == MISSING:
        return 'NO ARMAR'
    if name == 'HP_VGND':
        return 'violeta (retorno HP)'
    if name == 'AGND':
        return 'gris (masa analógica)'
    if name == 'GND':
        return 'negro'
    if name.startswith('+5V'):
        return 'rojo' if 'TEENSY' not in name else 'rojo/blanco'
    if name.startswith(('+3.3V', '+3V3')):
        return 'naranja' if rail_family(name) != '3V3_ANALOG' else 'naranja/blanco'
    if 'SDA' in name:
        return 'azul'
    if 'SCL' in name:
        return 'amarillo'
    if 'LED' in name:
        return 'verde'
    return 'blanco (marcar señal)'


def check_cables(netlists, cables, aliases=None, panel=None, excluded=None):
    """Return per-pin review tables and errors; missing boards remain visibly pending."""
    aliases = aliases or {}
    panel = panel or {}
    excluded = excluded or {}
    results, errors, used = [], [], set()
    for cable in cables:
        name, ways = cable['name'], cable['ways']
        result = dict(cable=cable, rows=[], errors=[], pending=False)
        results.append(result)

        def fail(message):
            message = f'{name}: {message}'
            result['errors'].append(message)
            errors.append(message)

        if not isinstance(ways, int) or ways <= 0:
            fail('invalid number of ways')
            continue
        if cable['kind'] not in ('JST-PH', 'JST-SH', 'IDC', 'header-2x4'):
            fail(f'unknown connector type {cable["kind"]}')
        physical = {str(i) for i in range(1, ways+1)}
        mapping = {str(a): str(b) for a, b in cable.get('mapping', {p: p for p in physical}).items()}
        if set(mapping) != physical or set(mapping.values()) != physical or len(mapping) != ways:
            fail(f'pin map must be a bijection of 1..{ways}')
            continue
        endpoints = []
        for board, ref in (cable['a'], cable['b']):
            if (board, ref) in used:
                fail(f'connector reused by multiple cables: {board}.{ref}')
            used.add((board, ref))
            if board == 'panel':
                contract = panel.get(ref)
                if contract is None:
                    fail(f'panel counterpart undocumented: {ref}')
                    endpoints.append(({}, None))
                    continue
                endpoint = contract['pins']
                pinset, source = set(endpoint), None
            elif board not in netlists:
                fail(f'PENDING: no netlist for {board}.{ref}')
                result['pending'] = True
                endpoints.append(({}, None))
                continue
            else:
                source = netlists[board]
                if ref not in source.pins:
                    fail(f'missing connector {board}.{ref}')
                    endpoints.append(({}, source))
                    continue
                pinset, endpoint = source.pins[ref], source.nets[ref]
            if pinset != physical:
                fail(f'{board}.{ref}: expected {ways} pins 1..{ways}, got {sorted(pinset)}')
            endpoints.append((endpoint, source))
        for a in sorted(mapping, key=int):
            b = mapping[a]
            raw_a, raw_b = (endpoints[0][0].get(a, MISSING), endpoints[1][0].get(b, MISSING))
            status = 'PASS'
            try:
                ca = canonical(cable['a'][0], raw_a, aliases, endpoints[0][1])
                cb = canonical(cable['b'][0], raw_b, aliases, endpoints[1][1])
                fa, fb = rail_family(ca), rail_family(cb)
                da, db = i2c_domain(cable['a'][0], ca), i2c_domain(cable['b'][0], cb)
                if MISSING in (ca, cb):
                    status = 'MISSING'
                    fail(f'pin {a}->{b}: pin not exported or endpoint missing')
                elif fa != fb and fa is not None and fb is not None:
                    status = 'RAIL_CROSS'
                    fail(f'RAIL_CROSS pin {a}->{b}: {raw_a} vs {raw_b}')
                elif da != db and (da is not None or db is not None):
                    status = 'I2C_DOMAIN_CROSS'
                    fail(f'I2C_DOMAIN_CROSS pin {a}->{b}: {raw_a} vs {raw_b}')
                elif a in cable.get('exceptions', {}):
                    expected = cable['exceptions'][a]
                    if (ca, cb) != tuple(expected):
                        status = 'MISMATCH'
                        fail(f'pin {a}->{b}: exception expects {expected}, got {(ca, cb)}')
                    else:
                        status = 'NC D8'
                elif ca != cb:
                    status = 'MISMATCH'
                    fail(f'pin {a}->{b}: {raw_a} != {raw_b} (canonical {ca} != {cb})')
                elif ca is None:
                    status = 'NC'
            except ValueError as error:
                ca, cb = raw_a, raw_b
                status = 'ALIAS ERROR'
                fail(f'pin {a}->{b}: {error}')
            contact = panel.get(cable['b'][1], {}).get('contacts', {}).get(b, '') if cable['b'][0] == 'panel' else ''
            result['rows'].append((a, b, raw_a, raw_b, ca, cb, suggested_color(ca), contact, status))
    # Any cable connector omitted from the manifest must be explicitly reviewed/excluded.
    for board, source in netlists.items():
        for ref in source.pins:
            if (re.match(r'^J(?!P)', ref) or (board == 'pcb-m' and ref == 'SW_PAD')) and (board, ref) not in used and (board, ref) not in excluded:
                errors.append(f'UNDOCUMENTED: cable connector {board}.{ref} has no counterpart/exclusion')
    return results, errors


def markdown(results, errors, netlists, excluded):
    """Render wiring tables with actual raw XML names, equivalences and pending contacts."""
    def cell(value):
        return 'NC' if value is None else str(value).replace('|', '\\|').replace('\n', ' ')

    lines = ['# Mazos Console — informe generado', '',
             'Regenerar XML de A y comprobar: `python3 apps/hardware/kicad/consola/tools/check_harness.py --export-pcb-a`.', '',
             f'**Resultado: {len(errors)} errores; {len(results)} cables declarados.**', '',
             'A/B de placas: XML independiente. B de panel: contrato esperado; NO continuidad medida.',
             'Vista por número de contacto, no por cara del housing. Comprobar orientación de pin1 antes de crimpar.',
             'Colores sugeridos; marcar ambos extremos. IDC: raya identifica pin1; mantener 1:1.',
             'Abrir puentes invalida continuidad de los alias de ramas; tablas asumen montaje por defecto.', '',
             '## Fuentes', '']
    for board, source in sorted(netlists.items()):
        try:
            source_path = Path(source.source).resolve().relative_to(TOOLS.parents[4])
        except ValueError:
            source_path = source.source
        lines.append(f'- {board}: `{source_path}`')
    lines += ['', '## Hallazgos', '']
    lines.extend(f'- {e}' for e in errors)
    if not errors:
        lines.append('- Sin errores de correspondencia en los netlists leídos.')
    lines += ['- D14: retirar físicamente R1/R2 de PCB-A; el esquema fabricado sigue mostrándolos.',
              '- Jacks/selector/Sharp: confirmar terminales de la pieza comprada con continuidad; no hay SKU mecánico definitivo.',
              '- Anillos LINE IN: terminación pendiente; coincidencia del cable no valida una entrada balanceada.', '']
    for result in results:
        c = result['cable']
        label = 'PENDING' if result['pending'] else 'FAIL' if result['errors'] else 'PASS'
        lines += [f'## {c["name"]} — {label}', '',
                  f'`{".".join(c["a"])}` ↔ `{".".join(c["b"])}` · {c["ways"]} vías · {c["kind"]}.', '']
        if c.get('note'):
            lines += [c['note'], '']
        if c['b'][0] == 'panel':
            lines += [harness.PANEL.get(c['b'][1], {}).get('description', ''), '']
        lines += ['| Pin A → B | Red en A (XML) | Red en B (XML / esperado panel) | Equivalencia A / B | Color | Contacto pieza B | Estado |',
                  '|---|---|---|---|---|---|---|']
        for a, b, ra, rb, ca, cb, color, contact, status in result['rows']:
            lines.append('| ' + ' | '.join(map(cell, (f'{a} → {b}', ra, rb, f'{cell(ca)} / {cell(cb)}', color, contact, status))) + ' |')
        lines.append('')
    lines += ['## Conectores excluidos explícitamente', '']
    lines.extend(f'- `{b}.{r}`: {reason}' for (b, r), reason in sorted(excluded.items()))
    lines += ['', '## Límites', '',
              'No valida longitud/sección/corriente de cable, crimpado, ruido, hot-plug, DRC ni mediciones de banco.',
              'Excepciones D8 por pin y alias por placa en `harness.py`; no modificar alias para ocultar un cruce.', '']
    return '\n'.join(lines)


def main():
    """Read existing exports; optional PCB-A export writes only to the chosen report directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--boards-directory', type=Path, default=TOOLS.parent / 'boards')
    parser.add_argument('--pcb-a-netlist', type=Path)
    parser.add_argument('--export-pcb-a', action='store_true', help='Export A without changing any PCB-A file')
    parser.add_argument('--report', type=Path, default=TOOLS / 'HARNESS.md')
    args = parser.parse_args()
    sources, load_errors = {}, []
    pcb_a_path = args.pcb_a_netlist
    if pcb_a_path is None and (args.report.parent / 'pcb-a-harness-netlist.xml').exists():
        pcb_a_path = args.report.parent / 'pcb-a-harness-netlist.xml'
    if args.export_pcb_a:
        from check_netlist import export_netlist, find_kicad_cli
        args.report.parent.mkdir(parents=True, exist_ok=True)
        # Keep the independent A export beside the report, never under boards/pcb-a.
        pcb_a_path = args.report.parent / 'pcb-a-harness-netlist.xml'
        cli = find_kicad_cli()
        if not cli:
            load_errors.append('PCB-A export: kicad-cli unavailable')
        else:
            try:
                export_netlist(cli, args.boards_directory / 'pcb-a/pcb-a.kicad_sch', pcb_a_path)
            except (OSError, ValueError, subprocess.CalledProcessError) as error:
                load_errors.append(f'PCB-A export failed: {error}')
                pcb_a_path = None
    for board in harness.BOARDS:
        path = pcb_a_path if board == 'pcb-a' and pcb_a_path else args.boards_directory / board / 'docs' / f'{board}-netlist.xml'
        try:
            sources[board] = read_netlist(path)
        except (OSError, ValueError, ET.ParseError) as error:
            load_errors.append(f'{board}: {error}')
    results, errors = check_cables(sources, harness.CABLES, harness.ALIASES, harness.PANEL, harness.EXCLUDED)
    errors = load_errors + errors
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(markdown(results, errors, sources, harness.EXCLUDED))
    for result in results:
        print(f'{"PENDING" if result["pending"] else "FAIL" if result["errors"] else "PASS"} {result["cable"]["name"]}')
    for error in errors:
        print(f'ERROR {error}', file=sys.stderr)
    print(f'{len(results)} cables, {len(errors)} errors; report: {args.report}')
    return int(bool(errors))


if __name__ == '__main__':
    sys.exit(main())

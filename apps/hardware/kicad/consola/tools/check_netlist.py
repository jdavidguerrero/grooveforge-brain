#!/usr/bin/env python3
"""Export and verify KiCad connectivity, ERC and PDF against a declarative board."""

import argparse
import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

from schgen import Library, generate
from schgen.generator import net_scopes, validate
from schgen.pcb_m import pcb_m

DEFAULT_CLI = Path('/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli')
DEFAULT_BOARD = Path(__file__).resolve().parents[1] / 'boards/pcb-m'


def find_kicad_cli():
    """Locate KiCad on PATH or in its standard macOS application bundle."""
    return shutil.which('kicad-cli') or (str(DEFAULT_CLI) if DEFAULT_CLI.is_file() else None)


def expected_pins(board):
    """Return physical pin expectations, with KiCad's hierarchical local-net names."""
    scopes = net_scopes(board)
    expected = {}
    for sheet in board.sheets:
        for symbol in sheet.symbols:
            # KiCad excludes virtual # references (including PWR_FLAG) from PCB netlists.
            if symbol.reference.startswith('#'):
                continue
            for number, net in symbol.pins.items():
                key = symbol.reference, str(number)
                scoped = net if net is None or net in scopes else f'/{sheet.name}/{net}'
                if key in expected and expected[key] != scoped:
                    raise ValueError(f'Inconsistent shared pin {key}')
                expected[key] = scoped
    return expected


def compare_netlist(board, path):
    """Reject missing/extra pins, wrong names, split/merged nets and connected NC pins."""
    expected = expected_pins(board)
    tree = ET.parse(path)
    actual = {}
    errors = []
    expected_refs = {key[0] for key in expected}
    actual_refs = {node.attrib['ref'] for node in tree.findall('./components/comp')}
    if expected_refs != actual_refs:
        errors.append(f'Components missing={sorted(expected_refs - actual_refs)} '
                      f'extra={sorted(actual_refs - expected_refs)}')
    net_codes = {}
    expected_codes = {}
    for net in tree.findall('./nets/net'):
        keys = [(node.attrib['ref'], node.attrib['pin']) for node in net.findall('node')]
        wanted = {expected[key] for key in keys if key in expected and expected[key] is not None}
        if len(wanted) > 1:
            errors.append(f'Merged nets at {net.attrib["name"]}: {sorted(wanted)}')
        for node, key in zip(net.findall('node'), keys):
            if key in actual:
                errors.append(f'Pin appears in multiple nets: {key}')
            actual[key] = net.attrib['name']
            if key not in expected:
                errors.append(f'Undeclared pin: {key}')
                continue
            target = expected[key]
            if target is None:
                if (len(keys) != 1 or not net.attrib['name'].startswith('unconnected-(')
                        or 'no_connect' not in node.attrib.get('pintype', '')):
                    errors.append(f'NC pin connected or flag missing: {key} on {net.attrib["name"]}')
            elif net.attrib['name'] != target:
                errors.append(f'{key}: expected {target}, got {net.attrib["name"]}')
            if target is not None:
                code = net.attrib['code']
                expected_codes.setdefault(target, set()).add(code)
                net_codes.setdefault(code, set()).add(target)
    for name, codes in expected_codes.items():
        if len(codes) > 1:
            errors.append(f'Split net: {name} in net codes {sorted(codes)}')
    for code, names in net_codes.items():
        if len(names) > 1:
            errors.append(f'Merged net code {code}: {sorted(names)}')
    for key in expected.keys() - actual.keys():
        errors.append(f'Pin without exported net: {key}, expected {expected[key]}')
    if errors:
        raise ValueError('\n'.join(errors))
    return {'pins': len(expected), 'nc': sum(net is None for net in expected.values()),
            'nets': len(set(expected.values()) - {None}), 'components': len(expected_refs)}


def export_netlist(cli, schematic, output):
    """Run KiCad's real kicadxml exporter, raising if the command fails."""
    subprocess.run([str(cli), 'sch', 'export', 'netlist', '--format', 'kicadxml',
                    '--output', str(output), str(schematic)], check=True)


def check(board, directory, cli, regenerate=False, library=None):
    """Verify the hierarchy and refresh all review artifacts; raise on connectivity/ERC errors."""
    directory = Path(directory)
    library = library or Library()
    validate(board, library)
    schematic = directory / f'{board.name}.kicad_sch'
    if regenerate:
        generate(board, directory, library)
    docs = directory / 'docs'
    docs.mkdir(parents=True, exist_ok=True)
    netlist = docs / f'{board.name}-netlist.xml'
    export_netlist(cli, schematic, netlist)
    failures = []
    try:
        stats = compare_netlist(board, netlist)
        print(f'Connectivity PASS: {stats["components"]} components, {stats["pins"]} pins '
              f'({stats["nc"]} NC), {stats["nets"]} named nets')
    except ValueError as error:
        failures.append(str(error))
    report = docs / 'erc-report.txt'
    subprocess.run([str(cli), 'sch', 'erc', '--severity-all', '--output', str(report), str(schematic)], check=True)
    text = report.read_text()
    counts = re.search(r'Errors (\d+)\s+Warnings (\d+)', text)
    if not counts:
        failures.append('Could not parse ERC summary')
    else:
        print(f'ERC: {counts[1]} errors, {counts[2]} warnings ({report})')
        if int(counts[1]):
            failures.append(f'ERC errors remain: {counts[1]}')
    subprocess.run([str(cli), 'sch', 'export', 'pdf', '--output',
                    str(docs / f'{board.name}-schematic.pdf'), str(schematic)], check=True)
    if failures:
        raise ValueError('\n'.join(failures))


def main():
    """Refresh exports and verify PCB-M, optionally regenerating it in the same command."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--regenerate', action='store_true', help='Generate before exporting and checking')
    parser.add_argument('--directory', type=Path, default=DEFAULT_BOARD)
    parser.add_argument('--description', type=Path, help='Python file exporting board() -> Board')
    parser.add_argument('--libraries', type=Path, default=None)
    parser.add_argument('--kicad-cli', default=find_kicad_cli())
    args = parser.parse_args()
    if not args.kicad_cli:
        parser.error('kicad-cli not found; install KiCad 10 or pass --kicad-cli')
    board = pcb_m()
    if args.description:
        spec = importlib.util.spec_from_file_location('board_description', args.description)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        board = module.board()
    try:
        check(board, args.directory, args.kicad_cli, args.regenerate,
              Library(args.libraries) if args.libraries else Library())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

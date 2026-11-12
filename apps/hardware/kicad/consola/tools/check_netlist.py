#!/usr/bin/env python3
"""Export and verify KiCad connectivity, ERC and PDF against a declarative board."""

import argparse
import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

from schgen import DEFAULT_LIBRARIES, Library, generate
from schgen.sexp import Quoted as Q, dumps
from schgen.generator import net_scopes, validate
from schgen.boards import BOARDS
from schgen.console_artifacts import write_artifacts

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


def export_netlist(cli, schematic, output, libraries=DEFAULT_LIBRARIES):
    """Run KiCad's real kicadxml exporter, raising if the command fails."""
    run_kicad([str(cli), 'sch', 'export', 'netlist', '--format', 'kicadxml',
               '--output', str(output), str(schematic)], Path(output).parent, libraries)


def run_kicad(command, directory, libraries=DEFAULT_LIBRARIES):
    """Keep KiCad temporary files/config inside the output tree, including in tests."""
    with tempfile.TemporaryDirectory(prefix='.kicad-runtime-', dir=Path(directory).resolve()) as runtime:
        env = os.environ.copy()
        env.update({key: runtime for key in ('TMPDIR', 'TMP', 'TEMP',
                    'KICAD_CONFIG_HOME', 'XDG_CONFIG_HOME', 'XDG_CACHE_HOME')})
        libraries = Path(libraries).resolve()
        # Fresh config must still resolve real libraries, rather than hiding link warnings.
        for config in (Path(runtime), Path(runtime) / '10.0'):
            config.mkdir(exist_ok=True)
            for table, paths in [('sym_lib_table', libraries.glob('*.kicad_sym')),
                                 ('fp_lib_table', (libraries.parent / 'footprints').glob('*.pretty'))]:
                nodes = [table, ['version', '7']]
                for path in sorted(paths):
                    nodes.append(['lib', ['name', Q(path.stem)], ['type', Q('KiCad')],
                                  ['uri', Q(str(path))], ['options', Q('')], ['descr', Q('')]])
                (config / table.replace('_', '-')).write_text(dumps(nodes)+'\n')
        subprocess.run(command, check=True, env=env)


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
    export_netlist(cli, schematic, netlist, library.directory)
    failures = []
    try:
        stats = compare_netlist(board, netlist)
        print(f'Connectivity PASS: {stats["components"]} components, {stats["pins"]} pins '
              f'({stats["nc"]} NC), {stats["nets"]} named nets')
    except ValueError as error:
        failures.append(str(error))
    report = docs / 'erc-report.txt'
    run_kicad([str(cli), 'sch', 'erc', '--severity-all', '--output', str(report), str(schematic)], docs, library.directory)
    text = report.read_text()
    counts = re.search(r'Errors (\d+)\s+Warnings (\d+)', text)
    if not counts:
        failures.append('Could not parse ERC summary')
    else:
        print(f'ERC: {counts[1]} errors, {counts[2]} warnings ({report})')
        if int(counts[1]):
            failures.append(f'ERC errors remain: {counts[1]}')
    run_kicad([str(cli), 'sch', 'export', 'pdf', '--output',
               str(docs / f'{board.name}-schematic.pdf'), str(schematic)], docs, library.directory)
    if failures:
        raise ValueError('\n'.join(failures))


def main():
    """Verify a registered board, every registered board, or an external description."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--regenerate', action='store_true', help='Generate before exporting and checking')
    parser.add_argument('--directory', type=Path, help='Board directory; parent of board directories with --all')
    source = parser.add_mutually_exclusive_group()
    source.add_argument('--board', choices=BOARDS, default='pcb-m')
    source.add_argument('--all', action='store_true', help='Check PCB-M and all four Console boards')
    source.add_argument('--description', type=Path, help='Python file exporting board() -> Board')
    parser.add_argument('--libraries', type=Path, default=None)
    parser.add_argument('--kicad-cli', default=find_kicad_cli())
    args = parser.parse_args()
    if not args.kicad_cli:
        parser.error('kicad-cli not found; install KiCad 10 or pass --kicad-cli')
    boards = [factory() for factory in BOARDS.values()] if args.all else [BOARDS[args.board]()]
    if args.description:
        spec = importlib.util.spec_from_file_location('board_description', args.description)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        boards = [module.board()]
    failed = False
    library = Library(args.libraries) if args.libraries else Library()
    for board in boards:
        if args.directory:
            directory = args.directory / board.name if args.all else args.directory
        else:
            directory = DEFAULT_BOARD.parent / board.name
        print(f'=== {board.name} ===', flush=True)
        try:
            if args.regenerate and not args.description:
                validate(board, library)
                write_artifacts(board.name, directory)
            check(board, directory, args.kicad_cli, args.regenerate, library)
        except (ValueError, OSError, subprocess.CalledProcessError) as error:
            print(f'FAIL {board.name}: {error}', file=sys.stderr)
            failed = True
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())

"""CLI: python3 -m schgen --board pcb-m --output path/to/pcb-m."""

import argparse
import importlib.util
from pathlib import Path

from .generator import DEFAULT_LIBRARIES, Library, generate
from .pcb_m import pcb_m


def main():
    """Generate PCB-M or a Python description exporting a zero-argument board() factory."""
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument('--board', choices=['pcb-m'], default='pcb-m')
    source.add_argument('--description', type=Path, help='Python file exporting board() -> Board')
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[2] / 'boards/pcb-m')
    parser.add_argument('--libraries', type=Path, default=DEFAULT_LIBRARIES)
    args = parser.parse_args()
    if args.description:
        spec = importlib.util.spec_from_file_location('board_description', args.description)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        board = module.board()
    else:
        board = pcb_m()
    print(generate(board, args.output, Library(args.libraries)))


if __name__ == '__main__':
    main()

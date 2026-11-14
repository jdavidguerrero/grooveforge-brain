"""CLI: python3 -m schgen --board pcb-k-left --output path/to/pcb-k-left."""

import argparse
import importlib.util
from pathlib import Path

from .generator import DEFAULT_LIBRARIES, Library, generate
from .boards import BOARDS
from .console_artifacts import write_artifacts


def main():
    """Generate a registered board or an external zero-argument board() factory."""
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument('--board', choices=BOARDS, default='pcb-m')
    source.add_argument('--description', type=Path, help='Python file exporting board() -> Board')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--libraries', type=Path, default=DEFAULT_LIBRARIES)
    args = parser.parse_args()
    if args.description:
        spec = importlib.util.spec_from_file_location('board_description', args.description)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        board = module.board()
    else:
        board = BOARDS[args.board]()
    output = args.output or Path(__file__).resolve().parents[2] / 'boards' / board.name
    print(generate(board, output, Library(args.libraries)))
    if not args.description:
        write_artifacts(board.name, output)


if __name__ == '__main__':
    main()

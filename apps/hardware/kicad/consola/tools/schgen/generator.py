"""Geometry, library embedding and electrical connectivity for KiCad schematics."""

import copy
from dataclasses import dataclass, field
from functools import lru_cache
import math
from pathlib import Path
import re
import uuid

from .sexp import Quoted as Q, parse, dumps, child, children

DEFAULT_LIBRARIES = Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols')
NAMESPACE = uuid.UUID('f3a6f61e-094a-5ce2-91f5-2e34fdfd7d65')
GRID = 1.27
PAPER_SIZES = {'A4': (297, 210), 'A3': (420, 297), 'A2': (594, 420),
               'A1': (841, 594), 'A0': (1189, 841)}


@dataclass
class Symbol:
    """One unit of a library symbol; pins map physical numbers to nets or None (NC)."""

    lib_id: str
    reference: str
    value: str
    footprint: str
    position: tuple
    pins: dict
    rotation: int = 0
    unit: int = 1
    dnp: bool = False
    fields: dict = field(default_factory=dict)
    in_bom: bool = True


@dataclass
class Sheet:
    """A named sheet with explicitly positioned symbol units and optional notes."""

    name: str
    symbols: list = field(default_factory=list)
    paper: str = 'A3'
    notes: list = field(default_factory=list)


@dataclass
class Board:
    """An independently generated hierarchy; global_nets overrides scope inference."""

    name: str
    sheets: list
    global_nets: set = field(default_factory=set)


def stable_uuid(name):
    """Return a UUID v5 derived solely from a persistent object name."""
    return str(uuid.uuid5(NAMESPACE, name))


def pin_position(position, pin_xy, rotation):
    """Transform a library point to sheet coordinates, including the inverted Y axis."""
    theta = math.radians(rotation)
    px, py = pin_xy
    x, y = position
    return (round(x + px * math.cos(theta) - py * math.sin(theta), 6),
            round(y - px * math.sin(theta) - py * math.cos(theta), 6))


def check_collisions(boxes):
    """Reject overlapping (name, (xmin, ymin, xmax, ymax)) drawing envelopes."""
    for index, (name, box) in enumerate(boxes):
        for other, bounds in boxes[:index]:
            if (max(box[0], bounds[0]) < min(box[2], bounds[2]) and
                    max(box[1], bounds[1]) < min(box[3], bounds[3])):
                raise ValueError(f'Collision: {name} with {other}: {box}, {bounds}')


class Library:
    """Read standard KiCad libraries and flatten inherited symbols for embedding."""

    def __init__(self, directory=DEFAULT_LIBRARIES):
        self.directory = Path(directory)

    @lru_cache(maxsize=None)
    def _library(self, name):
        tree = parse((self.directory / f'{name}.kicad_sym').read_text())
        return {str(s[1]): s for s in children(tree, 'symbol')}

    @lru_cache(maxsize=None)
    def definition(self, lib_id):
        """Return a standalone library definition, resolving any extends chain."""
        library, name = lib_id.split(':', 1)
        node = copy.deepcopy(self._library(library)[name])
        parents = children(node, 'extends')
        if parents:
            parent = copy.deepcopy(self.definition(f'{library}:{parents[0][1]}'))
            inherited_name = str(parent[1]).split(':')[-1]
            overrides = {p[1]: p for p in children(node, 'property')}
            merged = []
            for item in parent[2:]:
                if isinstance(item, list) and item[0] == 'property' and item[1] in overrides:
                    merged.append(overrides.pop(item[1]))
                else:
                    merged.append(item)
            merged.extend(overrides.values())
            for item in children(['symbol'] + merged, 'symbol'):
                item[1] = Q(str(item[1]).replace(inherited_name + '_', name + '_', 1))
            node = ['symbol', Q(lib_id)] + merged
        node[1] = Q(lib_id)
        return node

    def units(self, lib_id, unit):
        """Return graphical sub-units shared with or belonging to the selected unit."""
        selected = []
        for node in children(self.definition(lib_id), 'symbol'):
            match = re.search(r'_(\d+)_(\d+)$', node[1])
            if match and int(match[1]) in (0, unit) and int(match[2]) in (0, 1):
                selected.append(node)
        return selected

    def pins(self, symbol):
        """Return all physical pins for one unit, including common power pins."""
        return [p for unit in self.units(symbol.lib_id, symbol.unit) for p in children(unit, 'pin')]


def _number(value):
    return str(round(float(value), 6)).rstrip('0').rstrip('.') if float(value) else '0'


def _at(x, y, angle=0):
    return ['at', _number(x), _number(y), str(angle)]


def _effects(size=1.0, justify=None):
    result = ['effects', ['font', ['size', str(size), str(size)]]]
    if justify:
        result.append(['justify', justify])
    return result


def _property(name, value, x, y, hidden=False):
    result = ['property', Q(name), Q(value), _at(x, y), _effects()]
    if hidden:
        result[-1].append(['hide', 'yes'])
    return result


def _pin_geometry(symbol, pin):
    at = child(pin, 'at')
    start = pin_position(symbol.position, tuple(map(float, at[1:3])), symbol.rotation)
    angle = (float(at[3]) + symbol.rotation) % 360
    # Library pin angles face inward; wires must leave the body in the opposite direction.
    end = (round(start[0] - 2.54 * math.cos(math.radians(angle)), 6),
           round(start[1] + 2.54 * math.sin(math.radians(angle)), 6))
    return start, end, int(angle)


def _drawing_bounds(library, symbol):
    points = []

    def visit(node):
        if not isinstance(node, list) or not node:
            return
        if node[0] in ('start', 'end', 'mid', 'center', 'xy', 'at') and len(node) >= 3:
            try:
                points.append(pin_position(symbol.position, (float(node[1]), float(node[2])), symbol.rotation))
            except ValueError:
                pass
        if node[0] == 'circle':
            center = child(node, 'center')
            radius = float(child(node, 'radius')[1])
            for sign_x in (-1, 1):
                for sign_y in (-1, 1):
                    points.append(pin_position(symbol.position,
                                  (float(center[1]) + sign_x * radius, float(center[2]) + sign_y * radius),
                                  symbol.rotation))
        for item in node:
            if isinstance(item, list):
                visit(item)

    for unit in library.units(symbol.lib_id, symbol.unit):
        visit(unit)
    if not points:
        raise ValueError(f'Empty unit: {symbol.reference}/{symbol.unit}')
    return points


def _envelope(library, symbol):
    points = _drawing_bounds(library, symbol)
    for pin in library.pins(symbol):
        number = str(child(pin, 'number')[1])
        _, end, angle = _pin_geometry(symbol, pin)
        if symbol.pins[number] is not None:
            width = 2.54 + len(symbol.pins[number]) * 1.27
            dx = -math.cos(math.radians(angle)) * width
            dy = math.sin(math.radians(angle)) * width
            points.extend([end, (end[0] + dx, end[1] + dy)])
    xs, ys = zip(*points)
    # Reserve a top band for the reference and value, including long connector names.
    text_width = max(len(symbol.reference), len(symbol.value)) * 1.27
    return (min(min(xs) - 1.27, symbol.position[0] - text_width / 2), min(ys) - 6.35,
            max(max(xs) + 1.27, symbol.position[0] + text_width / 2), max(ys) + 2.54)


def validate(board, library):
    """Validate pins, grid, references, units and drawing collisions before any writes."""
    seen = set()
    reference_data = {}
    if not board.sheets or len({s.name for s in board.sheets}) != len(board.sheets):
        raise ValueError('Sheet names must be unique and nonempty')
    if not re.fullmatch(r'[A-Za-z0-9_-]+', board.name):
        raise ValueError('Board name must be a safe filename stem')
    for sheet in board.sheets:
        if not re.fullmatch(r'[A-Za-z0-9_-]+', sheet.name):
            raise ValueError('Sheet name must be a safe filename stem')
        if sheet.paper not in PAPER_SIZES:
            raise ValueError(f'Unsupported paper size: {sheet.paper}')
        boxes = []
        for symbol in sheet.symbols:
            key = symbol.reference, symbol.unit
            if key in seen:
                raise ValueError(f'Duplicate reference/unit: {key}')
            seen.add(key)
            identity = (symbol.lib_id, symbol.value, symbol.footprint, symbol.dnp, symbol.fields, symbol.in_bom)
            if symbol.reference in reference_data and reference_data[symbol.reference] != identity:
                raise ValueError(f'Inconsistent multi-unit symbol: {symbol.reference}')
            reference_data[symbol.reference] = identity
            if symbol.rotation not in (0, 90, 180, 270):
                raise ValueError(f'Rotation must be a quarter turn: {key}')
            if any(abs(v / GRID - round(v / GRID)) > 1e-6 for v in symbol.position):
                raise ValueError(f'Position off 1.27 mm grid: {key}')
            physical = {str(child(p, 'number')[1]) for p in library.pins(symbol)}
            if not physical or physical != set(symbol.pins):
                raise ValueError(f'{key}: missing pins {physical - set(symbol.pins)}, '
                                 f'unknown pins {set(symbol.pins) - physical}')
            if any(net is not None and (not isinstance(net, str) or not net or net.startswith('/'))
                   for net in symbol.pins.values()):
                raise ValueError(f'Invalid net name: {key}')
            bounds = _envelope(library, symbol)
            width, height = PAPER_SIZES[sheet.paper]
            if bounds[0] < 10 or bounds[1] < 10 or bounds[2] > width - 10 or bounds[3] > height - 10:
                raise ValueError(f'Symbol outside drawing frame: {key} on {sheet.paper}: {bounds}')
            boxes.append((f'{symbol.reference}/{symbol.unit}', bounds))
        check_collisions(boxes)


def net_scopes(board):
    """Infer global scope for rails and nets present in more than one sheet."""
    sheets_by_net = {}
    for sheet in board.sheets:
        for symbol in sheet.symbols:
            for net in symbol.pins.values():
                if net is not None:
                    sheets_by_net.setdefault(net, set()).add(sheet.name)
    return {net for net, sheets in sheets_by_net.items()
            if len(sheets) > 1 or net.startswith('+') or net in ('GND', 'AGND')} | set(board.global_nets)


def _header(board, name, paper):
    return ['kicad_sch', ['version', '20260101'], ['generator', Q('schgen')],
            ['generator_version', Q('1.0')], ['uuid', stable_uuid(f'{board.name}/{name}')],
            ['paper', Q(paper)]]


def _sheet_tree(board, sheet, library, global_nets):
    tree = _header(board, sheet.name, sheet.paper)
    tree.append(['lib_symbols'] + [copy.deepcopy(library.definition(lib_id))
                                  for lib_id in sorted({s.lib_id for s in sheet.symbols})])
    root_id = stable_uuid(f'{board.name}/root')
    sheet_id = stable_uuid(f'{board.name}/sheet/{sheet.name}')
    for symbol in sheet.symbols:
        key = f'{board.name}/{sheet.name}/{symbol.reference}/{symbol.unit}'
        identifier = stable_uuid(key)
        bounds = _envelope(library, symbol)
        x, y = symbol.position
        node = ['symbol', ['lib_id', Q(symbol.lib_id)], _at(x, y, symbol.rotation),
                ['unit', str(symbol.unit)], ['in_bom', 'yes' if symbol.in_bom else 'no'], ['on_board', 'yes'],
                ['dnp', 'yes' if symbol.dnp else 'no'], ['uuid', identifier],
                _property('Reference', symbol.reference, x, bounds[1] + 1.27),
                _property('Value', symbol.value, x, bounds[1] + 3.81),
                _property('Footprint', symbol.footprint, x, y, True)]
        node.extend(_property(name, value, x, y, True)
                    for name, value in sorted(symbol.fields.items()))
        for pin in library.pins(symbol):
            number = str(child(pin, 'number')[1])
            start, end, angle = _pin_geometry(symbol, pin)
            pin_id = f'{key}/pin/{number}'
            node.append(['pin', Q(number), ['uuid', stable_uuid(pin_id)]])
            net = symbol.pins[number]
            if net is None:
                tree.append(['no_connect', _at(*start)[:3], ['uuid', stable_uuid(pin_id + '/nc')]])
                continue
            tree.append(['wire', ['pts', ['xy', *map(_number, start)], ['xy', *map(_number, end)]],
                         ['stroke', ['width', '0'], ['type', 'default']],
                         ['uuid', stable_uuid(pin_id + '/wire')]])
            global_net = net in global_nets
            label = ['global_label' if global_net else 'label', Q(net)]
            if global_net:
                label.append(['shape', 'bidirectional'])
            label.extend([_at(*end, angle), _effects(justify='right' if angle in (0, 90) else 'left'),
                          ['uuid', stable_uuid(pin_id + '/label')]])
            if global_net:
                label.append(_property('Intersheetrefs', '${INTERSHEET_REFS}', *end, hidden=True))
            tree.append(label)
        node.append(['instances', ['project', Q(board.name),
                     ['path', Q(f'/{root_id}/{sheet_id}'), ['reference', Q(symbol.reference)],
                      ['unit', str(symbol.unit)]]]])
        tree.append(node)
    for index, note in enumerate(sheet.notes):
        tree.append(['text', Q(note), _at(15.24, 20.32 + index * 3.81), _effects(justify='left'),
                     ['uuid', stable_uuid(f'{board.name}/{sheet.name}/note/{index}')]])
    return tree


def generate(board, output, library=None):
    """Validate and write a complete deterministic hierarchy; return the root path."""
    library = library or Library()
    validate(board, library)
    output = Path(output)
    rows = math.ceil(len(board.sheets) / 3)
    papers = [name for name, (_, height) in PAPER_SIZES.items()
              if 20.32 + (rows - 1) * 43.18 + 38.1 < height - 50]
    if not papers:
        raise ValueError('Too many sheets for a single root drawing')
    root = _header(board, 'root', papers[0])
    root.append(['lib_symbols'])
    root_id = stable_uuid(f'{board.name}/root')
    scopes = net_scopes(board)
    trees = []
    for index, sheet in enumerate(board.sheets):
        x, y = 20.32 + (index % 3) * 88.9, 20.32 + (index // 3) * 43.18
        root.append(['sheet', _at(x, y)[:3], ['size', '76.2', '35.56'],
                     ['stroke', ['width', '0.1524'], ['type', 'default']], ['fill', ['color', '0', '0', '0', '0']],
                     ['uuid', stable_uuid(f'{board.name}/sheet/{sheet.name}')],
                     _property('Sheetname', sheet.name, x + 38.1, y - 2.54),
                     _property('Sheetfile', f'sheets/{sheet.name}.kicad_sch', x + 38.1, y + 38.1),
                     ['instances', ['project', Q(board.name),
                      ['path', Q(f'/{root_id}'), ['page', Q(str(index + 2))]]]]])
        trees.append((sheet.name, _sheet_tree(board, sheet, library, scopes)))
    root.append(['sheet_instances', ['path', Q('/'), ['page', Q('1')]]])
    (output / 'sheets').mkdir(parents=True, exist_ok=True)
    for name, tree in trees:
        (output / 'sheets' / f'{name}.kicad_sch').write_text(dumps(tree) + '\n')
    path = output / f'{board.name}.kicad_sch'
    path.write_text(dumps(root) + '\n')
    return path

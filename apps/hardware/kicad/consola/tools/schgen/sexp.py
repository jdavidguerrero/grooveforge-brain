"""Small, lossless atom reader for KiCad S expressions (standard library only)."""

import json
import re


class Quoted(str):
    """A string atom that must remain quoted when serialized."""


def parse(text):
    """Read exactly one S expression, preserving quoted versus bare atoms."""
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+', text)
    stack = []
    result = None
    for token in tokens:
        if token == '(':
            node = []
            if stack:
                stack[-1].append(node)
            elif result is not None:
                raise ValueError('Multiple root expressions')
            else:
                result = node
            stack.append(node)
        elif token == ')':
            if not stack:
                raise ValueError('Unexpected closing parenthesis')
            stack.pop()
        else:
            if not stack:
                raise ValueError('Atom outside expression')
            stack[-1].append(Quoted(json.loads(token)) if token.startswith('"') else token)
    if stack or result is None:
        raise ValueError('Incomplete expression')
    return result


def dumps(node, depth=0):
    """Serialize an expression deterministically with readable nesting."""
    if not isinstance(node, list):
        return json.dumps(str(node), ensure_ascii=False) if isinstance(node, Quoted) else str(node)
    if not any(isinstance(item, list) for item in node):
        return '(' + ' '.join(dumps(item) for item in node) + ')'
    lines = ['(' + ' '.join(dumps(item) for item in node if not isinstance(item, list))]
    lines.extend('  ' * (depth + 1) + dumps(item, depth + 1) for item in node if isinstance(item, list))
    return '\n'.join(lines) + ')'


def children(node, key):
    """Return immediate child expressions with the given head."""
    return [item for item in node if isinstance(item, list) and item and item[0] == key]


def child(node, key):
    """Return the first matching child, or raise for malformed input."""
    return children(node, key)[0]

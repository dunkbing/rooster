#!/usr/bin/env python3
"""Merges GN argument files into one args.gn; later files override earlier ones.

Each input holds `name = value` lines (comments and blank lines allowed).
Extra `name=value` pairs on the command line override every file.
"""

import argparse
import re
import sys
from pathlib import Path

LINE = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+?)\s*$')


def parse(text, origin):
    args = {}
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        match = LINE.match(line)
        if not match:
            sys.exit(f'{origin}:{number}: not a `name = value` line: {raw}')
        args[match.group(1)] = match.group(2)
    return args


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files', nargs='+', type=Path)
    parser.add_argument('--set', action='append', default=[], metavar='NAME=VALUE')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()

    merged = {}
    for path in args.files:
        merged.update(parse(path.read_text(encoding='utf-8'), path))
    merged.update(parse('\n'.join(args.set), '--set'))

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(''.join(f'{k} = {v}\n' for k, v in merged.items()), encoding='utf-8')


if __name__ == '__main__':
    main()

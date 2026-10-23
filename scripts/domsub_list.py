#!/usr/bin/env python3
"""Writes Helium's domain substitution list minus config/domsub-keep.txt.

Every kept path must be on Helium's list, so an entry that stops mattering
after an upstream change is noticed instead of lingering.
"""

import argparse
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', type=Path, required=True, help="helium's domain_substitution.list")
    parser.add_argument('--keep', type=Path, required=True, help='config/domsub-keep.txt')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()

    keep = set()
    for line in args.keep.read_text(encoding='utf-8').splitlines():
        if line.strip() and not line.startswith('#'):
            keep.add(line.split()[0])
    paths = [line.strip() for line in args.list.read_text(encoding='utf-8').splitlines() if line.strip()]

    missing = keep - set(paths)
    if missing:
        sys.exit('domsub-keep.txt names files not on the substitution list:\n  ' +
                 '\n  '.join(sorted(missing)))
    args.out.write_text('\n'.join(p for p in paths if p not in keep) + '\n', encoding='utf-8')
    print(f'domain substitution: {len(paths) - len(keep)} files, {len(keep)} kept as-is')


if __name__ == '__main__':
    main()

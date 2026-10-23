#!/usr/bin/env python3
"""Copies Rooster's src/ overlay into the Chromium tree.

The overlay holds files Chromium doesn't have: Rooster's own code (//rooster)
and the vendored crates for adblock-rust. Changes to Chromium's own files are
patches instead, so this refuses to overwrite anything that already exists.
"""

import argparse
import shutil
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--overlay', type=Path, required=True, help="rooster's src/")
    parser.add_argument('--tree', type=Path, required=True, help='Chromium src')
    args = parser.parse_args()

    files = [p for p in args.overlay.rglob('*') if p.is_file()]
    clashes = [p.relative_to(args.overlay) for p in files
               if (args.tree / p.relative_to(args.overlay)).exists()]
    if clashes:
        sys.exit('overlay files already exist in the tree (make them patches):\n  ' +
                 '\n  '.join(str(p) for p in sorted(clashes)))

    for path in files:
        target = args.tree / path.relative_to(args.overlay)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    print(f'overlay: {len(files)} files')


if __name__ == '__main__':
    main()

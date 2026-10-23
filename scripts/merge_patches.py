#!/usr/bin/env python3
"""Builds the combined patch series Rooster applies to Chromium.

Order: helium's series, then helium-macos's, then Rooster's own, minus every
patch named in config/skip.txt. The result is a directory of symlinks plus a
`series` file, in the layout helium's utils/patches.py expects.

A skip entry that no longer exists upstream is an error, so a renamed or
dropped Helium patch is noticed instead of silently applying again.
"""

import argparse
import shutil
import sys
from pathlib import Path


def read_series(patches_dir):
    """Returns the patch paths listed in a series file, in order."""
    lines = (patches_dir / 'series').read_text(encoding='utf-8').splitlines()
    return [line.strip() for line in lines if line.strip() and not line.startswith('#')]


def read_skip_list(path):
    """Parses config/skip.txt: `<patch path>  <reason>` per line."""
    skips = {}
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        name, _, reason = line.partition(' ')
        if not reason.strip():
            sys.exit(f'{path}:{number}: skipped patch {name} needs a reason')
        skips[name] = reason.strip()
    return skips


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helium', type=Path, required=True, help='helium checkout')
    parser.add_argument('--helium-macos', type=Path, help='helium-macos checkout')
    parser.add_argument('--rooster', type=Path, required=True, help='rooster repo root')
    parser.add_argument('--skip', type=Path, required=True, help='skip list')
    parser.add_argument('--out', type=Path, required=True, help='output directory')
    args = parser.parse_args()

    sources = [args.helium / 'patches']
    if args.helium_macos:
        sources.append(args.helium_macos / 'patches')
    sources.append(args.rooster / 'patches')

    skips = read_skip_list(args.skip)
    unused_skips = set(skips)

    if args.out.exists():
        shutil.rmtree(args.out)
    args.out.mkdir(parents=True)

    series = []
    for source in sources:
        for name in read_series(source):
            if name in skips:
                unused_skips.discard(name)
                continue
            if name in series:
                sys.exit(f'{name} is listed by more than one series')
            target = args.out / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.symlink_to((source / name).resolve())
            series.append(name)

    if unused_skips:
        sys.exit('skip list names patches no series has:\n  ' + '\n  '.join(sorted(unused_skips)))

    (args.out / 'series').write_text('\n'.join(series) + '\n', encoding='utf-8')
    print(f'{len(series)} patches, {len(skips)} skipped')


if __name__ == '__main__':
    main()

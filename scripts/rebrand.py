#!/usr/bin/env python3
"""Turns the Helium-patched Chromium tree into Rooster.

Runs after Helium's own steps (patches, domain and name substitution, i18n,
resources), driven by config/branding.ini:

  branding  chrome/app/theme/chromium/BRANDING (product name, bundle id, ...)
  source    identifiers Helium hard-codes in source: internal scheme, Keychain
            item, profile directory, crash product name, flag labels. Every
            entry must match exactly, so a Helium update that moves one fails
            here instead of shipping a half-renamed browser.
  strings   "Helium" -> product name and helium:// -> <scheme>:// in every
            .grd/.grdp message and .xtb translation, re-keying translations
            the same way Helium's own name substitution does.
"""

import argparse
import configparser
import re
import sys
from pathlib import Path


def load_branding(path):
    config = configparser.ConfigParser()
    config.read(path, encoding='utf-8')
    branding = {
        'name': config['product']['name'].strip(),
        'company': config['product']['company'].strip(),
        'scheme': config['product']['scheme'].strip(),
        'bundle_id': config['mac']['bundle_id'].strip(),
        'team_id': config['mac'].get('team_id', '').strip(),
    }
    if not re.fullmatch(r'[a-z][a-z0-9+.-]*', branding['scheme']):
        sys.exit(f'invalid scheme: {branding["scheme"]!r}')
    if not re.fullmatch(r'[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+', branding['bundle_id']):
        sys.exit(f'invalid bundle_id: {branding["bundle_id"]!r}')
    return branding


def write_file(path, text):
    """Replaces the file through a new inode, so hard-linked copies of the
    source tree (used to try patch sets cheaply) keep their content."""
    tmp = path.with_name(path.name + '.rooster-tmp')
    tmp.write_text(text, encoding='utf-8')
    tmp.chmod(path.stat().st_mode)
    tmp.replace(path)


def rewrite_branding(tree, b):
    path = tree / 'chrome/app/theme/chromium/BRANDING'
    values = {
        'COMPANY_FULLNAME': b['company'],
        'COMPANY_SHORTNAME': b['company'],
        'PRODUCT_FULLNAME': b['name'],
        'PRODUCT_SHORTNAME': b['name'],
        'PRODUCT_INSTALLER_FULLNAME': f'{b["name"]} Installer',
        'PRODUCT_INSTALLER_SHORTNAME': f'{b["name"]} Installer',
        'COPYRIGHT': f'Copyright @LASTCHANGE_YEAR@ {b["company"]}. All rights reserved.',
        'MAC_BUNDLE_ID': b['bundle_id'],
        'MAC_TEAM_ID': b['team_id'],
    }
    lines = path.read_text(encoding='utf-8').splitlines()
    seen = set()
    for i, line in enumerate(lines):
        key = line.split('=', 1)[0]
        if key in values:
            lines[i] = f'{key}={values[key]}'
            seen.add(key)
    missing = set(values) - seen
    if missing:
        sys.exit(f'{path}: missing keys {sorted(missing)}')
    write_file(path, '\n'.join(lines) + '\n')


def source_replacements(b):
    """(path, old, new) edits; each old must occur exactly once."""
    name, scheme = b['name'], b['scheme']
    return [
        ('content/public/common/url_constants.h',
         'kHeliumUIScheme[] = "helium";', f'kHeliumUIScheme[] = "{scheme}";'),
        ('components/url_formatter/url_fixer.cc',
         'kHeliumUIScheme[] = "helium";', f'kHeliumUIScheme[] = "{scheme}";'),
        ('chrome/browser/resources/tab_search/tab_search_item.ts',
         "protocol === 'helium:'", f"protocol === '{scheme}:'"),
        ('chrome/browser/resources/tab_search/tab_search_item.ts',
         "createTextNode('helium://')", f"createTextNode('{scheme}://')"),
        ('chrome/common/chrome_paths_mac.mm',
         'product_dir_name = "net.imput.helium";', f'product_dir_name = "{b["bundle_id"]}";'),
        ('components/os_crypt/common/keychain_password_mac.mm',
         'kDefaultServiceName[] = "Helium Storage Key";',
         f'kDefaultServiceName[] = "{name} Storage Key";'),
        ('components/os_crypt/common/keychain_password_mac.mm',
         'kDefaultAccountName[] = "Helium";', f'kDefaultAccountName[] = "{name}";'),
        ('chrome/app/chrome_crash_reporter_client.cc',
         'const char* product_name = "Helium";', f'const char* product_name = "{name}";'),
        ('chrome/installer/mac/signing/chromium_config.py',
         "return 'Helium'", f"return '{name}'"),
    ]


def rewrite_source(tree, b):
    for path, old, new in source_replacements(b):
        file = tree / path
        text = file.read_text(encoding='utf-8')
        if text.count(old) != 1:
            sys.exit(f'{path}: expected one "{old}", found {text.count(old)}')
        write_file(file, text.replace(old, new))

    # chrome://flags labels Helium's flags "Helium flag."; only whole words, so
    # identifiers like kHeliumNoiseCommandLine are left alone.
    flags = tree / 'chrome/browser/helium_flag_entries.h'
    text, count = re.subn(r'\bHelium\b', b['name'], flags.read_text(encoding='utf-8'))
    if not count:
        sys.exit(f'{flags}: no Helium flag labels found')
    write_file(flags, text)


def rewrite_strings(tree, helium, b):
    """Reuses Helium's name substitution, with Helium as the name to replace.

    Runs in this process: worker processes started with "spawn" (the macOS
    default) would re-import Helium's own Chrome -> Helium patterns.
    """
    sys.path.insert(0, str(helium / 'utils'))
    import name_substitution
    import name_substitution_utils as util

    util.REPLACEMENT_REGEXES = [(re.compile(pattern), replacement) for pattern, replacement in [
        (r'(\b)helium://', rf'\1{b["scheme"]}://'),
        (r'\bHelium\b', b['name']),
    ]]
    util.add_grit_to_path(tree)

    grit_files = name_substitution.get_substitutable_files(tree, ['grd', 'grdp'])
    results = [name_substitution.substitute_grit_file((path, tree, False, False))
               for path in grit_files]
    fp_map = util.merge_fp_maps([r for r in results if r is not None])

    xtb_files = name_substitution.get_substitutable_files(tree, ['xtb'])
    changed = sum(
        1 for path in xtb_files
        if name_substitution.substitute_xtb_file((path, tree, fp_map, False, False)))
    print(f'strings: {sum(r is not None for r in results)} grit files, {changed} xtb files')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--tree', type=Path, required=True, help='Chromium src')
    parser.add_argument('--helium', type=Path, required=True, help='helium checkout')
    parser.add_argument('--config', type=Path, required=True, help='branding.ini')
    parser.add_argument('--only', choices=['branding', 'source', 'strings'], action='append')
    args = parser.parse_args()

    b = load_branding(args.config)
    stages = args.only or ['branding', 'source', 'strings']
    if 'branding' in stages:
        rewrite_branding(args.tree, b)
    if 'source' in stages:
        rewrite_source(args.tree, b)
    if 'strings' in stages:
        rewrite_strings(args.tree, args.helium, b)
    print(f'rebranded to {b["name"]} ({", ".join(stages)})')


if __name__ == '__main__':
    main()

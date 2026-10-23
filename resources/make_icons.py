#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""Generates every Rooster logo asset from one geometric definition.

The mark is a rooster head built from circles, an oval and a triangle, so the
same shapes can be written as raster PNGs (Pillow, 4x supersampled), an SVG
and Chromium's vector .icon format. Outputs go to resources/branding/ and are
committed; rerun this after changing the design:

    uv run resources/make_icons.py

uv installs Pillow for that run only. The wordmark uses Inter when installed,
else a fallback font.
"""

import io
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
OUT = HERE / 'branding'

# Colors (RGB).
FLAME = (232, 71, 28)      # gradient bottom
AMBER = (255, 167, 38)     # gradient top
WHITE = (255, 251, 245)
BEAK = (255, 210, 74)
COMB = (200, 30, 30)
EYE = (58, 26, 10)
INK = (24, 24, 24)

# The mark in a 100x100 box: (kind, params, part). Parts are colored per use.
MARK = [
    ('circle', (34.0, 24.0, 9.0), 'comb'),
    ('circle', (46.0, 18.5, 10.5), 'comb'),
    ('circle', (58.0, 24.0, 9.0), 'comb'),
    ('poly', ((25.0, 50.0), (26.5, 70.0), (33.0, 88.0), (55.0, 88.0), (59.0, 72.0), (63.0, 55.0)), 'head'),
    ('circle', (44.0, 46.0, 20.0), 'head'),
    ('poly', ((61.0, 39.5), (80.0, 47.0), (61.0, 54.5)), 'beak'),
    ('oval', (58.0, 64.5, 6.0, 8.5), 'wattle'),
    ('circle', (50.5, 42.0, 3.8), 'eye'),
]


def draw_mark(draw, x, y, size, colors):
    """Draws the mark scaled into a size x size box at (x, y)."""
    s = size / 100.0
    for kind, params, part in MARK:
        color = colors.get(part)
        if color is None:
            continue
        if kind == 'circle':
            cx, cy, r = params
            draw.ellipse([x + (cx - r) * s, y + (cy - r) * s, x + (cx + r) * s, y + (cy + r) * s], fill=color)
        elif kind == 'oval':
            cx, cy, rx, ry = params
            draw.ellipse([x + (cx - rx) * s, y + (cy - ry) * s, x + (cx + rx) * s, y + (cy + ry) * s], fill=color)
        else:
            draw.polygon([(x + px * s, y + py * s) for px, py in params], fill=color)


def squircle_mask(size, radius_ratio=0.225):
    mask = Image.new('L', (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=size * radius_ratio, fill=255)
    return mask


def app_icon(size, inset=0.0):
    """Gradient tile with the mark. `inset` leaves macOS's transparent margin."""
    big = size * 4
    tile = int(big * (1 - 2 * inset))
    gradient = Image.new('RGBA', (tile, tile))
    pixels = gradient.load()
    for py in range(tile):
        t = py / (tile - 1)
        row = tuple(int(AMBER[i] + (FLAME[i] - AMBER[i]) * t) for i in range(3)) + (255,)
        for px in range(tile):
            pixels[px, py] = row
    canvas = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    offset = (big - tile) // 2
    canvas.paste(gradient, (offset, offset), squircle_mask(tile))
    mark = tile * 0.74
    draw_mark(ImageDraw.Draw(canvas), offset + (tile - mark) / 2, offset + (tile - mark) / 2 + tile * 0.01, mark,
              {'comb': COMB, 'head': WHITE, 'wattle': COMB, 'beak': BEAK, 'eye': EYE})
    return canvas.resize((size, size), Image.LANCZOS)


def mono_mark(size, color):
    big = size * 4
    canvas = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw_mark(draw, 0, 0, big, {'comb': color, 'head': color, 'wattle': color, 'beak': color})
    draw_mark(draw, 0, 0, big, {'eye': (0, 0, 0, 0)})
    return canvas.resize((size, size), Image.LANCZOS)


def load_font(px):
    for name in ('Inter-Medium.otf', 'Inter-Medium.ttf', 'Inter Medium.ttf', 'Helvetica.ttc', 'DejaVuSans.ttf'):
        for folder in ('/usr/share/fonts/opentype/inter', '/Library/Fonts', '/System/Library/Fonts',
                       '/usr/share/fonts/truetype/dejavu', ''):
            try:
                return ImageFont.truetype(str(Path(folder) / name) if folder else name, px)
            except OSError:
                continue
    return ImageFont.load_default()


def wordmark(width, height, color):
    """Mark + "Rooster", the layout of Chromium's product_logo.png."""
    big_w, big_h = width * 4, height * 4
    canvas = Image.new('RGBA', (big_w, big_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw_mark(draw, 0, 0, big_h, {'comb': color, 'head': color, 'wattle': color, 'beak': color})
    draw_mark(draw, 0, 0, big_h, {'eye': (0, 0, 0, 0)})
    font = load_font(int(big_h * 0.78))
    box = draw.textbbox((0, 0), 'Rooster', font=font)
    draw.text((big_h * 1.12, (big_h - (box[3] - box[1])) / 2 - box[1]), 'Rooster', font=font, fill=color)
    return canvas.resize((width, height), Image.LANCZOS)


def svg(size=256):
    s = size / 100.0
    parts = {'comb': '#C81E1E', 'head': '#FFFBF5', 'wattle': '#C81E1E', 'beak': '#FFD24A', 'eye': '#3A1A0A'}
    body = []
    for kind, params, part in MARK:
        fill = parts[part]
        if kind == 'circle':
            cx, cy, r = (v * s for v in params)
            body.append(f'<circle cx="{cx:g}" cy="{cy:g}" r="{r:g}" fill="{fill}"/>')
        elif kind == 'oval':
            cx, cy, rx, ry = (v * s for v in params)
            body.append(f'<ellipse cx="{cx:g}" cy="{cy:g}" rx="{rx:g}" ry="{ry:g}" fill="{fill}"/>')
        else:
            points = ' '.join(f'{px * s:g},{py * s:g}' for px, py in params)
            body.append(f'<polygon points="{points}" fill="{fill}"/>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {size} {size}">\n'
            '  <defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1">'
            '<stop offset="0" stop-color="#FFA726"/><stop offset="1" stop-color="#E8471C"/></linearGradient></defs>\n'
            f'  <rect width="{size}" height="{size}" rx="{size * 0.225:g}" fill="url(#g)"/>\n'
            f'  <g transform="translate({size * 0.13:g} {size * 0.14:g}) scale(0.74)">\n    '
            + '\n    '.join(body) + '\n  </g>\n</svg>\n')


def mark_svg(size=1024):
    """The mark alone, placed as on the app tile, for macOS Icon Composer."""
    s = size / 100.0 * 0.74
    ox, oy = size * 0.13, size * 0.14
    parts = {'comb': '#C81E1E', 'head': '#FFFBF5', 'wattle': '#C81E1E', 'beak': '#FFD24A', 'eye': '#3A1A0A'}
    body = []
    for kind, params, part in MARK:
        fill = parts[part]
        if kind == 'circle':
            cx, cy, r = params
            body.append(f'<circle cx="{ox + cx * s:g}" cy="{oy + cy * s:g}" r="{r * s:g}" fill="{fill}"/>')
        elif kind == 'oval':
            cx, cy, rx, ry = params
            body.append(f'<ellipse cx="{ox + cx * s:g}" cy="{oy + cy * s:g}" rx="{rx * s:g}" ry="{ry * s:g}" fill="{fill}"/>')
        else:
            points = ' '.join(f'{ox + px * s:g},{oy + py * s:g}' for px, py in params)
            body.append(f'<polygon points="{points}" fill="{fill}"/>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {size} {size}">\n  '
            + '\n  '.join(body) + '\n</svg>\n')


ICON_JSON = """{
  "fill" : {
    "linear-gradient" : [
      "srgb:1.00000,0.65490,0.14902,1.00000",
      "srgb:0.90980,0.27843,0.10980,1.00000"
    ]
  },
  "groups" : [
    {
      "layers" : [
        {
          "glass" : false,
          "image-name" : "rooster.svg",
          "name" : "rooster"
        }
      ],
      "shadow" : {
        "kind" : "neutral",
        "opacity" : 0.5
      },
      "translucency" : {
        "enabled" : false,
        "value" : 0.5
      }
    }
  ],
  "supported-platforms" : {
    "squares" : "shared"
  }
}
"""


def write_mac_catalog():
    """Inputs for chrome/app/theme/chromium/mac, compiled by `rooster icons`."""
    mac = OUT / 'mac'
    (mac / 'AppIcon.icon/Assets').mkdir(parents=True, exist_ok=True)
    (mac / 'AppIcon.icon/icon.json').write_text(ICON_JSON, encoding='utf-8')
    (mac / 'AppIcon.icon/Assets/rooster.svg').write_text(mark_svg(), encoding='utf-8')
    (mac / 'AppIcon.appiconset').mkdir(exist_ok=True)
    (mac / 'Icon.iconset').mkdir(exist_ok=True)
    tile = app_icon(1024, inset=100 / 1024)
    for size in (16, 32, 64, 128, 256, 512, 1024):
        tile.resize((size, size), Image.LANCZOS).save(mac / f'AppIcon.appiconset/appicon_{size}.png')
    tile.resize((256, 256), Image.LANCZOS).save(mac / 'Icon.iconset/icon_256x256.png')
    tile.resize((512, 512), Image.LANCZOS).save(mac / 'Icon.iconset/icon_256x256@2x.png')


def vector_icon(canvas, color_argb=None, background=None):
    """Chromium .icon source. Monochrome unless colors are given."""
    s = canvas / 100.0
    def f(v):
        text = f'{v * s:.2f}'.rstrip('0').rstrip('.')
        return text + 'f' if '.' in text else text  # "14f" isn't valid C++
    lines = ['// Copyright 2026 The Rooster Authors',
             '// You can use, redistribute, and/or modify this source code under',
             '// the terms of the GPL-3.0 license that can be found in the LICENSE file.',
             '', f'CANVAS_DIMENSIONS, {canvas},']
    inset, scale = (0.0, 1.0)
    if background:
        lines += ['NEW_PATH,', 'PATH_COLOR_ARGB, ' + ', '.join(f'0x{c:02X}' for c in (255, *background)) + ',',
                  f'ROUND_RECT, 0, 0, {canvas}, {canvas}, {f(22.5)},']
        inset, scale = 13.0, 0.74
    g = lambda v, axis: f(inset + v * scale + (1.0 if axis == 'y' and background else 0.0))
    for kind, params, part in MARK:
        if part == 'eye':
            continue
        lines.append('NEW_PATH,')
        if color_argb:
            color = {'beak': BEAK, 'comb': COMB, 'wattle': COMB}.get(part, color_argb)
            lines.append('PATH_COLOR_ARGB, ' + ', '.join(f'0x{c:02X}' for c in (255, *color)) + ',')
        if kind == 'circle':
            cx, cy, r = params
            lines.append(f'CIRCLE, {g(cx, "x")}, {g(cy, "y")}, {f(r * scale)},')
        elif kind == 'oval':
            cx, cy, rx, ry = params
            lines.append(f'OVAL, {g(cx, "x")}, {g(cy, "y")}, {f(rx * scale)}, {f(ry * scale)},')
        else:
            (x0, y0), *rest = params
            lines.append(f'MOVE_TO, {g(x0, "x")}, {g(y0, "y")},')
            lines += [f'LINE_TO, {g(px, "x")}, {g(py, "y")},' for px, py in rest]
            lines.append('CLOSE,')
    cx, cy, r = MARK[-1][1]
    lines += ['NEW_PATH,', 'PATH_MODE_CLEAR,' if not color_argb else
              'PATH_COLOR_ARGB, ' + ', '.join(f'0x{c:02X}' for c in (255, *EYE)) + ',',
              f'CIRCLE, {g(cx, "x")}, {g(cy, "y")}, {f(r * scale)}']
    return '\n'.join(lines) + '\n'


def main():
    (OUT / 'app_icon').mkdir(parents=True, exist_ok=True)
    (OUT / 'generated').mkdir(exist_ok=True)

    app_icon(1024).save(OUT / 'app_icon/raw.png')
    for size in (16, 24, 32, 48, 64, 128, 256):
        app_icon(size).save(OUT / f'generated/product_logo_{size}.png')

    # macOS: the tile sits inside Apple's icon grid margin (100 of 1024 px).
    icns_sizes = [(16, 16), (32, 32), (64, 64), (128, 128), (256, 256), (512, 512), (1024, 1024)]
    app_icon(1024, inset=100 / 1024).save(OUT / 'app.icns', sizes=icns_sizes)
    write_mac_catalog()

    wordmark(140, 32, INK).save(OUT / 'product_logo.png')
    wordmark(280, 64, INK).save(OUT / 'product_logo_200.png')
    wordmark(140, 32, (255, 255, 255)).save(OUT / 'product_logo_white.png')
    wordmark(280, 64, (255, 255, 255)).save(OUT / 'product_logo_white_200.png')
    mono_mark(22, INK).save(OUT / 'product_logo_22_mono.png')

    (OUT / 'product_logo.svg').write_text(svg(), encoding='utf-8')
    (OUT / 'product_logo.icon').write_text(vector_icon(56), encoding='utf-8')
    (OUT / 'product_logo_color.icon').write_text(vector_icon(256, WHITE, FLAME), encoding='utf-8')
    print(f'wrote assets to {OUT}')


if __name__ == '__main__':
    main()

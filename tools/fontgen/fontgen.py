#!/usr/bin/env python3
"""Converts TrueType fonts into 1-bit bitmap fonts for the UI layer.

Each font size and weight becomes a C++ source file in lib/ui/src/ui/fonts/
with the glyph bitmaps and metrics (see lib/ui/src/ui/font.h). Glyphs are
rasterised by FreeType (through Pillow) without anti-aliasing, exactly as
they appear on the 1-bit e-paper panel.

Usage (from the repository root):

    python tools/fontgen/fontgen.py

The output is deterministic; commit the generated files together with any
change to this script or the font list.
"""

import hashlib
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

import emoji_icons

ROOT = pathlib.Path(__file__).resolve().parents[2]
FONT_DIR = pathlib.Path(__file__).resolve().parent / "fonts"
OUT_DIR = ROOT / "lib" / "ui" / "src" / "ui" / "fonts"

# Source fonts (variable TrueType). The generated files carry the family,
# origin and license of their source.
SOURCES = {
    "roboto": {
        "file": "RobotoCondensed-Variable.ttf",
        "family": "Roboto Condensed",
        "origin": "Roboto v3.008, google/fonts ofl/robotocondensed",
        "license": "SIL Open Font License 1.1, see OFL.txt",
        "license_file": "OFL.txt",
    },
    "fraunces": {
        "file": "Fraunces-Variable.ttf",
        "family": "Fraunces",
        "origin": "Fraunces v1.000, google/fonts ofl/fraunces",
        "license": "SIL Open Font License 1.1, see OFL-Fraunces.txt",
        "license_file": "OFL-Fraunces.txt",
    },
}

# Size of the emoji icons relative to the font size (see emoji_icons.py).
ICON_SCALE = 0.9

# Oversampled rasterisation: (factor, coverage threshold); see render_oversampled().
OVERSAMPLED = (8, 0.45)

# (C++ name, output file, weight, pixel size[, characters[, source[, oversampled]]]).
# Sizes follow the "Briefing" design from 2026-10-21; they are provisional
# until checked on
# the panel from 2-3 m. Without a character list: CHARSET. Without a source:
# Roboto Condensed. Fraunces is set like in the design: optical size = pixel
# size, softness 50, no "wonky" letters.
FONTS = [
    ("kRobotoCondensedBold15", "roboto_condensed_bold_15.cpp", 700, 15),  # column titles, delays
    ("kRobotoCondensedBold16", "roboto_condensed_bold_16.cpp", 700, 16),  # header line, weekdays
    ("kRobotoCondensedBold17", "roboto_condensed_bold_17.cpp", 700, 17),  # line labels, times
    ("kRobotoCondensedBold20", "roboto_condensed_bold_20.cpp", 700, 20),  # line symbol widths
    ("kRobotoCondensedBold21", "roboto_condensed_bold_21.cpp", 700, 21),  # departure times
    ("kRobotoCondensedMedium13", "roboto_condensed_medium_13.cpp", 500, 13),  # data credit
    ("kRobotoCondensedMedium15", "roboto_condensed_medium_15.cpp", 500, 15),  # column notes, hours
    # Medium 16 rasterised oversampled: FreeType's own 1-bit hinting gives
    # some digits ("1", "3", "4", "5") 1 px stems at this size.
    ("kRobotoCondensedMedium16", "roboto_condensed_medium_16.cpp", 500, 16, None, "roboto",
     OVERSAMPLED),  # low temperatures, footers
    ("kRobotoCondensedMedium17", "roboto_condensed_medium_17.cpp", 500, 17),  # event times
    ("kRobotoCondensedMedium20", "roboto_condensed_medium_20.cpp", 500, 20),  # destinations
    ("kFraunces38", "fraunces_38.cpp", 420, 38, None, "fraunces"),  # sentence
    ("kFraunces34", "fraunces_34.cpp", 420, 34, None, "fraunces"),  # long sentence
    ("kFraunces24Medium", "fraunces_24_medium.cpp", 500, 24, None, "fraunces"),  # "7° bis 14°"
    ("kFraunces20", "fraunces_20.cpp", 420, 20, None, "fraunces"),  # events
    ("kFraunces20Bold", "fraunces_20_bold.cpp", 640, 20, None, "fraunces"),  # next event
    ("kFraunces18", "fraunces_18.cpp", 420, 18, None, "fraunces"),  # events later in the week
    # Word mark on the start screen: only the letters of "InkTavla".
    ("kFraunces96Bold", "fraunces_96_bold.cpp", 640, 96, sorted(set(map(ord, "InkTavla"))), "fraunces"),
]

# Characters: ASCII, Latin-1 Supplement, Latin Extended-A and typographic
# punctuation that appears in station names, messages and dates.
CHARSET = (
    list(range(0x20, 0x7F))
    + list(range(0xA0, 0x180))
    + [0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E, 0x2022, 0x2026,
       0x20AC, 0x2212]
)


def load(source, weight, size):
    font = ImageFont.truetype(str(FONT_DIR / SOURCES[source]["file"]), size)
    if source == "roboto":
        font.set_variation_by_axes([weight])
        return font
    # Fraunces: set every axis by name, in the order of the font.
    wanted = {"Weight": weight, "Optical Size": min(144, max(9, size)), "Softness": 50,
              "Wonky": 0}
    values = []
    for axis in font.get_variation_axes():
        name = axis["name"].decode() if isinstance(axis["name"], bytes) else axis["name"]
        values.append(wanted.get(name, axis["default"]))
    font.set_variation_by_axes(values)
    return font


def render(font, codepoint):
    """Returns (width, height, left, top, advance_x8, rows) for one glyph.

    left/top: position of the bitmap's top-left pixel relative to the pen
    position on the baseline; rows: list of rows, each a list of 0/1.
    """
    char = chr(codepoint)
    advance_x8 = round(font.getlength(char) * 8)
    mask, (ox, oy) = font.getmask2(char, mode="1", anchor="ls")
    w, h = mask.size
    pixels = [[1 if mask.getpixel((x, y)) else 0 for x in range(w)] for y in range(h)]
    # Trim empty borders.
    rows = [i for i, row in enumerate(pixels) if any(row)]
    cols = [x for x in range(w) if any(pixels[y][x] for y in range(h))]
    if not rows:
        return 0, 0, 0, 0, advance_x8, []
    y0, y1, x0, x1 = rows[0], rows[-1], cols[0], cols[-1]
    trimmed = [row[x0:x1 + 1] for row in pixels[y0:y1 + 1]]
    return x1 - x0 + 1, y1 - y0 + 1, ox + x0, oy + y0, advance_x8, trimmed


def render_oversampled(small, big, size, codepoint, factor, threshold):
    """Like render(), but rasterised by `big` (the font at `factor` times the
    size of `small`), averaged down and thresholded: even stems where
    FreeType's own 1-bit hinting makes some of them thinner than others.
    The advance stays the one of `small`."""
    char = chr(codepoint)
    advance_x8 = round(small.getlength(char) * 8)
    cells = 3 * size  # room around the pen position (left, above, right)
    canvas = Image.new("L", (cells * factor, cells * factor), 0)
    pen_x, pen_y = size, 2 * size  # pen position in small pixels
    ImageDraw.Draw(canvas).text((pen_x * factor, pen_y * factor), char, font=big, fill=255,
                                anchor="ls")
    reduced = canvas.resize((cells, cells), Image.BOX)
    limit = threshold * 255
    pixels = [[1 if reduced.getpixel((x, y)) >= limit else 0 for x in range(cells)]
              for y in range(cells)]
    rows = [i for i, row in enumerate(pixels) if any(row)]
    cols = [x for x in range(cells) if any(pixels[y][x] for y in range(cells))]
    if not rows:
        return 0, 0, 0, 0, advance_x8, []
    y0, y1, x0, x1 = rows[0], rows[-1], cols[0], cols[-1]
    trimmed = [row[x0:x1 + 1] for row in pixels[y0:y1 + 1]]
    return x1 - x0 + 1, y1 - y0 + 1, x0 - pen_x, y0 - pen_y, advance_x8, trimmed


def pack(rows):
    bits = [bit for row in rows for bit in row]
    data = bytearray()
    for i in range(0, len(bits), 8):
        byte = 0
        for j, bit in enumerate(bits[i:i + 8]):
            byte |= bit << (7 - j)
        data.append(byte)
    return bytes(data)


def generate(name, filename, weight, size, charset=None, source="roboto", oversampled=None):
    src = SOURCES[source]
    font = load(source, weight, size)
    ascent, descent = font.getmetrics()
    if oversampled:
        big = load(source, weight, size * oversampled[0])

        def glyph_of(cp):
            return render_oversampled(font, big, size, cp, *oversampled)
    else:
        def glyph_of(cp):
            return render(font, cp)
    cap_height = -glyph_of(ord("H"))[3]
    missing = render(font, 0x10FFFD)  # private use: renders as .notdef
    glyphs, bitmap = [], bytearray()
    for cp in charset or CHARSET:
        if render(font, cp) == missing:
            print(f"  U+{cp:04X} not in the font, skipped")
            continue
        glyph = glyph_of(cp)
        w, h, left, top, adv, rows = glyph
        glyphs.append((cp, len(bitmap), w, h, left, top, adv))
        bitmap += pack(rows)
    if not charset:
        # Emoji icons (own drawings, see emoji_icons.py) as glyphs in the
        # private use area, a little smaller than the font size; the bottom
        # sits slightly below the baseline, like colour emoji.
        icon = max(12, round(size * ICON_SCALE))
        for index, (_, painter, rotation) in enumerate(emoji_icons.ICONS):
            rows = emoji_icons.render(painter, icon, rotation)
            top = -(icon - round(icon * 0.1))
            glyphs.append((emoji_icons.FIRST_CODEPOINT + index, len(bitmap), icon, icon, 0, top,
                           (icon + 2) * 8))
            bitmap += pack(rows)
    for g in glyphs:
        assert 0 <= g[2] <= 255 and 0 <= g[3] <= 255, g
        assert -128 <= g[4] <= 127 and -128 <= g[5] <= 127, g

    lines = [
        "// Generated by tools/fontgen/fontgen.py - do not edit.",
        "// clang-format off",
        f"// {src['family']}, weight {weight}, {size} px, 1 bit per pixel"
        + (f", rasterised {oversampled[0]}x and thresholded at {round(oversampled[1] * 100)} %."
           if oversampled else "."),
        f"// Source: {src['origin']}.",
        f"// License: {src['license']}.",
        "",
        '#include "ui/fonts.h"',
        "",
        "namespace ui::fonts {",
        "",
        "namespace {",
        "",
        f"const uint8_t kBitmaps[{max(len(bitmap), 1)}] = {{",
    ]
    for i in range(0, len(bitmap), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in bitmap[i:i + 16]) + ",")
    if not bitmap:
        lines.append("    0x00,")
    lines += ["};", "", f"const Glyph kGlyphs[{len(glyphs)}] = {{"]
    lines.append("    // codepoint, offset, width, height, left, top, advance (1/8 px)")
    for cp, off, w, h, left, top, adv in glyphs:
        lines.append(f"    {{0x{cp:04X}, {off}, {w}, {h}, {left}, {top}, {adv}}},")
    lines += [
        "};",
        "",
        "}  // namespace",
        "",
        f"const Font {name} = {{\"{src['family']} {weight} {size}\", kGlyphs, {len(glyphs)}, "
        f"kBitmaps, {size}, {ascent}, {descent}, {cap_height}}};",
        "",
        "}  // namespace ui::fonts",
        "",
    ]
    path = OUT_DIR / filename
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    return path, len(glyphs), len(bitmap)


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for source in SOURCES.values():
        digest = hashlib.sha256((FONT_DIR / source["file"]).read_bytes()).hexdigest()
        print(f"{source['file']} sha256 {digest}")
    for entry in FONTS:
        path, count, size_bytes = generate(*entry)
        print(f"{path.relative_to(ROOT)}: {count} glyphs, {size_bytes} bytes bitmap")
    for source in SOURCES.values():
        name = source["license_file"]
        (OUT_DIR / name).write_bytes((FONT_DIR / name).read_bytes())
    return 0


if __name__ == "__main__":
    sys.exit(main())

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

from PIL import ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
FONT_DIR = pathlib.Path(__file__).resolve().parent / "fonts"
OUT_DIR = ROOT / "lib" / "ui" / "src" / "ui" / "fonts"

SOURCE = {
    "file": "RobotoCondensed-Variable.ttf",
    "family": "Roboto Condensed",
    "origin": "Roboto v3.008, google/fonts ofl/robotocondensed",
    "license": "SIL Open Font License 1.1, see OFL.txt",
}

# Characters of large fonts that only show numbers (temperatures).
NUMBERS = [ord(c) for c in " +-0123456789"] + [0xB0, 0x2212]  # degree sign, minus

# (C++ name, output file, weight, pixel size[, characters]). Sizes follow the
# overall UI design from 2026-10-20 (style A); they are provisional until
# checked on the panel from 2-3 m. Without a character list: CHARSET.
FONTS = [
    ("kRobotoCondensedBold16", "roboto_condensed_bold_16.cpp", 700, 16),  # section names
    ("kRobotoCondensedMedium17", "roboto_condensed_medium_17.cpp", 500, 17),  # tile notes
    ("kRobotoCondensedBold17", "roboto_condensed_bold_17.cpp", 700, 17),  # tile titles
    ("kRobotoCondensedBold20", "roboto_condensed_bold_20.cpp", 700, 20),  # line symbols
    ("kRobotoCondensedBold21", "roboto_condensed_bold_21.cpp", 700, 21),  # delays
    ("kRobotoCondensedMedium24", "roboto_condensed_medium_24.cpp", 500, 24),  # destinations
    ("kRobotoCondensedMedium26", "roboto_condensed_medium_26.cpp", 500, 26),  # header date
    ("kRobotoCondensedBold26", "roboto_condensed_bold_26.cpp", 700, 26),  # departure times
    ("kRobotoCondensedBold30", "roboto_condensed_bold_30.cpp", 700, 30),  # header time
    ("kRobotoCondensedMedium18", "roboto_condensed_medium_18.cpp", 500, 18),  # weather notes
    ("kRobotoCondensedBold18", "roboto_condensed_bold_18.cpp", 700, 18),  # chart temperatures
    ("kRobotoCondensedMedium20", "roboto_condensed_medium_20.cpp", 500, 20),  # weather texts
    ("kRobotoCondensedBold22", "roboto_condensed_bold_22.cpp", 700, 22),  # weekdays, highs, next event
    ("kRobotoCondensedMedium22", "roboto_condensed_medium_22.cpp", 500, 22),  # calendar entries
    ("kRobotoCondensedBold56", "roboto_condensed_bold_56.cpp", 700, 56, NUMBERS),  # temperature
]

# Characters: ASCII, Latin-1 Supplement, Latin Extended-A and typographic
# punctuation that appears in station names, messages and dates.
CHARSET = (
    list(range(0x20, 0x7F))
    + list(range(0xA0, 0x180))
    + [0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E, 0x2022, 0x2026,
       0x20AC, 0x2212]
)


def load(weight, size):
    font = ImageFont.truetype(str(FONT_DIR / SOURCE["file"]), size)
    font.set_variation_by_axes([weight])
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


def pack(rows):
    bits = [bit for row in rows for bit in row]
    data = bytearray()
    for i in range(0, len(bits), 8):
        byte = 0
        for j, bit in enumerate(bits[i:i + 8]):
            byte |= bit << (7 - j)
        data.append(byte)
    return bytes(data)


def generate(name, filename, weight, size, charset=None):
    font = load(weight, size)
    ascent, descent = font.getmetrics()
    cap_height = -render(font, ord("H"))[3]
    missing = render(font, 0x10FFFD)  # private use: renders as .notdef
    glyphs, bitmap = [], bytearray()
    for cp in charset or CHARSET:
        glyph = render(font, cp)
        if glyph == missing:
            print(f"  U+{cp:04X} not in the font, skipped")
            continue
        w, h, left, top, adv, rows = glyph
        glyphs.append((cp, len(bitmap), w, h, left, top, adv))
        bitmap += pack(rows)
    for g in glyphs:
        assert 0 <= g[2] <= 255 and 0 <= g[3] <= 255, g
        assert -128 <= g[4] <= 127 and -128 <= g[5] <= 127, g

    lines = [
        "// Generated by tools/fontgen/fontgen.py - do not edit.",
        "// clang-format off",
        f"// {SOURCE['family']}, weight {weight}, {size} px, 1 bit per pixel.",
        f"// Source: {SOURCE['origin']}.",
        f"// License: {SOURCE['license']}.",
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
        f"const Font {name} = {{\"{SOURCE['family']} {weight} {size}\", kGlyphs, {len(glyphs)}, "
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
    digest = hashlib.sha256((FONT_DIR / SOURCE["file"]).read_bytes()).hexdigest()
    print(f"{SOURCE['file']} sha256 {digest}")
    for entry in FONTS:
        path, count, size_bytes = generate(*entry)
        print(f"{path.relative_to(ROOT)}: {count} glyphs, {size_bytes} bytes bitmap")
    (OUT_DIR / "OFL.txt").write_bytes((FONT_DIR / "OFL.txt").read_bytes())
    return 0


if __name__ == "__main__":
    sys.exit(main())

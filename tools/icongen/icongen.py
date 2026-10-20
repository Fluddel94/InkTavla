#!/usr/bin/env python3
"""Draws the weather symbols of the UI design as 1-bit bitmaps.

The symbols are own drawings (no third-party icons): simple shapes drawn
with Pillow at four times the size, scaled down and turned into black and
white, exactly like the drawings of the overall UI design (2026-10-20). The
result is a C++ source file in lib/ui/src/ui/ with all symbols in the
sizes the UI uses (see lib/ui/src/ui/icons.h).

Usage (from the repository root, Pillow needed: see
tools/fontgen/requirements.txt):

    python tools/icongen/icongen.py

The output is deterministic; commit the generated file together with any
change to this script.
"""

import math
import pathlib
import sys

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "lib" / "ui" / "src" / "ui" / "icons_data.cpp"

SS = 4  # supersampling factor

# Weather symbols: (C++ enum name, kind). Sizes: (pixel size, stroke or None
# for the default stroke of size / 11).
WEATHER = [
    ("kSun", "sun"),
    ("kPartlyCloudy", "partly"),
    ("kCloudy", "cloud"),
    ("kRain", "rain"),
    ("kFog", "fog"),
    ("kMoon", "moon"),
    ("kPartlyCloudyNight", "partly_night"),
    ("kSnow", "snow"),
    ("kThunder", "thunder"),
]
SIZES = [(26, 2.3), (32, 3.0), (50, None)]


def mask(size, painter):
    n = size * SS
    m = Image.new("L", (n, n), 0)
    painter(ImageDraw.Draw(m), n)
    m = m.resize((size, size), Image.LANCZOS)
    return m.point(lambda v: 255 if v >= 120 else 0).convert("1")


def rline(d, p0, p1, w):
    d.line([p0, p1], fill=255, width=int(w))
    for (x, y) in (p0, p1):
        d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=255)


def sun(d, cx, cy, r, st, ray_len):
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=255, width=int(st))
    g = r * 0.4
    for i in range(8):
        a = i * math.pi / 4
        p0 = (cx + math.cos(a) * (r + g), cy + math.sin(a) * (r + g))
        p1 = (cx + math.cos(a) * (r + g + ray_len), cy + math.sin(a) * (r + g + ray_len))
        rline(d, p0, p1, st)


def cloud_shapes(x0, yb, w, inset):
    return [
        ("c", x0 + 0.28 * w, yb - 0.17 * w, 0.17 * w - inset),
        ("c", x0 + 0.55 * w, yb - 0.30 * w, 0.27 * w - inset),
        ("c", x0 + 0.80 * w, yb - 0.16 * w, 0.16 * w - inset),
        ("r", x0 + 0.28 * w, yb - 0.30 * w, x0 + 0.80 * w, yb - inset),
    ]


def paint(d, shapes, val):
    for s in shapes:
        if s[0] == "c":
            _, cx, cy, r = s
            d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=val)
        else:
            _, a, b, c2, e = s
            d.rectangle((a, b, c2, e), fill=val)


def cloud(d, x0, yb, w, st, gap=0):
    if gap:
        paint(d, cloud_shapes(x0, yb, w, -gap), 0)
    paint(d, cloud_shapes(x0, yb, w, 0), 255)
    paint(d, cloud_shapes(x0, yb, w, st), 0)


def crescent(d, n, x0, y0, s):
    """Filled crescent moon in the square (x0, y0, s)."""
    d.ellipse((x0 + s * 0.18, y0 + s * 0.14, x0 + s * 0.82, y0 + s * 0.78), fill=255)
    d.ellipse((x0 + s * 0.36, y0 + s * 0.04, x0 + s * 0.98, y0 + s * 0.66), fill=0)


def weather(kind, size, stroke):
    def painter(d, n):
        st = stroke * SS if stroke else max(2 * SS, n / 11)
        if kind == "sun":
            sun(d, n / 2, n / 2, n * 0.19, st, n * 0.14)
        elif kind == "moon":
            crescent(d, n, 0, 0, n)
        elif kind in ("partly", "partly_night"):
            if kind == "partly":
                if n / SS >= 48:
                    cx, cy, r = n * 0.36, n * 0.36, n * 0.15
                    inner, outer, width = 1.45, 1.95, st * 0.8
                else:
                    # Small sizes: a smaller sun, so that longer rays fit.
                    cx, cy, r = n * 0.33, n * 0.33, n * 0.11
                    inner, outer, width = 1.75, 2.75, st
                d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
                for i in range(8):
                    if n / SS < 48 and i in (1, 2, 0):
                        continue  # hidden behind the cloud anyway, only stubs would remain
                    a = i * math.pi / 4
                    rline(d, (cx + math.cos(a) * r * inner, cy + math.sin(a) * r * inner),
                          (cx + math.cos(a) * r * outer, cy + math.sin(a) * r * outer), width)
            else:
                crescent(d, n, n * 0.02, n * 0.0, n * 0.62)
            cloud(d, n * 0.22, n * 0.88, n * 0.76, st, gap=st * 1.3)
        elif kind == "cloud":
            cloud(d, n * 0.06, n * 0.78, n * 0.88, st)
        elif kind == "rain":
            cloud(d, n * 0.06, n * 0.62, n * 0.88, st)
            for x in (0.32, 0.52, 0.72):
                rline(d, (n * x, n * 0.74), (n * (x - 0.07), n * 0.92), st)
        elif kind == "snow":
            cloud(d, n * 0.06, n * 0.62, n * 0.88, st)
            for x, y in ((0.30, 0.80), (0.52, 0.86), (0.74, 0.80)):
                r = n * 0.065
                rline(d, (n * x - r, n * y), (n * x + r, n * y), st * 0.8)
                rline(d, (n * x, n * y - r), (n * x, n * y + r), st * 0.8)
        elif kind == "thunder":
            cloud(d, n * 0.06, n * 0.62, n * 0.88, st)
            d.polygon([(n * 0.56, n * 0.60), (n * 0.40, n * 0.80), (n * 0.52, n * 0.80),
                       (n * 0.44, n * 0.98), (n * 0.66, n * 0.72), (n * 0.54, n * 0.72),
                       (n * 0.62, n * 0.60)], fill=255)
        elif kind == "fog":
            cloud(d, n * 0.06, n * 0.60, n * 0.88, st)
            for y, (a, b) in ((0.74, (0.12, 0.88)), (0.88, (0.24, 0.76))):
                rline(d, (n * a, n * y), (n * b, n * y), st)
    return mask(size, painter)


def drop(size):
    def painter(d, n):
        cx, cy, r = n / 2, n * 0.62, n * 0.26
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
        d.polygon([(cx - r * 0.95, cy - r * 0.3), (cx, n * 0.08), (cx + r * 0.95, cy - r * 0.3)],
                  fill=255)
    return mask(size, painter)


def pack(img):
    w, h = img.size
    data = bytearray()
    for y in range(h):
        for bx in range(0, w, 8):
            byte = 0
            for i in range(8):
                x = bx + i
                if x < w and img.getpixel((x, y)):
                    byte |= 0x80 >> i
            data.append(byte)
    return bytes(data)


def array(name, img):
    data = pack(img)
    lines = [f"const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    lines.append("};")
    return lines


def main():
    lines = [
        "// Generated by tools/icongen/icongen.py - do not edit.",
        "// clang-format off",
        "// Weather symbols of the UI design: own drawings, 1 bit, rows padded to",
        "// whole bytes, most significant bit = leftmost pixel.",
        "",
        '#include "ui/icons.h"',
        "",
        "namespace ui {",
        "",
        "namespace {",
        "",
    ]
    table = []
    for enum, kind in WEATHER:
        for size, stroke in SIZES:
            name = f"k{kind.title().replace('_', '')}{size}"
            lines += array(name, weather(kind, size, stroke)) + [""]
            table.append((enum, size, name))
    lines += array("kDrop15", drop(15)) + [""]
    lines += ["}  // namespace", "", "const WeatherIconEntry kWeatherIcons[] = {"]
    for enum, size, name in table:
        lines.append(f"    {{WeatherIcon::{enum}, {{{size}, {size}, {name}}}}},")
    lines += [
        "};",
        f"const size_t kWeatherIconCount = {len(table)};",
        "",
        "const Icon kDropIcon = {15, 15, kDrop15};",
        "",
        "}  // namespace ui",
        "",
    ]
    OUT.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"{OUT.relative_to(ROOT)}: {len(table)} weather symbols + drop")
    return 0


if __name__ == "__main__":
    sys.exit(main())

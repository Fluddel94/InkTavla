#!/usr/bin/env python3
"""Draws the weather symbols of the UI design as 1-bit bitmaps.

The symbols are own drawings (no third-party icons): simple shapes drawn
with Pillow at four times the size, scaled down and turned into black and
white, like the drawings of the UI designs (2026-10-20 and 2026-10-21). The
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

# Weather symbols: (C++ enum name, kind).
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
# Only for the weather column (days 44 px, parts of tomorrow 30 px;
# 2026-10-29): rain by intensity (light 1 stroke, kRain 2, heavy 3), showers
# with the sun, freezing rain (also sleet). The band shows the umbrella for
# rain (by intensity, see BAND_EXTRA).
DAY_ONLY = [
    ("kRainLight", "rain_light"),
    ("kRainHeavy", "rain_heavy"),
    ("kShowers", "showers"),
    ("kFreezingRain", "freezing"),
    # W1 (2026-11-22): light snow (one star; kSnow with three stars is the
    # heavy snow), showers by intensity, thunderstorm with hail.
    ("kSnowLight", "snow_light"),
    ("kShowersLight", "showers_light"),
    ("kShowersModerate", "showers_moderate"),
    ("kThunderHail", "thunder_hail"),
]
# Band pictures beyond WEATHER (W1, 2026-11-22): the umbrella with two or six
# drops for light or heavy rain, heavy snow, thunderstorm with hail; wind
# from the storm threshold: windsock, windsock with driven rain, snowflake
# with wind lines.
BAND_EXTRA = [
    ("kRainLight", "rain_light"),
    ("kRainHeavy", "rain_heavy"),
    ("kSnowHeavy", "snow_heavy"),
    ("kThunderHail", "thunder_hail"),
    ("kWindSock", "wind_sock"),
    ("kStormRain", "storm_rain"),
    ("kSnowWind", "snow_wind"),
]
# Sizes: (pixel size, stroke); 30 px for the parts of tomorrow on step L
# (2026-11-16). The symbols fill their square (the drawing
# scaled up until it touches the edges): the days of the weather column.
TIGHT_SIZES = [(44, 3.0), (30, 2.4)]

# Large weather pictures for the band of the "Briefing" layout: drawn in
# their final size (not scaled up) into a box of BAND_W x BAND_H. Rain is an
# umbrella with four drops falling on it (2026-10-26) instead of the rain
# cloud.
BAND_W, BAND_H = 156, 132
BAND_SYMBOL = 124  # size of the weather symbols in the box
BAND_STROKE = 8


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


def star(d, x, y, r, w):
    """Small snow star: three strokes crossing at 60 degrees."""
    for k in range(3):
        a = math.pi / 2 + k * math.pi / 3
        rline(d, (x - math.cos(a) * r, y - math.sin(a) * r),
              (x + math.cos(a) * r, y + math.sin(a) * r), w)


def strokes(d, n, xs, st):
    """Slanted rain strokes below the cloud."""
    for x in xs:
        rline(d, (n * x, n * 0.74), (n * (x - 0.07), n * 0.92), st)


def sun(d, cx, cy, r, st, ray_len):
    # Filled disc (Tobias, 2026-10-21: the sun is filled in every symbol).
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
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
                    if (n / SS < 48 or n / SS >= 100) and i in (1, 2, 0):
                        continue  # hidden behind the cloud anyway, only stubs would remain
                    a = i * math.pi / 4
                    rline(d, (cx + math.cos(a) * r * inner, cy + math.sin(a) * r * inner),
                          (cx + math.cos(a) * r * outer, cy + math.sin(a) * r * outer), width)
            else:
                crescent(d, n, n * 0.02, n * 0.0, n * 0.62)
            cloud(d, n * 0.22, n * 0.88, n * 0.76, st, gap=st * 1.3)
        elif kind == "cloud":
            cloud(d, n * 0.06, n * 0.78, n * 0.88, st)
        elif kind in ("rain_light", "rain", "rain_heavy"):
            cloud(d, n * 0.06, n * 0.62, n * 0.88, st)
            strokes(d, n, {"rain_light": (0.52,), "rain": (0.42, 0.62),
                           "rain_heavy": (0.32, 0.52, 0.72)}[kind], st)
        elif kind in ("showers", "showers_light", "showers_moderate"):
            # Sun behind the cloud (rays only where they show), rain below:
            # one, two or three strokes (W1: codes 80, 81, 82).
            cx, cy, r = n * 0.33, n * 0.30, n * 0.11
            d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
            for i in range(3, 8):
                a = i * math.pi / 4
                rline(d, (cx + math.cos(a) * r * 1.75, cy + math.sin(a) * r * 1.75),
                      (cx + math.cos(a) * r * 2.75, cy + math.sin(a) * r * 2.75), st)
            cloud(d, n * 0.22, n * 0.70, n * 0.76, st, gap=st * 1.3)
            xs = {"showers_light": (0.62,), "showers_moderate": (0.52, 0.72),
                  "showers": (0.44, 0.62, 0.80)}[kind]
            for x in xs:
                rline(d, (n * x, n * 0.80), (n * (x - 0.06), n * 0.94), st)
        elif kind == "freezing":
            cloud(d, n * 0.06, n * 0.62, n * 0.88, st)
            strokes(d, n, (0.30, 0.74), st)
            star(d, n * 0.50, n * 0.84, n * 0.085, st * 0.75)
        elif kind == "snow":
            cloud(d, n * 0.06, n * 0.62, n * 0.88, st)
            for x, y in ((0.28, 0.82), (0.52, 0.86), (0.76, 0.82)):
                star(d, n * x, n * y, n * 0.085, st * 0.75)
        elif kind == "snow_light":
            cloud(d, n * 0.06, n * 0.62, n * 0.88, st)
            star(d, n * 0.52, n * 0.84, n * 0.095, st * 0.75)
        elif kind == "thunder_hail":
            # The bolt moved to the left, three hail grains on the right.
            cloud(d, n * 0.06, n * 0.62, n * 0.88, st)
            d.polygon([(n * 0.46, n * 0.60), (n * 0.30, n * 0.80), (n * 0.42, n * 0.80),
                       (n * 0.34, n * 0.98), (n * 0.56, n * 0.72), (n * 0.44, n * 0.72),
                       (n * 0.52, n * 0.60)], fill=255)
            r = n * 0.055
            for x, y in ((0.70, 0.78), (0.84, 0.84), (0.72, 0.94)):
                d.ellipse((n * x - r, n * y - r, n * x + r, n * y + r), fill=255)
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


def tight(kind, size, stroke):
    """The symbol drawn as large as possible inside size x size, centred."""
    best = None
    for n in range(size, 2 * size):
        img = weather(kind, n, stroke)
        box = img.getbbox()
        if box is None:
            break
        if box[2] - box[0] > size or box[3] - box[1] > size:
            break
        best = img.crop(box)
    out = Image.new("1", (size, size), 0)
    out.paste(best, ((size - best.width) // 2, (size - best.height) // 2))
    return out


def umbrella(size):
    """Open umbrella with a scalloped edge and a hooked handle."""
    def painter(d, n):
        st = n / 14
        cx = n / 2
        d.pieslice((n * 0.06, n * 0.12, n * 0.94, n * 0.92), 180, 360, fill=255)
        for i in range(4):
            x0 = n * 0.06 + i * n * 0.22
            d.ellipse((x0, n * 0.44, x0 + n * 0.22, n * 0.60), fill=0)
        d.rectangle((cx - st / 2, n * 0.48, cx + st / 2, n * 0.84), fill=255)
        d.arc((cx - n * 0.14, n * 0.74, cx + st / 2, n * 0.94), 0, 180, fill=255, width=int(st))
        d.ellipse((cx - st / 2, n * 0.06, cx + st / 2, n * 0.14), fill=255)
    return mask(size, painter)


def drop(size):
    """One falling drop, its round end at the bottom."""
    def painter(d, n):
        cx, cy, r = n / 2, n * 0.68, n * 0.28
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
        d.polygon([(cx - r, cy - r * 0.2), (cx, cy - r * 2.4), (cx + r, cy - r * 0.2)], fill=255)
    return mask(size, painter)


def snowflake(size, stroke):
    """Six arms with two pairs of branches each."""
    def painter(d, n):
        st = stroke * SS
        c = n / 2
        arm = n * 0.44
        for i in range(6):
            a = math.pi / 2 + i * math.pi / 3
            ex, ey = c + math.cos(a) * arm, c - math.sin(a) * arm
            rline(d, (c, c), (ex, ey), st)
            for along, length in ((0.45, 0.17), (0.72, 0.13)):
                bx, by = c + math.cos(a) * arm * along, c - math.sin(a) * arm * along
                for side in (-1, 1):
                    b = a + side * math.pi / 4
                    rline(d, (bx, by), (bx + math.cos(b) * n * length,
                                        by - math.sin(b) * n * length), st)
    return mask(size, painter)


def band_mask(painter):
    """Band-sized drawing at SS times the size, turned into 1 bit."""
    m = Image.new("L", (BAND_W * SS, BAND_H * SS), 0)
    painter(ImageDraw.Draw(m), SS)
    m = m.resize((BAND_W, BAND_H), Image.LANCZOS)
    return m.point(lambda v: 255 if v >= 120 else 0).convert("1")


def windsock(d, k, x0, y0, length, mouth, end, droop, st):
    """Windsock on a pole with a foot: mouth at (x0, y0), five stripes,
    alternately filled and outlined, the end drooping."""
    rline(d, (x0 * k, (y0 - mouth * 0.15) * k), (x0 * k, 118 * k), st * k)
    rline(d, ((x0 - 12) * k, 118 * k), ((x0 + 12) * k, 118 * k), st * k)

    def edge(t):
        x = x0 + 4 + t * length
        h = mouth + (end - mouth) * t
        cy = y0 + mouth / 2 + droop * t * t
        return x, cy - h / 2, cy + h / 2

    for i in range(5):
        xa, ta, ba = edge(i / 5)
        xb, tb, bb = edge((i + 1) / 5)
        poly = [(xa * k, ta * k), (xb * k, tb * k), (xb * k, bb * k), (xa * k, ba * k)]
        if i % 2 == 0:
            d.polygon(poly, fill=255)
        else:
            d.line(poly + [poly[0]], fill=255, width=int(st * 0.8 * k), joint="curve")


def wind_curls(d, k, lines, st):
    """Wind lines (x0, x1, y, curl): curl -1 ends at the left in a swirl
    turning up, +1 turning down, 0 is a plain line."""
    for x0, x1, y, curl in lines:
        r = 12
        rline(d, ((x0 + r if curl else x0) * k, y * k), (x1 * k, y * k), st * k)
        if curl:
            cx, cy = x0 + r, y + curl * r
            box = ((cx - r) * k, (cy - r) * k, (cx + r) * k, (cy + r) * k)
            d.arc(box, 90, 360, fill=255, width=int(st * k)) if curl < 0 else \
                d.arc(box, 0, 270, fill=255, width=int(st * k))


def driven_rain(d, k, starts, st, length):
    """Rain strokes slanted by the wind."""
    for x, y in starts:
        rline(d, (x * k, y * k), ((x - length * 0.6) * k, (y + length) * k), st * k)


def band_wind(kind):
    """Wind pictures of the band (W1): windsock, windsock with driven rain,
    snowflake with wind lines."""
    def painter(d, k):
        if kind == "wind_sock":
            windsock(d, k, 20, 16, 124, 58, 18, 26, 7)
        elif kind == "storm_rain":
            windsock(d, k, 18, 6, 122, 46, 16, 12, 7)
            driven_rain(d, k, ((60, 70), (94, 70), (128, 70), (88, 100), (122, 100)), 6, 18)
        elif kind == "snow_wind":
            flake = snowflake(90, BAND_STROKE * 0.8)
            d.bitmap((66 * k, 22 * k), flake.convert("L").resize((90 * k, 90 * k)), fill=255)
            wind_curls(d, k, ((4, 60, 42, -1), (14, 56, 67, 0), (4, 60, 92, 1)), 7)
    return band_mask(painter)


def band(kind):
    """Weather picture for the band (BAND_W x BAND_H, 1 bit)."""
    img = Image.new("1", (BAND_W, BAND_H), 0)
    drops = {"rain": ((30, 2), (58, 12), (86, 0), (114, 10)),
             "rain_light": ((48, 6), (94, 4)),
             "rain_heavy": ((10, 6), (32, 0), (54, 10), (76, 2), (98, 10), (120, 0))}
    if kind in drops:
        img.paste(1, ((BAND_W - 104) // 2, BAND_H - 104), umbrella(104))
        for x, y in drops[kind]:
            img.paste(1, (x, y), drop(18))
        return img
    if kind in ("wind_sock", "storm_rain", "snow_wind"):
        return band_wind(kind)
    if kind == "snow_heavy":
        # A large flake and two small ones.
        img.paste(1, (BAND_W - 96 - 6, BAND_H - 96 - 2), snowflake(96, BAND_STROKE * 0.75))
        for x, y, size in ((4, 6, 44), (6, 70, 34)):
            img.paste(1, (x, y), snowflake(size, 3.2))
        return img
    if kind == "snow":
        symbol = snowflake(BAND_SYMBOL - 8, BAND_STROKE * 0.85)
        img.paste(1, ((BAND_W - symbol.width) // 2, (BAND_H - symbol.height) // 2), symbol)
        return img
    symbol = weather(kind, BAND_SYMBOL, BAND_STROKE)
    img.paste(1, ((BAND_W - BAND_SYMBOL) // 2, (BAND_H - BAND_SYMBOL) // 2), symbol)
    return img


def calendar_icon(size):
    """Small calendar sheet with two rings (all-day events)."""
    def painter(d, n):
        st = max(1.6 * SS, n / 8)
        d.rounded_rectangle((n * 0.08, n * 0.16, n * 0.92, n * 0.94), radius=n * 0.1, outline=255,
                            width=int(st))
        d.rectangle((n * 0.08, n * 0.16, n * 0.92, n * 0.38), fill=255)
        for x in (0.3, 0.7):
            d.rectangle((n * x - st / 2, n * 0.04, n * x + st / 2, n * 0.24), fill=255)
    return mask(size, painter)


def pin(size):
    """Location pin (a station in the departure column)."""
    def painter(d, n):
        cx, r = n / 2, n * 0.32
        d.ellipse((cx - r, n * 0.06, cx + r, n * 0.06 + 2 * r), fill=255)
        d.polygon([(cx - r * 0.86, n * 0.06 + r * 1.5), (cx, n * 0.98),
                   (cx + r * 0.86, n * 0.06 + r * 1.5)], fill=255)
        q = r * 0.42
        d.ellipse((cx - q, n * 0.06 + r - q, cx + q, n * 0.06 + r + q), fill=0)
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
    for enum, kind in WEATHER + DAY_ONLY:
        for size, stroke in TIGHT_SIZES:
            name = f"k{kind.title().replace('_', '')}{size}Tight"
            lines += array(name, tight(kind, size, stroke)) + [""]
            table.append((enum, size, name))
    lines += array("kCalendar15", calendar_icon(15)) + [""]
    lines += array("kPin16", pin(16)) + [""]
    band_table = []
    for enum, kind in WEATHER + BAND_EXTRA:
        name = f"kBand{kind.title().replace('_', '')}"
        lines += array(name, band(kind)) + [""]
        band_table.append((enum, name))
    lines += ["}  // namespace", "", "const WeatherIconEntry kWeatherIcons[] = {"]
    for enum, size, name in table:
        lines.append(f"    {{WeatherIcon::{enum}, {{{size}, {size}, {name}}}}},")
    lines += ["};", f"const size_t kWeatherIconCount = {len(table)};", "",
              "const WeatherIconEntry kBandIcons[] = {"]
    for enum, name in band_table:
        lines.append(f"    {{WeatherIcon::{enum}, {{{BAND_W}, {BAND_H}, {name}}}}},")
    lines += [
        "};",
        f"const size_t kBandIconCount = {len(band_table)};",
        "",
        "const Icon kCalendarIcon = {15, 15, kCalendar15};",
        "const Icon kPinIcon = {16, 16, kPin16};",
        "",
        "}  // namespace ui",
        "",
    ]
    OUT.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"{OUT.relative_to(ROOT)}: {len(table)} weather symbols, {len(band_table)} band "
          "pictures, calendar, pin")
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""Emoji icons: own drawings, 1 bit, for event titles in the calendar.

The UI fonts only contain letters and punctuation. The calendar shows a
small set of emoji as simple pictures instead (see lib/ui/src/ui/emoji.h,
which maps Unicode emoji to these icons). fontgen.py draws every icon in the
size of the font and adds it to the font as a glyph in the Unicode private
use area (U+E000 + index in ICONS).

Every painter draws into a 18 x 18 grid (coordinates in "units"); `u` is the
number of pixels per unit. Everything is drawn at four times the size and
scaled down, like tools/icongen/icongen.py. The icons are original
drawings, no third-party artwork.
"""

import math

from PIL import Image, ImageChops, ImageDraw

SS = 4  # supersampling factor
FIRST_CODEPOINT = 0xE000


def render(painter, size, rotation=0):
    """Returns the icon as rows of 0/1 (1 = ink), size x size pixels."""
    n = size * SS
    img = Image.new("L", (n, n), 0)
    painter(ImageDraw.Draw(img), n / 18)
    if rotation:
        img = img.rotate(rotation, resample=Image.BICUBIC)
    img = img.resize((size, size), Image.LANCZOS).point(lambda v: 255 if v >= 120 else 0)
    return [[1 if img.getpixel((x, y)) else 0 for x in range(size)] for y in range(size)]


def P(u, pts):
    return [(x * u, y * u) for x, y in pts]


def poly(d, u, pts, f=255):
    d.polygon(P(u, pts), fill=f)


def circ(d, u, cx, cy, r, f=255):
    d.ellipse(((cx - r) * u, (cy - r) * u, (cx + r) * u, (cy + r) * u), fill=f)


def ring(d, u, cx, cy, r, w):
    d.ellipse(
        ((cx - r) * u, (cy - r) * u, (cx + r) * u, (cy + r) * u), outline=255, width=int(w * u)
    )


def line(d, u, pts, w, f=255):
    pp = P(u, pts)
    d.line(pp, fill=f, width=int(w * u), joint="curve")
    for x, y in pp:
        d.ellipse((x - w * u / 2, y - w * u / 2, x + w * u / 2, y + w * u / 2), fill=f)


def rect(d, u, x0, y0, x1, y1, f=255):
    d.rectangle((x0 * u, y0 * u, x1 * u, y1 * u), fill=f)


def arc(d, u, cx, cy, r, a0, a1, w):
    d.arc(
        ((cx - r) * u, (cy - r) * u, (cx + r) * u, (cy + r) * u), a0, a1, fill=255, width=int(w * u)
    )


def mini_heart(d, u, cx, cy, s, f=255):
    circ(d, u, cx - s * 0.5, cy - s * 0.25, s * 0.55, f)
    circ(d, u, cx + s * 0.5, cy - s * 0.25, s * 0.55, f)
    poly(
        d,
        u,
        [(cx - s * 1.02, cy - s * 0.05), (cx + s * 1.02, cy - s * 0.05), (cx, cy + s * 1.15)],
        f,
    )


def face(d, u):
    ring(d, u, 9, 9, 8, 1.6)


def draw_heart(d, u):
    mini_heart(d, u, 9, 7.4, 5.2)


def draw_laughing_face(d, u):
    face(d, u)
    circ(d, u, 6, 6.2, 1.1)
    circ(d, u, 12, 6.2, 1.1)
    d.pieslice((5.2 * u, 6.6 * u, 12.8 * u, 14.2 * u), 0, 180, fill=255)


def draw_smiling_face(d, u):
    face(d, u)
    circ(d, u, 6, 6.8, 1.1)
    circ(d, u, 12, 6.8, 1.1)
    arc(d, u, 9, 8.5, 4.6, 25, 155, 1.6)


def draw_loving_face(d, u):
    face(d, u)
    mini_heart(d, u, 5.8, 6.4, 2.4)
    mini_heart(d, u, 12.2, 6.4, 2.4)
    arc(d, u, 9, 9.2, 4.2, 30, 150, 1.6)


def draw_crying_face(d, u):
    face(d, u)
    circ(d, u, 6, 6, 1.1)
    circ(d, u, 12, 6, 1.1)
    d.pieslice((6 * u, 10.8 * u, 12 * u, 16.8 * u), 180, 360, fill=255)
    line(d, u, [(4.8, 8), (4.8, 12.4)], 1.4)
    line(d, u, [(13.2, 8), (13.2, 12.4)], 1.4)


def draw_winking_face(d, u):
    face(d, u)
    circ(d, u, 6, 6.8, 1.1)
    line(d, u, [(10.6, 6.8), (13.4, 6.8)], 1.4)
    arc(d, u, 9, 8.5, 4.6, 25, 155, 1.6)


def draw_thumbs_up(d, u):
    rect(d, u, 1.8, 8.2, 4.8, 16.6)
    poly(
        d,
        u,
        [
            (6.2, 16.6),
            (6.2, 8.6),
            (9.3, 2),
            (11.6, 2.4),
            (10.6, 7),
            (15.6, 7),
            (16.2, 8.6),
            (15.4, 15.4),
            (14, 16.6),
        ],
    )


def draw_party(d, u):
    poly(d, u, [(1.8, 16.6), (5.4, 7), (11, 12.6)])
    for x, y in [(11, 3.6), (14.8, 6.2), (9, 2.4), (14.2, 10.4), (16.2, 2.8)]:
        circ(d, u, x, y, 1)


def draw_cake(d, u):
    rect(d, u, 2.5, 11.2, 15.5, 16.6)
    rect(d, u, 4.5, 7.8, 13.5, 11)
    for x in (6.5, 9, 11.5):
        line(d, u, [(x, 4.6), (x, 7)], 1.4)
        circ(d, u, x, 3.2, 1.1)


def draw_gift(d, u):
    rect(d, u, 1.8, 6.5, 16.2, 9.5)
    rect(d, u, 3.2, 9.5, 14.8, 16.6)
    rect(d, u, 8.1, 6.5, 9.9, 9.5, 0)
    rect(d, u, 8.1, 9.5, 9.9, 16.6, 0)
    ring(d, u, 6.6, 3.8, 2.3, 1.3)
    ring(d, u, 11.4, 3.8, 2.3, 1.3)


def draw_star(d, u):
    pts = []
    for i in range(10):
        r = 8.7 if i % 2 == 0 else 3.7
        a = -math.pi / 2 + i * math.pi / 5
        pts.append((9 + r * math.cos(a), 9.6 + r * math.sin(a)))
    poly(d, u, pts)


def draw_check(d, u):
    line(d, u, [(2.5, 9.6), (7, 14.2), (15.6, 3.8)], 2.8)


def draw_warning(d, u):
    poly(d, u, [(9, 1.6), (17, 16), (1, 16)])
    line(d, u, [(9, 7), (9, 11)], 1.9, 0)
    circ(d, u, 9, 13.7, 1.1, 0)


def draw_exclamation(d, u):
    poly(d, u, [(6.8, 1.8), (11.2, 1.8), (10.3, 11.4), (7.7, 11.4)])
    circ(d, u, 9, 15, 1.8)


def draw_fire(d, u):
    poly(
        d,
        u,
        [
            (9.5, 1),
            (12.5, 5.5),
            (14.8, 9.5),
            (14.2, 13.6),
            (11.8, 16.6),
            (6.2, 16.6),
            (3.8, 13.6),
            (3.4, 9.8),
            (5.2, 6.8),
            (7, 9),
            (7.8, 5),
        ],
    )
    circ(d, u, 9, 13.6, 2.3, 0)


def draw_calendar(d, u):
    rect(d, u, 2, 3.5, 16, 16.6)
    rect(d, u, 3.6, 8.6, 14.4, 15, 0)
    rect(d, u, 5, 10.4, 8, 13.4)
    rect(d, u, 10, 10.4, 13, 13.4)
    line(d, u, [(5.6, 1.4), (5.6, 5)], 1.7)
    line(d, u, [(12.4, 1.4), (12.4, 5)], 1.7)


def draw_clock(d, u):
    ring(d, u, 9, 10.2, 6.3, 1.8)
    circ(d, u, 3.4, 3.9, 2.3)
    circ(d, u, 14.6, 3.9, 2.3)
    line(d, u, [(9, 10.2), (9, 6.6)], 1.6)
    line(d, u, [(9, 10.2), (11.6, 11.8)], 1.6)
    line(d, u, [(4.4, 16.2), (5.6, 15)], 1.5)
    line(d, u, [(13.6, 16.2), (12.4, 15)], 1.5)


def draw_phone(d, u):
    d.arc((4 * u, -4 * u, 22 * u, 14 * u), 90, 180, fill=255, width=int(2.8 * u))
    line(d, u, [(4, 5), (7.6, 5)], 3.6)
    line(d, u, [(13, 14), (13, 10.4)], 3.6)


def draw_briefcase(d, u):
    rect(d, u, 6, 2.4, 12, 5)
    rect(d, u, 7.5, 3.9, 10.5, 5, 0)
    rect(d, u, 1.8, 5, 16.2, 16.2)
    rect(d, u, 1.8, 9.6, 16.2, 10.8, 0)
    rect(d, u, 8, 8.6, 10, 11.8)


def draw_house(d, u):
    poly(d, u, [(0.6, 9.8), (9, 1.8), (17.4, 9.8), (14, 9.8), (14, 16.6), (4, 16.6), (4, 9.8)])


def draw_medical(d, u):
    ring(d, u, 9, 9, 8.1, 1.6)
    rect(d, u, 7.1, 3.8, 10.9, 14.2)
    rect(d, u, 3.8, 7.1, 14.2, 10.9)


def draw_tooth(d, u):
    poly(
        d,
        u,
        [
            (4.4, 2),
            (7.4, 2),
            (9, 3.2),
            (10.6, 2),
            (13.6, 2),
            (16, 5),
            (15.6, 9.4),
            (13.8, 16.6),
            (11.8, 16.6),
            (10.2, 11),
            (7.8, 11),
            (6.2, 16.6),
            (4.2, 16.6),
            (2.4, 9.4),
            (2, 5),
        ],
    )


def draw_cart(d, u):
    line(d, u, [(1.4, 2.6), (4, 2.6), (6.2, 12.2), (15, 12.2)], 1.7)
    poly(d, u, [(4.6, 5), (16.4, 5), (14.8, 10.6), (6, 10.6)])
    circ(d, u, 7.6, 15.4, 1.6)
    circ(d, u, 13.2, 15.4, 1.6)


def draw_airplane(d, u):
    d.ellipse((7.2 * u, 0.6 * u, 10.8 * u, 17.2 * u), fill=255)
    poly(d, u, [(9, 5.6), (17, 11.6), (17, 13.4), (9, 10.6), (1, 13.4), (1, 11.6)])
    poly(d, u, [(9, 13.6), (12.8, 16.2), (12.8, 17.4), (9, 16.4), (5.2, 17.4), (5.2, 16.2)])


def draw_car(d, u):
    rect(d, u, 1.4, 8.4, 16.6, 13.8)
    poly(d, u, [(3.8, 8.8), (6.6, 3.4), (11.8, 3.4), (14.4, 8.8)])
    poly(d, u, [(5.8, 8), (7.5, 4.9), (8.8, 4.9), (8.8, 8)], 0)
    poly(d, u, [(9.9, 8), (9.9, 4.9), (11.2, 4.9), (12.8, 8)], 0)
    for x in (5, 13):
        circ(d, u, x, 14.2, 3.3, 0)
        circ(d, u, x, 14.2, 2.3)


def draw_train(d, u):
    rect(d, u, 3.6, 1.4, 14.4, 14.4)
    rect(d, u, 5.6, 3.4, 12.4, 8, 0)
    circ(d, u, 6.6, 11.6, 1.2, 0)
    circ(d, u, 11.4, 11.6, 1.2, 0)
    line(d, u, [(5.2, 14.4), (3.6, 16.8)], 1.6)
    line(d, u, [(12.8, 14.4), (14.4, 16.8)], 1.6)


def draw_dining(d, u):
    ring(d, u, 9, 9.2, 4.6, 1.5)
    for x in (1.2, 3.4):
        line(d, u, [(x, 2), (x, 6.2)], 1.1)
    line(d, u, [(1.2, 6.2), (3.4, 6.2)], 1.1)
    line(d, u, [(2.3, 6.2), (2.3, 16.4)], 1.5)
    poly(d, u, [(14.8, 2), (16.8, 2.6), (16.8, 9.6), (14.8, 9.6)])
    line(d, u, [(15.8, 9.4), (15.8, 16.4)], 1.5)


def draw_coffee(d, u):
    poly(d, u, [(2.4, 7), (12.6, 7), (11.6, 14.6), (3.4, 14.6)])
    ring(d, u, 13, 9.8, 2.4, 1.5)
    rect(d, u, 10.4, 7, 13, 8.5, 255)
    rect(d, u, 1.2, 15.8, 14, 17)
    line(d, u, [(5, 2.4), (5, 5.4)], 1.4)
    line(d, u, [(9, 2.4), (9, 5.4)], 1.4)


def draw_soccer(d, u):
    n = int(18 * u)
    ring(d, u, 9, 9, 8.2, 1.5)
    poly(
        d,
        u,
        [
            (
                9 + 3.1 * math.cos(-math.pi / 2 + i * 2 * math.pi / 5),
                9 + 3.1 * math.sin(-math.pi / 2 + i * 2 * math.pi / 5),
            )
            for i in range(5)
        ],
    )
    tmp = Image.new("L", (n, n), 0)
    td = ImageDraw.Draw(tmp)
    for i in range(5):
        a = -math.pi / 2 + math.pi / 5 + i * 2 * math.pi / 5
        cx, cy = 9 + 8.4 * math.cos(a), 9 + 8.4 * math.sin(a)
        td.ellipse(((cx - 2.6) * u, (cy - 2.6) * u, (cx + 2.6) * u, (cy + 2.6) * u), fill=255)
    disc = Image.new("L", (n, n), 0)
    ImageDraw.Draw(disc).ellipse((0.8 * u, 0.8 * u, 17.2 * u, 17.2 * u), fill=255)
    d._image.paste(255, mask=ImageChops.multiply(tmp, disc))


# (name, painter, rotation in degrees). The order is the glyph order:
# U+E000 + index; keep it in sync with ui::EmojiIcon in emoji.h.
ICONS = [
    ("heart", draw_heart, 0),
    ("laughing_face", draw_laughing_face, 0),
    ("smiling_face", draw_smiling_face, 0),
    ("loving_face", draw_loving_face, 0),
    ("crying_face", draw_crying_face, 0),
    ("winking_face", draw_winking_face, 0),
    ("thumbs_up", draw_thumbs_up, 0),
    ("party", draw_party, 0),
    ("cake", draw_cake, 0),
    ("gift", draw_gift, 0),
    ("star", draw_star, 0),
    ("check", draw_check, 0),
    ("warning", draw_warning, 0),
    ("exclamation", draw_exclamation, 0),
    ("fire", draw_fire, 0),
    ("calendar", draw_calendar, 0),
    ("clock", draw_clock, 0),
    ("phone", draw_phone, 0),
    ("briefcase", draw_briefcase, 0),
    ("house", draw_house, 0),
    ("medical", draw_medical, 0),
    ("tooth", draw_tooth, 0),
    ("cart", draw_cart, 0),
    ("airplane", draw_airplane, -45),
    ("car", draw_car, 0),
    ("train", draw_train, 0),
    ("dining", draw_dining, 0),
    ("coffee", draw_coffee, 0),
    ("soccer", draw_soccer, 0),
]

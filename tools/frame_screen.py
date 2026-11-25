#!/usr/bin/env python3
"""Draws the picture of the screen for the README: docs/images/screen.png.

Takes the reference image test/fixtures/ui/sim_briefing_morning.png (the
whole screen in the simulation, see test/test_app_simulation) and puts a
light frame with rounded corners and a soft shadow around it, so that it
looks like a display. Run it after the reference image has changed:

    pip install -r tools/fontgen/requirements.txt
    python tools/frame_screen.py

The output is deterministic and has no metadata. Needs Pillow.
"""

import pathlib

from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "test" / "fixtures" / "ui" / "sim_briefing_morning.png"
TARGET = ROOT / "docs" / "images" / "screen.png"

SCALE = 4  # drawn larger and scaled down, for smooth corners
PAD = 24  # room for the shadow
BEZEL = 36  # width of the frame around the screen
RADIUS = 22  # corners of the frame
BODY = (236, 236, 232, 255)  # light grey, like a case of light filament
EDGE = (200, 200, 196, 255)  # outline of the frame
INNER = (170, 170, 168, 255)  # thin line around the screen
SHADOW = (0, 0, 0, 70)


def main():
    screen = Image.open(SOURCE).convert("RGB")
    w, h = screen.size
    width, height = w + 2 * (BEZEL + PAD), h + 2 * (BEZEL + PAD)
    s = SCALE
    big = Image.new("RGBA", (width * s, height * s), (0, 0, 0, 0))

    shadow = Image.new("RGBA", big.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        [PAD * s, (PAD + 6) * s, (width - PAD) * s, (height - PAD + 6) * s], RADIUS * s, fill=SHADOW
    )
    big = Image.alpha_composite(big, shadow.filter(ImageFilter.GaussianBlur(10 * s)))

    draw = ImageDraw.Draw(big)
    draw.rounded_rectangle(
        [PAD * s, PAD * s, (width - PAD) * s - 1, (height - PAD) * s - 1],
        RADIUS * s,
        fill=BODY,
        outline=EDGE,
        width=2 * s,
    )
    x0 = y0 = (PAD + BEZEL) * s
    draw.rectangle([x0 - 2 * s, y0 - 2 * s, x0 + (w + 2) * s - 1, y0 + (h + 2) * s - 1], fill=INNER)

    out = big.resize((width, height), Image.LANCZOS)
    out.paste(screen, (PAD + BEZEL, PAD + BEZEL))
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    out.save(TARGET, optimize=True)
    print(f"frame_screen: wrote {TARGET.relative_to(ROOT)} ({width} x {height})")


if __name__ == "__main__":
    main()

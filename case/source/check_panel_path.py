"""Checks in 2D that a rigid panel (height H, thickness t) can be inserted into the pocket of the
frame: it stands on its bottom edge in the slot behind the lip, tilted forward, and swings back
until it rests against the back plate. Uses the upright coordinates and the values of
inktavla_case.py (the frame is the same in both variants).

Upright coordinates: back plate at Y' < 0, slot of width SLOT_W behind the lip (up to Z' = LIP_H,
inner face slanted by LIP_TAPER), lip up to the front face Y' = FRONT_U, rim above the panel with a
rising ceiling and a lead-in chamfer.

Usage:  python check_panel_path.py
Prints for a reserve of 0 and 0.3 (a panel that much larger all round) whether the path is free.
"""
import math
import sys

import numpy as np

sys.argv = ["inktavla_case.py", "slim"]
import inktavla_case as g  # noqa: E402 - only the constants are used

H, t = g.PANEL[1], g.PANEL[2]


def make(margin):
    w, lip, front, taper = g.SLOT_W, g.LIP_H, g.FRONT_U, g.LIP_TAPER
    top, slope, flat = g.POCKET_TOP, g.CEIL_SLOPE, g.CEIL_FLAT

    def ceil(y):
        return top + slope * y if y <= flat else top + slope * flat + (y - flat)

    def free(y, z):
        if y < -1e-9 or z < -1e-9:
            return False                                             # back plate, floor
        if z <= lip and y > w + taper * z / lip - 1e-9 and y < front:
            return False                                             # lip
        if y < front and z > ceil(y) - margin:
            return False                                             # rim above the panel
        return True
    return free


def fits(free, a, y0, zb):
    s, c = math.sin(a), math.cos(a)
    for u in np.linspace(0, H, 400):          # rear and front face of the panel
        for v in (0.0, t):
            if not free(y0 + u * s + v * c, zb + u * c - v * s):
                return False
    for v in np.linspace(0, t, 6):            # bottom and top edge
        for u in (0.0, H):
            if not free(y0 + u * s + v * c, zb + u * c - v * s):
                return False
    return True


def path(free, label):
    A = np.radians(np.arange(4.0, -0.001, -0.02))
    Z = np.arange(0.0, 12.0, 0.05)
    Y = (0.0, 0.05)
    reach = {(y, z) for y in Y for z in Z if fits(free, A[0], y, z) and A[0] * H > 3}
    for a in A[1:]:
        nxt = {(y, z) for y in Y for z in Z
               if any(abs(z - z0) < 0.11 for (_, z0) in reach) and fits(free, a, y, z)}
        if not nxt:
            print(f"{label}: BLOCKED at a tilt of {math.degrees(a):.2f} deg")
            return False
        reach = nxt
    print(f"{label}: free, bottom edge ends at Z' = {min(z for _, z in reach):.2f}")
    return True


if __name__ == "__main__":
    ok = all([path(make(m), f"reserve {m}") for m in (0.0, 0.3)])
    sys.exit(0 if ok else 1)

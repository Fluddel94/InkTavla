"""Puts the lid on back first: swings it in steps about its rear bottom edge (an X axis through
Y_BACK, FLOOR) and reports where it or the plungers overlap the case. Tongues against their beads
are wanted (that is the snap); everything else should stay free.

Usage:  python check_lid_swing.py scene_dir slim|slim-battery
scene_dir is the folder written by inktavla_case.py --scene for the same variant.
"""
import math
import sys

import numpy as np
import trimesh

scene, variant = sys.argv[1], sys.argv[2]
sys.argv = ["inktavla_case.py", variant]
import inktavla_case as g  # noqa: E402 - reads the variant from sys.argv

H = trimesh.load(f"{scene}/case.stl")
L = trimesh.load(f"{scene}/lid.stl")
P = trimesh.load(f"{scene}/plungers.stl")
pivot = np.array([0, g.Y_BACK, g.FLOOR])
for deg in (25, 20, 15, 10, 7, 5, 3, 2, 1, 0.5):
    R = trimesh.transformations.rotation_matrix(math.radians(deg), [1, 0, 0], pivot)  # + = front up
    out = []
    for name, m in (("lid", L), ("plungers", P)):
        mm = m.copy()
        mm.apply_transform(R)
        i = trimesh.boolean.intersection([H, mm], engine="manifold")
        if i.volume > 1e-3:
            for b in sorted(i.split(only_watertight=False), key=lambda b: -abs(b.volume))[:3]:
                lo, hi = b.bounds
                out.append(f"{name} {abs(b.volume):.2f} mm3 at X {lo[0]:.0f}..{hi[0]:.0f} "
                           f"Y {lo[1]:.1f}..{hi[1]:.1f} Z {lo[2]:.1f}..{hi[2]:.1f}")
    print(f"{deg:>4} deg:", "; ".join(out) if out else "free")

"""Checks the parts of one variant: closed solids, no overlaps between parts and hardware ghosts,
sizes of the print files (bed 180 x 180).

Usage:  python check.py scene_dir [print_dir]
scene_dir is the folder written by inktavla_case.py --scene, print_dir the folder with the STL files
(default: none). Overlaps below 0.001 mm3 are not listed; the case touches the panel along a face,
which shows up as a few thousandths at most.
"""
import glob
import itertools
import os
import sys

import trimesh

scene = sys.argv[1]
S = {os.path.basename(f)[:-4]: trimesh.load(f) for f in glob.glob(os.path.join(scene, "*.stl"))}
for k, m in sorted(S.items()):
    print(f"{k:12s} watertight={m.is_watertight} vol={m.volume / 1000:7.2f}cm3 bounds={m.bounds.round(2).tolist()}")
print("--- intersections (mm3)")
names = [n for n in ["case", "lid", "press_posts", "plungers", "board", "components", "solder", "battery", "panel"]
         if n in S]
SKIP = {("lid", "press_posts")}  # the press posts are part of the lid
found = 0
for a, b in itertools.combinations(names, 2):
    if (a, b) in SKIP:
        continue
    try:
        v = trimesh.boolean.intersection([S[a], S[b]], engine="manifold").volume
    except Exception as e:  # noqa: BLE001 - report and go on
        v = f"ERR {e}"
    if not isinstance(v, float) or v > 0.001:
        found += 1
        print(f"{a} x {b}: {v}")
print("none" if not found else f"{found} found")
if len(sys.argv) > 2:
    print("--- print files")
    for f in sorted(glob.glob(os.path.join(sys.argv[2], "*.stl"))):
        m = trimesh.load(f)
        e = m.extents
        print(f"{os.path.basename(f):30s} {e[0]:6.1f} x {e[1]:6.1f} x {e[2]:6.1f}  vol {m.volume / 1000:6.2f} cm3  "
              f"watertight={m.is_watertight} bodies={len(m.split(only_watertight=False))}")

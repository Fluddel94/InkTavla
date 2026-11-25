"""InkTavla case: "Slim" (USB cable, no battery) and "Slim+Battery" (with room for the kit's
battery), for the Seeed TRMNL 7.5" (OG) DIY Kit. Built with CadQuery 2.8.0.

Usage:  python inktavla_case.py slim|slim-battery [out_dir] [--scene scene_dir]
Writes the print files (STL, already in print orientation) to out_dir (default: the folder of the
variant next to this source folder, e.g. case/slim) together with a preview picture. With --scene,
the assembled parts and "ghosts" of the hardware (board, components, battery, panel) are written to
scene_dir as well, for check.py and check_lid_swing.py; they never go into the repository.

All dimensions in mm. Global coordinates:
  X = width (display centre 0), Y = depth (front = +Y), Z = height above table.
"Upright" coordinates (Y', Z') describe the display frame before it is tilted back by TILT:
  origin = rear face of the panel at its bottom edge, Y' = across the panel (front +),
  Z' = along the panel (up +).

The EE04 board lies upside down (components facing down), so its buttons and the USB-C socket
face backwards. PCB coordinates (Seeed KiCad): px = distance from the button edge (0 .. 41.23),
py = distance from the XIAO end (0 .. 80). Mapping: X = X_OFF - py, Y = YB + px.

How the case works:
  - The panel stands in a pocket of the frame, tilted back by 29 deg. It is inserted from the front:
    its bottom edge goes into the slot behind a fixed lip, then its top swings back under the rim.
    check_panel_path.py checks that a rigid panel can take this path.
  - Below the panel a cove turns the FPC backwards into a channel straight to the board's connector,
    which sits 7 below the panel's bottom edge, so the FPC is not pulled or folded.
  - The board lies in four corner pieces on the floor; posts of the lid press it down.
  - The lid snaps in with four side tongues and two front tongues; its back wall stands in a groove
    between two ribs on the floor. Put it on back first: check_lid_swing.py checks the swing.
  - Plungers in the lid's back wall press the board's buttons; labels R 3 2 1 above or below them.
  - A small arched cutout in the back plate lets a finger push the panel out from behind.
  - The logo mark is raised on the front below the display.
Comments name earlier prototype generations (G1 .. G9) where a value was changed after a test print.
"""
import math
import os
import sys
import json
import cadquery as cq

# ------------------------------------------------------------------ variant
# "slim":         board and display low, low base, no battery (powered by the USB cable)
# "slim-battery": same look, board 4.4 higher above room for the kit's battery
VARIANTS = ("slim", "slim-battery")
VARIANT = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else "slim"
assert VARIANT in VARIANTS, f"variant must be one of {VARIANTS}"
SLIM = VARIANT == "slim"
NAME = VARIANT                           # file names
MARK = "Slim" if SLIM else "Slim+B"      # second line of the text inside the case (short: room)

# ------------------------------------------------------------------ hardware
PCB_W, PCB_L, PCB_T, PCB_R = 41.23, 80.0, 1.6, 2.0
HOLES = [(2.47, 2.55), (38.68, 2.55), (2.47, 77.45), (38.68, 77.45)]  # (px, py), d 2.5
BTN_PY = [31.59, 40.93, 50.26, 59.59]        # side buttons (last one = reset)
USB_PY = 14.38
BTN_OVER, USB_OVER = 1.0, 1.5                # protrusion over the board edge (measured)
BTN_H = 1.75   # button centre below the component face (fits, test print G2)
USB_H = 2.8    # USB-C centre below the component face (fits, test print G2)
BAT = (61.0, 40.0, 6.0)                      # battery X, Y, thickness (measured)
PANEL = (170.2, 111.2, 1.3)                  # display panel W, H, thickness (1.3 measured)
# keep-out on the board's top face (solder joints of buttons and switch, test print G2)
SOLDER_PX = 7.5                              # from the button edge
SOLDER_PY = (27.3, 75.5)                     # first button - 2 .. battery switch + 1.3
CONN_PY = (22.3, 37.8)                       # 24-pin FPC connector (front edge, underside)

# ------------------------------------------------------------------ layout C2
FLOOR = 1.6         # floor thickness
POST_H = 6.6 if SLIM else 11.0   # slim: components 4.5 + 2.1 for the FPC slack under the board
#      # component face above floor
ZC = FLOOR + POST_H  # component face (board underside) above table = 12.6
X_OFF = 30.1        # X of the board's XIAO end (py = 0); FPC connector centre at X ~ 0
WALL, COLLAR, LID, CLAMP = 2.0, 2.5, 1.8, 2.0   # CLAMP: headroom above the board
WEDGE_X = (-60.0, 60.0)   # outer side faces of the rear housing: symmetric (G4: -60 .. 40)
Z_TOP = 21.6 if SLIM else 26.0   # slim: 4.4 lower, like the board and the display
#       # height of the lid where it meets the frame's back plate (G6: 21, G5: 23, G4: 26).
                    # Moves with the display (G7: 5 up), so the lid front stays at the same place on the
                    # plate; lower = lid front further forward (front press posts must stay behind it)
RC = 7.0            # family look (G9: 4.0); the cavity corners are rounded with RC - WALL
#           # radius of the two rear vertical corners (belong to the lid)
R_TOP = 2.0 if SLIM else 3.0   # G9: 2.5; slim: flat back wall up to the labels
#        # radius of the rear top edge
FOOT_BEHIND = 6.0   # frame foot ends this far in front of the housing's back face
YF = 45.4           # FPC edge (front) of the board (as G4)
YB = YF - PCB_W     # button edge (rear) of the board
Y_BACK = YB - USB_OVER - COLLAR - WALL   # outer back face of the housing
H = ZC + PCB_T + CLAMP + LID + 0.3       # height at the back wall
BTN_Z = ZC - BTN_H
USB_Z = ZC - USB_H


def X(py): return X_OFF - py
def Y(px): return YB + px


BTN_X = [X(v) for v in BTN_PY]
USB_X = X(USB_PY)

# ------------------------------------------------------------------ display frame
TILT_DEG = 29.0                     # like TRMNL Stand-1.5 (G4: 15)
TILT = math.radians(TILT_DEG)
CT, ST = math.cos(TILT), math.sin(TILT)
CONN_Z = ZC - 1.0                   # FPC slot of the connector (board upside down)
P0 = (YF + 12.5, CONN_Z + 7.0)      # panel bottom edge, rear face (table Y, Z): 7 above the connector
                                    # (G5: 4, G6: 2 - FPC had too little room)
FR_HALF = 87.5          # frame half width (175)
PLATE_T = 2.8           # back plate behind the panel (G9: 2.8, G8: 3.2, G7: 2.4)
SLOT_W = 1.4            # slot behind the bottom lip (panel 1.3)
LIP_T = 1.2             # bottom lip thickness (front flush with the rim)
LIP_H = 9.0            # lip overlaps the panel's bottom border by this much (G7: 9, G6: 10, G5: 8; G6: display
                        # 2 lower, lip top stays at the G5 height -> 2 more of the border covered)
SLOT_HALF = 85.65       # pocket 171.3 wide -> rim 1.85 at the sides (13.11.: +0.15 per side for PLA shrinkage; was 85.5)
POCKET_CLR = 0.7        # play along the panel (111.2 + 0.7; G7: 0.6, G6: 0.4 - top edge too tight)
RIM_TOP = 2.0           # rim above the panel
POCKET_TOP = PANEL[1] + POCKET_CLR      # 111.6 (upright Z')
FR_TOP = POCKET_TOP + RIM_TOP           # 113.6
# 13.11.2026 - insertion path (display hung at the top corner in every test print since G6):
# the panel swings in about its bottom edge. With a 1.4 slot behind a 9 high lip it can only tilt
# 0.6 deg, but its top must tilt about 1.4 deg to pass the rim (2.6 deep). A rigid panel never fits;
# it only went in by bending. Checked with the 2D path simulation (check_panel_path.py):
LIP_TAPER = 0.3         # inner face of the lip slanted: slot 1.4 at the floor -> 1.7 at the lip top
CEIL_SLOPE = 0.3        # pocket ceiling rises 0.3 per mm towards the front (45 deg overhang when printed)
CEIL_FLAT = 1.6         # ... up to Y' 1.6, then a 45 deg lead-in chamfer to the front face
RELIEF_R = 0.0          # 16.11.: corner reliefs removed (they cut through the lip to the front face;
                        # not needed: 0.15 side play covers the printed corner radius)
FRONT_U = SLOT_W + LIP_T                # front face (upright Y')
FOOT_T = 6.0   # G9: 8.0
#           # foot behind the display (the "wings" left and right of the housing)
FOOT_FILLET = 14.0   # sweeping transition foot -> plate (G9: 6.0)
FRAME_R = 1.6         # rounding of the whole frame silhouette (rim, top, plate edge, foot)
#      # concave fillet foot -> back plate
# FPC deflector and channel
# FPC measured (07.11.2026): 24 from the panel edge, ~2 of it inside the connector
FPC_LEN, FPC_IN_CONN = 24.0, 2.0
DEFL_HALF = 40.0        # shallow recess under the panel's bottom edge (FPC root), was 35
DEFL_SIDE_D = 2.5       # its depth below the panel edge, was 1.5
CH_HALF = 16.0          # cove + channel through the plate bottom and the foot (32 wide, was 26)
DEFL_TAN = -0.5         # the cove radius starts this far below the panel edge (Z'), so the lip's
                        # inner face runs straight into the radius without any step
BAT_TOP = 1.6 + 6.0     # FLOOR + battery thickness
CH_FLOOR = 1.6 if SLIM else 6.0   # slim: flush with the cavity floor (no step for the FPC tip)
#         # floor of cove + straight channel (G6: 7.9 just above the battery); lower
                        # ends the FPC tip further inside: at 6.0 it reaches the channel exit
DEFL_R = 6.0            # cove radius, fixed (G6: 4 computed - too small); the lip's inner face runs
                        # straight on below the panel edge until the radius starts
CH_CEIL = P0[1] + 3.0                   # horizontal bridge through the plate bottom (was +1)
# arch in the back plate (to push the display out from behind), pointed top
ARCH = True
# G9: small, low cutout with steep flanks. The layer shift of G7/G8 came from the nozzle catching
# on the curled 45 deg flanks high up on the plate (true overhang 52 deg from vertical at 29 deg tilt).
ARCH_HALF, ARCH_GAP, ARCH_R_BOT = 20.0, 4.0, 3.0   # G8: 35 / 4 / 5
ARCH_APEX = 55.0        # apex above the panel bottom (upright Z'); G8: 89
ARCH_FLANK = 60.0       # in-plane angle of the flanks above horizontal; G8: 45
ARCH_SHOULDER_R = 8.0   # radius between the vertical sides and the flanks

# ------------------------------------------------------------------ board holder
# corner supports: one piece per board corner = square support under the corner (carries the
# board around the mounting hole) + L-shaped wall around the corner (locates the board)
GAP = 0.25              # board edge -> wall
BR_T = 1.6              # wall thickness
CORNER_IN = 5.0         # support and wall legs reach this far from the board corner (clear of
                        # XIAO, battery switch, USB-C, FPC connectors, battery cable notch)
BR_TOP = ZC + PCB_T - 0.2   # wall top 0.2 below the board's top face (press posts never land on it)
BR_CHAMFER = 0.5        # lead-in: one even 45 deg chamfer along the whole inner top edge
R_OUT = PCB_R + GAP + BR_T  # outer corner radius, concentric with the board corner (3.85)
R_SUP = 1.0             # radius of the support's inner corner (towards the board centre)
BAT_X0 = -43.0
BAT_Y0 = YF - 40.5
BAT_CLR, GUIDE_T, GUIDE_H = 0.3, 1.2, 4.0
GUIDE_LEG = 10.0     # legs along the long sides, from each corner inwards
GUIDE_TAB = 12.0     # one tab in the middle of each short side; corners stay open for the cable

# ------------------------------------------------------------------ lid, snap, plungers
GAP_B, GAP_R, GAP_F = 0.15, 0.15, 0.2   # lid clearance: bottom, rear corner ends, frame plate
PRESS_D, PRESS_GAP = 4.6, 0.0           # solid press posts on the board (0 = touching)
PRESS_ROOT_D, PRESS_ROOT_H = 6.6, 1.5   # cone at the root (lid underside)
SNAP_BEAD, BEAD_R, SNAP_CLR = 0.5, 0.6, 0.15
TONGUE_T, TONGUE_L, BEAD_DOWN = 1.2, 5.0, 2.5
GROOVE_EXTRA = 0.2                      # tolerance if the press posts come out a bit long
TONGUE_SEG = [(8.0, 23.0), (31.0, 46.0)]   # tongue segments, measured from Y_BACK
SILL_T = 1.2                            # rib on the floor just inside the lid's back wall
SILL_H = 1.5 if SLIM else 3.0           # slim: low, so it runs under the plungers and the USB socket
# 13.11.2026: the back wall stands in a groove (inner sill + outer rib): the lid can no longer slide
# backwards. Lid goes on back wall first, tilted ~20 deg front up, then the front is pressed down.
REAR_GAP = 0.25                         # outer rib <-> back wall
REAR_RIB_H, REAR_RIB_T, REAR_RIB_LEAN = 1.5, 1.4, 0.6   # height, thickness at the floor, inner face
                                                        # set back at the top (wall tilts in there)
REAR_RIB_GAP = 4.0                      # extra width of the gaps at the nail notch and the USB plug
NOTCH = False   # 13.11.: finger-nail notch removed (unreachable behind the rear rib; lid is lifted at the USB opening)
NOTCH_X, NOTCH_W, NOTCH_D = 0.0, 12.0, 0.8   # finger-nail notch under the back wall (centre), only if NOTCH
TONGUE_FIL_IN, TONGUE_FIL_OUT, WALL_RELIEF = 1.0, 0.8, 1.1   # root fillets of the tongues, relief
                                                          # of the housing wall's inner top edge
FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fonts", "RobotoCondensed-Bold.ttf")
LOGO_W, LOGO_STROKE, LOGO_H, LOGO_S = (16.0, 1.0, 0.6, 12.6) if SLIM else (22.0, 1.0, 0.6, 15.8)  # logo width, stroke, height, centre
                                                             # (along the front face from the table)
LOGO_BEVEL = 0.3            # lower edges of the raised logo set back (>= 45 deg when printed)
BTN_LABELS = ["1", "2", "3", "R"]                  # in BTN_PY order (X = -1.5 .. -29.5); from behind
                                                  # left to right: R 3 2 1 (the board lies upside down,
                                                  # so KEY3 is the button next to reset)
LABEL_SIZE, LABEL_Z, LABEL_D = (4.5, 10.2, 0.5) if SLIM else (6.0, 5.4, 0.5)   # slim: above the buttons
FSNAP_X, FSNAP_W = 21.0, 8.0               # front snap tongues at X = +-FSNAP_X (FPC channel +-16)
STRIP = 4.0                             # top strip in front of the lid that belongs to the housing
SHAFT_D, HOLE_CLR = 3.0, 0.5
COLLAR_D = 5.0
SHAFT_OUT = 3.0
USB_OPEN = (13.5, 8.0, 2.5)


# ------------------------------------------------------------------ helpers
def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane().box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def cyl(r, h, pnt, d):
    return cq.Workplane().add(cq.Solid.makeCylinder(r, h, cq.Vector(*pnt), cq.Vector(*d)))


def prism_yz(pts, x0, x1):
    w = cq.Wire.makePolygon([cq.Vector(x0, y, z) for y, z in pts], close=True)
    return cq.Workplane().add(cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(x1 - x0, 0, 0)))


def prism_xz_along(pts, y0, vec):
    """Profile in XZ at Y=y0, extruded along an arbitrary vector (sheared prism)."""
    w = cq.Wire.makePolygon([cq.Vector(x, y0, z) for x, z in pts], close=True)
    return cq.Workplane().add(cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(*vec)))


def rbox_xy(x0, x1, y0, y1, z0, z1, r):
    return box(x0, x1, y0, y1, z0, z1).edges("|Z").fillet(r)


def text_solid(txt, size, depth):
    """Text as a solid in the XY plane (z = 0 .. -depth), centred on its bounding box.
    Built from the glyph outlines (matplotlib TextPath, even-odd fill) because OCCT's own text
    makes invalid solids from some glyphs of the bold font (e.g. 'a')."""
    from matplotlib.textpath import TextPath
    from matplotlib.font_manager import FontProperties
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    tp = TextPath((0, 0), txt, size=size, prop=FontProperties(fname=FONT))
    geo = None
    for ring in tp.to_polygons(closed_only=True):
        if len(ring) < 3:
            continue
        p = Polygon(ring).buffer(0)
        geo = p if geo is None else geo.symmetric_difference(p)
    geo = unary_union(geo)
    x0, y0, x1, y1 = geo.bounds
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    out = None
    for poly in getattr(geo, "geoms", [geo]):
        poly = poly.simplify(0.01)
        outer = cq.Wire.makePolygon([cq.Vector(x - cx, y - cy, 0) for x, y in poly.exterior.coords[:-1]], close=True)
        inner = [cq.Wire.makePolygon([cq.Vector(x - cx, y - cy, 0) for x, y in r.coords[:-1]], close=True)
                 for r in poly.interiors]
        sol = cq.Solid.extrudeLinear(cq.Face.makeFromWires(outer, inner), cq.Vector(0, 0, -depth))
        w = cq.Workplane().add(sol)
        out = w if out is None else out.union(w)
    return out


def text_top(txt, size, x, y, z, depth=0.4):
    return text_solid(txt, size, depth).translate((x, y, z))


def text_back(txt, size, x, z, y_face, depth):
    """Text cut into a face looking towards -Y (back wall), readable from behind."""
    return text_solid(txt, size, depth).rotate((0, 0, 0), (1, 0, 0), 90).translate((x, y_face, z))


def pcb_rect(px0, px1, py0, py1, z0, z1):
    xs = sorted((X(py0), X(py1)))
    ys = sorted((Y(px0), Y(px1)))
    return box(xs[0], xs[1], ys[0], ys[1], z0, z1)


def u2t(yu, zu):
    """Upright frame coordinates -> table coordinates (Y, Z)."""
    return (P0[0] + yu * CT - zu * ST, P0[1] + yu * ST + zu * CT)


def t2u(y, z):
    dy, dz = y - P0[0], z - P0[1]
    return (dy * CT + dz * ST, -dy * ST + dz * CT)


def upright(wp):
    """Tilt a solid modelled in upright coordinates into table coordinates."""
    return wp.rotate((0, 0, 0), (1, 0, 0), TILT_DEG).translate((0, P0[0], P0[1]))


def ubox(x0, x1, y0, y1, z0, z1):
    return upright(box(x0, x1, y0, y1, z0, z1))


def line_y(yu, z):
    """Y of the line Y' = yu (a face parallel to the panel) at table height z."""
    zu = (z - P0[1] - yu * ST) / CT
    return u2t(yu, zu)[0]


def plate_rear_y(z):
    """Y of the back plate's rear face at height z (table)."""
    return line_y(-PLATE_T, z)


def fillet_pts(corner, d1, d2, r, n=12):
    """Tangent arc points for the corner between two rays from `corner` (unit dirs d1, d2)."""
    ang = math.acos(d1[0] * d2[0] + d1[1] * d2[1])
    t = r / math.tan(ang / 2)
    p1 = (corner[0] + d1[0] * t, corner[1] + d1[1] * t)
    p2 = (corner[0] + d2[0] * t, corner[1] + d2[1] * t)
    b = (d1[0] + d2[0], d1[1] + d2[1]); bn = math.hypot(*b)
    c = (corner[0] + b[0] / bn * r / math.sin(ang / 2), corner[1] + b[1] / bn * r / math.sin(ang / 2))
    a1 = math.atan2(p1[1] - c[1], p1[0] - c[0]); a2 = math.atan2(p2[1] - c[1], p2[0] - c[0])
    da = (a2 - a1 + math.pi) % (2 * math.pi) - math.pi
    return [(c[0] + r * math.cos(a1 + da * i / n), c[1] + r * math.sin(a1 + da * i / n)) for i in range(n + 1)]


Y_F = plate_rear_y(Z_TOP)                 # top line meets the back plate here
SL = (Z_TOP - H) / (Y_F - Y_BACK)          # slope of the lid's top face


def T(y):
    """Outer top face of the lid at depth y."""
    return H + (y - Y_BACK) * SL


def lid_front():
    """Point where the underside of the top (T - LID) meets the plate rear face."""
    y = Y_F
    for _ in range(60):
        y = plate_rear_y(T(y) - LID)
    return y, T(y) - LID


Y_C, Z_C = lid_front()
Y_FR = YF + 3.1         # front wall of the cavity = rear face of the display foot (channel ends 3 mm
                        # in front of the connector; 1.25 behind the front corner pieces)


# ------------------------------------------------------------------ frame
def frame():
    yb0 = Y_BACK + FOOT_BEHIND
    zu_bot = t2u(line_y(FRONT_U, 0.0), 0.0)[1]
    p_fb = u2t(FRONT_U, zu_bot)                     # front face meets the table
    p_ft = u2t(FRONT_U, FR_TOP)
    p_rt = u2t(-PLATE_T, FR_TOP)
    corner = (plate_rear_y(FOOT_T), FOOT_T)         # plate rear face meets the foot top
    up = (-ST, CT)
    arc = fillet_pts(corner, up, (-1.0, 0.0), FOOT_FILLET)
    wp = (cq.Workplane("YZ", origin=(-FR_HALF, 0, 0)).moveTo(yb0, 0.0).lineTo(p_fb[0], 0.0).lineTo(*p_ft)
          .lineTo(*p_rt).lineTo(*arc[0]).threePointArc(arc[len(arc) // 2], arc[-1]).lineTo(yb0, FOOT_T).close()
          .extrude(2 * FR_HALF))
    # family look: round the whole silhouette of both side faces and the top / foot edges
    solid = wp.val()
    sel = []
    for e in solid.Edges():
        vs = e.Vertices()
        if max(v.Z for v in vs) < 1e-6:
            continue                                   # edges on the table stay sharp
        if abs(vs[0].X - vs[-1].X) < 1e-6:
            sel.append(e)                              # perimeter of a side face
        elif any(abs(vs[0].Y - q[0]) < 1e-4 and abs(vs[0].Z - q[1]) < 1e-4 for q in (p_ft, p_rt, (yb0, FOOT_T))):
            sel.append(e)                              # top front, top rear, foot rear
    wp = cq.Workplane().add(solid.fillet(FRAME_R, sel))
    # slot behind the bottom lip, pocket open to the front above the lip
    wp = wp.cut(ubox(-SLOT_HALF, SLOT_HALF, 0.0, SLOT_W, 0.0, POCKET_TOP))
    wp = wp.cut(ubox(-SLOT_HALF, SLOT_HALF, 0.0, FRONT_U + 5, LIP_H, POCKET_TOP))
    if ARCH:
        wp = wp.cut(arch_cut())
    for c in entry_cuts():                 # one by one: a combined cut trips OCCT's clean-up
        wp = wp.cut(c)
    return wp


def entry_cuts():
    """Slanted lip, rising ceiling with lead-in chamfer and corner reliefs (upright coordinates)."""
    def prism(pts):
        w = cq.Wire.makePolygon([cq.Vector(-SLOT_HALF, y, z) for y, z in pts], close=True)
        return cq.Workplane().add(cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(2 * SLOT_HALF, 0, 0)))
    pt = POCKET_TOP
    cuts = []
    c = prism([(SLOT_W - 0.01, 0.0), (SLOT_W, 0.0), (SLOT_W + LIP_TAPER, LIP_H), (SLOT_W + LIP_TAPER, LIP_H + 0.5),
               (SLOT_W - 0.01, LIP_H + 0.5)])
    zf = pt + CEIL_SLOPE * CEIL_FLAT
    cuts.append(upright(c))
    c = (prism([(-0.01, pt - 0.3), (-0.01, pt), (CEIL_FLAT, zf), (FRONT_U + 0.5, zf + FRONT_U + 0.5 - CEIL_FLAT),
                       (FRONT_U + 0.5, pt - 0.3)]))
    cuts.append(upright(c))
    for x in ((-SLOT_HALF, SLOT_HALF) if RELIEF_R > 0 else ()):
        for z in (0.0, pt):
            cuts.append(upright(cq.Workplane().add(cq.Solid.makeCylinder(
                RELIEF_R, FRONT_U + 0.6, cq.Vector(x, -0.05, z), cq.Vector(0, 1, 0)))))
    return cuts


def arch_profile():
    """Cutout outline in upright (X, Z'): flat bottom with R corners, vertical sides, round
    shoulders (ARCH_SHOULDER_R), straight flanks at ARCH_FLANK above horizontal, pointed apex.
    Along the shoulders the edge is steeper than the flanks, so the flanks are the worst overhang."""
    zb = t2u(plate_rear_y(Z_TOP + ARCH_GAP), Z_TOP + ARCH_GAP)[1]
    phi = math.radians(ARCH_FLANK)
    t0 = math.pi / 2 - phi                  # shoulder arc 0 .. t0, tangent to the flank
    rs = ARCH_SHOULDER_R
    cx = ARCH_HALF - rs
    xt = cx + rs * math.cos(t0)
    zc = ARCH_APEX - xt * math.tan(phi) - rs * math.sin(t0)
    zt = zc + rs * math.sin(t0)
    rb = ARCH_R_BOT
    assert zc >= zb + rb, (zc, zb)
    pts = []
    for i in range(7):                                # bottom right corner
        t = -math.pi / 2 + (math.pi / 2) * i / 6
        pts.append((ARCH_HALF - rb + rb * math.cos(t), zb + rb + rb * math.sin(t)))
    for i in range(9):                                # right shoulder
        t = t0 * i / 8
        pts.append((cx + rs * math.cos(t), zc + rs * math.sin(t)))
    pts.append((0.0, ARCH_APEX))
    for i in range(9):                                # left shoulder
        t = t0 * (8 - i) / 8
        pts.append((-cx - rs * math.cos(t), zc + rs * math.sin(t)))
    for i in range(7):                                # bottom left corner
        t = math.pi + (math.pi / 2) * i / 6
        pts.append((-ARCH_HALF + rb + rb * math.cos(t), zb + rb + rb * math.sin(t)))
    return pts, zb, zc, zt


def arch_cut():
    pts, *_ = arch_profile()
    w = cq.Wire.makePolygon([cq.Vector(x, -PLATE_T - 6.0, z) for x, z in pts], close=True)
    s = cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, PLATE_T + 6.1, 0))
    return upright(cq.Workplane().add(s))


def fpc_cove():
    """Outer wall of the FPC cove (table Y, Z): the lip's inner face continues straight down,
    turns backwards with radius DEFL_R and runs out straight and horizontally on the floor
    CH_FLOOR into the cavity (over the battery)."""
    up = (-ST, CT)
    zu = (CH_FLOOR - P0[1] - SLOT_W * ST) / CT
    corner = u2t(SLOT_W, zu)                       # lip inner face line meets the floor
    arc = fillet_pts(corner, up, (-1.0, 0.0), DEFL_R, 24)
    return arc + [(Y_FR - 1.0, CH_FLOOR)]


def fpc_cut():
    """Cove under the panel's bottom edge + channel back to the board (table YZ profile).
    The FPC leaves the panel downwards along the plate. When the panel is inserted, its tip
    meets the large radius at a flat angle, is turned backwards and slides along the floor
    out of the channel in front of the connector. The deep cove also takes the FPC's slack."""
    lip_in = u2t(SLOT_W, 0.5)
    q = u2t(0.0, 0.5)                               # plate front face, a bit above the panel edge
    ceil_y = line_y(0.0, CH_CEIL)
    prof = [lip_in] + fpc_cove() + [(Y_FR - 1.0, CH_CEIL), (ceil_y, CH_CEIL), q]
    ch = prism_yz(prof, -CH_HALF, CH_HALF)
    # outside the channel: shallow recess under the panel edge (FPC root can be wider)
    side = ubox(-DEFL_HALF, DEFL_HALF, 0.0, SLOT_W, -DEFL_SIDE_D, 0.5)
    side = side.edges("|X").fillet(0.6)
    side = side.cut(box(-CH_HALF + 0.01, CH_HALF - 0.01, 0, 120, 0, 60))   # not inside the cove
    return ch.union(side)


# ------------------------------------------------------------------ rear housing body (wedge)
def wedge():
    """Outer solid of the rear housing (housing + lid), front part buried in the display foot."""
    th = math.atan(SL)
    ang = math.pi / 2 + th                     # inner angle at the rear top corner
    d = R_TOP / math.tan(ang / 2)              # corner -> tangent points
    t1 = (Y_BACK, H - d)
    u = (math.cos(th), math.sin(th))
    t2 = (Y_BACK + d * u[0], H + d * u[1])
    bis = (u[0] + 0.0, u[1] - 1.0)             # bisector (into the solid)
    n = math.hypot(*bis)
    cdist = R_TOP / math.sin(ang / 2)
    ctr = (Y_BACK + bis[0] / n * cdist, H + bis[1] / n * cdist)
    mid = (ctr[0] - bis[0] / n * R_TOP, ctr[1] - bis[1] / n * R_TOP)
    y_end = Y_F + 1.0
    wp = (cq.Workplane("YZ", origin=(WEDGE_X[0], 0, 0)).moveTo(Y_BACK, 0)
          .lineTo(plate_rear_y(0) + 1.0, 0).lineTo(y_end, T(y_end)).lineTo(*t2)
          .threePointArc(mid, t1).close().extrude(WEDGE_X[1] - WEDGE_X[0]))
    rounded = rbox_xy(WEDGE_X[0], WEDGE_X[1], Y_BACK, 90, -1, 60, RC)
    return wp.intersect(rounded)


def cavity():
    pts = [(Y_BACK + WALL, FLOOR), (Y_FR, FLOOR), (Y_FR, T(Y_FR) - LID),
           (Y_BACK + WALL, T(Y_BACK + WALL) - LID)]
    c = prism_yz(pts, WEDGE_X[0] + WALL, WEDGE_X[1] - WALL)
    return c.intersect(rbox_xy(WEDGE_X[0] + WALL, WEDGE_X[1] - WALL, Y_BACK + WALL, 90, -1, 60, RC - WALL))


def lid_region(bottom, rear_end, front_off):
    """YZ region that belongs to the lid (top plate + back wall with rounded rear corners).
    The front end is vertical, STRIP in front of the back plate, so the lid can be lowered
    straight down for the last few mm although the back plate leans over the housing."""
    yr = Y_BACK + RC - rear_end
    yf = Y_F - STRIP - front_off
    ztop = Z_TOP + 6.0
    pts = [(Y_BACK - 1, bottom), (yr, bottom), (yr, T(yr) - LID), (yf, T(yf) - LID),
           (yf, ztop), (Y_BACK - 1, ztop)]
    return prism_yz(pts, WEDGE_X[0] - 1, WEDGE_X[1] + 1)


def slope_vec(length):
    return (0.0, length, length * SL)


# ------------------------------------------------------------------ board holder features
def rrect_wire(cx, cy, w, h, r, z):
    """Rounded rectangle wire (corner radius r) in the plane Z = z."""
    f = cq.Face.makeFromWires(cq.Wire.makePolygon(
        [cq.Vector(cx - w / 2, cy - h / 2, z), cq.Vector(cx + w / 2, cy - h / 2, z),
         cq.Vector(cx + w / 2, cy + h / 2, z), cq.Vector(cx - w / 2, cy + h / 2, z)], close=True))
    return f.fillet2D(r, f.Vertices()).outerWire()


def board_pocket():
    """Space the board may occupy above the supports: board outline + GAP, widening by
    BR_CHAMFER at the top (even lead-in chamfer on all walls, also around the corners)."""
    cx, cy = (X(0) + X(PCB_L)) / 2, (YB + YF) / 2
    w, h, r = PCB_L + 2 * GAP, PCB_W + 2 * GAP, PCB_R + GAP
    z1 = BR_TOP - BR_CHAMFER
    body = cq.Workplane().add(cq.Solid.extrudeLinear(
        cq.Face.makeFromWires(rrect_wire(cx, cy, w, h, r, ZC - 0.01)), cq.Vector(0, 0, z1 - ZC + 0.01)))
    c = BR_CHAMFER + 0.02
    top = cq.Solid.makeLoft([rrect_wire(cx, cy, w, h, r, z1),
                             rrect_wire(cx, cy, w + 2 * c, h + 2 * c, r + c, BR_TOP + 0.02)])
    above = cq.Solid.extrudeLinear(cq.Face.makeFromWires(
        rrect_wire(cx, cy, w + 2 * c, h + 2 * c, r + c, BR_TOP + 0.01)), cq.Vector(0, 0, 2))
    return body.union(cq.Workplane().add(top)).union(cq.Workplane().add(above))


def corner_piece(hx, hy, z0, pocket):
    """Support + L wall for the board corner next to hole (hx, hy), one solid."""
    front = hx > PCB_W / 2
    low = hy < PCB_L / 2
    ex, sx = (PCB_W, 1) if front else (0.0, -1)      # long edge (px) and outward direction
    ey, sy = (0.0, -1) if low else (PCB_L, 1)        # short edge (py) and outward direction
    o = GAP + BR_T
    px = sorted((ex + sx * o, ex - sx * CORNER_IN))
    py = sorted((ey + sy * o, ey - sy * CORNER_IN))
    xs = sorted((X(py[0]), X(py[1])))
    ys = sorted((Y(px[0]), Y(px[1])))
    c_out = (X(ey + sy * o), Y(ex + sx * o), 0)         # outer corner (rounded R_OUT)
    c_in = (X(ey - sy * CORNER_IN), Y(ex - sx * CORNER_IN), 0)   # inner corner of the support
    sup = box(xs[0], xs[1], ys[0], ys[1], z0, ZC)
    sup = sup.edges("|Z").edges(cq.selectors.NearestToPointSelector(c_out)).fillet(R_OUT)
    sup = sup.edges("|Z").edges(cq.selectors.NearestToPointSelector(c_in)).fillet(R_SUP)
    wall = box(xs[0], xs[1], ys[0], ys[1], ZC - 0.01, BR_TOP)
    wall = wall.edges("|Z").edges(cq.selectors.NearestToPointSelector(c_out)).fillet(R_OUT)
    wall = wall.cut(pocket)
    return sup.union(wall)


def board_holder():
    z0 = FLOOR - 0.5
    part = None

    def add(p):
        nonlocal part
        part = p if part is None else part.union(p)
    pocket = board_pocket()
    for (hx, hy) in HOLES:
        add(corner_piece(hx, hy, z0, pocket))
    # battery guides: legs along the long sides at each corner, tabs in the middle of the
    # short sides; the corners stay open so the cable can leave towards the side
    if SLIM:
        return part                                  # no battery, no guides
    bx0, bx1 = BAT_X0 - BAT_CLR, BAT_X0 + BAT[0] + BAT_CLR
    by0, by1 = BAT_Y0 - BAT_CLR, BAT_Y0 + BAT[1] + BAT_CLR
    zg = FLOOR + GUIDE_H
    for ya in ((by0 - GUIDE_T, by0), (by1, by1 + GUIDE_T)):
        add(box(bx0, bx0 + GUIDE_LEG, *ya, z0, zg))
        add(box(bx1 - GUIDE_LEG, bx1, *ya, z0, zg))
    ym = (by0 + by1) / 2
    for xa in ((bx0 - GUIDE_T, bx0), (bx1, bx1 + GUIDE_T)):
        add(box(*xa, ym - GUIDE_TAB / 2, ym + GUIDE_TAB / 2, z0, zg))
    return part


# ------------------------------------------------------------------ snap (bead on the housing, tongue on the lid)
def rear_rib():
    """Outer half of the groove behind the lid's back wall, on a floor strip (table up to FLOOR).
    Gaps (above the floor) at the finger-nail notch and at the USB plug."""
    yo = Y_BACK - REAR_GAP
    yt = yo - REAR_RIB_T
    zt = FLOOR + REAR_RIB_H
    prof = [(yt, 0.0), (Y_BACK + 0.5, 0.0), (Y_BACK + 0.5, FLOOR), (yo, FLOOR), (yo - REAR_RIB_LEAN, zt),
            (yt + 0.4, zt), (yt, zt - 0.4)]
    x0, x1 = WEDGE_X[0] + RC, WEDGE_X[1] - RC
    rib = prism_yz(prof, x0, x1)
    for xc, w in ([(NOTCH_X, NOTCH_W)] if NOTCH else []) + [(USB_X, USB_OPEN[0])]:
        hw = w / 2 + REAR_RIB_GAP / 2
        rib = rib.cut(box(xc - hw, xc + hw, yt - 1, Y_BACK, FLOOR, zt + 1))
    return rib


def snap_parts():
    beads, tongues, relief = None, None, None
    for side in (-1, 1):
        xw = WEDGE_X[0] + WALL if side < 0 else WEDGE_X[1] - WALL      # inner wall face
        s = 1 if side < 0 else -1                                      # direction into the cavity
        xb = xw + s * (SNAP_BEAD - BEAD_R)                            # bead axis
        for a, b in TONGUE_SEG:
            ya, yb = Y_BACK + a, Y_BACK + b
            L = yb - ya
            zl = T(ya) - LID - BEAD_DOWN
            ax = cq.Vector(0, 1, SL).normalized()
            Lb = (L + 2.0) * math.hypot(1, SL)
            bead = cq.Workplane().add(cq.Solid.makeCylinder(BEAD_R, Lb, cq.Vector(xb, ya - 1.0, zl - SL), ax))
            keep = box(min(xw, xw + s * 2), max(xw, xw + s * 2), ya - 2, yb + 2, 0, 40)
            bead = bead.intersect(keep)
            beads = bead if beads is None else beads.union(bead)
            t0 = xw + s * SNAP_CLR
            t1 = t0 + s * TONGUE_T
            ztop = T(ya) - LID
            fi, fo = TONGUE_FIL_IN, TONGUE_FIL_OUT
            arc_in = [(t1 + s * (fi - fi * math.cos(a)), ztop - fi + fi * math.sin(a))
                      for a in [math.radians(v) for v in range(0, 91, 15)]]
            arc_out = [(t0 - s * (fo - fo * math.cos(a)), ztop - fo + fo * math.sin(a))
                       for a in [math.radians(v) for v in range(90, -1, -15)]]
            prof = ([(t0 - s * fo, ztop + 0.3)] + arc_out[1:] +
                    [(t0, ztop - TONGUE_L + 0.8), (t0 + s * 0.8, ztop - TONGUE_L), (t1, ztop - TONGUE_L)] +
                    arc_in + [(t1 + s * fi, ztop + 0.3)])
            tg = prism_xz_along(prof, ya, slope_vec(L))
            for dz in [i * GROOVE_EXTRA / 4 for i in range(5)]:   # groove, a little longer downwards
                gr = cq.Workplane().add(cq.Solid.makeCylinder(BEAD_R + 0.1, Lb, cq.Vector(xb, ya - 1.0, zl - SL - dz), ax))
                tg = tg.cut(gr)
            tongues = tg if tongues is None else tongues.union(tg)
            # 45 deg relief on the wall's inner top edge for the outer root fillet
            w = WALL_RELIEF
            rl = prism_xz_along([(xw + s * 0.2, ztop - w - 0.2), (xw + s * 0.2, ztop + 0.2),
                                 (xw - s * (w + 0.0), ztop + 0.2)], ya - 1.0, slope_vec(L + 2.0))
            relief = rl if relief is None else relief.union(rl)
    return beads, tongues, relief


def front_snap_parts():
    """Two snap tongues at the lid's front edge (left and right of the FPC channel) and their
    counterparts: a block on the cavity's front wall below the top strip with a bead along X.
    The tongue's front face is the lid's front face; it bends backwards when it passes the bead."""
    yl = Y_F - STRIP - GAP_F            # lid front face
    yw = yl + SNAP_CLR                  # block rear face (wall for the bead)
    yt = yl - TONGUE_T                  # tongue rear face
    ztop = T(yt) - LID                  # lid underside at the tongue's rear face (lowest)
    zl = ztop - BEAD_DOWN
    yb = yw + (BEAD_R - SNAP_BEAD)      # bead axis
    blocks, tongues = None, None
    for side in (-1, 1):
        x0, x1 = side * FSNAP_X - FSNAP_W / 2, side * FSNAP_X + FSNAP_W / 2
        fi = TONGUE_FIL_IN
        arc = [(yt - fi + fi * math.cos(a), ztop - fi + fi * math.sin(a))
               for a in [math.radians(v) for v in range(0, 91, 15)]]
        prof = ([(yl, ztop + 0.6), (yl, ztop - TONGUE_L + 0.8), (yl - 0.8, ztop - TONGUE_L),
                 (yt, ztop - TONGUE_L)] + [(yt - (p[0] - yt) * 0 + (p[0] - yt), p[1]) for p in arc] +
                [(yt - fi, ztop + 0.6)])
        tg = prism_yz(prof, x0, x1)
        for dz in [i * GROOVE_EXTRA / 4 for i in range(5)]:
            tg = tg.cut(cyl(BEAD_R + 0.1, FSNAP_W + 2, (x0 - 1, yb, zl - dz), (1, 0, 0)))
        tongues = tg if tongues is None else tongues.union(tg)
        # block: from the wall back to yw, 45 deg underside (prints without support), up into the
        # gusset under the strip
        zb = zl - 2.0
        zs = T(yw) - LID + 0.2          # up into the top strip (never above its top face)
        blk = prism_yz([(yw, zb), (Y_FR + 0.5, zb - (Y_FR + 0.5 - yw)), (Y_FR + 0.5, zs), (yw, zs)],
                       x0 - 1.0, x1 + 1.0)
        blk = blk.cut(box(x0 - 2, x1 + 2, yl - 3, yw, 0, 60))     # keep the lid's space free
        blk = blk.union(cyl(BEAD_R, FSNAP_W + 2.0, (x0 - 1.0, yb, zl), (1, 0, 0)))
        blocks = blk if blocks is None else blocks.union(blk)
    return blocks, tongues


# ------------------------------------------------------------------ logo
def logo():
    """Logo mark raised on the front face below the display, centred (2026-11-16, replaces the
    2 x 2 grid): a band across the top and two columns of equal width below it, the lower left one
    filled. Outline 225 x 200 as before; the line under the band at 45.6 % of the height from the
    top (centre), the column divider in the middle. Strokes thicker than the screen logo for
    printing."""
    W, w, h = LOGO_W, LOGO_STROKE, LOGO_H
    H = W * 200 / 225
    x0, x1, y0, y1 = -W / 2, W / 2, -H / 2, H / 2
    xv, yh = x0 + 0.5 * W, y1 - 0.456 * H
    rects = [(x0, x1, y1 - w, y1), (x0, x1, y0, y0 + w), (x0, x0 + w, y0, y1), (x1 - w, x1, y0, y1),
             (xv - w / 2, xv + w / 2, y0, yh), (x0, x1, yh - w / 2, yh + w / 2), (x0, xv, y0, yh)]
    up = (-ST, CT)                                   # up the front face (table Y, Z)
    n = (CT, ST)                                     # outward normal
    y0t, z0t = u2t(FRONT_U, 0.0)
    base = (y0t - up[0] * z0t / CT, 0.0)             # front face at the table
    out = None
    for a, b, c, d in rects:
        # (s, t): s up the face, t along the normal; lower edge bevelled
        prof = [(c, -0.3), (d, -0.3), (d, h), (c + LOGO_BEVEL, h), (c, 0.0)]
        pts = [(base[0] + up[0] * (LOGO_S + sv) + n[0] * tv, base[1] + up[1] * (LOGO_S + sv) + n[1] * tv)
               for sv, tv in prof]
        # seen from the front (looking towards -Y) the viewer's right is -X: mirror x
        p = prism_yz(pts, -b, -a)
        out = p if out is None else out.union(p)
    return out


# ------------------------------------------------------------------ housing
def housing():
    body = frame().union(wedge())
    body = body.cut(cavity())
    body = body.cut(lid_region(FLOOR, 0.0, 0.0))
    # top strip at the back plate (part of the housing), carried by a 45 deg gusset down to the
    # cavity's front wall (full width, no notches). The lid's front edge only butts against the
    # strip; vertically the lid is held by the press posts on the board and the snap tongues.
    ys = Y_F - STRIP
    p1 = (ys, T(ys) - LID)
    t = Y_FR + 0.5 - p1[0]
    p3 = (p1[0] + t, p1[1] - t)
    strip = prism_yz([p1, (ys, T(ys)), (Y_F + 1.0, T(Y_F + 1.0)), (Y_FR + 1.0, p3[1]), p3],
                     WEDGE_X[0] + WALL - 0.2, WEDGE_X[1] - WALL + 0.2)
    body = body.union(strip)
    body = body.cut(fpc_cut())             # after the ledge: keeps the channel exit free
    # sill on the floor just inside the lid's back wall (locates the wall, keeps it from bending in)
    ys = Y_BACK + WALL + SNAP_CLR
    sill = prism_yz([(ys, FLOOR - 0.5), (ys + SILL_T, FLOOR - 0.5), (ys + SILL_T, FLOOR + SILL_H),
                     (ys + 0.4, FLOOR + SILL_H), (ys, FLOOR + SILL_H - 0.4)],
                    WEDGE_X[0] + RC, WEDGE_X[1] - RC)
    body = body.union(sill)
    body = body.union(rear_rib())
    body = body.union(board_holder())
    beads, _, wrelief = snap_parts()
    body = body.union(beads)
    body = body.cut(wrelief)
    fblocks, _ = front_snap_parts()
    body = body.union(fblocks)
    # finger-nail notch under the back wall
    if NOTCH:
        body = body.cut(box(NOTCH_X - NOTCH_W / 2, NOTCH_X + NOTCH_W / 2, Y_BACK - 1, Y_BACK + WALL,
                            FLOOR - NOTCH_D, FLOOR + 1))
    body = body.cut(text_top("InkTavla", 6.0, 45.0, 28.5, FLOOR, 0.6))
    body = body.cut(text_top(MARK, 6.0, 45.0, 21.0, FLOOR, 0.6))
    body = body.union(logo())
    return body


# ------------------------------------------------------------------ lid
def lid():
    lw = wedge().intersect(lid_region(FLOOR + GAP_B, GAP_R, GAP_F)).cut(cavity())
    _, tongues, _ = snap_parts()
    lw = lw.union(tongues)
    _, ftongues = front_snap_parts()
    lw = lw.union(ftongues)
    lw = lw.union(press_posts())
    for x in BTN_X:
        lw = lw.cut(cyl((SHAFT_D + HOLE_CLR) / 2, WALL + 2, (x, Y_BACK - 1, BTN_Z), (0, 1, 0)))
    uw, uh, ur = USB_OPEN
    u = box(USB_X - uw / 2, USB_X + uw / 2, Y_BACK - 1, Y_BACK + WALL + 1, USB_Z - uh / 2, USB_Z + uh / 2).edges("|Y").fillet(ur)
    if SLIM:
        # the opening would end 0.35 above the lid's bottom edge: open it downwards instead
        u = u.union(box(USB_X - uw / 2, USB_X + uw / 2, Y_BACK - 1, Y_BACK + WALL + 1, -1, USB_Z))
    lw = lw.cut(u)
    for x, t in zip(BTN_X, BTN_LABELS):
        lw = lw.cut(text_back(t, LABEL_SIZE, x, LABEL_Z, Y_BACK, LABEL_D))
    return lw


def press_points():
    """Above the mounting holes; moved towards the board end where the post would touch the
    solder joints of the battery switch."""
    pts = []
    r = PRESS_D / 2
    for (hx, hy) in HOLES:
        x, y = X(hy), Y(hx)
        if hx < SOLDER_PX and SOLDER_PY[0] < hy + r and hy - r < SOLDER_PY[1] + 0.05:
            x -= (SOLDER_PY[1] + 0.05) - (hy - r)
        pts.append((x, y))
    return pts


def press_post(x, y):
    zb = ZC + PCB_T + PRESS_GAP
    top = T(y + PRESS_D / 2) - LID + 0.3
    p = cyl(PRESS_D / 2, top - zb, (x, y, zb), (0, 0, 1))
    p = p.faces("<Z").edges().chamfer(0.3)
    zr = T(y - PRESS_ROOT_D / 2) - LID          # lowest point of the lid underside over the root
    cone = cq.Workplane().add(cq.Solid.makeCone(PRESS_D / 2, PRESS_ROOT_D / 2, PRESS_ROOT_H + 1.0,
                                                cq.Vector(x, y, zr - PRESS_ROOT_H), cq.Vector(0, 0, 1)))
    return p.union(cone)


def press_posts():
    out = None
    for (x, y) in press_points():
        t = press_post(x, y)
        out = t if out is None else out.union(t)
    return out


# ------------------------------------------------------------------ plungers
def plunger_len():
    gap = (YB - BTN_OVER) - (Y_BACK + WALL)
    return gap - 0.3, WALL + SHAFT_OUT


def plunger_local():
    lc, ls = plunger_len()
    p = cyl(COLLAR_D / 2, lc, (0, 0, 0), (0, 0, 1)).union(cyl(SHAFT_D / 2, ls + 0.01, (0, 0, lc - 0.01), (0, 0, 1)))
    return p.faces(">Z").edges().chamfer(0.3).faces("<Z").edges().chamfer(0.3)


def plungers_assembly():
    lc, ls = plunger_len()
    out = None
    for x in BTN_X:
        p = cyl(COLLAR_D / 2, lc, (x, Y_BACK + WALL, BTN_Z), (0, 1, 0))
        p = p.union(cyl(SHAFT_D / 2, ls, (x, Y_BACK + WALL, BTN_Z), (0, -1, 0)))
        out = p if out is None else out.union(p)
    return out


# ------------------------------------------------------------------ ghosts (checks and previews)
def board_ghost():
    def comp(px0, px1, py0, py1, h, base=ZC):
        return pcb_rect(px0, px1, py0, py1, base - h, base)
    b = rbox_xy(X(PCB_L), X(0), YB, YF, ZC, ZC + PCB_T, PCB_R)
    for (hx, hy) in HOLES:
        b = b.cut(cyl(1.25, 5, (X(hy), Y(hx), ZC - 1), (0, 0, 1)))
    parts = [comp(0.1, 21.05, 5.48, 23.28, 1.2),
             comp(-USB_OVER, 6.05, USB_PY - 4.47, USB_PY + 4.47, 3.2, ZC - 1.2),
             comp(36.29, 41.19, 22.34, 37.84, 2.0), comp(35.58, 40.48, 40.71, 69.31, 2.0),
             comp(24.31, 32.21, 2.93, 10.53, 4.5),
             comp(1.42, 4.92, 65.16, 74.16, 3.5),
             comp(-1.78, 1.42, 69.06, 70.56, 1.5, ZC - 0.5)]        # battery switch lever
    for yc in BTN_PY:
        parts.append(comp(1.25, 4.05, yc - 2.25, yc + 2.2, 3.5))
        parts.append(comp(-BTN_OVER, 1.25, yc - 1.15, yc + 1.15, 2.5, ZC - 0.5))
    comps = parts[0]
    for p in parts[1:]:
        comps = comps.union(p)
    # solder joints on the top face (keep-out reported in the test print)
    solder = pcb_rect(0.6, 6.5, SOLDER_PY[0] + 2.0, SOLDER_PY[1] - 1.3, ZC + PCB_T, ZC + PCB_T + 1.0)
    bat = box(BAT_X0, BAT_X0 + BAT[0], BAT_Y0, BAT_Y0 + BAT[1], FLOOR, FLOOR + BAT[2])
    return b, comps, solder, bat


def panel_ghost():
    w, h, t = PANEL
    return ubox(-w / 2, w / 2, 0.0, t, 0.0, h)


def _dubins(p0, h0, p1, h1, r):
    """Shortest forward path (min bend radius r) from p0 heading h0 to p1 heading h1."""
    m = lambda a: a % (2 * math.pi)
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    d = math.hypot(dx, dy) / r
    th = m(math.atan2(dy, dx)); a = m(h0 - th); b = m(h1 - th)
    sa, sb, ca, cb, cab = math.sin(a), math.sin(b), math.cos(a), math.cos(b), math.cos(a - b)
    res = []
    q = 2 + d * d - 2 * cab + 2 * d * (sa - sb)
    if q >= 0:
        t = math.atan2(cb - ca, d + sa - sb); res.append(m(-a + t) + math.sqrt(q) + m(b - t))
    q = 2 + d * d - 2 * cab + 2 * d * (sb - sa)
    if q >= 0:
        t = math.atan2(ca - cb, d - sa + sb); res.append(m(a - t) + math.sqrt(q) + m(-b + t))
    q = -2 + d * d + 2 * cab + 2 * d * (sa + sb)
    if q >= 0:
        pp = math.sqrt(q); t = math.atan2(-ca - cb, d + sa + sb) - math.atan2(-2, pp)
        res.append(m(-a + t) + pp + m(-m(b) + t))
    q = d * d - 2 + 2 * cab - 2 * d * (sa + sb)
    if q >= 0:
        pp = math.sqrt(q); t = math.atan2(ca + cb, d - sa - sb) - math.atan2(2, pp)
        res.append(m(a - t) + pp + m(m(b) - t))
    return min(res) * r


def fpc_path():
    """FPC check. Returns (guide points, guide length, shortest plugged length, free length).
    guide: centre line of the FPC tip during insertion (start at the panel edge, along the
    cove 0.5 inside the outer wall, up the ramp and on to the connector front).
    plugged: shortest possible run from the panel edge (heading down along the plate) into
    the connector (heading back), min bend radius 1.5."""
    start = u2t(0.65, 0.0)
    cove = fpc_cove()
    pts = [start] + [(p[0], p[1] + 0.5) for p in cove[:-1]]
    pts.append((YF + 0.5, CH_FLOOR + 0.5))
    guide = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts[:-1], pts[1:]))
    plugged = _dubins(start, math.atan2(-CT, ST), (YF, CONN_Z), math.pi, 1.5)
    return pts, guide, plugged, FPC_LEN - FPC_IN_CONN


# ------------------------------------------------------------------ export
# Header of every STL: the project, the license and where it comes from, nothing else (no paths or
# user names; CI checks this with tools/png_strip.py --check).
STL_HEADER = "InkTavla case - CC BY-NC-SA 4.0 - github.com/Fluddel94/InkTavla"


def to_bed(wp, rot=None):
    if rot:
        for axis, ang in rot:
            wp = wp.rotate((0, 0, 0), axis, ang)
    vals = wp.vals()
    bb = vals[0].BoundingBox() if len(vals) == 1 else cq.Compound.makeCompound(vals).BoundingBox()
    return wp.translate((-(bb.xmin + bb.xmax) / 2, -(bb.ymin + bb.ymax) / 2, -bb.zmin))


def set_stl_header(path):
    """Binary STL: header text padded with NUL to 80 bytes; ASCII STL: the name after solid/endsolid."""
    data = open(path, "rb").read()
    if data[:5] == b"solid" and b"facet" in data[:400]:
        lines = data.split(b"\n")
        lines = [b"solid " + STL_HEADER.encode() if ln.startswith(b"solid") else
                 b"endsolid " + STL_HEADER.encode() if ln.startswith(b"endsolid") else ln for ln in lines]
        data = b"\n".join(lines)
    else:
        data = STL_HEADER.encode("ascii").ljust(80, b"\0") + data[80:]
    open(path, "wb").write(data)


def stl(wp, path, tol=0.02):
    cq.exporters.export(wp, path, tolerance=tol, angularTolerance=0.1)
    # the fillet corners of the frame tessellate into a few sliver triangles that do not share
    # vertices exactly: weld them so the STL is one closed body
    import trimesh
    m = trimesh.load(path)
    if not m.is_watertight:
        m.merge_vertices(digits_vertex=2)
        m.update_faces(m.nondegenerate_faces())
        m.update_faces(m.unique_faces())
        m.remove_unreferenced_vertices()
        trimesh.repair.fill_holes(m)
        trimesh.repair.fix_normals(m)
        assert m.is_watertight, path
        m.export(path)
    set_stl_header(path)


def preview(scene, path, az=20, el=20):
    """View of the assembled case with the panel (az 20: from the front left, 200: from behind)."""
    import trimesh
    from render_simple import render
    from PIL import Image
    parts = [(trimesh.load(os.path.join(scene, n + ".stl")), [int(c[i:i + 2], 16) / 255 for i in (0, 2, 4)])
             for n, c in (("case", "d9d4c7"), ("lid", "d9d4c7"), ("plungers", "8a8a8a"), ("panel", "3c3c3c"))]
    im = render(parts, az, el)
    im.resize((im.width // 2, im.height // 2), Image.LANCZOS).save(path, optimize=True)


def main(out, scene):
    import tempfile
    os.makedirs(out, exist_ok=True)
    tmp = None
    if scene is None:
        tmp = tempfile.TemporaryDirectory()
        scene = tmp.name
    os.makedirs(scene, exist_ok=True)
    hs = housing(); ld = lid(); pa = plungers_assembly()
    pcb, comps, solder, bat = board_ghost()
    for name, wp in [("case", hs), ("lid", ld), ("press_posts", press_posts()), ("plungers", pa), ("board", pcb),
                     ("components", comps), ("solder", solder), ("battery", bat), ("panel", panel_ghost())]:
        if SLIM and name == "battery":
            continue
        stl(wp, os.path.join(scene, name + ".stl"), 0.05)
    stl(to_bed(hs), os.path.join(out, NAME + "-case.stl"))
    ang = math.degrees(math.atan(SL))
    stl(to_bed(ld, [((1, 0, 0), -ang), ((0, 1, 0), 180)]), os.path.join(out, NAME + "-lid.stl"))
    pl = plunger_local()
    six = cq.Workplane()
    for i in range(6):
        six = six.add(pl.translate(((i % 3) * 8.0, (i // 3) * 8.0, 0)).vals())
    stl(to_bed(six), os.path.join(out, NAME + "-plungers-6x.stl"))
    preview(scene, os.path.join(out, NAME + ".png"))
    preview(scene, os.path.join(out, NAME + "-back.png"), 200, 20)

    lc, ls_ = plunger_len()
    _, fpc_guide, fpc_plugged, fpc_free = fpc_path()
    _, zb, zc, zt = arch_profile()
    info = dict(P0=[round(v, 2) for v in P0], YF=round(YF, 2), YB=round(YB, 2), Y_BACK=round(Y_BACK, 2), ZC=ZC,
                H=round(H, 2), Z_TOP=Z_TOP, Y_F=round(Y_F, 2), Y_FR=round(Y_FR, 2), slope_deg=round(ang, 2),
                Y_C=round(Y_C, 2), Z_C=round(Z_C, 2), BTN_Z=round(BTN_Z, 2), USB_Z=round(USB_Z, 2),
                BTN_X=[round(v, 2) for v in BTN_X], USB_X=round(USB_X, 2), collar=round(lc, 2), shaft=round(ls_, 2),
                press=[(round(x, 2), round(y, 2)) for x, y in press_points()], fpc_free=fpc_free, fpc_plugged_min=round(fpc_plugged, 1), fpc_slack=round(fpc_free - fpc_plugged, 1),
                fpc_guide_to_conn=round(fpc_guide, 1), ch_floor=round(CH_FLOOR, 2), defl_r=round(DEFL_R, 2), ch_ceil=round(CH_CEIL, 2),
                arch_zb=round(zb, 1), arch_zc=round(zc, 1), top_front=[round(v, 2) for v in u2t(FRONT_U, FR_TOP)],
                foot_front=round(line_y(FRONT_U, 0.0), 2))
    json.dump(info, open(os.path.join(scene, "info.json"), "w"), indent=1)
    print(json.dumps(info))
    if tmp:
        tmp.cleanup()


if __name__ == "__main__":
    args = [a for a in sys.argv[2:]]
    scene_dir = None
    if "--scene" in args:
        n = args.index("--scene")
        scene_dir = args[n + 1]
        del args[n:n + 2]
    here = os.path.dirname(os.path.abspath(__file__))
    main(args[0] if args else os.path.join(here, "..", VARIANT), scene_dir)

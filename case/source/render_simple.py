"""Simple shaded preview without a browser: orthographic z-buffer render of STL files (numpy).
Usage: python3 render_simple.py out.png az el file.stl[:#color] [file2.stl[:#color] ...]
az/el in degrees: az 0 = from the front (+Y), 180 = from behind; el = camera height angle."""
import sys, math, numpy as np, trimesh
from PIL import Image


def render(parts, az, el, px=1400, pad=30):
    a, e = math.radians(az), math.radians(el)
    d = np.array([math.sin(a) * math.cos(e), math.cos(a) * math.cos(e), math.sin(e)])   # towards camera
    r = np.cross([0, 0, 1], d); r /= np.linalg.norm(r); u = np.cross(d, r)
    L = d * 0.6 + u * 0.6 + r * 0.5; L /= np.linalg.norm(L)
    tris = []
    for m, col in parts:
        v = m.vertices
        P = np.stack([v @ r, v @ u, v @ d], 1)[m.faces]
        sh = np.clip(m.face_normals @ L, 0, 1) * 0.65 + 0.35
        tris.append((P, sh[:, None] * np.array(col)[None, :], m.face_normals @ d > 0))
    allp = np.concatenate([t[0].reshape(-1, 3) for t in tris])
    lo, hi = allp[:, :2].min(0), allp[:, :2].max(0)
    s = (px - 2 * pad) / (hi[0] - lo[0]); H = int((hi[1] - lo[1]) * s) + 2 * pad
    img = np.full((H, px, 3), 255.0); zb = np.full((H, px), -1e9)
    for P, C, front in tris:
        for t, c, f in zip(P, C, front):
            if not f:
                continue
            x = (t[:, 0] - lo[0]) * s + pad; y = H - ((t[:, 1] - lo[1]) * s + pad); z = t[:, 2]
            x0, x1 = int(max(x.min(), 0)), int(min(x.max() + 1, px - 1)); y0, y1 = int(max(y.min(), 0)), int(min(y.max() + 1, H - 1))
            if x1 < x0 or y1 < y0:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            den = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
            if abs(den) < 1e-9:
                continue
            w0 = ((y[1] - y[2]) * (gx - x[2]) + (x[2] - x[1]) * (gy - y[2])) / den
            w1 = ((y[2] - y[0]) * (gx - x[2]) + (x[0] - x[2]) * (gy - y[2])) / den
            w2 = 1 - w0 - w1
            zz = w0 * z[0] + w1 * z[1] + w2 * z[2]
            sub = zb[y0:y1 + 1, x0:x1 + 1]
            m = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6) & (zz > sub)
            sub[m] = zz[m]; img[y0:y1 + 1, x0:x1 + 1][m] = c * 255
    return Image.fromarray(img.astype(np.uint8))


if __name__ == '__main__':
    out, az, el = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
    parts = []
    for a in sys.argv[4:]:
        f, _, c = a.partition(':#')
        c = c or 'd9d4c7'
        parts.append((trimesh.load(f), [int(c[i:i + 2], 16) / 255 for i in (0, 2, 4)]))
    im = render(parts, az, el)
    im.resize((im.width // 2, im.height // 2), Image.LANCZOS).save(out)

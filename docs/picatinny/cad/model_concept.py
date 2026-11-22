# -*- coding: utf-8 -*-
"""Nerf 用皮卡汀尼快拆夹 —— 概念模型 + 渲染（纯 Python，无需 CAD 内核）。

坐标系（mm）：X 沿导轨，Y 横跨导轨，Z 向上，z=0 为导轨底面。

输出：
    model_concept.png   等轴测视图 + 剖面视图
    model_concept.stl   装配体网格（仅供查看，未开螺栓孔）
"""
import math
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- 导轨剖面
W_TOP, W_MAX, W_BASE, H = 19.00, 21.20, 15.67, 9.32
Z_CROWN = H - (W_MAX - W_TOP) / 2          # 8.22
Z_DOVE = Z_CROWN - (W_MAX - W_BASE) / 2    # 5.455

RAIL = [(-W_TOP / 2, H), (W_TOP / 2, H), (W_MAX / 2, Z_CROWN), (W_BASE / 2, Z_DOVE),
        (W_BASE / 2, 0.0), (-W_BASE / 2, 0.0), (-W_BASE / 2, Z_DOVE), (-W_MAX / 2, Z_CROWN)]


def offset_poly(pts, d):
    n = len(pts)
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    cw = area < 0
    lines = []
    for i in range(n):
        (x1, y1), (x2, y2) = pts[i], pts[(i + 1) % n]
        dx, dy = x2 - x1, y2 - y1
        L = math.hypot(dx, dy)
        nx, ny = ((-dy / L, dx / L) if cw else (dy / L, -dx / L))
        lines.append(((x1 + nx * d, y1 + ny * d), (dx, dy)))
    out = []
    for i in range(n):
        p1, d1 = lines[i - 1]
        p2, d2 = lines[i]
        den = d1[0] * d2[1] - d1[1] * d2[0]
        if abs(den) < 1e-9:
            out.append(p2)
            continue
        t = ((p2[0] - p1[0]) * d2[1] - (p2[1] - p1[1]) * d2[0]) / den
        out.append((p1[0] + d1[0] * t, p1[1] + d1[1] * t))
    return out


CAV = offset_poly(RAIL, 0.35)   # 0 顶 1 右上角 2 右脊 3 右燕尾底 4 右下 5 左下 6 左燕尾底 7 左脊
crownL, doveL = CAV[7], CAV[6]
crownR, doveR = CAV[2], CAV[3]


# ---------------------------------------------------------------- 网格构造
def tri_poly(pts2d, a, b, yz=True):
    """把凸多边形（可非凸则先扇形）沿 X 从 a 到 b 拉伸，返回三角形列表。"""
    return extrude(pts2d, a, b, yz=True)


def _earclip(poly):
    """耳切三角化（简单多边形，逆时针/顺时针均可）。"""
    pts = list(poly)
    n = len(pts)
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    if area < 0:
        pts.reverse()
    idx = list(range(len(pts)))
    tris = []
    guard = 0
    while len(idx) > 3 and guard < 10000:
        guard += 1
        ear = False
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = pts[i0], pts[i1], pts[i2]
            cross = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
            if cross <= 1e-12:
                continue
            ok = True
            for j in idx:
                if j in (i0, i1, i2):
                    continue
                p = pts[j]
                d1 = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
                d2 = (c[0] - b[0]) * (p[1] - b[1]) - (c[1] - b[1]) * (p[0] - b[0])
                d3 = (a[0] - c[0]) * (p[1] - c[1]) - (a[1] - c[1]) * (p[0] - c[0])
                if d1 >= 0 and d2 >= 0 and d3 >= 0:
                    ok = False
                    break
            if ok:
                tris.append((pts[i0], pts[i1], pts[i2]))
                idx.pop(k)
                ear = True
                break
        if not ear:
            break
    if len(idx) == 3:
        tris.append((pts[idx[0]], pts[idx[1]], pts[idx[2]]))
    return tris


def extrude(poly, a, b):
    """多边形在 YZ 平面，沿 X 拉伸 a..b，返回 (n,3,3) 三角形数组。"""
    tris2d = _earclip(poly)
    out = []
    for (p, q, r) in tris2d:
        out.append([(a, p[0], p[1]), (a, q[0], q[1]), (a, r[0], r[1])])
        out.append([(b, r[0], r[1]), (b, q[0], q[1]), (b, p[0], p[1])])
    n = len(poly)
    for i in range(n):
        (y1, z1), (y2, z2) = poly[i], poly[(i + 1) % n]
        out.append([(a, y1, z1), (a, y2, z2), (b, y2, z2)])
        out.append([(a, y1, z1), (b, y2, z2), (b, y1, z1)])
    return np.array(out, dtype=float)


def cylinder(cx, cz, cy0, cy1, r, seg=40):
    """沿 Y 轴的圆柱。"""
    out = []
    ring = [(cx + r * math.cos(2 * math.pi * i / seg), cz + r * math.sin(2 * math.pi * i / seg))
            for i in range(seg)]
    for i in range(seg):
        x1, z1 = ring[i]
        x2, z2 = ring[(i + 1) % seg]
        out.append([(x1, cy0, z1), (x2, cy0, z2), (x2, cy1, z2)])
        out.append([(x1, cy0, z1), (x2, cy1, z2), (x1, cy1, z1)])
    for i in range(1, seg - 1):
        out.append([(cx, cy0, cz), (ring[i][0], cy0, ring[i][1]), (ring[i + 1][0], cy0, ring[i + 1][1])])
        out.append([(cx, cy1, cz), (ring[i + 1][0], cy1, ring[i + 1][1]), (ring[i][0], cy1, ring[i][1])])
    return np.array(out, dtype=float)


def box(x0, x1, y0, y1, z0, z1):
    p = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
         (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    f = [(0, 3, 2), (0, 2, 1), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
         (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return np.array([[p[i] for i in t] for t in f], dtype=float)


# ---------------------------------------------------------------- 零件
L0, L1 = -13.0, 13.0

# 主体 A：左侧固定夹 + 顶部横梁
A_poly = [(-15.5, 17), (9.6, 17), (9.6, 11.2), (crownL[0], 11.2), crownL, doveL,
          (doveL[0], 4), (-15.5, 4)]
A = extrude(A_poly, L0, L1)

# 夹条 B：右侧活动夹
B_poly = [(crownR[0], 17), (15.5, 17), (15.5, 5), (doveR[0], 5), doveR, crownR]
B = extrude(B_poly, L0, L1)

# 导轨：带槽，X 方向 -32..32
SLOT_D = 3.00
Z_SLOT = H - SLOT_D                      # 6.32
half_at_slot = W_BASE / 2 + (Z_SLOT - Z_DOVE)   # 该高度处的半宽
RAIL_CLIP = [(-half_at_slot, Z_SLOT), (half_at_slot, Z_SLOT),
             (W_BASE / 2, Z_DOVE), (W_BASE / 2, 0.0),
             (-W_BASE / 2, 0.0), (-W_BASE / 2, Z_DOVE)]

slots = [(-14.0, -8.77), (-3.76, 1.47), (6.25, 11.48)]
rail_tris = []
seg_start = -24.0
for (s0, s1) in slots:
    rail_tris.append(extrude(RAIL, seg_start, s0))     # 槽间实心段
    rail_tris.append(extrude(RAIL_CLIP, s0, s1))       # 槽底
    seg_start = s1
rail_tris.append(extrude(RAIL, seg_start, 24.0))
RAIL_MESH = np.concatenate(rail_tris, axis=0)

# 螺栓 + 凸轮扳手（全部按 Y 轴）
bolt = cylinder(0, 13.5, -17.5, 19.5, 2.5, 36)
washer = cylinder(0, 13.5, -17.5, -16.0, 5.2, 36)
cam = cylinder(0, 13.5, 16.0, 19.0, 4.6, 36)
lever = box(-2.2, 2.2, 16.4, 18.6, 17.2, 27.0)  # 简化扳手把
HARDWARE = np.concatenate([bolt, washer, cam, lever], axis=0)

PARTS = [
    ("rail", RAIL_MESH, (128, 138, 148)),
    ("body", A, (74, 144, 226)),
    ("bar", B, (245, 158, 66)),
    ("hw", HARDWARE, (200, 205, 212)),
]


# ---------------------------------------------------------------- 渲染
def render(parts, W=1500, Hh=1000, scale=17.5, title="", subtitle=""):
    img = Image.new("RGB", (W, Hh), (250, 250, 251))
    zbuf = np.full((Hh, W), 1e18, dtype=np.float64)  # 固定二维缓冲
    buf = np.zeros((Hh, W, 3), dtype=np.uint8) + 250

    eye = np.array([1.15, 1.35, 0.72])
    eye = eye / np.linalg.norm(eye)
    up = np.array([0.0, 0.0, 1.0])
    right = np.cross(up, eye)
    right /= np.linalg.norm(right)
    up2 = np.cross(eye, right)

    center = np.array([0.0, 0.0, 10.0])
    light = np.array([-0.45, -0.75, 0.55])
    light /= np.linalg.norm(light)

    for name, tris, color in parts:
        if tris is None or len(tris) == 0:
            continue
        v = tris - center
        sx = v.dot(right)
        sy = v.dot(up2)
        depth = v.dot(eye)
        px = (W / 2 + sx * scale).astype(int)
        py = (Hh / 2 - sy * scale).astype(int)
        n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
        nl = np.linalg.norm(n, axis=1)
        nl[nl == 0] = 1
        n = n / nl[:, None]
        shade = np.abs(n.dot(light)) * 0.62 + 0.34
        for t in range(len(tris)):
            x = px[t]
            y = py[t]
            d = depth[t]
            x0, x1 = x.min(), x.max()
            y0, y1 = y.min(), y.max()
            if x1 < 0 or y1 < 0 or x0 >= W or y0 >= Hh:
                continue
            x0, x1 = max(0, x0), min(W - 1, x1)
            y0, y1 = max(0, y0), min(Hh - 1, y1)
            if x1 < x0 or y1 < y0:
                continue
            ax, ay = x[0], y[0]
            bx, by = x[1], y[1]
            cx, cy = x[2], y[2]
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-12:
                continue
            gy, gx = np.mgrid[y0:y1 + 1, x0:x1 + 1]
            w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
            w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
            w2 = 1 - w0 - w1
            mask = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
            if not mask.any():
                continue
            zz = w0 * d[0] + w1 * d[1] + w2 * d[2]
            sub = zbuf[y0:y1 + 1, x0:x1 + 1]
            upd = mask & (zz < sub)
            if not upd.any():
                continue
            sub[upd] = zz[upd]
            c = np.array(color) * shade[t]
            buf[y0:y1 + 1, x0:x1 + 1][upd] = np.clip(c, 0, 255).astype(np.uint8)

    img = Image.fromarray(buf)
    d = ImageDraw.Draw(img)
    try:
        f1 = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 30)
        f2 = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 20)
    except Exception:
        f1 = f2 = ImageFont.load_default()
    d.text((36, 28), title, font=f1, fill=(30, 40, 50))
    d.text((36, 70), subtitle, font=f2, fill=(90, 100, 110))
    return img


sub = ("灰=导轨 1913 | 蓝=主体A（固定夹） | 橙=夹条B（活动夹） | 银=横栓/凸轮扳手  |  单位 mm")
img = render(PARTS, title="Nerf 皮卡汀尼快拆夹 · 概念模型", subtitle=sub)
img.save(os.path.join(OUT, "model_concept.png"))
print("render ok")

# STL（装配体，仅供查看）
def to_stl(path, parts):
    tri = np.concatenate([p[1] for p in parts if p[1] is not None], axis=0)
    with open(path, "w", encoding="ascii") as f:
        f.write("solid concept\n")
        for t in tri:
            n = np.cross(t[1] - t[0], t[2] - t[0])
            l = np.linalg.norm(n)
            n = n / l if l else n
            f.write("facet normal %.4f %.4f %.4f\n outer loop\n" % tuple(n))
            for v in t:
                f.write("  vertex %.4f %.4f %.4f\n" % tuple(v))
            f.write(" endloop\nendfacet\n")
        f.write("endsolid concept\n")


to_stl(os.path.join(OUT, "model_concept.stl"), PARTS)
print("stl ok")

# -*- coding: utf-8 -*-
"""渲染瞄具壳体：前视 / 后视 / 俯视 三个等轴测。"""
import json
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))
S = np.array(json.load(open(os.path.join(OUT, "userbody_tess.json")))["B"], dtype=float)
COL = (140, 160, 190)


def render(tris, eye, W, H, scale, center):
    buf = np.zeros((H, W, 3), dtype=np.uint8) + 252
    zbuf = np.full((H, W), 1e18)
    eye = np.array(eye, float)
    eye /= np.linalg.norm(eye)
    up = np.array([0.0, 0.0, 1.0])
    right = np.cross(up, eye)
    right /= np.linalg.norm(right)
    up2 = np.cross(eye, right)
    light = np.array([-0.4, -0.7, 0.6])
    light /= np.linalg.norm(light)
    center = np.array(center, float)
    v = tris - center
    sx = v.dot(right) * scale + W / 2
    sy = -v.dot(up2) * scale + H / 2
    depth = v.dot(eye)
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    nl = np.linalg.norm(n, axis=1)
    nl[nl == 0] = 1
    n = n / nl[:, None]
    shade = np.abs(n.dot(light)) * 0.66 + 0.30
    for t in range(len(tris)):
        x, y, d = sx[t].astype(int), sy[t].astype(int), depth[t]
        x0, x1 = x.min(), x.max()
        y0, y1 = y.min(), y.max()
        if x1 < 0 or y1 < 0 or x0 >= W or y0 >= H:
            continue
        x0, x1 = max(0, x0), min(W - 1, x1)
        y0, y1 = max(0, y0), min(H - 1, y1)
        if x1 < x0 or y1 < y0:
            continue
        ax, ay, bx, by, cx, cy = x[0], y[0], x[1], y[1], x[2], y[2]
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
        c = np.clip(np.array(COL) * shade[t], 0, 255).astype(np.uint8)
        buf[y0:y1 + 1, x0:x1 + 1][upd] = c
    return Image.fromarray(buf)


PW, PH, SC = 520, 560, 4.2
panels = [
    ((1.0, 1.1, -0.7), (0, 0, 45), "-X 端（前）"),
    ((-1.0, 1.1, -0.7), (0, 0, 45), "+X 端（后）"),
    ((-0.5, 0.9, -1.0), (0, 0, 45), "俯视"),
    ((1.0, -0.9, 0.5), (0, 0, 45), "仰视"),
]
W, H = PW * 4 + 50, PH + 100
canvas = Image.new("RGB", (W, H), "white")
d = ImageDraw.Draw(canvas)
f1 = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 30)
f2 = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 18)
d.text((18, 12), "用户 clamp_body_a_1.STEP · bbox 154×45×80（mm）", font=f1, fill=(30, 40, 50))
for i, (eye, center, cap) in enumerate(panels):
    im = render(S, eye, PW, PH, SC, center)
    x = 10 + i * (PW + 10)
    canvas.paste(im, (x, 54))
    d.rectangle((x, 54, x + PW - 1, 54 + PH - 1), outline=(225, 228, 232))
    d.text((x + 8, 60 + PH), cap, font=f2, fill=(70, 80, 90))
canvas.save(os.path.join(OUT, "userbody_preview.png"))
print("saved")

# -*- coding: utf-8 -*-
"""渲染：改好的本体（前/后）+ 顶盖。"""
import json
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))
B = np.array(json.load(open(os.path.join(OUT, "body2_tess.json")))["B"], dtype=float)
L = np.array(json.load(open(os.path.join(OUT, "lid_tess.json")))["L"], dtype=float)


def render(tris, eye, W, H, scale, center, col):
    buf = np.zeros((H, W, 3), dtype=np.uint8) + 252
    zbuf = np.full((H, W), 1e18)
    eye = np.array(eye, float) / np.linalg.norm(eye)
    up = np.array([0.0, 0.0, 1.0])
    right = np.cross(up, eye)
    right /= np.linalg.norm(right)
    up2 = np.cross(eye, right)
    light = np.array([-0.4, -0.7, 0.6])
    light /= np.linalg.norm(light)
    v = tris - np.array(center, float)
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
        ax, ay, bx_, by_ = x[0], y[0], x[1], y[1]
        cx, cy = x[2], y[2]
        den = (by_ - cy) * (ax - cx) + (cx - bx_) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        gy, gx = np.mgrid[y0:y1 + 1, x0:x1 + 1]
        w0 = ((by_ - cy) * (gx - cx) + (cx - bx_) * (gy - cy)) / den
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
        c = np.clip(np.array(col) * shade[t], 0, 255).astype(np.uint8)
        buf[y0:y1 + 1, x0:x1 + 1][upd] = c
    return Image.fromarray(buf)


PW, PH = 620, 700
panels = [
    (B, (1.0, 0.85, -0.5), 3.6, (0, 0, 44), (135, 155, 185), "本体：-X 端（前）两个窗口"),
    (B, (-1.0, 0.85, -0.5), 3.6, (0, 0, 44), (135, 155, 185), "本体：+X 端（屏幕座）"),
    (L, (0.55, 0.95, -1.0), 6.0, (0, 0, 86), (150, 165, 190), "顶盖：五键 + 4×M2"),
]
W, H = PW * 3 + 40, PH + 100
canvas = Image.new("RGB", (W, H), "white")
d = ImageDraw.Draw(canvas)
f1 = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 28)
f2 = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 18)
d.text((18, 12), "本体开孔 + 顶盖（按 clamp_body_a_1.STEP 实测坐标，单位 mm）", font=f1, fill=(30, 40, 50))
for i, (tris, eye, sc, ce, col, cap) in enumerate(panels):
    im = render(tris, eye, PW, PH, sc, ce, col)
    x = 10 + i * (PW + 10)
    canvas.paste(im, (x, 54))
    d.rectangle((x, 54, x + PW - 1, 54 + PH - 1), outline=(228, 231, 235))
    d.text((x + 8, 60 + PH), cap, font=f2, fill=(70, 80, 90))
canvas.save(os.path.join(OUT, "body2_lid_preview.png"))
print("saved")

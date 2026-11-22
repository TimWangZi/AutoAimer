# -*- coding: utf-8 -*-
"""渲染 FreeCAD 建好的夹具：A 单体 / B 单体 / 装配。"""
import json
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))
data = json.load(open(os.path.join(OUT, "tess.json")))
A = np.array(data["A"], dtype=float)
B = np.array(data["B"], dtype=float)
CA, CB = (86, 148, 216), (243, 166, 90)


def render(parts, eye, W, H, scale, center):
    buf = np.zeros((H, W, 3), dtype=np.uint8) + 252
    zbuf = np.full((H, W), 1e18)
    eye = np.array(eye, dtype=float)
    eye /= np.linalg.norm(eye)
    up = np.array([0.0, 0.0, 1.0])
    right = np.cross(up, eye)
    if np.linalg.norm(right) < 1e-6:
        right = np.array([1.0, 0.0, 0.0])
    right /= np.linalg.norm(right)
    up2 = np.cross(eye, right)
    light = np.array([-0.5, -0.7, 0.55])
    light /= np.linalg.norm(light)
    center = np.array(center, dtype=float)

    for _, tris, color in parts:
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
            x = sx[t].astype(int)
            y = sy[t].astype(int)
            d = depth[t]
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
            c = np.clip(np.array(color) * shade[t], 0, 255).astype(np.uint8)
            buf[y0:y1 + 1, x0:x1 + 1][upd] = c
    return Image.fromarray(buf)


PW, PH, SC = 600, 640, 10.5
panels = [
    ([("A", A, CA)], (0.9, 1.15, -0.95), (0, 0, 10.5), "A 顶视：顶面平整（安装面）+ 侧面 2×⌀5 横栓孔"),
    ([("A", A, CA)], (0.75, -1.25, 0.85), (0, 0, 10.5), "A 底视：45° 燕尾腔贯通"),
    ([("A", A, CA), ("B", B, CB)], (-0.95, -1.15, -0.9), (0, 0, 10.5), "装配：B 与 A 用 2 颗 M5 夹紧，孔距 20.02"),
]

W, H = PW * 3 + 40, PH + 110
canvas = Image.new("RGB", (W, H), "white")
d = ImageDraw.Draw(canvas)
try:
    f1 = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 30)
    f2 = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 18)
except Exception:
    f1 = f2 = ImageFont.load_default()

d.text((18, 14), "皮卡汀尼快拆夹 · FreeCAD 实体（单位 mm）", font=f1, fill=(30, 40, 50))
for i, (parts, eye, center, cap) in enumerate(panels):
    im = render(parts, eye, PW, PH, SC, center)
    x = 10 + i * (PW + 10)
    canvas.paste(im, (x, 56))
    d.rectangle((x, 56, x + PW - 1, 56 + PH - 1), outline=(225, 228, 232))
    d.text((x + 8, 62 + PH), cap, font=f2, fill=(70, 80, 90))

d.text((18, H - 28), "A：固定夹 + 顶部安装面；B：活动夹条，45° 唇边压在燕尾面上；⌀5 横栓贯穿两者",
       font=f2, fill=(90, 100, 110))
canvas.save(os.path.join(OUT, "clamp_preview.png"))
print("saved", canvas.size)

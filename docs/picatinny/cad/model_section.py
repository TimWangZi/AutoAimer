# -*- coding: utf-8 -*-
"""剖面示意：导轨 + 夹具体 A + 夹条 B + 横栓/凸轮（X=0 处剖切）。"""
import math
import os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))
W_TOP, W_MAX, W_BASE, H = 19.00, 21.20, 15.67, 9.32
Z_CROWN = H - (W_MAX - W_TOP) / 2
Z_DOVE = Z_CROWN - (W_MAX - W_BASE) / 2
RAIL = [(-W_TOP / 2, H), (W_TOP / 2, H), (W_MAX / 2, Z_CROWN), (W_BASE / 2, Z_DOVE),
        (W_BASE / 2, 0.0), (-W_BASE / 2, 0.0), (-W_BASE / 2, Z_DOVE), (-W_MAX / 2, Z_CROWN)]

# 与 model_concept.py 一致的夹具体轮廓（YZ）
crownL = (-W_MAX / 2 - 0.35 * 1.24, Z_CROWN - 0.1)
doveL = (-W_BASE / 2 - 0.35, Z_DOVE - 0.05)
crownR = (W_MAX / 2 + 0.35 * 1.24, Z_CROWN - 0.1)
doveR = (W_BASE / 2 + 0.35, Z_DOVE - 0.05)

A = [(-15.5, 17), (9.6, 17), (9.6, 11.2), (crownL[0], 11.2), crownL, doveL, (doveL[0], 4), (-15.5, 4)]
B = [(crownR[0], 17), (15.5, 17), (15.5, 5), (doveR[0], 5), doveR, crownR]

S = 30.0
OX, OY = 560, 640
def P(y, z):
    return (OX + y * S, OY - z * S)

Wpx, Hpx = 1400, 780
img = Image.new("RGB", (Wpx, Hpx), "white")
d = ImageDraw.Draw(img)
try:
    f = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 26)
    fs = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 20)
except Exception:
    f = fs = ImageFont.load_default()

def poly_fill(pts, fill, outline=(40, 50, 60), w=3):
    p = [P(*q) for q in pts]
    d.polygon(p, fill=fill)
    d.line(p + [p[0]], fill=outline, width=w, joint="curve")

poly_fill(RAIL, (176, 186, 196))
poly_fill(A, (86, 148, 216))
poly_fill(B, (243, 166, 90))

# 横栓（沿 Y）
bolt = [(y, 13.5 - 2.5) for y in (-17.5, 19.5)] + [(y, 13.5 + 2.5) for y in (19.5, -17.5)]
poly_fill(bolt, (206, 212, 218), (120, 130, 140), 2)
# 垫片 / 凸轮
for (y0, y1, r) in [(-17.5, -16.0, 5.2), (16.0, 19.0, 4.6)]:
    disc = [(y0, 13.5 - r), (y1, 13.5 - r), (y1, 13.5 + r), (y0, 13.5 + r)]
    poly_fill(disc, (206, 212, 218), (120, 130, 140), 2)
# 扳手把
poly_fill([(16.4, 17.2), (18.6, 17.2), (18.6, 27.0), (16.4, 27.0)], (206, 212, 218), (120, 130, 140), 2)

# 标注
def note(xy, text, color=(60, 70, 80), font=fs, anchor="lm"):
    d.text(xy, text, font=font, fill=color, anchor=anchor)

d.text((36, 26), "X=0 剖面：夹紧机理", font=f, fill=(30, 40, 50))
note((36, 66), "蓝 = 夹具体 A（固定夹）  橙 = 夹条 B（活动夹）  灰 = 导轨 1913  银 = M5 横栓 + 凸轮扳手")

note((OX + 9.6 * S + 18, OY - 15.0 * S), "A 顶部横梁", (30, 90, 160))
note((OX + 11.0 * S + 18, OY - 12.0 * S), "B 沿 Y 被横栓拉向 A", (190, 110, 40))
note((OX + 8.0 * S, OY - 1.5 * S), "B 的 45° 唇边压在燕尾面上", (190, 110, 40))





note((36, 700), "导轨：顶面 19.00 / 脊线 21.20（顶面下 1.10）/ 燕尾 45°×2.76 / 底面 15.67", (60, 70, 80), fs)
note((36, 732), "M5 横栓穿过 A 的横梁与 B，扳合凸轮把 B 沿 Y 拉向 A，B 的 45° 唇边沿燕尾下滑 → 向下压紧", (60, 70, 80), fs)
img.save(os.path.join(OUT, "model_section.png"))
print("section ok")

# -*- coding: utf-8 -*-
"""AutoAimer 软蛋瞄具 总布置草图（侧视 + 前视 + 俯视）。

X 沿枪管（+X 朝前），Y 横跨，Z 向上；Z=0 取导轨顶面。单位 mm。
尺寸是布置假想值，标注出来方便按实物改。
"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))

RAIL_H = 9.32
CLAMP_L, CLAMP_W = 50.0, 25.1
CLAMP_Z0, CLAMP_Z1 = -5.32, 7.68

BODY_L, BODY_W = 100.0, 42.0
BODY_Z0, BODY_Z1 = 7.68, 57.7

SCR_D, SCR_T, SCR_ZC = 40.0, 4.0, 33.0     # 屏幕模块 40x40x4
CAM_W, CAM_T, CAM_ZC = 24.0, 4.5, 44.0
LENS_D = 10.0
RNG_W, RNG_H, RNG_ZC = 16.0, 8.0, 18.0

PCB_L, PCB_H, PCB_Z0 = 55.0, 12.0, 10.0
BAT_L, BAT_H = 40.0, 6.0

BTN_X = [-42.0, -33.0, -24.0, -15.0, -6.0]

C_BODY = (236, 240, 244)
C_EDGE = (38, 50, 56)
C_CLAMP = (200, 214, 226)
C_SCR = (70, 90, 110)
C_CAM = (60, 70, 80)
C_PCB = (110, 165, 125)
C_BAT = (200, 160, 90)
C_GRAY = (120, 144, 156)
C_DIM = (21, 101, 192)

W, H = 1500, 1150
img = Image.new("RGB", (W, H), "white")
d = ImageDraw.Draw(img)
try:
    f_title = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 32)
    f_lbl = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 19)
    f_dim = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 17)
    f_sm = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 15)
except Exception:
    f_title = f_lbl = f_dim = f_sm = ImageFont.load_default()


def rect(p1, p2, col, outline=C_EDGE, w=2):
    x0, x1 = sorted((p1[0], p2[0]))
    y0, y1 = sorted((p1[1], p2[1]))
    if col is None:
        d.rectangle([x0, y0, x1, y1], outline=outline, width=w)
    else:
        d.rectangle([x0, y0, x1, y1], fill=col, outline=outline, width=w)


def ell(c, r, col, outline=C_EDGE, w=2):
    cx, cy = c
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col, outline=outline, width=w)


def label(xy, s, font=f_lbl, fill=C_EDGE, anchor="lm", bg=True):
    bb = d.textbbox(xy, s, font=font, anchor=anchor)
    if bg:
        d.rectangle((bb[0] - 3, bb[1] - 2, bb[2] + 3, bb[3] + 2), fill="white")
    d.text(xy, s, font=font, fill=fill, anchor=anchor)


def dim_h(x1, x2, y, txt, col=C_DIM):
    d.line([(x1, y), (x2, y)], fill=col, width=1)
    for xx in (x1, x2):
        d.line([(xx, y - 5), (xx, y + 5)], fill=col, width=1)
    label(((x1 + x2) / 2, y - 11), txt, f_dim, col, "ms", False)


def dim_v(x, y1, y2, txt, col=C_DIM):
    d.line([(x, y1), (x, y2)], fill=col, width=1)
    for yy in (y1, y2):
        d.line([(x - 5, yy), (x + 5, yy)], fill=col, width=1)
    label((x + 10, (y1 + y2) / 2), txt, f_dim, col, "lm", False)


S = 5.5
OY = 520

# ================================================================== 侧视
OX = 380
def PS(x, z):
    return (OX + x * S, OY - z * S)


d.text((50, 26), "AutoAimer 软蛋瞄具 · 总布置草图", font=f_title, fill=(30, 40, 50))
d.text((50, 68), "侧视（+X 朝前，Z=0 为导轨顶面）· 单位 mm · 尺寸为布置假想值",
       font=f_sm, fill=(90, 100, 110))

rect(PS(-62, 0), PS(62, -RAIL_H), (170, 180, 190))
label(PS(-60, -4.8), "导轨", f_sm, (70, 80, 90), "lm")
rect(PS(-CLAMP_L / 2, CLAMP_Z1), PS(CLAMP_L / 2, CLAMP_Z0), C_CLAMP)
label((PS(-24, 0)[0], PS(0, 2.2)[1]), "夹具体 50", f_sm, (40, 60, 80), "mm")

rect(PS(-BODY_L / 2, BODY_Z1), PS(BODY_L / 2, BODY_Z0), C_BODY)
# 屏幕模块（后端，贴着后面）
rect(PS(-BODY_L / 2, SCR_ZC + SCR_D / 2), PS(-BODY_L / 2 + SCR_T, SCR_ZC - SCR_D / 2), C_SCR)
# 摄像头模块（前端）
rect(PS(BODY_L / 2, CAM_ZC + CAM_W / 2), PS(BODY_L / 2 + CAM_T, CAM_ZC - CAM_W / 2), C_CAM)
rect(PS(BODY_L / 2 + CAM_T, CAM_ZC + LENS_D / 2), PS(BODY_L / 2 + CAM_T + 2.5, CAM_ZC - LENS_D / 2), (25, 32, 40))
# 测距窗口
rect(PS(BODY_L / 2, RNG_ZC + RNG_H / 2), PS(BODY_L / 2 + 1.6, RNG_ZC - RNG_H / 2), (95, 200, 200))
# PCB 仓、电池
pcb = PS(-PCB_L / 2, PCB_Z0 + PCB_H)
rect(pcb, PS(PCB_L / 2, PCB_Z0), None, C_PCB, 2)
label((pcb[0] + 8, pcb[1] + 11), "PCB 仓 55×25×12", f_sm, (60, 120, 70), "lm", False)
bat = PS(-BODY_L / 2 + 5, PCB_Z0 + PCB_H + BAT_H)
rect(bat, PS(-BODY_L / 2 + 5 + BAT_L, PCB_Z0 + PCB_H), None, C_BAT, 2)
label((bat[0] + 8, bat[1] + 8), "电池 40×28×6", f_sm, (170, 120, 40), "lm", False)
# 按键
for bx in BTN_X:
    cx, cy = PS(bx, BODY_Z1)
    d.rectangle([cx - 9, cy - 6, cx + 9, cy], fill=(90, 100, 110))
label(PS(0, BODY_Z1 + 2), "五键", f_sm, (70, 80, 90), "lm")

label(PS(-BODY_L / 2 - 3, SCR_ZC), "屏", f_sm, "white", "rm", False)
label(PS(BODY_L / 2 + CAM_T + 8, CAM_ZC), "SC3336", f_sm, C_CAM, "lm")
label(PS(BODY_L / 2 + 6, RNG_ZC), "SDM20", f_sm, (20, 140, 140), "lm")

dim_h(PS(-BODY_L / 2, 0)[0], PS(BODY_L / 2, 0)[0], PS(0, BODY_Z1)[1] - 22, "壳体长 100")
dim_v(PS(-BODY_L / 2 - 18, 0)[0], PS(0, BODY_Z1)[1], PS(0, 0)[1], "总高 57.7")
dim_h(PS(-CLAMP_L / 2, 0)[0], PS(CLAMP_L / 2, 0)[0], PS(0, CLAMP_Z0)[1] + 26, "50")

# ================================================================== 前视
FX = 900
def PF(y, z):
    return (FX + y * S, OY - z * S)


d.text((FX - 120, 120), "前视（从枪口方向看）", font=f_lbl, fill=(30, 40, 50))
rect(PF(-BODY_W / 2, BODY_Z1), PF(BODY_W / 2, BODY_Z0), C_BODY)
ell(PF(0, CAM_ZC), LENS_D / 2 * S, (25, 32, 40))
ell(PF(0, CAM_ZC), LENS_D / 2 * S + 6, None, C_EDGE, 1)
rect(PF(-RNG_W / 2, RNG_ZC + RNG_H / 2), PF(RNG_W / 2, RNG_ZC - RNG_H / 2), (95, 200, 200))
for bx in BTN_X:
    cx = PF(bx, BODY_Z1)[0]
    d.rectangle([cx - 5, PF(0, BODY_Z1)[1] - 5, cx + 5, PF(0, BODY_Z1)[1]], fill=(90, 100, 110))
label(PF(9, CAM_ZC), "SC3336 镜头", f_sm, C_CAM, "lm")
label(PF(0, RNG_ZC - 10), "SDM20 测距窗", f_sm, (20, 140, 140), "mm")
dim_h(PF(-BODY_W / 2, 0)[0], PF(BODY_W / 2, 0)[0], PF(0, BODY_Z1)[1] - 14, "42")
dim_v(PF(BODY_W / 2 + 6, 0)[0], PF(0, BODY_Z1)[1], PF(0, BODY_Z0)[1], "50")

# ================================================================== 俯视
TX, TY = 380, 800
def PT(x, y):
    return (TX + x * S, TY + y * S)


d.text((50, 600), "俯视（+X 朝右为前）", font=f_lbl, fill=(30, 40, 50))
rect(PT(-BODY_L / 2, -BODY_W / 2), PT(BODY_L / 2, BODY_W / 2), C_BODY)
rect(PT(-CLAMP_L / 2, -CLAMP_W / 2), PT(CLAMP_L / 2, CLAMP_W / 2), None, C_GRAY, 2)
d.line([PT(-BODY_L / 2, -SCR_D / 2), PT(-BODY_L / 2, SCR_D / 2)], fill=C_SCR, width=6)
label(PT(-BODY_L / 2 + 6, -SCR_D / 2 - 6), "屏（后端）", f_sm, C_SCR, "lm")
for bx in BTN_X:
    cx, cy = PT(bx, 0)
    d.ellipse([cx - 7, cy - 7, cx + 7, cy + 7], fill=(90, 100, 110))
label(PT(4, 0), "五键", f_sm, (70, 80, 90), "lm")
rect(PT(16, -BODY_W / 2), PT(31, -BODY_W / 2 + 6), (60, 70, 80))
label(PT(34, -BODY_W / 2 + 3), "USB-C", f_sm, (60, 70, 80), "lm")
label(PT(-BODY_L / 2, -CLAMP_W / 2 - 9), "灰色框=下方夹具体 50×25.1", f_sm, (90, 110, 130), "lm")
dim_h(PT(-BODY_L / 2, 0)[0], PT(BODY_L / 2, 0)[0], PT(0, BODY_W / 2)[1] + 30, "100")

# ================================================================== 说明
notes = [
    "壳体 100×42×50，坐在 50 长的夹具体上；总高 57.7（从导轨顶面算），连导轨约 67。",
    "前端 SC3336（φ10 镜头）+ SDM20 测距窗；后端 GC9A01 圆屏（φ32.4 可视）；顶部五键；左侧 USB-C。",
    "PCB 仓 55×25×12 放 RV1103 Pico，电池 40×28×6 叠在上方；IMU 贴 PCB 前端，远离电池和 SPI 排线。",
]
for i, s in enumerate(notes):
    d.text((50, 1050 + i * 26), s, font=f_sm, fill=(70, 80, 90))

img.save(os.path.join(OUT, "sight_layout.png"))
print("saved")

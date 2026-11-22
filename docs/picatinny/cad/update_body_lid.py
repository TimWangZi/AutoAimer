# -*- coding: utf-8 -*-
"""在用户的 clamp_body_a_1.STEP 上增量开孔，并生成配套顶盖（独立部件）。

本体：导入 STEP → 布尔切两个窗口 → 导出 clamp_body_a_2
顶盖：按本体顶部开口 110×35、四角孔 (±63.5, ±19.8) 建模
"""
import json
import os

import FreeCAD as App
import Part

OUT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(OUT, "clamp_body_a_1.STEP")

# ------------------------------------------------------------------ 开孔参数（实测本体的坐标）
CAM_Y, CAM_Z, CAM_D = 0.0, 56.0, 10.5     # 镜头孔：镜筒轴线
RNG_Y, RNG_Z, RNG_W, RNG_H = 0.0, 46.0, 16.0, 8.0   # SDM20 窗口
CUT_X0, CUT_X1 = -82.0, -54.0             # 从筒口一直切穿前壁

# 顶盖
LID_X, LID_Y = 66.8, 22.5                 # 与本体外轮廓齐平
LID_T = 4.0
OPEN_L, OPEN_W = 110.0, 35.0
SPIGOT_CL = 0.15
SPIGOT_T = 3.0
SCREW_POS = [(63.5, 19.8), (63.5, -19.8), (-63.5, 19.8), (-63.5, -19.8)]
SCREW_D = 2.2
BTN_CX, BTN_OFF, BTN_R, BTN_OK_R = 38.0, 10.0, 3.1, 3.25
BTNS = [(BTN_CX, BTN_OFF, BTN_R), (BTN_CX, -BTN_OFF, BTN_R),
        (BTN_CX - BTN_OFF, 0.0, BTN_R), (BTN_CX + BTN_OFF, 0.0, BTN_R),
        (BTN_CX, 0.0, BTN_OK_R)]

# ------------------------------------------------------------------ 本体
doc = App.newDocument("Body")
src = doc.addObject("Part::Feature", "clamp_body_a_1")
src.Shape = Part.read(SRC)
doc.recompute()
print("imported vol %.1f  valid %s" % (src.Shape.Volume, src.Shape.isValid()))


def cyl_x(name, r, y, z):
    c = doc.addObject("Part::Cylinder", name)
    c.Radius, c.Height = r, CUT_X1 - CUT_X0
    c.Placement = App.Placement(App.Vector(CUT_X0, y, z),
                                App.Rotation(App.Vector(0, 0, 1), App.Vector(1, 0, 0)))
    c.Label = name
    return c


def box_x(name, y0, y1, z0, z1):
    b = doc.addObject("Part::Box", name)
    b.Length, b.Width, b.Height = CUT_X1 - CUT_X0, y1 - y0, z1 - z0
    b.Placement.Base = App.Vector(CUT_X0, y0, z0)
    b.Label = name
    return b


cut1 = doc.addObject("Part::Cut", "body_cam_cut")
cut1.Base, cut1.Tool = src, cyl_x("cam_hole", CAM_D / 2, CAM_Y, CAM_Z)
doc.recompute()
cut2 = doc.addObject("Part::Cut", "body_rng_cut")
cut2.Base = cut1
cut2.Tool = box_x("rng_hole", RNG_Y - RNG_W / 2, RNG_Y + RNG_W / 2, RNG_Z - RNG_H / 2, RNG_Z + RNG_H / 2)
doc.recompute()

body = cut2
print("body vol %.1f  valid %s  solids %d" % (body.Shape.Volume, body.Shape.isValid(), len(body.Shape.Solids)))
doc.saveAs(os.path.join(OUT, "clamp_body_a_2.FCStd"))
Part.export([body], os.path.join(OUT, "clamp_body_a_2.step"))
body.Shape.exportStl(os.path.join(OUT, "clamp_body_a_2.stl"))
v, f = body.Shape.tessellate(0.25)
json.dump({"B": [[[round(v[i].x, 3), round(v[i].y, 3), round(v[i].z, 3)] for i in fc] for fc in f]},
          open(os.path.join(OUT, "body2_tess.json"), "w"))

# ------------------------------------------------------------------ 顶盖
doc2 = App.newDocument("Lid")


def bx(name, x0, x1, y0, y1, z0, z1):
    b = doc2.addObject("Part::Box", name)
    b.Length, b.Width, b.Height = x1 - x0, y1 - y0, z1 - z0
    b.Placement.Base = App.Vector(x0, y0, z0)
    b.Label = name
    return b


def cz(name, r, x, y, z0, z1):
    c = doc2.addObject("Part::Cylinder", name)
    c.Radius, c.Height = r, z1 - z0
    c.Placement.Base = App.Vector(x, y, z0)
    c.Label = name
    return c


def cut(name, base, tool):
    o = doc2.addObject("Part::Cut", name)
    o.Base, o.Tool = base, tool
    o.Label = name
    return o


def fuse(name, a, b):
    o = doc2.addObject("Part::Fuse", name)
    o.Base, o.Tool = a, b
    o.Label = name
    return o


Z0 = 84.0                                  # 本体顶面
plate = bx("01 plate", -LID_X, LID_X, -LID_Y, LID_Y, Z0, Z0 + LID_T)
spigot = bx("02 spigot", -OPEN_L / 2 + SPIGOT_CL, OPEN_L / 2 - SPIGOT_CL,
            -OPEN_W / 2 + SPIGOT_CL, OPEN_W / 2 - SPIGOT_CL, Z0 - SPIGOT_T, Z0)
lid = fuse("03 lid", plate, spigot)
doc2.recompute()

tools = []
for i, (sx, sy) in enumerate(SCREW_POS):
    tools.append(cz("04 screw %d" % i, SCREW_D / 2, sx, sy, Z0 - SPIGOT_T - 1, Z0 + LID_T + 1))
for i, (bx_, by_, br_) in enumerate(BTNS):
    tools.append(cz("05 btn %d" % i, br_, bx_, by_, Z0 - SPIGOT_T - 1, Z0 + LID_T + 1))
tool = tools[0]
for i, t in enumerate(tools[1:]):
    tool = fuse("06 fuse %d" % i, tool, t)
lid = cut("07 holes", lid, tool)
doc2.recompute()

print("lid vol %.1f  valid %s" % (lid.Shape.Volume, lid.Shape.isValid()))
doc2.saveAs(os.path.join(OUT, "lid.FCStd"))
Part.export([lid], os.path.join(OUT, "lid.step"))
lid.Shape.exportStl(os.path.join(OUT, "lid.stl"))
v, f = lid.Shape.tessellate(0.15)
json.dump({"L": [[[round(v[i].x, 3), round(v[i].y, 3), round(v[i].z, 3)] for i in fc] for fc in f]},
          open(os.path.join(OUT, "lid_tess.json"), "w"))
print("done")

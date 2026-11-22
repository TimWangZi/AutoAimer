# -*- coding: utf-8 -*-
"""瞄具顶盖（独立部件）：100×42×5 法兰 + 定位止口 + 五键孔 + 4×M2.5 沉孔。

坐标：顶盖自身，法兰底面 z=0，+X 朝前（摄像头在 -X，屏幕在 +X，按键在 +X 端）。
运行： FreeCADCmd.exe build_lid.py
输出： lid.FCStd / lid.step / lid.stl / lid_tess.json
"""
import json
import os

import FreeCAD as App
import Part

OUT = os.path.dirname(os.path.abspath(__file__))

L, W = 100.0, 42.0
T = 5.0                     # 法兰厚
TONGUE_T = 3.0              # 止口高
TONGUE_CL = 0.15            # 止口单边间隙（配合本体 95.2×37.2 开口）
BODY_OPEN_L, BODY_OPEN_W = 95.2, 37.2

BTN_X, BTN_Y, BTN_R = 32.0, 9.0, 3.1
BTN_C_R = 3.25
SCREW_X, SCREW_Y = 40.0, 16.5
SCREW_D, CB_D, CB_DEPTH = 2.8, 5.2, 2.0

doc = App.newDocument("SightLid")


def box(name, x0, x1, y0, y1, z0, z1):
    b = doc.addObject("Part::Box", name)
    b.Length, b.Width, b.Height = x1 - x0, y1 - y0, z1 - z0
    b.Placement.Base = App.Vector(x0, y0, z0)
    b.Label = name
    return b


def cyl(name, r, x, y, z0, z1):
    c = doc.addObject("Part::Cylinder", name)
    c.Radius, c.Height = r, z1 - z0
    c.Placement.Base = App.Vector(x, y, z0)
    c.Label = name
    return c


def cut(name, base, tool):
    o = doc.addObject("Part::Cut", name)
    o.Base, o.Tool = base, tool
    o.Label = name
    return o


def fuse(name, a, b):
    o = doc.addObject("Part::Fuse", name)
    o.Base, o.Tool = a, b
    o.Label = name
    return o


part = box("01 flange", -L / 2, L / 2, -W / 2, W / 2, 0, T)
tongue = box("02 tongue", -BODY_OPEN_L / 2 + TONGUE_CL, BODY_OPEN_L / 2 - TONGUE_CL,
             -BODY_OPEN_W / 2 + TONGUE_CL, BODY_OPEN_W / 2 - TONGUE_CL, -TONGUE_T, 0)
part = fuse("03 lid body", part, tongue)
doc.recompute()

# 五键孔
holes = [cyl("04 btn up", BTN_R, BTN_X, BTN_Y, -TONGUE_T - 1, T + 1),
         cyl("04 btn down", BTN_R, BTN_X, -BTN_Y, -TONGUE_T - 1, T + 1),
         cyl("04 btn left", BTN_R, BTN_X - BTN_Y, 0, -TONGUE_T - 1, T + 1),
         cyl("04 btn right", BTN_R, BTN_X + BTN_Y, 0, -TONGUE_T - 1, T + 1),
         cyl("04 btn ok", BTN_C_R, BTN_X, 0, -TONGUE_T - 1, T + 1)]
tool = holes[0]
for i, h in enumerate(holes[1:]):
    tool = fuse("05 btn fuse %d" % i, tool, h)
part = cut("05 buttons", part, tool)
doc.recompute()

# 4 个 M2.5 沉头
screws = []
for i, (sx, sy) in enumerate([(SCREW_X, SCREW_Y), (SCREW_X, -SCREW_Y),
                              (-SCREW_X, SCREW_Y), (-SCREW_X, -SCREW_Y)]):
    screws.append(cyl("06 screw %d" % i, SCREW_D / 2, sx, sy, -TONGUE_T - 1, T + 1))
    screws.append(cyl("06 cbore %d" % i, CB_D / 2, sx, sy, T - CB_DEPTH, T + 1))
tool = screws[0]
for i, s in enumerate(screws[1:]):
    tool = fuse("07 screw fuse %d" % i, tool, s)
part = cut("07 screw holes", part, tool)
doc.recompute()

s = part.Shape
print("lid volume %.1f mm3" % s.Volume)
print("bbox", s.BoundBox)
print("solids", len(s.Solids), "valid", s.isValid())

doc.saveAs(os.path.join(OUT, "lid.FCStd"))
Part.export([part], os.path.join(OUT, "lid.step"))
s.exportStl(os.path.join(OUT, "lid.stl"))

verts, facets = s.tessellate(0.15)
tris = [[[round(verts[i].x, 3), round(verts[i].y, 3), round(verts[i].z, 3)] for i in f] for f in facets]
json.dump({"L": tris}, open(os.path.join(OUT, "lid_tess.json"), "w"))
print("done")

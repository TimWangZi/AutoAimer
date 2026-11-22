# -*- coding: utf-8 -*-
"""AutoAimer 瞄具壳体 —— Part 工作台的 CSG 特征树（每个长方体/圆柱都能改尺寸和位置）。

坐标：壳体自身，底面 z=0，+X 朝前，+Y 向右。单位 mm。
运行： FreeCADCmd.exe build_shell.py
输出： shell.FCStd / shell.step / shell.stl / shell_tess.json
"""
import json
import os

import FreeCAD as App
import Part

OUT = os.path.dirname(os.path.abspath(__file__))

# ------------------------------------------------------------------ 参数
L, W, H = 100.0, 42.0, 50.0
WALL, LID = 2.4, 2.4

SCR_D, SCR_Z = 32.4, 25.3
CAM_D, CAM_Z = 10.5, 36.3
RNG_W, RNG_H, RNG_Z = 16.0, 8.0, 10.3
BTN_D = 6.0
BTN_X = [-42.0, -33.0, -24.0, -15.0, -6.0]
USBC_W, USBC_H, USBC_X, USBC_Z = 9.0, 3.5, 20.5, 13.8
LEDGE_Z, LEDGE_T, LEDGE_IN, LEDGE_OUT = 10.0, 2.0, 13.0, 18.6
PCB_X = 27.5

doc = App.newDocument("SightShell")


def box(name, x0, x1, y0, y1, z0, z1):
    b = doc.addObject("Part::Box", name)
    b.Length, b.Width, b.Height = x1 - x0, y1 - y0, z1 - z0
    b.Placement.Base = App.Vector(x0, y0, z0)
    b.Label = name
    return b


def cyl_x(name, r, x0, x1, cy, cz):
    c = doc.addObject("Part::Cylinder", name)
    c.Radius, c.Height = r, x1 - x0
    c.Placement = App.Placement(App.Vector(x0, cy, cz),
                                App.Rotation(App.Vector(0, 0, 1), App.Vector(1, 0, 0)))
    c.Label = name
    return c


def cyl_z(name, r, z0, z1, cx, cy):
    c = doc.addObject("Part::Cylinder", name)
    c.Radius, c.Height = r, z1 - z0
    c.Placement.Base = App.Vector(cx, cy, z0)
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


# 1 外形
shell = box("01 outer", -L / 2, L / 2, -W / 2, W / 2, 0, H)
doc.recompute()

# 2 内腔（底部开口）
shell = cut("02 cavity", shell, box("02 cavity tool", -L / 2 + WALL, L / 2 - WALL,
                                    -W / 2 + WALL, W / 2 - WALL, -1, H - LID))
doc.recompute()

# 3 屏幕窗口
shell = cut("03 screen", shell, cyl_x("03 screen tool", SCR_D / 2, -L / 2 - 1, -L / 2 + WALL + 1, 0, SCR_Z))
# 4 镜头窗口
shell = cut("04 camera", shell, cyl_x("04 camera tool", CAM_D / 2, L / 2 - WALL - 1, L / 2 + 1, 0, CAM_Z))
# 5 测距窗口
shell = cut("05 ranger", shell, box("05 ranger tool", L / 2 - WALL - 1, L / 2 + 1,
                                    -RNG_W / 2, RNG_W / 2, RNG_Z - RNG_H / 2, RNG_Z + RNG_H / 2))
# 6 五键
btns = cyl_z("06 btn tool 1", BTN_D / 2, H - LID - 1, H + 1, BTN_X[0], 0)
for i, bx in enumerate(BTN_X[1:]):
    btns = fuse("06 btn fuse %d" % i, btns, cyl_z("06 btn tool %d" % (i + 2), BTN_D / 2, H - LID - 1, H + 1, bx, 0))
shell = cut("06 buttons", shell, btns)
# 7 USB-C
shell = cut("07 usbc", shell, box("07 usbc tool", USBC_X - USBC_W / 2, USBC_X + USBC_W / 2,
                                  -W / 2 - 1, -W / 2 + WALL + 1, USBC_Z - USBC_H / 2, USBC_Z + USBC_H / 2))
doc.recompute()

# 8 PCB 支撑台
ledge = fuse("08 ledge", box("08 ledge L", -PCB_X, PCB_X, LEDGE_IN, LEDGE_OUT, LEDGE_Z, LEDGE_Z + LEDGE_T),
             box("08 ledge R", -PCB_X, PCB_X, -LEDGE_OUT, -LEDGE_IN, LEDGE_Z, LEDGE_Z + LEDGE_T))
shell = fuse("09 shell final", shell, ledge)
doc.recompute()

s = shell.Shape
print("volume %.1f mm3" % s.Volume)
print("bbox", s.BoundBox)
print("solids", len(s.Solids), "valid", s.isValid())

doc.saveAs(os.path.join(OUT, "shell.FCStd"))
Part.export([shell], os.path.join(OUT, "shell.step"))
s.exportStl(os.path.join(OUT, "shell.stl"))

verts, facets = s.tessellate(0.2)
tris = [[[round(verts[i].x, 3), round(verts[i].y, 3), round(verts[i].z, 3)] for i in f] for f in facets]
json.dump({"S": tris}, open(os.path.join(OUT, "shell_tess.json"), "w"))
print("done")

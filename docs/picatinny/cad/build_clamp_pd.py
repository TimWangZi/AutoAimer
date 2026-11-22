# -*- coding: utf-8 -*-
"""用 FreeCAD PartDesign 建夹具：可编辑的特征树（Sketch → Pad → Pocket）。

运行： FreeCADCmd.exe build_clamp_pd.py
输出： clamp.FCStd / clamp.step / clamp_body.stl / clamp_bar.stl / tess.json
"""
import json
import math
import os

import FreeCAD as App
import Part
import Sketcher

OUT = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- 参数
W_TOP, W_MAX, W_BASE, H = 19.00, 21.20, 15.67, 9.32
Z_CROWN = H - (W_MAX - W_TOP) / 2
Z_DOVE = Z_CROWN - (W_MAX - W_BASE) / 2
GAP = 0.35
LEN = 50.0
Z_TOP, Z_BOT = 17.0, 4.0
BOLT_Z, BOLT_R = 13.5, 2.5
BOLT_X = [-10.01, 10.01]      # 2 x 10.01 = 20.02，严格对齐槽距
TOP_HOLE_R, TOP_HOLE_D = 2.25, 5.5
TOP_HOLES = []          # 顶面不加工安装孔

RAIL = [(-W_TOP / 2, H), (W_TOP / 2, H), (W_MAX / 2, Z_CROWN), (W_BASE / 2, Z_DOVE),
        (W_BASE / 2, 0.0), (-W_BASE / 2, 0.0), (-W_BASE / 2, Z_DOVE), (-W_MAX / 2, Z_CROWN)]


def offset_poly(pts, d):
    n = len(pts)
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    cw = area < 0
    lines = []
    for i in range(n):
        (y1, z1), (y2, z2) = pts[i], pts[(i + 1) % n]
        dy, dz = y2 - y1, z2 - z1
        L = math.hypot(dy, dz)
        ny, nz = ((-dz / L, dy / L) if cw else (dz / L, -dy / L))
        lines.append(((y1 + ny * d, z1 + nz * d), (dy, dz)))
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


CAV = offset_poly(RAIL, GAP)
crownL, doveL, crownR, doveR = CAV[7], CAV[6], CAV[2], CAV[3]
Z_CAV_TOP = CAV[0][1]

A_POLY = [(-15.5, Z_TOP), (9.6, Z_TOP), (9.6, Z_CAV_TOP), (crownL[0], Z_CAV_TOP),
          crownL, doveL, (doveL[0], Z_BOT), (-15.5, Z_BOT)]
B_POLY = [(crownR[0], Z_TOP), (15.5, Z_TOP), (15.5, 5.0), (doveR[0], 5.0), doveR, crownR]

doc = App.newDocument("PicatinnyClamp")


def plane(body, idx):
    return body.Origin.OriginFeatures[idx]


def make_body(name, poly, x_planes, holes_y, holes_top=None):
    body = doc.addObject("PartDesign::Body", name)
    body.Label = name

    # --- 轮廓草图（YZ 平面）
    sk = body.newObject("Sketcher::SketchObject", name + "_Profile")
    sk.Label = "Profile"
    sk.AttachmentSupport = [(plane(body, 5), "")]      # YZ_Plane
    sk.MapMode = "FlatFace"
    for i in range(len(poly)):
        y1, z1 = poly[i]
        y2, z2 = poly[(i + 1) % len(poly)]
        sk.addGeometry(Part.LineSegment(App.Vector(y1, z1, 0), App.Vector(y2, z2, 0)), False)
    for i in range(len(poly)):               # 端点重合，保证闭合
        sk.addConstraint(Sketcher.Constraint("Coincident", i, 2, (i + 1) % len(poly), 1))
    for i in range(len(poly)):               # 水平/竖直约束
        y1, z1 = poly[i]
        y2, z2 = poly[(i + 1) % len(poly)]
        if abs(z2 - z1) < 1e-6:
            sk.addConstraint(Sketcher.Constraint("Horizontal", i))
        elif abs(y2 - y1) < 1e-6:
            sk.addConstraint(Sketcher.Constraint("Vertical", i))

    pad = body.newObject("PartDesign::Pad", name + "_Pad")
    pad.Label = "Pad %g mm" % (x_planes[1] - x_planes[0])
    pad.Profile = sk
    pad.Length = x_planes[1] - x_planes[0]
    pad.SideType = 'Symmetric'
    pad.Type = 0
    doc.recompute()

    # --- 横栓孔（XZ 平面，沿 Y 贯穿）
    sk2 = body.newObject("Sketcher::SketchObject", name + "_BoltHoles")
    sk2.Label = "Bolt holes ⌀%g" % (BOLT_R * 2)
    sk2.AttachmentSupport = [(plane(body, 4), "")]     # XZ_Plane
    sk2.MapMode = "FlatFace"
    for bx in holes_y:
        sk2.addGeometry(Part.Circle(App.Vector(bx, BOLT_Z, 0), App.Vector(0, 0, 1), BOLT_R), False)
    pk2 = body.newObject("PartDesign::Pocket", name + "_BoltPocket")
    pk2.Label = "Pocket bolt ⌀%g" % (BOLT_R * 2)
    pk2.Profile = sk2
    pk2.Type = 1                             # ThroughAll
    pk2.SideType = 'Symmetric'
    doc.recompute()

    # --- 顶部盲孔（基准面 z=Z_TOP）
    if holes_top:
        dp = body.newObject("PartDesign::Plane", "TopPlane")
        dp.AttachmentSupport = [(plane(body, 3), "")]  # XY_Plane
        dp.MapMode = "FlatFace"
        dp.AttachmentOffset = App.Placement(App.Vector(0, 0, Z_TOP), App.Rotation())
        doc.recompute()
        sk3 = body.newObject("Sketcher::SketchObject", name + "_TopHoles")
        sk3.Label = "Top holes ⌀%g x%g deep" % (TOP_HOLE_R * 2, TOP_HOLE_D)
        sk3.AttachmentSupport = [(dp, "")]
        sk3.MapMode = "FlatFace"
        for (hx, hy) in holes_top:
            sk3.addGeometry(Part.Circle(App.Vector(hx, hy, 0), App.Vector(0, 0, 1), TOP_HOLE_R), False)
        pk3 = body.newObject("PartDesign::Pocket", name + "_TopPocket")
        pk3.Label = "Pocket top ⌀%g" % (TOP_HOLE_R * 2)
        pk3.Profile = sk3
        pk3.Type = 0                         # Length
        pk3.Length = TOP_HOLE_D
        doc.recompute()

    return body


bodyA = make_body("ClampBody_A", A_POLY, (-LEN / 2, LEN / 2), BOLT_X, TOP_HOLES)
bodyB = make_body("ClampBar_B", B_POLY, (-LEN / 2, LEN / 2), BOLT_X)

doc.recompute()

print("A volume %.1f" % bodyA.Shape.Volume)
print("B volume %.1f" % bodyB.Shape.Volume)
print("A bbox   ", bodyA.Shape.BoundBox)
print("pitch %.2f (2 x 10.01)" % (BOLT_X[1] - BOLT_X[0]))

doc.saveAs(os.path.join(OUT, "clamp.FCStd"))
Part.export([bodyA, bodyB], os.path.join(OUT, "clamp.step"))
Part.export([bodyA], os.path.join(OUT, "clamp_body_A.step"))
Part.export([bodyB], os.path.join(OUT, "clamp_bar_B.step"))
bodyA.Shape.exportStl(os.path.join(OUT, "clamp_body.stl"))
bodyB.Shape.exportStl(os.path.join(OUT, "clamp_bar.stl"))


def tess(shape):
    verts, facets = shape.tessellate(0.15)
    return [[[round(verts[i].x, 3), round(verts[i].y, 3), round(verts[i].z, 3)] for i in f]
            for f in facets]


with open(os.path.join(OUT, "tess.json"), "w") as f:
    json.dump({"A": tess(bodyA.Shape), "B": tess(bodyB.Shape)}, f)
print("done")

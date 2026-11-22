# -*- coding: utf-8 -*-
"""导入用户的 clamp_body_a_1.STEP，导出三角面片 + 平面面清单，供查看与开孔定位。"""
import json
import os

import FreeCAD as App
import Part

OUT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(OUT, "clamp_body_a_1.STEP")

s = Part.read(SRC)
print("vol %.1f  valid %s  faces %d" % (s.Volume, s.isValid(), len(s.Faces)))

# 平面清单：法向 + 面积 + 中心 + 包围盒
rows = []
for i, f in enumerate(s.Faces):
    if f.Surface.__class__.__name__ != "Plane":
        continue
    n = f.normalAt(0, 0)
    c = f.CenterOfMass
    bb = f.BoundBox
    rows.append((round(n.x, 2), round(n.y, 2), round(n.z, 2),
                 round(f.Area, 1), round(c.x, 1), round(c.y, 1), round(c.z, 1),
                 [round(bb.XMin, 1), round(bb.XMax, 1), round(bb.YMin, 1), round(bb.YMax, 1),
                  round(bb.ZMin, 1), round(bb.ZMax, 1)]))
rows.sort(key=lambda r: -r[3])
print("planar faces (normal / area / center / bbox):")
for r in rows[:26]:
    print("  n=%-16s A=%8.1f c=(%6.1f,%6.1f,%6.1f) bb=%s" % (str(r[:3]), r[3], r[4], r[5], r[6], r[7]))

verts, facets = s.tessellate(0.25)
tris = [[[round(verts[i].x, 3), round(verts[i].y, 3), round(verts[i].z, 3)] for i in f] for f in facets]
json.dump({"B": tris}, open(os.path.join(OUT, "userbody_tess.json"), "w"))
print("tris", len(tris))

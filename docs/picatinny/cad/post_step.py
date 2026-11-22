# -*- coding: utf-8 -*-
"""给 STEP 里的产品/实体填上有意义的名字，方便 SolidWorks 里识别。"""
import os
import re

D = os.path.dirname(os.path.abspath(__file__))

JOBS = [
    ("clamp.step", "PicatinnyClamp", ["ClampBody_A", "ClampBar_B"]),
    ("clamp_body_A.step", "ClampBody_A", ["ClampBody_A"]),
    ("clamp_bar_B.step", "ClampBar_B", ["ClampBar_B"]),
]

for fname, product, solids in JOBS:
    path = os.path.join(D, fname)
    with open(path, "r", encoding="utf-8") as f:
        txt = f.read()

    # 产品名：替换 PRODUCT 里的前两个字符串
    txt = re.sub(r"PRODUCT\('[^']*'", "PRODUCT('%s'" % product, txt, count=1)
    txt = re.sub(r"(PRODUCT\('[^']*',\s*)'[^']*'", r"\1'%s'" % product, txt, count=1)

    # 实体名：按出现顺序替换 MANIFOLD_SOLID_BREP('',...)
    for name in solids:
        txt = txt.replace("MANIFOLD_SOLID_BREP('',", "MANIFOLD_SOLID_BREP('%s'," % name, 1)

    with open(path, "w", encoding="utf-8") as f:
        f.write(txt)
    print("%-20s product=%-16s solids=%s" % (fname, product, ",".join(solids)))

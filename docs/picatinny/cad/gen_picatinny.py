# -*- coding: utf-8 -*-
"""生成皮卡汀尼导轨（MIL-STD-1913）参考几何：DXF + 预览图。

尺寸单位 mm，原点在导轨底面中心，X 沿导轨长度，Y 宽度，Z 高度。

剖面（自上而下）：
    .748 ±.002  顶面      19.00
    .835 -.005  脊线      21.20   ← 顶部 45° 倒角斜出去的位置，在顶面下 1.10
    .108        燕尾面高   2.77   ← 45° 收回，是夹具真正的承力面
    .617 -.010  底面      15.67
    .367 MIN    总高       9.32
"""
import math
import os

# ------------------------------------------------------------------ 参数
W_TOP = 19.00      # 顶面宽 = .748 ±.002
W_MAX = 21.20      # 脊线宽 = .835 -.005
W_BASE = 15.67     # 底面宽 = .617 -.010
H = 9.32           # 总高 = .367 MIN

CROWN_H = (W_MAX - W_TOP) / 2      # 顶部 45° 倒角高 = 1.10
DOVE_H = (W_MAX - W_BASE) / 2      # 燕尾 45° 面高 = 2.77（标准 .108）
Z_CROWN = H - CROWN_H              # 8.22
Z_DOVE = Z_CROWN - DOVE_H          # 5.45

SLOT_W = 5.23      # 槽宽 = .206 (+.008)
SLOT_PITCH = 10.01 # 槽距 = .394
SLOT_DEPTH = 3.00  # 槽深 = .118 (+.008)
RAIL_LEN = 200.0
FIRST_SLOT = 8.0

GAP = 0.05         # 45° 面 / 侧面配合间隙（单边）
GAP_BOTTOM = 0.50  # 底部让位

OUT_DIR = os.path.dirname(os.path.abspath(__file__))


# ------------------------------------------------------------ 几何构造
def rail_profile():
    """导轨剖面多边形（顺时针，8 个顶点）。"""
    return [
        (-W_TOP / 2, H),        # 顶面左
        (W_TOP / 2, H),         # 顶面右
        (W_MAX / 2, Z_CROWN),   # 脊线右
        (W_BASE / 2, Z_DOVE),   # 燕尾下缘右
        (W_BASE / 2, 0.0),      # 底右
        (-W_BASE / 2, 0.0),     # 底左
        (-W_BASE / 2, Z_DOVE),  # 燕尾下缘左
        (-W_MAX / 2, Z_CROWN),  # 脊线左
    ]


def offset_polygon(pts, offsets):
    """凸多边形按每条边外法线偏移（边 i 为 pts[i] -> pts[i+1]）。"""
    n = len(pts)
    area = 0.0
    for i in range(n):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % n]
        area += x1 * y2 - x2 * y1
    cw = area < 0

    lines = []
    for i in range(n):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % n]
        dx, dy = x2 - x1, y2 - y1
        L = math.hypot(dx, dy)
        nx, ny = (-dy / L, dx / L) if cw else (dy / L, -dx / L)
        off = offsets[i]
        lines.append(((x1 + nx * off, y1 + ny * off), (dx, dy)))

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


# --------------------------------------------------------------- DXF 输出
def _line(layer, p1, p2):
    return ["0", "LINE", "8", layer,
            "10", f"{p1[0]:.4f}", "20", f"{p1[1]:.4f}", "30", "0.0",
            "11", f"{p2[0]:.4f}", "21", f"{p2[1]:.4f}", "31", "0.0"]


def _text(layer, pt, s, h=2.0):
    return ["0", "TEXT", "8", layer,
            "10", f"{pt[0]:.4f}", "20", f"{pt[1]:.4f}", "30", "0.0",
            "40", f"{h}", "1", s]


def write_dxf(path, entities):
    head = ["0", "SECTION", "2", "HEADER",
            "9", "$ACADVER", "1", "AC1009",
            "9", "$INSUNITS", "70", "4",
            "0", "ENDSEC"]
    body = ["0", "SECTION", "2", "ENTITIES"] + entities + ["0", "ENDSEC", "0", "EOF"]
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(head + body) + "\n")


def poly_entities(layer, pts, close=True):
    out = []
    n = len(pts)
    for i in range(n if close else n - 1):
        out += _line(layer, pts[i], pts[(i + 1) % n])
    return out


def make_profile():
    pts = rail_profile()
    e = poly_entities("PROFILE", pts)
    e += _line("CENTER", (0, -3), (0, H + 3))
    e += _line("CENTER", (-W_MAX / 2 - 4, 0), (W_MAX / 2 + 4, 0))
    for s in (-1, 1):   # 基准 C：顶面两端，间距 19.00
        e += _line("DATUM", (s * W_TOP / 2, H - 1), (s * W_TOP / 2, H + 3))
    for s in (-1, 1):   # 脊线
        e += _line("DATUM", (s * W_MAX / 2, Z_CROWN - 1), (s * W_MAX / 2, Z_CROWN + 3))
    e += _text("NOTE", (-W_MAX / 2 - 8, H + 7),
               "PICATINNY 1913 PROFILE (mm)", 2.2)
    e += _text("NOTE", (-W_MAX / 2 - 8, H + 3),
               "TOP %.2f  RIDGE %.2f (z %.2f)  BASE %.2f  H %.2f"
               % (W_TOP, W_MAX, Z_CROWN, W_BASE, H), 1.6)
    e += _text("NOTE", (-W_MAX / 2 - 8, -6),
               "2x45 TOP CHAMFER %.2f   2x45 DOVETAIL %.2f" % (CROWN_H, DOVE_H), 1.6)
    write_dxf(os.path.join(OUT_DIR, "rail_profile_1913.dxf"), e)
    return pts


def make_cavity():
    # 边序：0 顶面(贴合) 1 上倒角 2 燕尾右 3 右立面 4 底面 5 左立面 6 燕尾左 7 上倒角
    offs = [0.0, GAP, GAP, GAP, GAP_BOTTOM, GAP, GAP, GAP]
    pts = offset_polygon(rail_profile(), offs)
    e = poly_entities("CAVITY", pts)
    e += _text("NOTE", (-W_MAX / 2 - 6, H + 7),
               "CAVITY FOR YOUR PART (rail +%.2f gap, bottom +%.2f)" % (GAP, GAP_BOTTOM), 2.0)
    write_dxf(os.path.join(OUT_DIR, "rail_cavity_0.05.dxf"), e)
    return pts


def slot_positions():
    xs, x = [], FIRST_SLOT
    while x + SLOT_W <= RAIL_LEN - 2.0:
        xs.append(x)
        x += SLOT_PITCH
    return xs


def make_side():
    pts = [(0.0, 0.0), (0.0, H)]
    for s in slot_positions():
        pts += [(s, H), (s, H - SLOT_DEPTH), (s + SLOT_W, H - SLOT_DEPTH), (s + SLOT_W, H)]
    pts += [(RAIL_LEN, H), (RAIL_LEN, 0.0)]
    e = poly_entities("PROFILE", pts)
    for s in slot_positions():
        xc = s + SLOT_W / 2
        e += _line("CENTER", (xc, -2.5), (xc, H - SLOT_DEPTH - 2.0))
    e += _line("CENTER", (0, 0), (RAIL_LEN, 0))
    e += _text("NOTE", (0, H + 7),
               "SIDE VIEW  L=%.0f  SLOT W=%.2f  PITCH=%.2f  DEPTH=%.2f  n=%d"
               % (RAIL_LEN, SLOT_W, SLOT_PITCH, SLOT_DEPTH, len(slot_positions())), 3.0)
    write_dxf(os.path.join(OUT_DIR, "rail_side_200mm.dxf"), e)
    return pts


# ------------------------------------------------------------------ 预览
def preview(profile, cavity, side):
    from PIL import Image, ImageDraw, ImageFont
    Wpx, Hpx = 1500, 900
    img = Image.new("RGB", (Wpx, Hpx), "white")
    d = ImageDraw.Draw(img)
    try:
        f = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 22)
        fs = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 17)
    except Exception:
        f = fs = ImageFont.load_default()

    ox, oy, s1 = 300, 400, 17
    tp = [(ox + x * s1, oy - y * s1) for x, y in profile]
    d.polygon(tp, fill=(225, 232, 236))
    d.line(tp + [tp[0]], fill=(38, 50, 56), width=3, joint="curve")
    cp = [(ox + x * s1, oy - y * s1) for x, y in cavity]
    d.line(cp + [cp[0]], fill=(21, 101, 192), width=2, joint="curve")

    # 标注
    def arrow(p1, p2, col=(230, 81, 0)):
        d.line([p1, p2], fill=col, width=2)
        d.text((p2[0] + 6, p2[1] - 10), "", font=fs)

    d.text((40, 24), "导轨剖面（黑）与零件空腔（蓝，单边 %.2f 间隙）" % GAP, font=f, fill=(38, 50, 56))
    d.text((40, 56), "顶面 %.2f → 45° 倒角 → 脊线 %.2f（z %.2f）→ 45° 燕尾 %.2f → 底面 %.2f，总高 %.2f"
           % (W_TOP, W_MAX, Z_CROWN, DOVE_H, W_BASE, H), font=fs, fill=(90, 90, 90))

    # 脊线标记
    for s in (-1, 1):
        cx, cy = ox + s * W_MAX / 2 * s1, oy - Z_CROWN * s1
        d.ellipse((cx - 5, cy - 5, cx + 5, cy + 5), fill=(230, 81, 0))
    d.text((ox + 185, oy - 120),
           "● 脊线 21.20\n   在顶面下 1.10", font=fs, fill=(230, 81, 0))
    d.text((ox + 185, oy - 250),
           "● 基准 C 19.00\n   顶面两端", font=fs, fill=(21, 101, 192))
    d.text((ox + 185, oy - 30),
           "● 燕尾 45°·%.2f\n   夹具承力面" % DOVE_H, font=fs, fill=(46, 125, 50))

    # 侧视
    ox2, oy2, s2 = 60, 820, 6.2
    sp = [(ox2 + x * s2, oy2 - y * s2) for x, y in side]
    d.line(sp + [sp[0]], fill=(38, 50, 56), width=2, joint="curve")
    d.text((40, 620), "200 mm 侧视：槽宽 5.23 / 槽距 10.01 / 槽深 3.00 / %d 个槽"
           % len(slot_positions()), font=f, fill=(38, 50, 56))

    img.save(os.path.join(OUT_DIR, "preview.png"))
    print("preview saved")


if __name__ == "__main__":
    p = make_profile()
    c = make_cavity()
    s = make_side()
    preview(p, c, s)
    print("Z_CROWN %.3f  Z_DOVE %.3f  CROWN_H %.3f  DOVE_H %.3f"
          % (Z_CROWN, Z_DOVE, CROWN_H, DOVE_H))
    print("profile:", [(round(x, 3), round(y, 3)) for x, y in p])
    print("cavity :", [(round(x, 3), round(y, 3)) for x, y in c])
    print("slots:", len(slot_positions()))

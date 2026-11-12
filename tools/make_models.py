#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_models.py — генератор 3D-геометрии носителей мода MusicRadio (MusicRadio, DayZ).

Что получается на выходе:

    Models/src/mr_cassette.obj/.mtl      компакт-кассета  100.0 x 63.8 x 12.0 мм
    Models/src/mr_minidisc.obj/.mtl      MiniDisc-картридж 68.0 x 72.0 x  5.0 мм
    Models/src/mr_memorycard.obj/.mtl    SD-карта          32.0 x 24.0 x  2.1 мм
    Models/preview/<model>_draft.svg     чертёж в трёх проекциях с размерами

Про .p3d честно: ODOL — бинарный формат Bohemia, его пишет только Object Builder
из DayZ Tools. Скриптом .p3d не создаётся, поэтому здесь готовится всё, что нужно
ДО сохранения: геометрия в метрах, габариты, именованные селекшены и UV.

Система координат — как в Object Builder / ODOL: X — вправо, Y — «в глубину»,
Z — вверх (у лежащего на земле предмета толщина — это размер по Z).
Импортировать в Object Builder с МАСШТАБОМ 1.0 (в файлах уже метры).

Запуск:
    python3 tools/make_models.py                 # все модели
    python3 tools/make_models.py --only cassette
"""
import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mr_asset_lib import (DIM, TEX, uv_rect, PlanarMapper, TileMapper,  # noqa: E402
                          sub_uv, tex_rect_px, layout, project_root)

ROOT = project_root()
L = layout(ROOT)
SRC_DIR = L["models_src"]       # Models/src  (в архиве — 1_MODELS)
PREVIEW_DIR = L["preview"]      # Models/preview (в архиве — 4_PREVIEW)

EPS = 1e-12

# ==========================================================================
#  МЕШ
# ==========================================================================
class Mesh:
    """Простой меш: вершины + UV + треугольники с группой и материалом."""

    def __init__(self, name):
        self.name = name
        self.v = []
        self.vt = []
        self.tris = []          # (group, material, [(vi, ti), ...])

    def add_v(self, p):
        self.v.append((float(p[0]), float(p[1]), float(p[2])))
        return len(self.v) - 1

    def add_vt(self, uv):
        self.vt.append((float(uv[0]), float(uv[1])))
        return len(self.vt) - 1

    def tri(self, group, mat, pts):
        """pts — список [(x, y, z), (u, v)]."""
        idx = []
        for pos, uv in pts:
            idx.append((self.add_v(pos), self.add_vt(uv)))
        self.tris.append((group, mat, idx))

    def quad(self, group, mat, a, b, c, d):
        self.tri(group, mat, [a, b, c])
        self.tri(group, mat, [a, c, d])

    def stats(self):
        groups = {}
        for g, m, _ in self.tris:
            groups.setdefault(g, [0, m])
            groups[g][0] += 1
        return len(self.v), len(self.tris), groups

    def write_obj(self, path):
        lines = [
            "# %s — модель мода MusicRadio (MusicRadio) для DayZ" % self.name,
            "# единицы: метры; оси: X вправо, Y в глубину, Z вверх",
            "# импорт в Object Builder: File -> Import -> Wavefront (.obj), МАСШТАБ 1.0",
            "# имена групп (g) = именованные селекшены в .p3d",
            "mtllib %s.mtl" % self.name,
            "",
        ]
        for p in self.v:
            lines.append("v %.6f %.6f %.6f" % p)
        lines.append("")
        for uv in self.vt:
            lines.append("vt %.6f %.6f" % uv)
        lines.append("")
        cur = None
        for group, mat, idx in self.tris:
            if (group, mat) != cur:
                lines.append("g %s" % group)
                lines.append("usemtl %s" % mat)
                cur = (group, mat)
            lines.append("f " + " ".join("%d/%d" % (vi + 1, ti + 1) for vi, ti in idx))
        lines.append("")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines))

    def write_mtl(self, path):
        mats = sorted({m for _, m, _ in self.tris})
        lines = ["# материалы модели %s" % self.name,
                 "# имя материала = имя .rvmat, который надо назначить в Object Builder",
                 ""]
        for m in mats:
            W, H = TEX[m]["size"]
            lines += [
                "newmtl %s" % m,
                "Ka 1.000 1.000 1.000",
                "Kd 1.000 1.000 1.000",
                "Ks 0.150 0.150 0.150",
                "Ns 30.0",
                "map_Kd ../../Models/tex_src/%s_co.png" % m,
                "# текстура %s_co.png: %dx%d px, в игру идёт .paa (TexView 2)" % (m, W, H),
                "",
            ]
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines))


# ==========================================================================
#  2D-ПРОФИЛИ (все полигоны — против часовой стрелки, если смотреть сверху)
# ==========================================================================
def rounded_rect(w, d, r, seg=6, cut_corner=None, cut=0.0):
    """
    Профиль скруглённого прямоугольника, центр в (0, 0).
    cut_corner — (sx, sy): какой угол срезать (знаки по X и Y), cut — размер среза.
    """
    hw, hd = w / 2.0, d / 2.0
    r = min(r, min(hw, hd) * 0.9)
    # углы обходим ПРОТИВ ЧАСОВОЙ: низ-право -> верх-право -> верх-лево -> низ-лево
    corners = [
        (hw - r, -(hd - r), -90.0, 0.0, (1, -1)),
        (hw - r, (hd - r), 0.0, 90.0, (1, 1)),
        (-(hw - r), (hd - r), 90.0, 180.0, (-1, 1)),
        (-(hw - r), -(hd - r), 180.0, 270.0, (-1, -1)),
    ]
    pts = []
    for cx, cy, a0, a1, sign in corners:
        if cut_corner is not None and sign == tuple(cut_corner) and cut > 0.0:
            # вместо дуги — прямая фаска
            pts.append((cx + r * math.cos(math.radians(a0)),
                        cy + r * math.sin(math.radians(a0))))
            pts.append((cx + r * math.cos(math.radians(a1)),
                        cy + r * math.sin(math.radians(a1))))
            continue
        for i in range(seg + 1):
            a = math.radians(a0 + (a1 - a0) * i / seg)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def poly_bbox(poly):
    xs = [p[0] for p in poly]
    ys = [p[1] for p in poly]
    return min(xs), min(ys), max(xs), max(ys)


def inward_normal(a, b, poly_ccw=True):
    """Нормаль ребра a->b внутрь полигона (для обхода против часовой — влево)."""
    ex, ey = b[0] - a[0], b[1] - a[1]
    L = math.hypot(ex, ey) or 1.0
    if poly_ccw:
        return (-ey / L, ex / L)
    return (ey / L, -ex / L)


def offset_poly(poly, dist):
    """Сдвиг замкнутого полигона внутрь на dist (miter, достаточно для фасок)."""
    n = len(poly)
    out = []
    for i in range(n):
        a, p, b = poly[(i - 1) % n], poly[i], poly[(i + 1) % n]
        n1 = inward_normal(a, p)
        n2 = inward_normal(p, b)
        nx, ny = n1[0] + n2[0], n1[1] + n2[1]
        L = math.hypot(nx, ny)
        if L < 1e-9:
            out.append((p[0], p[1]))
        else:
            k = dist / max(0.35, math.cos(0.5 * math.acos(
                max(-1.0, min(1.0, n1[0] * n2[0] + n1[1] * n2[1])))))
            out.append((p[0] + k * nx / L, p[1] + k * ny / L))
    return out


def clip_halfplane(poly, a, b, c):
    """Sutherland–Hodgman: оставить часть, где a*x + b*y + c >= 0."""
    out = []
    n = len(poly)
    for i in range(n):
        p, q = poly[i], poly[(i + 1) % n]
        dp = a * p[0] + b * p[1] + c
        dq = a * q[0] + b * q[1] + c
        if dp >= -1e-12:
            out.append(p)
        if (dp >= -1e-12) != (dq >= -1e-12):
            t = dp / (dp - dq)
            out.append((p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1])))
    return out


def clip_rect(poly, x0, x1, y0, y1):
    poly = clip_halfplane(poly, 1.0, 0.0, -x0)
    poly = clip_halfplane(poly, -1.0, 0.0, x1)
    poly = clip_halfplane(poly, 0.0, 1.0, -y0)
    poly = clip_halfplane(poly, 0.0, -1.0, y1)
    return poly


# ==========================================================================
#  ПРИМИТИВЫ
# ==========================================================================
def emit_cap(mesh, group, mat, poly, z, uvfn, facing=1, hole=None):
    """
    Крышка (верхняя/нижняя грань) с опциональным прямоугольным вырезом.
    Вырез режется разбиением на 3x3 ячейки: центр выбрасывается, остальное
    отсекается по ячейке (полигон выпуклый — это корректно).
    """
    if hole is None:
        cells = [poly]
    else:
        hx0, hy0, hx1, hy1 = hole
        bx0, by0, bx1, by1 = poly_bbox(poly)
        xs = [min(bx0, hx0), hx0, hx1, max(bx1, hx1)]
        ys = [min(by0, hy0), hy0, hy1, max(by1, hy1)]
        cells = []
        for i in range(3):
            for j in range(3):
                if i == 1 and j == 1:
                    continue
                cl = clip_rect(poly, xs[i], xs[i + 1], ys[j], ys[j + 1])
                if len(cl) >= 3:
                    cells.append(cl)
    for cell in cells:
        pts = [(p[0], p[1], z) for p in cell]
        uvs = [uvfn(p[0], p[1]) for p in cell]
        if facing < 0:
            pts = pts[::-1]
            uvs = uvs[::-1]
        for k in range(1, len(pts) - 1):
            mesh.tri(group, mat, [(pts[0], uvs[0]), (pts[k], uvs[k]),
                                  (pts[k + 1], uvs[k + 1])])


def emit_band(mesh, group, mat, poly_a, poly_b, z0, z1, uvr, v0=0.0, v1=1.0):
    """
    Боковая полоса между двумя профилями одинаковой длины.
    UV: u — доля периметра, v — доля высоты внутри области uvr.
    """
    n = len(poly_a)
    cum = [0.0]
    for i in range(n):
        a, b = poly_a[i], poly_a[(i + 1) % n]
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    total = cum[-1] or 1.0
    ru0, rv0, ru1, rv1 = uvr
    for i in range(n):
        j = (i + 1) % n
        a0, a1 = poly_a[i], poly_a[j]
        b0, b1 = poly_b[i], poly_b[j]
        u0 = ru0 + (cum[i] / total) * (ru1 - ru0)
        u1 = ru0 + (cum[j] / total if j != 0 else 1.0) * (ru1 - ru0)
        v_lo = rv0 + v0 * (rv1 - rv0)
        v_hi = rv0 + v1 * (rv1 - rv0)
        mesh.quad(
            group, mat,
            ((a0[0], a0[1], z0), (u0, v_lo)),
            ((a1[0], a1[1], z0), (u1, v_lo)),
            ((b1[0], b1[1], z1), (u1, v_hi)),
            ((b0[0], b0[1], z1), (u0, v_hi)),
        )


def prism(mesh, group, mat, w, d, z0, z1, r=0.002, seg=6, cut_corner=None,
          cut=0.0, bevel=0.0, uvfn_top=None, uvfn_bot=None,
          rect_side_long=None, rect_side_short=None, hole_top=None,
          hole_bottom=None):
    """
    Скруглённая призма (корпус): крышки + фаски + боковины.
    Области текстуры: верх / низ / «длинная» боковина / «короткая» боковина.
    """
    outer = rounded_rect(w, d, r, seg, cut_corner, cut)
    if bevel > 0.0:
        inner = offset_poly(outer, bevel)
    else:
        inner = outer

    def side_rect(i):
        a, b = outer[i], outer[(i + 1) % len(outer)]
        nx, ny = (b[1] - a[1]), -(b[0] - a[0])
        L = math.hypot(nx, ny) or 1.0
        nx, ny = nx / L, ny / L
        return rect_side_long if abs(ny) >= abs(nx) else rect_side_short

    if bevel > 0.0:
        # нижняя фаска, основная боковина, верхняя фаска
        for i in range(len(outer)):
            pass
        emit_band(mesh, group, mat, inner, outer, z0, z0 + bevel,
                  side_rect(0), 0.0, 0.18)
        emit_band(mesh, group, mat, outer, outer, z0 + bevel, z1 - bevel,
                  side_rect(0), 0.18, 0.82)
        emit_band(mesh, group, mat, outer, inner, z1 - bevel, z1,
                  side_rect(0), 0.82, 1.0)
    else:
        emit_band(mesh, group, mat, outer, outer, z0, z1, side_rect(0))

    emit_cap(mesh, group, mat, inner, z0, uvfn_bot, facing=-1, hole=hole_bottom)
    emit_cap(mesh, group, mat, inner, z1, uvfn_top, facing=+1, hole=hole_top)
    return outer, inner


def box(mesh, group, mat, x0, x1, y0, y1, z0, z1, uvr, uvr_top=None):
    """Параллелепипед. uvr — область для боковин, uvr_top — для верх/низа."""
    ru0, rv0, ru1, rv1 = uvr
    top = uvr_top or uvr

    def uv_rect_face(rect, s, t):
        a0, b0, a1, b1 = rect
        return (a0 + s * (a1 - a0), b0 + t * (b1 - b0))

    # низ / верх
    for z, facing, rect in ((z0, -1, top), (z1, +1, top)):
        pts = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
        uvs = [uv_rect_face(rect, 0, 0), uv_rect_face(rect, 1, 0),
               uv_rect_face(rect, 1, 1), uv_rect_face(rect, 0, 1)]
        if facing < 0:
            pts, uvs = pts[::-1], uvs[::-1]
        mesh.quad(group, mat,
                  ((pts[0][0], pts[0][1], z), uvs[0]),
                  ((pts[1][0], pts[1][1], z), uvs[1]),
                  ((pts[2][0], pts[2][1], z), uvs[2]),
                  ((pts[3][0], pts[3][1], z), uvs[3]))
    # боковины
    faces = [
        ((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)),   # -Y
        ((x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)),   # +Y
        ((x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)),   # +X
        ((x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)),   # -X
    ]
    for f in faces:
        mesh.quad(group, mat,
                  (f[0], uv_rect_face(uvr, 0, 0)),
                  (f[1], uv_rect_face(uvr, 1, 0)),
                  (f[2], uv_rect_face(uvr, 1, 1)),
                  (f[3], uv_rect_face(uvr, 0, 1)))


def cavity(mesh, group, mat, x0, x1, y0, y1, z_floor, z_top, uvr_wall, uvfn_floor):
    """
    Внутренняя полость (окно кассеты / окно MD): пол + четыре стенки.
    Верхней грани НЕТ — иначе она перекроет проём и спрячет катушки.
    Нормали стенок смотрят внутрь полости.
    """
    plate(mesh, group, mat, x0, x1, y0, y1, z_floor, uvfn_floor, facing=+1)
    ru0, rv0, ru1, rv1 = uvr_wall
    walls = [
        ((x0, y0, z_top), (x1, y0, z_top), (x1, y0, z_floor), (x0, y0, z_floor)),
        ((x1, y1, z_top), (x0, y1, z_top), (x0, y1, z_floor), (x1, y1, z_floor)),
        ((x1, y0, z_top), (x1, y1, z_top), (x1, y1, z_floor), (x1, y0, z_floor)),
        ((x0, y1, z_top), (x0, y0, z_top), (x0, y0, z_floor), (x0, y1, z_floor)),
    ]
    for w in walls:
        mesh.quad(group, mat,
                  (w[0], (ru0, rv1)), (w[1], (ru1, rv1)),
                  (w[2], (ru1, rv0)), (w[3], (ru0, rv0)))


def plate(mesh, group, mat, x0, x1, y0, y1, z, uvfn, facing=+1):
    """Плоская плашка (этикетка, стекло, контакты) — одна грань."""
    pts = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    uvs = [uvfn(x, y) for x, y in pts]
    if facing < 0:
        pts, uvs = pts[::-1], uvs[::-1]
    mesh.quad(group, mat,
              ((pts[0][0], pts[0][1], z), uvs[0]),
              ((pts[1][0], pts[1][1], z), uvs[1]),
              ((pts[2][0], pts[2][1], z), uvs[2]),
              ((pts[3][0], pts[3][1], z), uvs[3]))


def cylinder(mesh, group, mat, cx, cy, z0, z1, r, seg=28, uvr=None,
             uvfn_cap=None, caps=(True, True)):
    """Цилиндр. Боковина — тайл/развёртка, крышки — плоская проекция."""
    if uvr is None:
        uvr = (0.0, 0.0, 1.0, 1.0)
    ru0, rv0, ru1, rv1 = uvr
    if uvfn_cap is None:
        uvfn_cap = lambda x, y: (0.5 + (x - cx) / (2 * r), 0.5 + (y - cy) / (2 * r))
    ring0, ring1 = [], []
    for i in range(seg):
        a = 2.0 * math.pi * i / seg
        ring0.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    ring1 = list(ring0)
    for i in range(seg):
        j = (i + 1) % seg
        u0 = ru0 + (i / seg) * (ru1 - ru0)
        u1 = ru0 + (j / seg if j != 0 else 1.0) * (ru1 - ru0)
        mesh.quad(group, mat,
                  ((ring0[i][0], ring0[i][1], z0), (u0, rv0)),
                  ((ring0[j][0], ring0[j][1], z0), (u1, rv0)),
                  ((ring1[j][0], ring1[j][1], z1), (u1, rv1)),
                  ((ring1[i][0], ring1[i][1], z1), (u0, rv1)))
    if caps[0]:
        emit_cap(mesh, group, mat, ring0[::-1], z0, uvfn_cap, facing=-1)
    if caps[1]:
        emit_cap(mesh, group, mat, ring1, z1, uvfn_cap, facing=+1)


def gear(mesh, group, mat, cx, cy, z0, z1, r_out, r_in, teeth=12, seg_uv=None):
    """Зубчатая катушка (ступица кассеты): чередование радиусов по окружности."""
    pts = []
    n = teeth * 2
    for i in range(n):
        a = 2.0 * math.pi * i / n
        r = r_out if i % 2 == 0 else r_in
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    uvr = (0.0, 0.0, 1.0, 1.0)
    tile = TileMapper(0.02)
    emit_band(mesh, group, mat, pts, pts, z0, z1, uvr)
    emit_cap(mesh, group, mat, pts[::-1], z0, tile, facing=-1)
    emit_cap(mesh, group, mat, pts, z1, tile, facing=+1)


# ==========================================================================
#  МОДЕЛЬ 1: КОМПАКТ-КАССЕТА
# ==========================================================================
def build_cassette():
    d = DIM['mr_cassette']
    W, D, T = d['w'], d['d'], d['t']
    z0, z1 = -T / 2.0, T / 2.0
    m = Mesh("mr_cassette")

    top_map = PlanarMapper('mr_cassette', 'top', -W / 2, W / 2, -D / 2, D / 2)
    bot_map = PlanarMapper('mr_cassette', 'bottom', -W / 2, W / 2, -D / 2, D / 2,
                           flip_u=True)

    # окно (вырез в верхней крышке) — сквозь него видны катушки с плёнкой
    win = (-0.0330, -0.0140, 0.0330, 0.0120)

    # --- корпус ---
    prism(m, "shell", "mr_cassette", W, D, z0, z1,
          r=0.0020, seg=6, bevel=0.0006,
          uvfn_top=top_map, uvfn_bot=bot_map,
          rect_side_long=uv_rect('mr_cassette', 'side_long'),
          rect_side_short=uv_rect('mr_cassette', 'side_short'),
          hole_top=win)

    # --- внутренняя полость окна: пол + стенки (катушки лежат в глубине) ---
    floor_z = 0.0030
    cavity(m, "cavity", "mr_cassette", win[0], win[2], win[1], win[3],
           floor_z, z1, uv_rect('mr_cassette', 'tape'),
           PlanarMapper('mr_cassette', 'tape', win[0], win[2], win[1], win[3]))

    # --- рулоны плёнки (левый полный, правый — наполовину смотан) ---
    for name, cx, r in (("tape_left", -0.0212, 0.0178), ("tape_right", 0.0212, 0.0128)):
        tm = PlanarMapper('mr_cassette', 'tape', cx - r, cx + r,
                          -0.0010 - r, -0.0010 + r)
        cylinder(m, name, "mr_cassette", cx, -0.0010, floor_z, 0.0046, r, seg=32,
                 uvr=uv_rect('mr_cassette', 'tape'), uvfn_cap=tm)

    # --- ступицы (зубчатые катушки) — строго под стеклом ---
    for name, cx in (("hub_left", -0.0212), ("hub_right", 0.0212)):
        gear(m, name, "mr_plastic", cx, -0.0010, 0.0042, 0.0052,
             r_out=0.0076, r_in=0.0062, teeth=12)

    # --- стекло окна: рамка по периметру, середина открыта — видно катушки ---
    # (сплошная плашка перекрыла бы рулоны; так окно читается как углубление
    #  под прозрачным стеклом, а селекшен `window` остаётся для материала)
    gz = 0.0054
    gi = (win[0] + 0.0030, win[1] + 0.0025, win[2] - 0.0030, win[3] - 0.0025)
    plate(m, "window", "mr_cassette", win[0], win[2], win[1], gi[1], gz, top_map)
    plate(m, "window", "mr_cassette", win[0], win[2], gi[3], win[3], gz, top_map)
    plate(m, "window", "mr_cassette", win[0], gi[0], gi[1], gi[3], gz, top_map)
    plate(m, "window", "mr_cassette", gi[2], win[2], gi[1], gi[3], gz, top_map)

    # --- бумажная этикетка (чуть выше корпуса: читается как наклейка) ---
    lab = (-0.0440, 0.0135, 0.0440, 0.0305)
    plate(m, "label", "mr_cassette", lab[0], lab[2], lab[1], lab[3],
          z1 + 0.0002, top_map, facing=+1)

    return m, dict(win=win, label=lab)


# ==========================================================================
#  МОДЕЛЬ 2: MINIDISC
# ==========================================================================
def build_minidisc():
    d = DIM['mr_minidisc']
    W, D, T = d['w'], d['d'], d['t']
    z0, z1 = -T / 2.0, T / 2.0
    m = Mesh("mr_minidisc")

    top_map = PlanarMapper('mr_minidisc', 'top', -W / 2, W / 2, -D / 2, D / 2)
    bot_map = PlanarMapper('mr_minidisc', 'bottom', -W / 2, W / 2, -D / 2, D / 2,
                           flip_u=True)

    # окно чтения/записи: по центру, чуть ниже середины корпуса.
    # Диск (d = 60 мм) лежит внутри картриджа целиком — иначе он вылезает
    # за габарит и предмет перестаёт соответствовать паспорту 68 x 72 мм.
    win = (-0.0210, -0.0170, 0.0210, 0.0030)

    # --- корпус со срезанным углом (у MD один угол скошен) ---
    prism(m, "shell", "mr_minidisc", W, D, z0, z1,
          r=0.0030, seg=6, cut_corner=(1, 1), cut=0.0060, bevel=0.0004,
          uvfn_top=top_map, uvfn_bot=bot_map,
          rect_side_long=uv_rect('mr_minidisc', 'side'),
          rect_side_short=uv_rect('mr_minidisc', 'side'),
          hole_top=win)

    # --- полость под окном: пол + стенки (верхней грани нет) ---
    floor_z = 0.0009
    tile = TileMapper(0.012)
    cavity(m, "cavity", "mr_plastic", win[0], win[2], win[1], win[3],
           floor_z, z1, uv_rect('mr_plastic', 'tile'), tile)

    # --- диск, видимый в окне (целиком внутри картриджа) ---
    disc = lambda x, y: (0.5 + (x - 0.0) / 0.060, 0.5 + (y + 0.0050) / 0.060)
    cylinder(m, "disc", "mr_disc", 0.0, -0.0050, floor_z, floor_z + 0.0002,
             0.0300, seg=48, uvr=(0.0, 0.0, 1.0, 1.0), uvfn_cap=disc,
             caps=(False, True))
    cylinder(m, "hub", "mr_metal", 0.0, -0.0050, floor_z + 0.0002, floor_z + 0.0008,
             0.0055, seg=20, uvr=uv_rect('mr_metal', 'tile'))

    # --- металлическая шторка: прикрывает верхнюю часть окна ---
    box(m, "shutter", "mr_metal", -0.0230, 0.0230, -0.0010, 0.0110,
        z1, z1 + 0.0004, uvr=uv_rect('mr_metal', 'tile'))

    # --- этикетка ---
    lab = (-0.0300, 0.0130, 0.0300, 0.0330)
    plate(m, "label", "mr_minidisc", lab[0], lab[2], lab[1], lab[3],
          z1 + 0.0002, top_map, facing=+1)

    return m, dict(win=win, label=lab)


# ==========================================================================
#  МОДЕЛЬ 3: КАРТА ПАМЯТИ (SD)
# ==========================================================================
def build_memorycard():
    d = DIM['mr_memorycard']
    W, D, T = d['w'], d['d'], d['t']
    z0, z1 = -T / 2.0, T / 2.0
    m = Mesh("mr_memorycard")

    top_map = PlanarMapper('mr_memorycard', 'label', -W / 2, W / 2, -D / 2, D / 2)
    tile = TileMapper(0.012)

    # --- корпус со срезанным углом ---
    bot_map = PlanarMapper('mr_memorycard', 'side', -W / 2, W / 2, -D / 2, D / 2)
    prism(m, "shell", "mr_memorycard", W, D, z0, z1,
          r=0.0008, seg=4, cut_corner=(1, 1), cut=0.0035, bevel=0.00015,
          uvfn_top=top_map, uvfn_bot=bot_map,
          rect_side_long=uv_rect('mr_memorycard', 'side'),
          rect_side_short=uv_rect('mr_memorycard', 'side'))

    # --- наклейка с маркировкой ---
    lab = (-0.0140, -0.0090, 0.0130, 0.0090)
    plate(m, "label", "mr_memorycard", lab[0], lab[2], lab[1], lab[3],
          z1 + 0.00005, top_map, facing=+1)

    # --- контактная площадка (на нижней стороне, у дальней кромки) ---
    cont = (-0.0135, -0.0112, 0.0135, -0.0052)
    cont_map = PlanarMapper('mr_memorycard', 'contacts',
                            cont[0], cont[2], cont[1], cont[3])
    plate(m, "contacts", "mr_memorycard", cont[0], cont[2], cont[1], cont[3],
          z0 - 0.00003, cont_map, facing=-1)

    # --- переключатель защиты от записи ---
    box(m, "switch", "mr_plastic", -W / 2 - 0.0002, -W / 2 + 0.0001,
        -0.0045, 0.0020, -0.0007, 0.0007, uvr=uv_rect('mr_plastic', 'tile'))

    return m, dict(label=lab, contacts=cont)


# ==========================================================================
#  ЧЕРТЕЖИ (SVG, три проекции с размерами)
# ==========================================================================
def _arrow(x1, y1, x2, y2):
    import math as _m
    a = _m.atan2(y2 - y1, x2 - x1)
    s = 5.0
    p1 = (x2 - s * _m.cos(a - 0.35), y2 - s * _m.sin(a - 0.35))
    p2 = (x2 - s * _m.cos(a + 0.35), y2 - s * _m.sin(a + 0.35))
    return ('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#888" '
            'stroke-width="0.8"/><polygon points="%.1f,%.1f %.1f,%.1f %.1f,%.1f" '
            'fill="#888"/>' % (x1, y1, x2, y2, x2, y2, p1[0], p1[1], p2[0], p2[1]))


def _dim_h(x0, x1, y, text):
    return (_arrow(x0, y, x1, y) + _arrow(x1, y, x0, y) +
            '<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#888" '
            'stroke-width="0.8"/>' % (x0, y, x1, y) +
            '<text x="%.1f" y="%.1f" font-family="DejaVu Sans, sans-serif" '
            'font-size="11" fill="#333" text-anchor="middle">%s</text>'
            % ((x0 + x1) / 2, y - 5, text))


def _dim_v(y0, y1, x, text):
    return (_arrow(x, y0, x, y1) + _arrow(x, y1, x, y0) +
            '<text x="%.1f" y="%.1f" font-family="DejaVu Sans, sans-serif" '
            'font-size="11" fill="#333" text-anchor="middle" '
            'transform="rotate(-90 %.1f %.1f)">%s</text>'
            % (x - 6, (y0 + y1) / 2, x - 6, (y0 + y1) / 2, text))


def _rect_mm(svg, x, y, w, h, fill, stroke="#444", extra=""):
    svg.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s" '
               'stroke="%s" stroke-width="0.9" %s/>' % (x, y, w, h, fill, stroke, extra))


def write_draft(path, title, mm, views):
    """
    views — список проекций: (заголовок, ширина мм, высота мм, список деталей).
    Детали рисуются как прямые команды SVG в координатах проекции.
    """
    scale = 3.0
    pad = 70
    blocks = []
    for name, w_mm, h_mm, body in views:
        blocks.append((name, w_mm * scale, h_mm * scale, body))
    tw = max(b[1] for b in blocks) + pad * 2
    th = sum(b[2] for b in blocks) + pad * 2 + 40 * len(blocks)
    svg = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<svg xmlns="http://www.w3.org/2000/svg" width="%.0f" height="%.0f" '
           'viewBox="0 0 %.0f %.0f">' % (tw, th, tw, th),
           '<rect width="100%" height="100%" fill="white"/>',
           '<text x="20" y="26" font-family="DejaVu Sans, sans-serif" '
           'font-size="16" font-weight="bold" fill="#111">%s</text>' % title,
           '<text x="20" y="44" font-family="DejaVu Sans, sans-serif" '
           'font-size="11" fill="#666">мм, вид снаружи; ось Z (толщина) — вверх</text>']
    y = pad
    for name, w, h, body in blocks:
        svg.append('<text x="%.0f" y="%.0f" font-family="DejaVu Sans, sans-serif" '
                   'font-size="12" font-weight="bold" fill="#333">%s</text>'
                   % (pad, y - 10, name))
        svg.append('<g transform="translate(%.0f,%.0f)">' % (pad, y))
        svg.extend(body)
        svg.append('</g>')
        y += h + 60
    svg.append('</svg>')
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(svg))


def draft_cassette(path, extra):
    d = DIM['mr_cassette']
    W, D, T = d['w'] * 1000, d['d'] * 1000, d['t'] * 1000
    s = 3.0
    win = [v * 1000 for v in extra['win']]
    lab = [v * 1000 for v in extra['label']]

    # --- вид сверху (лицевая сторона) ---
    top = []
    _rect_mm(top, 0, 0, W * s, D * s, "#e9e6df")
    _rect_mm(top, (win[0] + W / 2) * s, (D / 2 - win[3]) * s,
             (win[2] - win[0]) * s, (win[3] - win[1]) * s, "#2b2f33")
    _rect_mm(top, (lab[0] + W / 2) * s, (D / 2 - lab[3]) * s,
             (lab[2] - lab[0]) * s, (lab[3] - lab[1]) * s, "#f6f2e6")
    for cx in (-21.2, 21.2):
        top.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="#7a7a7a" '
                   'stroke-width="0.8"/>' % ((cx + W / 2) * s, (D / 2 + 1.0) * s, 7.6 * s))
    for sx in (-1, 1):
        for sy in (-1, 1):
            top.append('<circle cx="%.1f" cy="%.1f" r="1.6" fill="#9a9a9a"/>'
                       % ((sx * 46 + W / 2) * s, (sy * 27.5 + D / 2) * s))
    top.append('<circle cx="%.1f" cy="%.1f" r="1.6" fill="#9a9a9a"/>'
               % (W / 2 * s, (-28.0 + D / 2) * s))
    top.append(_dim_h(0, W * s, D * s + 26, "%.1f" % W))
    top.append(_dim_v(0, D * s, -16, "%.1f" % D))

    # --- вид спереди (длинная сторона) ---
    front = []
    _rect_mm(front, 0, 0, W * s, T * s, "#dcd8d0")
    front.append(_dim_h(0, W * s, T * s + 26, "%.1f" % W))
    front.append(_dim_v(0, T * s, -16, "%.1f" % T))

    # --- вид сбоку (короткая сторона) ---
    side = []
    _rect_mm(side, 0, 0, D * s, T * s, "#dcd8d0")
    side.append(_dim_h(0, D * s, T * s + 26, "%.1f" % D))
    side.append(_dim_v(0, T * s, -16, "%.1f" % T))

    write_draft(path, "Компакт-кассета mr_cassette (%s x %s x %s мм)" %
                (W, D, T), (W, D, T),
                [("Вид сверху (+Z, сторона этикетки)", W, D, top),
                 ("Вид спереди (+Y)", W, T, front),
                 ("Вид сбоку (+X)", D, T, side)])


def draft_minidisc(path, extra):
    d = DIM['mr_minidisc']
    W, D, T = d['w'] * 1000, d['d'] * 1000, d['t'] * 1000
    s = 4.0
    win = [v * 1000 for v in extra['win']]
    lab = [v * 1000 for v in extra['label']]

    top = []
    _rect_mm(top, 0, 0, W * s, D * s, "#dfe3e6")
    top.append('<polygon points="%.1f,%.1f %.1f,%.1f %.1f,%.1f" fill="white"/>'
               % (W * s - 6 * s, 0, W * s, 6 * s, W * s, 0))
    _rect_mm(top, (win[0] + W / 2) * s, (D / 2 - win[3]) * s,
             (win[2] - win[0]) * s, (win[3] - win[1]) * s, "#3a3f45")
    _rect_mm(top, (lab[0] + W / 2) * s, (D / 2 - lab[3]) * s,
             (lab[2] - lab[0]) * s, (lab[3] - lab[1]) * s, "#f2f4f6")
    top.append(_dim_h(0, W * s, D * s + 26, "%.1f" % W))
    top.append(_dim_v(0, D * s, -16, "%.1f" % D))

    front = []
    _rect_mm(front, 0, 0, W * s, T * s, "#cfd4d8")
    front.append(_dim_h(0, W * s, T * s + 26, "%.1f" % W))
    front.append(_dim_v(0, T * s, -16, "%.1f" % T))

    side = []
    _rect_mm(side, 0, 0, D * s, T * s, "#cfd4d8")
    side.append(_dim_h(0, D * s, T * s + 26, "%.1f" % D))
    side.append(_dim_v(0, T * s, -16, "%.1f" % T))

    write_draft(path, "MiniDisc mr_minidisc (%s x %s x %s мм)" % (W, D, T),
                (W, D, T),
                [("Вид сверху (+Z, сторона шторки)", W, D, top),
                 ("Вид спереди (+Y)", W, T, front),
                 ("Вид сбоку (+X)", D, T, side)])


def draft_memorycard(path, extra):
    d = DIM['mr_memorycard']
    W, D, T = d['w'] * 1000, d['d'] * 1000, d['t'] * 1000
    s = 8.0
    lab = [v * 1000 for v in extra['label']]
    cont = [v * 1000 for v in extra['contacts']]

    top = []
    _rect_mm(top, 0, 0, W * s, D * s, "#2f3338")
    top.append('<polygon points="%.1f,%.1f %.1f,%.1f %.1f,%.1f" fill="white"/>'
               % (W * s - 3.5 * s, 0, W * s, 3.5 * s, W * s, 0))
    _rect_mm(top, (lab[0] + W / 2) * s, (D / 2 - lab[3]) * s,
             (lab[2] - lab[0]) * s, (lab[3] - lab[1]) * s, "#e8e2d2")
    top.append(_dim_h(0, W * s, D * s + 26, "%.1f" % W))
    top.append(_dim_v(0, D * s, -16, "%.1f" % D))

    back = []
    _rect_mm(back, 0, 0, W * s, D * s, "#26292d")
    _rect_mm(back, (cont[0] + W / 2) * s, (D / 2 + cont[1]) * s,
             (cont[2] - cont[0]) * s, (cont[3] - cont[1]) * s, "#c9a227")
    back.append(_dim_h(0, W * s, D * s + 26, "%.1f" % W))
    back.append(_dim_v(0, D * s, -16, "%.1f" % D))

    side = []
    _rect_mm(side, 0, 0, D * s, T * s, "#2a2d31")
    side.append(_dim_h(0, D * s, T * s + 26, "%.1f" % D))
    side.append(_dim_v(0, T * s, -16, "%.1f" % T))

    write_draft(path, "Карта памяти mr_memorycard (%s x %s x %s мм)" % (W, D, T),
                (W, D, T),
                [("Вид сверху (+Z, наклейка)", W, D, top),
                 ("Вид снизу (-Z, контакты)", W, D, back),
                 ("Вид сбоку (+X)", D, T, side)])


# ==========================================================================
#  MAIN
# ==========================================================================
BUILDERS = {
    'cassette': (build_cassette, draft_cassette),
    'minidisc': (build_minidisc, draft_minidisc),
    'memorycard': (build_memorycard, draft_memorycard),
}


def main():
    ap = argparse.ArgumentParser(description="Генератор моделей носителей MusicRadio")
    ap.add_argument("--only", choices=list(BUILDERS), help="только одна модель")
    args = ap.parse_args()

    os.makedirs(SRC_DIR, exist_ok=True)
    os.makedirs(PREVIEW_DIR, exist_ok=True)

    todo = [args.only] if args.only else list(BUILDERS)
    for key in todo:
        builder, drafter = BUILDERS[key]
        mesh, extra = builder()
        name = "mr_%s" % key
        obj_path = os.path.join(SRC_DIR, name + ".obj")
        mesh.write_obj(obj_path)
        mesh.write_mtl(os.path.join(SRC_DIR, name + ".mtl"))
        drafter(os.path.join(PREVIEW_DIR, name + "_draft.svg"), extra)
        nv, nt, groups = mesh.stats()
        print("[ok] %-14s %5d верш. %5d треуг.  группы: %s"
              % (name, nv, nt, ", ".join("%s(%d)" % (g, c) for g, (c, _) in sorted(groups.items()))))


if __name__ == "__main__":
    main()

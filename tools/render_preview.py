#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
render_preview.py — превью-рендер моделей носителей (без DayZ, «на коленке»).

Нужен, чтобы глазами проверить геометрию и UV-развертку ДО того, как модель
поедет в Object Builder: мягкий софтверный растеризатор с z-буфером, текстурой
по UV и простым освещением (блики берём из настоящей карты _smdi).

Это НЕ иконка для инвентаря и не замена Objec Builder'у — только контрольный
прогон. Иконку потом делается в Object Builder: Export -> Icon.

Запуск:
    python3 tools/render_preview.py                # все модели + общий лист
    python3 tools/render_preview.py --only cassette
"""
import argparse
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_asset_lib import TILE_MATERIALS, layout, project_root  # noqa: E402

ROOT = project_root()
L = layout(ROOT)
SRC = L["models_src"]
TEX_SRC = L["tex_src"]
PREVIEW = L["preview"]

SS = 2  # суперсэмплинг: рендерим в 2x и уменьшаем


# ---------------------------------------------------------------- загрузка
def load_obj(path):
    V, VT = [], []
    tris = []
    cur_g, cur_m = "default", "default"
    for line in open(path, encoding="utf-8"):
        t = line.strip()
        if not t or t.startswith("#"):
            continue
        parts = t.split()
        key = parts[0]
        if key == "v":
            V.append([float(x) for x in parts[1:4]])
        elif key == "vt":
            VT.append([float(x) for x in parts[1:3]])
        elif key == "g":
            cur_g = parts[1]
        elif key == "usemtl":
            cur_m = parts[1]
        elif key == "f":
            idx = []
            for tok in parts[1:]:
                a = tok.split("/")
                vi = int(a[0]) - 1
                ti = int(a[1]) - 1 if len(a) > 1 and a[1] else vi
                idx.append((vi, ti))
            for k in range(1, len(idx) - 1):
                tris.append((cur_g, cur_m, (idx[0], idx[k], idx[k + 1])))
    return np.asarray(V, np.float64), np.asarray(VT, np.float64), tris


class Texture:
    def __init__(self, mat):
        co = Image.open(os.path.join(TEX_SRC, mat + "_co.png")).convert("RGB")
        self.co = np.asarray(co, np.float32) / 255.0
        smdi_p = os.path.join(TEX_SRC, mat + "_smdi.png")
        self.smdi = (np.asarray(Image.open(smdi_p).convert("L"), np.float32) / 255.0
                     if os.path.exists(smdi_p) else np.full(self.co.shape[:2], 0.2))
        self.wrap = mat in TILE_MATERIALS
        self.h, self.w = self.co.shape[:2]

    def sample(self, u, v):
        H, W = self.h, self.w
        if self.wrap:
            x = np.mod(u * W - 0.5, W)
            y = np.mod((1.0 - v) * H - 0.5, H)     # v=0 — нижняя кромка картинки
        else:
            x = np.clip(u * W - 0.5, 0, W - 1)
            y = np.clip((1.0 - v) * H - 0.5, 0, H - 1)
        x0 = np.floor(x).astype(np.int64); y0 = np.floor(y).astype(np.int64)
        x1 = (x0 + 1) % W if self.wrap else np.clip(x0 + 1, 0, W - 1)
        y1 = (y0 + 1) % H if self.wrap else np.clip(y0 + 1, 0, H - 1)
        fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
        c00 = self.co[y0, x0]; c10 = self.co[y0, x1]
        c01 = self.co[y1, x0]; c11 = self.co[y1, x1]
        top = c00 * (1 - fx) + c10 * fx
        bot = c01 * (1 - fx) + c11 * fx
        col = top * (1 - fy) + bot * fy
        s00 = self.smdi[y0, x0]; s10 = self.smdi[y0, x1]
        s01 = self.smdi[y1, x0]; s11 = self.smdi[y1, x1]
        st = s00 * (1 - fx[..., 0]) + s10 * fx[..., 0]
        sb = s01 * (1 - fx[..., 0]) + s11 * fx[..., 0]
        spec = st * (1 - fy[..., 0]) + sb * fy[..., 0]
        return col, spec


# ---------------------------------------------------------------- камера
def look_at(eye, target, up=(0, 0, 1)):
    f = np.asarray(target, np.float64) - np.asarray(eye, np.float64)
    f /= np.linalg.norm(f)
    up = np.asarray(up, np.float64)
    s = np.cross(f, up); s /= np.linalg.norm(s)
    u = np.cross(s, f)
    M = np.eye(4)
    M[0, :3] = s; M[1, :3] = u; M[2, :3] = -f
    M[:3, 3] = -M[:3, :3] @ np.asarray(eye, np.float64)
    return M


def render(V, VT, tris, textures, size=(900, 640), azim=35.0, elev=26.0,
           fov=26.0, bg=(24, 26, 30)):
    W, H = size[0] * SS, size[1] * SS
    mn, mx = V.min(axis=0), V.max(axis=0)
    center = (mn + mx) / 2.0
    radius = float(np.linalg.norm(mx - mn)) / 2.0
    dist = radius / math.sin(math.radians(fov / 2.0)) * 0.82

    a = math.radians(azim); e = math.radians(elev)
    eye = center + dist * np.array([math.cos(e) * math.cos(a),
                                    math.cos(e) * math.sin(a),
                                    math.sin(e)])
    MV = look_at(eye, center)
    Vh = np.concatenate([V, np.ones((len(V), 1))], axis=1)
    Vc = (MV @ Vh.T).T[:, :3]                    # camera space, -Z вперёд
    focal = 1.0 / math.tan(math.radians(fov / 2.0))
    depth = np.clip(-Vc[:, 2], 1e-6, None)
    sx = (Vc[:, 0] * focal / (depth * (W / H)) * 0.5 + 0.5) * W
    sy = (0.5 - Vc[:, 1] * focal / depth * 0.5) * H

    img = np.zeros((H, W, 3), np.float32)
    img[:, :] = np.array(bg, np.float32) / 255.0
    zbuf = np.full((H, W), np.inf, np.float32)

    light = np.array([-0.45, 0.72, 0.55]); light /= np.linalg.norm(light)
    half_v = light + np.array([0.0, 0.0, 1.0])
    half_v /= np.linalg.norm(half_v)

    for group, mat, (ia, ib, ic) in tris:
        tex = textures.get(mat)
        if tex is None:
            continue
        p0, p1, p2 = V[ia[0]], V[ib[0]], V[ic[0]]
        n = np.cross(p1 - p0, p2 - p0)
        ln = np.linalg.norm(n)
        if ln < 1e-12:
            continue
        n = n / ln

        x0, y0, z0 = sx[ia[0]], sy[ia[0]], depth[ia[0]]
        x1, y1, z1 = sx[ib[0]], sy[ib[0]], depth[ib[0]]
        x2, y2, z2 = sx[ic[0]], sy[ic[0]], depth[ic[0]]
        minx = max(0, int(math.floor(min(x0, x1, x2))))
        maxx = min(W - 1, int(math.ceil(max(x0, x1, x2))))
        miny = max(0, int(math.floor(min(y0, y1, y2))))
        maxy = min(H - 1, int(math.ceil(max(y0, y1, y2))))
        if minx > maxx or miny > maxy:
            continue
        xs = np.arange(minx, maxx + 1, dtype=np.float32)
        ys = np.arange(miny, maxy + 1, dtype=np.float32)
        X, Y = np.meshgrid(xs + 0.5, ys + 0.5)
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-9:
            continue
        w0 = ((x1 - X) * (y2 - Y) - (x2 - X) * (y1 - Y)) / area
        w1 = ((x2 - X) * (y0 - Y) - (x0 - X) * (y2 - Y)) / area
        w2 = 1.0 - w0 - w1
        mask = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not mask.any():
            continue
        # перспективно-корректная интерполяция UV
        iz = w0 / z0 + w1 / z1 + w2 / z2
        zz = 1.0 / np.maximum(iz, 1e-9)
        u = (w0 * VT[ia[1]][0] / z0 + w1 * VT[ib[1]][0] / z1
             + w2 * VT[ic[1]][0] / z2) * zz
        v = (w0 * VT[ia[1]][1] / z0 + w1 * VT[ib[1]][1] / z1
             + w2 * VT[ic[1]][1] / z2) * zz
        sub = mask[..., None]
        col, spec = tex.sample(u * np.ones_like(X), v * np.ones_like(X))
        nd = abs(float(np.dot(n, light)))
        diff = 0.34 + 0.78 * nd
        sp = float(np.clip(np.dot(n, half_v), 0, 1)) ** 26 * 0.35 * spec
        shade = (diff + sp)[..., None]
        rgb = np.clip(col * shade, 0, 1)
        zz_f = zz.astype(np.float32)
        zi = zz_f[..., None]
        prev = zbuf[miny:maxy + 1, minx:maxx + 1]
        upd = (zz_f < prev) & mask
        upd3 = upd[..., None]
        img[miny:maxy + 1, minx:maxx + 1] = np.where(
            upd3, rgb, img[miny:maxy + 1, minx:maxx + 1])
        zbuf[miny:maxy + 1, minx:maxx + 1] = np.where(upd, zz_f, prev)

    img = np.clip(img, 0, 1)
    im = Image.fromarray((img * 255).astype(np.uint8)).resize(size, Image.LANCZOS)
    return im


CAPTION_FONTS = (
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "C:/Windows/Fonts/arialbd.ttf",
    "C:/Windows/Fonts/tahomabd.ttf",
)


def _caption_font(size):
    for p in CAPTION_FONTS:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                pass
    return ImageFont.load_default()


def add_caption(im, title, sub):
    d = ImageDraw.Draw(im)
    f = _caption_font(17)
    f2 = _caption_font(13)
    d.text((14, 12), title, font=f, fill=(236, 238, 242))
    d.text((14, 34), sub, font=f2, fill=(150, 156, 166))
    return im


MODELS = [
    ("cassette", "Кассета ue_cassette", "100.0 x 63.8 x 12.0 мм"),
    ("minidisc", "MiniDisc ue_minidisc", "68.0 x 72.0 x 5.0 мм"),
    ("memorycard", "Карта памяти ue_memorycard", "32.0 x 24.0 x 2.1 мм"),
]


def main():
    ap = argparse.ArgumentParser(description="Превью-рендер моделей Uness")
    ap.add_argument("--only", choices=[m[0] for m in MODELS])
    ap.add_argument("--azim", type=float, default=38.0)
    ap.add_argument("--elev", type=float, default=28.0)
    ap.add_argument("--album", default="", help="обложка альбома: rock/pop/...")
    args = ap.parse_args()

    os.makedirs(PREVIEW, exist_ok=True)
    cache = {}

    def tex_for(mat):
        key = mat + ("_" + args.album if args.album else "")
        if key in cache:
            return cache[key]
        p = os.path.join(TEX_SRC, key + "_co.png")
        t = Texture(key if os.path.exists(p) else mat)
        cache[key] = t
        return t

    shots = []
    for key, title, sub in MODELS:
        if args.only and key != args.only:
            continue
        obj = os.path.join(SRC, "ue_%s.obj" % key)
        V, VT, tris = load_obj(obj)
        mats = sorted({m for _, m, _ in tris})
        textures = {m: tex_for(m) for m in mats}
        im = render(V, VT, tris, textures, size=(820, 560),
                    azim=args.azim, elev=args.elev)
        add_caption(im, title, sub)
        out = os.path.join(PREVIEW, "render_ue_%s%s.png" %
                           (key, "_" + args.album if args.album else ""))
        im.save(out)
        shots.append((out, im))
        print("[ok] %s (%d треуг.)" % (out, len(tris)))

    if len(shots) > 1:
        # общий лист: все три модели в ряд, каждая со своим масштабом
        pad = 18
        cw = max(im.width for _, im in shots)
        sheet = Image.new("RGB", (len(shots) * cw + pad * (len(shots) + 1),
                                  max(im.height for _, im in shots) + pad * 2),
                          (18, 20, 24))
        for i, (_, im) in enumerate(shots):
            sheet.paste(im, (pad + i * (cw + pad), pad))
        out = os.path.join(PREVIEW, "render_all%s.png" %
                           ("_" + args.album if args.album else ""))
        sheet.save(out)
        print("[ok] общий лист: %s" % out)


if __name__ == "__main__":
    main()

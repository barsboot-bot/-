#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_textures.py — процедурные текстуры носителей мода «унесённые» (Uness, DayZ).

Никаких скачанных картинок: всё рисуется кодом (numpy + Pillow), поэтому
лицензия чистая, а результат воспроизводим (фиксированный seed).

На каждый материал получается три карты, как их ждёт DayZ:

    <mat>_co.png    цвет (diffuse)      -> в .paa как DXT1
    <mat>_nohq.png  нормали, +Y вверх   -> в .paa как DXT5
    <mat>_smdi.png  блики (specular)    -> в .paa как DXT1

Материалы:
    ue_cassette     1024x512  кассета (лицо + оборот + торцы + плёнка)
    ue_minidisc     1024x512  MD-картридж (шторка + этикетка)
    ue_memorycard    256x256  SD-карта (наклейка + контакты)
    ue_plastic       256x256  тайловый пластик (катушки, корпус карты)
    ue_metal         256x256  тайловый металл (шторка MD)
    ue_paper         256x256  тайловая бумага (этикетки)
    ue_disc          256x256  диск MD (радужные кольца)

Отдельно рисуются ОБЛОЖКИ АЛЬБОМОВ: на каждый плейлист мода — свой комплект
кассеты и MD (имя альбома напечатано прямо на этикетке).

Запуск:
    python3 tools/make_textures.py              # всё: общие листы + обложки
    python3 tools/make_textures.py --base       # только общие листы
    python3 tools/make_textures.py --albums     # только обложки альбомов
    python3 tools/make_textures.py --dry-run    # показать, ничего не писать
"""
import argparse
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_asset_lib import TEX, tex_rect_px, tex_size  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEX_SRC = os.path.join(ROOT, "Models", "tex_src")
PREVIEW = os.path.join(ROOT, "Models", "preview")

SEED = 20261112
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

# Плейлисты мода (совпадают с классами в config.cpp и папками Music/)
ALBUMS = [
    {"id": "rock",    "title": "РОК-ХИТЫ 80-х", "sub": "СБОРНИК · СТОРОНА A",
     "color": (150, 46, 40),  "minutes": 60},
    {"id": "pop",     "title": "ПОП-МУЗЫКА",    "sub": "СБОРНИК · СТОРОНА A",
     "color": (168, 84, 132), "minutes": 60},
    {"id": "classic", "title": "КЛАССИКА РОКА", "sub": "СБОРНИК · СТОРОНА A",
     "color": (44, 72, 128),  "minutes": 46},
    {"id": "dance",   "title": "ТАНЦЫ · MIX",   "sub": "СБОРНИК · СТОРОНА A",
     "color": (36, 116, 112), "minutes": 74},
]

# ==========================================================================
#  ШУМ / ГЕНЕРАЦИЯ КАРТ
# ==========================================================================
def rng_for(tag):
    h = abs(hash((SEED, tag))) % (2 ** 32)
    return np.random.default_rng(h)


def tileable_noise(shape, freq, rng, octaves=4, persistence=0.5,
                   aniso=(1.0, 1.0)):
    """Бесшовный value-noise: частоты делят размер, интерполяция — smoothstep."""
    h, w = shape
    out = np.zeros((h, w), np.float32)
    amp, total, f = 1.0, 0.0, float(freq)
    for _ in range(octaves):
        gw = max(2, int(round(f * aniso[0])))
        gh = max(2, int(round(f * aniso[1])))
        g = rng.random((gh, gw)).astype(np.float32)
        yi = (np.linspace(0, gh, h, endpoint=False))
        xi = (np.linspace(0, gw, w, endpoint=False))
        y0 = np.floor(yi).astype(np.int64); y1 = (y0 + 1) % gh
        x0 = np.floor(xi).astype(np.int64); x1 = (x0 + 1) % gw
        ty = (yi - y0).astype(np.float32); tx = (xi - x0).astype(np.float32)
        ty = ty * ty * (3 - 2 * ty); tx = tx * tx * (3 - 2 * tx)
        a = g[np.ix_(y0, x0)]; b = g[np.ix_(y0, x1)]
        c = g[np.ix_(y1, x0)]; d = g[np.ix_(y1, x1)]
        n = (a * (1 - ty)[:, None] * (1 - tx)[None, :]
             + b * (1 - ty)[:, None] * tx[None, :]
             + c * ty[:, None] * (1 - tx)[None, :]
             + d * ty[:, None] * tx[None, :])
        out += amp * n
        total += amp
        amp *= persistence
        f *= 2.0
    return (out / total).astype(np.float32)


def _conv1d(a, k, axis):
    r = len(k) // 2
    out = np.zeros_like(a)
    if axis == 1:
        ap = np.pad(a, ((0, 0), (r, r)), mode="edge")
        for i, kv in enumerate(k):
            out += kv * ap[:, i:i + a.shape[1]]
    else:
        ap = np.pad(a, ((r, r), (0, 0)), mode="edge")
        for i, kv in enumerate(k):
            out += kv * ap[i:i + a.shape[0], :]
    return out


def blurF(arr, radius):
    """Гауссово размытие float-карты (PIL не умеет F-режим, считаем сами)."""
    if radius <= 0:
        return arr
    r = max(1, int(math.ceil(radius * 3)))
    x = np.arange(-r, r + 1, dtype=np.float32)
    k = np.exp(-(x * x) / (2.0 * radius * radius))
    k /= k.sum()
    out = _conv1d(np.asarray(arr, dtype=np.float32), k, 1)
    return _conv1d(out, k, 0)


def height_to_normal(height, strength=1.8, blur=1.2):
    """Карта нормалей из высот. +Y вверх (OpenGL), как ждёт DayZ `_nohq`."""
    hb = blurF(height, blur)
    gy, gx = np.gradient(hb.astype(np.float32))
    nx = -gx * strength
    ny = gy * strength          # строки идут вниз -> для «Y вверх» берём +gy
    nz = np.ones_like(nx)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx, ny, nz = nx / l, ny / l, nz / l
    rgb = np.stack([nx, ny, nz], axis=-1) * 0.5 + 0.5
    return (np.clip(rgb, 0, 1) * 255).astype(np.uint8)


def specular_map(co_arr, shiny, blur=1.0):
    """Блики: светлое и гладкое блестит сильнее."""
    lum = (0.299 * co_arr[..., 0] + 0.587 * co_arr[..., 1]
           + 0.114 * co_arr[..., 2]) / 255.0
    lum = blurF(lum.astype(np.float32), blur)
    s = np.clip(0.20 + 0.80 * lum, 0, 1) * np.clip(shiny, 0, 1)
    s = np.clip(s, 0, 1)
    arr = (s * 255).astype(np.uint8)
    return np.stack([arr, arr, arr], axis=-1)


# ==========================================================================
#  ХОЛСТ МАТЕРИАЛА
# ==========================================================================
class Material:
    """Холст: цвет (PIL) + высоты + «гладкость» для бликов."""

    def __init__(self, name, base_rgb, base_shiny=0.20):
        self.name = name
        W, H = tex_size(name)
        self.W, self.H = W, H
        self.img = Image.new("RGB", (W, H), tuple(int(c) for c in base_rgb))
        self.height = np.zeros((H, W), np.float32)
        self.shiny = np.full((H, W), base_shiny, np.float32)

    def paste(self, rect_name, sub_img, sub_h=None, sub_s=None):
        x0, y0, x1, y1 = tex_rect_px(self.name, rect_name)
        w, h = x1 - x0, y1 - y0
        if sub_img.size != (w, h):
            sub_img = sub_img.resize((w, h), Image.LANCZOS)
        self.img.paste(sub_img, (x0, y0))
        if sub_h is not None:
            sh = np.asarray(sub_h, dtype=np.float32)
            if sh.shape != (h, w):
                sh = np.asarray(Image.fromarray(sh, mode="F").resize((w, h), Image.BILINEAR))
            self.height[y0:y1, x0:x1] = sh
        if sub_s is not None:
            ss = np.asarray(sub_s, dtype=np.float32)
            if ss.shape != (h, w):
                ss = np.asarray(Image.fromarray(ss, mode="F").resize((w, h), Image.BILINEAR))
            self.shiny[y0:y1, x0:x1] = ss

    def save(self, out_dir, suffix="", dry=False):
        name = self.name + suffix
        co = np.asarray(self.img, dtype=np.uint8)
        nohq = height_to_normal(self.height, strength=1.8, blur=1.2)
        smdi = specular_map(co, self.shiny, blur=1.0)
        files = []
        for suf, arr in (("_co", co), ("_nohq", nohq), ("_smdi", smdi)):
            p = os.path.join(out_dir, "%s%s.png" % (name, suf))
            files.append(os.path.basename(p))
            if not dry:
                Image.fromarray(arr, "RGB").save(p, optimize=True)
        return files


# ==========================================================================
#  РИСОВАЛКИ
# ==========================================================================
class Xform:
    """миллиметры (центр в нуле, Y вверх) -> пиксели региона"""

    def __init__(self, w_mm, h_mm, w_px, h_px):
        self.sx = w_px / float(w_mm)
        self.sy = h_px / float(h_mm)
        self.w_mm, self.h_mm = w_mm, h_mm
        self.w_px, self.h_px = w_px, h_px

    def p(self, x, y):
        return (self.sx * (x + self.w_mm / 2.0), self.sy * (self.h_mm / 2.0 - y))

    def box(self, x0, x1, y0, y1):
        a = self.p(x0, y1)
        b = self.p(x1, y0)
        return [a[0], a[1], b[0], b[1]]

    def s(self, v):
        return v * self.sx


def ibox(box):
    """Целочисленный прямоугольник: срезы numpy не принимают float."""
    return [int(round(v)) for v in box]


def font(size, bold=False):
    try:
        return ImageFont.truetype(FONT_BOLD if bold else FONT_REG, int(size))
    except Exception:
        return ImageFont.load_default()


def fit_text(draw, text, max_w, max_h, bold=False, start=64, min_size=7):
    """Подобрать шрифт, чтобы строка влезла в прямоугольник."""
    size = start
    while size > min_size:
        f = font(size, bold)
        bbox = draw.textbbox((0, 0), text, font=f)
        if (bbox[2] - bbox[0]) <= max_w and (bbox[3] - bbox[1]) <= max_h:
            return f, (bbox[2] - bbox[0], bbox[3] - bbox[1])
        size -= 2
    return font(min_size, bold), draw.textbbox((0, 0), text, font=font(min_size, bold))[2:]


def wrap_text(draw, text, max_w, bold=False, size=30):
    """Перенос по словам, чтобы строка влезла по ширине."""
    f = font(size, bold)
    words, lines, cur = text.split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if draw.textlength(t, font=f) <= max_w or not cur:
            cur = t
        else:
            lines.append(cur)
            cur = w
    if cur:
        lines.append(cur)
    return lines, f


def grain(h, w, rng, amp=10.0, freq=64.0, octaves=3):
    return tileable_noise((h, w), freq, rng, octaves) * amp


def plastic_base(w, h, rgb, rng, amp=8.0, freq=48.0, speckle=0.0):
    """Пластиковая подложка: шум + мелкие вкрапления."""
    n = grain(h, w, rng, amp=amp, freq=freq)
    arr = np.zeros((h, w, 3), np.float32)
    for c in range(3):
        arr[..., c] = rgb[c]
    out = np.clip(arr + n[..., None], 0, 255)
    if speckle > 0:
        sp = rng.random((h, w))
        m = (sp > 1 - speckle).astype(np.float32)
        out += (m * rng.uniform(-14, 14, (h, w)))[..., None].astype(np.float32)
    return out.astype(np.uint8)


def bevel_mask(h, w, px, soft=True):
    """Маска фаски по краю: 1 у кромки, 0 внутри. px — толщина в пикселях."""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.minimum(np.minimum(yy, h - 1 - yy), np.minimum(xx, w - 1 - xx))
    m = np.clip(1.0 - d / max(1.0, px), 0, 1)
    if soft:
        m = m ** 1.5
    return m


def add_screw(draw, xf, x_mm, y_mm, r_mm=1.5):
    x, y = xf.p(x_mm, y_mm)
    r = xf.s(r_mm)
    draw.ellipse([x - r, y - r, x + r, y + r], fill=(120, 122, 126),
                 outline=(74, 76, 80))
    draw.ellipse([x - r * 0.75, y - r * 0.75, x + r * 0.3, y + r * 0.3],
                 fill=(160, 162, 166))
    draw.line([x - r * 0.6, y - r * 0.2, x + r * 0.6, y + r * 0.2],
              fill=(58, 60, 64), width=max(1, int(r * 0.35)))


def scratches(img, rng, n=26, alpha=26):
    """Царапинки: поверхностный слой, как у б/у вещи."""
    d = ImageDraw.Draw(img, "RGBA")
    W, H = img.size
    for _ in range(n):
        x = rng.uniform(0, W); y = rng.uniform(0, H)
        ln = rng.uniform(8, 90); a = rng.uniform(0, math.pi)
        x2 = x + math.cos(a) * ln; y2 = y + math.sin(a) * ln
        v = int(rng.uniform(-alpha, alpha))
        c = (255, 255, 255, abs(v)) if v > 0 else (0, 0, 0, abs(v))
        d.line([x, y, x2, y2], fill=c, width=1)
    return img


def grime(img, rng, amount=0.35, blobs=18):
    """Пятна грязи/пыли по краям — вещь из постапокалипсиса."""
    W, H = img.size
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for _ in range(blobs):
        x = rng.uniform(0, W); y = rng.uniform(0, H)
        r = rng.uniform(W * 0.04, W * 0.22)
        a = int(rng.uniform(6, 34) * amount)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(30, 26, 22, a))
    return Image.alpha_composite(img.convert("RGBA"), layer).convert("RGB")


# --------------------------------------------------------------------------
#  КАССЕТА: лицевая сторона
# --------------------------------------------------------------------------
def cassette_top(album=None, w=512, h=328):
    rng = rng_for("cassette_top" + (album["id"] if album else ""))
    xf = Xform(100.0, 63.8, w, h)
    base = plastic_base(w, h, (52, 54, 58), rng, amp=9.0, freq=40.0, speckle=0.004)
    img = Image.fromarray(base)
    d = ImageDraw.Draw(img, "RGBA")

    height = np.zeros((h, w), np.float32)
    shiny = np.full((h, w), 0.22, np.float32)

    # фаска корпуса (свет сверху-слева, тень снизу-справа)
    bm = bevel_mask(h, w, xf.s(2.2))
    arr = np.asarray(img).astype(np.float32)
    arr[..., 0] += bm * 26; arr[..., 1] += bm * 26; arr[..., 2] += bm * 28
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img, "RGBA")
    height -= bm * 0.55

    # окно (в нём видны катушки; здесь — тёмное стекло с бликом)
    win = (-33.0, -14.0, 33.0, 12.0)          # (x0, y0, x1, y1) в мм
    wb = xf.box(win[0], win[2], win[1], win[3])
    _w = ibox(wb)
    d.rectangle(wb, fill=(30, 33, 38))
    d.rectangle([wb[0] + 1, wb[1] + 1, wb[2] - 1, wb[3] - 1],
                outline=(18, 19, 22), width=2)
    # блик по диагонали
    d.polygon([xf.p(-33.0, 12.0), xf.p(-18.0, 12.0), xf.p(6.0, -14.0),
               xf.p(-9.0, -14.0)], fill=(96, 104, 116, 70))
    d.polygon([xf.p(12.0, 12.0), xf.p(20.0, 12.0), xf.p(44.0, -14.0),
               xf.p(36.0, -14.0)], fill=(96, 104, 116, 40))
    # рамка окна: тень сверху, свет снизу (углубление)
    d.rectangle(wb, outline=(96, 100, 108), width=1)
    d.line([xf.p(-33.0, 12.0), xf.p(33.0, 12.0)], fill=(128, 132, 140), width=2)
    height[_w[1]:_w[3], _w[0]:_w[2]] -= 0.5
    shiny[_w[1]:_w[3], _w[0]:_w[2]] = 0.62

    # метки-насечки под окном
    for i, xx in enumerate(range(-30, 31, 10)):
        d.line([xf.p(xx, -16.0), xf.p(xx, -17.6)], fill=(120, 124, 130), width=1)

    # проёмы лентопротяжного механизма по нижней кромке
    for x0, x1 in ((-26.0, -18.0), (-9.0, 9.0), (18.0, 26.0)):
        d.rectangle(xf.box(x0, x1, -30.5, -23.0), fill=(24, 25, 28))
        d.line([xf.p(x0, -23.0), xf.p(x1, -23.0)], fill=(118, 122, 128), width=1)
        _b = ibox(xf.box(x0, x1, -30.5, -23.0))
        height[_b[1]:_b[3], _b[0]:_b[2]] -= 0.6

    # ---- этикетка ----
    lab = (-44.0, 13.5, 44.0, 30.5)
    lb = xf.box(lab[0], lab[2], lab[1], lab[3])
    _i = ibox(lb)
    d.rectangle([lb[0] + 2, lb[1] + 3, lb[2] + 2, lb[3] + 3], fill=(18, 18, 20, 90))
    d.rectangle(lb, fill=(232, 226, 208))
    d.rectangle(lb, outline=(150, 144, 128), width=1)
    # линейки
    for yy in (26.6, 22.4, 18.2):
        d.line([xf.p(-41.0, yy), xf.p(41.0, yy)], fill=(176, 186, 206), width=1)

    # цветная полоса альбома
    col = album["color"] if album else (86, 92, 104)
    d.rectangle(xf.box(-44.0, 44.0, 27.6, 30.5), fill=col)

    # SIDE A
    d.rectangle(xf.box(-43.0, -31.0, 14.6, 19.4), fill=(248, 248, 244),
                outline=(120, 116, 104), width=1)
    tf, _ = fit_text(d, "SIDE A", xf.s(11.0), xf.s(4.0), bold=True, start=30)
    d.text(xf.p(-42.0, 18.6), "SIDE A", font=tf, fill=(40, 40, 44))

    # название альбома
    title = album["title"] if album else "MIXTAPE"
    sub = album["sub"] if album else "СБОРНИК · СТОРОНА A"
    lines, f = wrap_text(d, title, xf.s(78.0), bold=True, size=26)
    sizes = [26, 22, 18, 15]
    fs = font(26, True)
    for i in range(len(lines), 0, -1):
        cand = wrap_text(d, title, xf.s(78.0), bold=True, size=sizes[min(i, 3)])
        if len(cand[0]) <= 2:
            lines, fs = cand[0], font(sizes[min(i, 3)], True)
            break
    yy = 26.0
    for ln in lines[:2]:
        d.text(xf.p(-30.0, yy), ln, font=fs, fill=(34, 34, 38))
        yy -= (fs.size + 3) / xf.sy          # пиксели -> миллиметры
    # подзаголовок
    sf, _ = fit_text(d, sub, xf.s(70.0), xf.s(3.4), start=20)
    d.text(xf.p(-30.0, 15.2), sub, font=sf, fill=(104, 100, 92))

    # длительность
    mins = album["minutes"] if album else 60
    tf2, _ = fit_text(d, "TYPE I · %d MIN" % mins, xf.s(30.0), xf.s(3.2), start=18)
    d.text(xf.p(12.0, 15.4), "TYPE I · %d MIN" % mins, font=tf2, fill=(72, 70, 64))

    height[_i[1]:_i[3], _i[0]:_i[2]] += 0.10
    shiny[_i[1]:_i[3], _i[0]:_i[2]] = 0.10

    # шурупы
    for sx in (-1, 1):
        for sy in (-1, 1):
            add_screw(d, xf, sx * 46.0, sy * 27.0)
    add_screw(d, xf, 0.0, -27.5)

    scratches(img, rng, n=34, alpha=22)
    img = grime(img, rng, amount=0.5)
    return img, height, shiny


def cassette_bottom(album=None, w=512, h=328):
    rng = rng_for("cassette_bottom")
    xf = Xform(100.0, 63.8, w, h)
    base = plastic_base(w, h, (48, 50, 53), rng, amp=8.0, freq=44.0, speckle=0.004)
    img = Image.fromarray(base)
    d = ImageDraw.Draw(img, "RGBA")
    height = np.zeros((h, w), np.float32)
    shiny = np.full((h, w), 0.20, np.float32)

    bm = bevel_mask(h, w, xf.s(2.0))
    arr = np.asarray(img).astype(np.float32)
    arr += (bm * 22)[..., None]
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img, "RGBA")
    height -= bm * 0.5

    # два отверстия под шпиндели с зубьями
    for cx in (-21.2, 21.2):
        x, y = xf.p(cx, -1.0)
        r = xf.s(6.6)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(28, 29, 32),
                  outline=(112, 116, 122), width=2)
        for i in range(12):
            a = 2 * math.pi * i / 12
            d.line([x + math.cos(a) * r * 0.45, y + math.sin(a) * r * 0.45,
                    x + math.cos(a) * r * 0.92, y + math.sin(a) * r * 0.92],
                   fill=(70, 72, 76), width=2)
        height[int(y - r):int(y + r), int(x - r):int(x + r)] -= 0.25

    # окна лентопротяга
    for x0, x1 in ((-30.0, -22.0), (-9.0, 9.0), (22.0, 30.0)):
        d.rectangle(xf.box(x0, x1, -30.0, -23.5), fill=(22, 23, 26))
        d.line([xf.p(x0, -23.5), xf.p(x1, -23.5)], fill=(116, 120, 126), width=1)

    # служебная печать
    tf, _ = fit_text(d, "SIDE B", xf.s(20.0), xf.s(4.4), bold=True, start=26)
    d.text(xf.p(-43.0, 27.4), "SIDE B", font=tf, fill=(126, 130, 136))
    sf, _ = fit_text(d, "TYPE I · NORMAL BIAS · 120 µs EQ", xf.s(66.0), xf.s(3.0), start=17)
    d.text(xf.p(-43.0, 21.6), "TYPE I · NORMAL BIAS · 120 µs EQ", font=sf,
           fill=(96, 100, 108))
    sf2, _ = fit_text(d, "LOW NOISE / HIGH OUTPUT", xf.s(50.0), xf.s(3.0), start=16)
    d.text(xf.p(-43.0, 17.8), "LOW NOISE / HIGH OUTPUT", font=sf2, fill=(88, 92, 100))
    if album:
        sf3, _ = fit_text(d, album["title"], xf.s(60.0), xf.s(4.0), start=22)
        d.text(xf.p(-43.0, 11.6), album["title"], font=sf3, fill=(112, 116, 124))

    for sx in (-1, 1):
        for sy in (-1, 1):
            add_screw(d, xf, sx * 46.0, sy * 27.0)
    add_screw(d, xf, 0.0, -27.5)

    scratches(img, rng, n=28, alpha=20)
    img = grime(img, rng, amount=0.55)
    return img, height, shiny


def cassette_side_long(w=512, h=61):
    rng = rng_for("cassette_side_long")
    img = Image.fromarray(plastic_base(w, h, (44, 46, 49), rng, amp=7.0, freq=8.0))
    d = ImageDraw.Draw(img, "RGBA")
    d.line([(0, h // 2), (w, h // 2)], fill=(86, 88, 92), width=1)
    d.line([(0, h // 2 + 1), (w, h // 2 + 1)], fill=(26, 27, 30), width=1)
    height = np.zeros((h, w), np.float32)
    height[h // 2 - 1:h // 2 + 1, :] -= 0.3
    shiny = np.full((h, w), 0.18, np.float32)
    return img, height, shiny


def cassette_side_short(w=327, h=61):
    rng = rng_for("cassette_side_short")
    img = Image.fromarray(plastic_base(w, h, (44, 46, 49), rng, amp=7.0, freq=8.0))
    d = ImageDraw.Draw(img, "RGBA")
    for x in range(4, w - 3, 6):
        d.line([(x, 6), (x, h - 7)], fill=(30, 31, 34), width=2)
        d.line([(x + 2, 6), (x + 2, h - 7)], fill=(72, 74, 78), width=1)
    height = np.zeros((h, w), np.float32)
    for x in range(4, w - 3, 6):
        height[6:h - 7, x:x + 2] -= 0.35
    shiny = np.full((h, w), 0.18, np.float32)
    return img, height, shiny


def cassette_tape(w=80, h=70):
    rng = rng_for("cassette_tape")
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    ang = np.arctan2(yy - h / 2, xx - w / 2)
    n = tileable_noise((h, w), 12.0, rng, 3, aniso=(3.0, 1.0))
    v = 0.5 + 0.5 * np.sin(ang * 46.0) * 0.16 + (n - 0.5) * 0.22
    arr = np.zeros((h, w, 3), np.float32)
    arr[..., 0] = 46 + v * 34
    arr[..., 1] = 30 + v * 22
    arr[..., 2] = 22 + v * 16
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    height = (n - 0.5) * 0.25
    shiny = np.full((h, w), 0.35, np.float32)
    return img, height, shiny


# --------------------------------------------------------------------------
#  MINIDISC
# --------------------------------------------------------------------------
def minidisc_top(album=None, w=448, h=474):
    rng = rng_for("minidisc_top" + (album["id"] if album else ""))
    xf = Xform(68.0, 72.0, w, h)
    base = plastic_base(w, h, (176, 180, 184), rng, amp=7.0, freq=44.0, speckle=0.002)
    img = Image.fromarray(base)
    d = ImageDraw.Draw(img, "RGBA")
    height = np.zeros((h, w), np.float32)
    shiny = np.full((h, w), 0.26, np.float32)

    bm = bevel_mask(h, w, xf.s(2.4))
    arr = np.asarray(img).astype(np.float32)
    arr += (bm * 24)[..., None]
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img, "RGBA")
    height -= bm * 0.5

    # зона шторки: углубление под металлической шторкой и окном
    d.rectangle(xf.box(-26.0, 26.0, -22.0, 13.0), fill=(150, 154, 159))
    d.line([xf.p(-26.0, 13.0), xf.p(26.0, 13.0)], fill=(206, 210, 214), width=2)
    _s = ibox(xf.box(-26.0, 26.0, -22.0, 13.0))
    height[_s[1]:_s[3], _s[0]:_s[2]] -= 0.35
    shiny[_s[1]:_s[3], _s[0]:_s[2]] = 0.30

    # окно чтения (совпадает с вырезом в модели: -21..21 x -17..3 мм)
    d.rectangle(xf.box(-21.0, 21.0, -17.0, 3.0), fill=(38, 42, 46))
    d.rectangle(xf.box(-21.0, 21.0, -17.0, 3.0), outline=(120, 124, 130), width=1)
    d.polygon([xf.p(-21.0, -12.0), xf.p(-7.0, -17.0), xf.p(9.0, 3.0),
               xf.p(-5.0, 3.0)], fill=(220, 226, 232, 40))
    _o = ibox(xf.box(-21.0, 21.0, -17.0, 3.0))
    height[_o[1]:_o[3], _o[0]:_o[2]] -= 0.5

    # направляющие шторки (пазы по бокам)
    for sx in (-1, 1):
        d.rectangle(xf.box(sx * 26.0 - 1.2, sx * 26.0 + 1.2, -32.0, 13.0),
                    fill=(128, 132, 138))

    # стрелка вставки у нижней кромки
    d.polygon([xf.p(-2.2, -25.0), xf.p(2.2, -25.0), xf.p(0.0, -30.0)],
              fill=(96, 100, 106))

    # ---- этикетка ----
    lab = (-30.0, 13.0, 30.0, 33.0)
    lb = xf.box(lab[0], lab[2], lab[1], lab[3])
    _i = ibox(lb)
    d.rectangle([lb[0] + 1, lb[1] + 2, lb[2] + 1, lb[3] + 2], fill=(60, 62, 66, 90))
    d.rectangle(lb, fill=(238, 240, 243))
    d.rectangle(lb, outline=(160, 164, 170), width=1)
    col = album["color"] if album else (60, 74, 96)
    d.rectangle(xf.box(-30.0, 30.0, 30.0, 33.0), fill=col)
    tf, _ = fit_text(d, "MD", xf.s(11.0), xf.s(2.6), bold=True, start=26)
    d.text(xf.p(-28.4, 30.4), "MD", font=tf, fill=(246, 248, 250))

    title = album["title"] if album else "MINIDISC"
    lines, fs = wrap_text(d, title, xf.s(54.0), bold=True, size=26)
    yy = 27.0
    for ln in lines[:2]:
        d.text(xf.p(-27.0, yy), ln, font=fs, fill=(36, 38, 42))
        yy -= (fs.size + 4) / xf.sy
    sf, _ = fit_text(d, album["sub"] if album else "СБОРНИК · СТОРОНА A",
                     xf.s(54.0), xf.s(2.6), start=17)
    d.text(xf.p(-27.0, 19.4), album["sub"] if album else "СБОРНИК · СТОРОНА A",
           font=sf, fill=(112, 116, 124))
    mins = album["minutes"] if album else 74
    sf2, _ = fit_text(d, "RECORDABLE · %d MIN" % mins, xf.s(50.0), xf.s(2.4), start=16)
    d.text(xf.p(-27.0, 15.4), "RECORDABLE · %d MIN" % mins, font=sf2, fill=(96, 100, 108))

    height[_i[1]:_i[3], _i[0]:_i[2]] += 0.12
    shiny[_i[1]:_i[3], _i[0]:_i[2]] = 0.12

    add_screw(d, xf, -30.5, -32.5, r_mm=0.9)
    add_screw(d, xf, 30.5, -32.5, r_mm=0.9)
    add_screw(d, xf, -30.5, 32.5, r_mm=0.9)
    add_screw(d, xf, 30.5, 32.5, r_mm=0.9)

    scratches(img, rng, n=22, alpha=18)
    img = grime(img, rng, amount=0.35)
    return img, height, shiny


def minidisc_bottom(album=None, w=448, h=474):
    rng = rng_for("minidisc_bottom")
    xf = Xform(68.0, 72.0, w, h)
    base = plastic_base(w, h, (172, 176, 180), rng, amp=7.0, freq=44.0, speckle=0.002)
    img = Image.fromarray(base)
    d = ImageDraw.Draw(img, "RGBA")
    height = np.zeros((h, w), np.float32)
    shiny = np.full((h, w), 0.24, np.float32)

    bm = bevel_mask(h, w, xf.s(2.4))
    arr = np.asarray(img).astype(np.float32)
    arr += (bm * 24)[..., None]
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img, "RGBA")
    height -= bm * 0.5

    # большая бумажная этикетка
    lab = (-32.0, -33.0, 32.0, 32.0)
    lb = xf.box(lab[0], lab[2], lab[1], lab[3])
    _i = ibox(lb)
    d.rectangle(lb, fill=(242, 243, 245))
    d.rectangle(lb, outline=(168, 172, 178), width=1)
    col = album["color"] if album else (60, 74, 96)
    d.rectangle(xf.box(-32.0, 32.0, 25.0, 32.0), fill=col)

    title = album["title"] if album else "MINIDISC"
    lines, fs = wrap_text(d, title, xf.s(58.0), bold=True, size=28)
    yy = 22.0
    for ln in lines[:3]:
        f2, _ = fit_text(d, ln, xf.s(58.0), xf.s(6.0), bold=True, start=28)
        d.text(xf.p(-28.0, yy), ln, font=f2, fill=(34, 36, 40))
        yy -= (f2.size + 5) / xf.sy
    if album:
        sf, _ = fit_text(d, album["sub"], xf.s(58.0), xf.s(3.4), start=18)
        d.text(xf.p(-28.0, yy), album["sub"], font=sf, fill=(110, 114, 122))
        yy -= (sf.size + 4) / xf.sy
    # «список треков» — напечатанные строчки
    for i in range(4):
        y = yy - i * 9
        d.line([xf.p(-28.0, y), xf.p(14.0, y)], fill=(206, 210, 216), width=1)
    sf2, _ = fit_text(d, "MDW-74 · %s MIN" % (album["minutes"] if album else 74),
                      xf.s(40.0), xf.s(3.2), start=17)
    d.text(xf.p(-28.0, -28.0), "MDW-74 · %s MIN" % (album["minutes"] if album else 74),
           font=sf2, fill=(104, 108, 116))

    # отверстия позиционирования
    for cx, cy in ((-29.0, -33.5), (29.0, -33.5)):
        x, y = xf.p(cx, cy)
        r = xf.s(1.6)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(120, 124, 130),
                  outline=(90, 94, 100))

    height[_i[1]:_i[3], _i[0]:_i[2]] += 0.10
    shiny[_i[1]:_i[3], _i[0]:_i[2]] = 0.10

    scratches(img, rng, n=18, alpha=16)
    img = grime(img, rng, amount=0.3)
    return img, height, shiny


def minidisc_side(w=512, h=28):
    rng = rng_for("minidisc_side")
    img = Image.fromarray(plastic_base(w, h, (158, 162, 167), rng, amp=6.0, freq=6.0))
    d = ImageDraw.Draw(img, "RGBA")
    d.line([(0, h // 2), (w, h // 2)], fill=(196, 200, 205), width=1)
    d.line([(0, h // 2 + 1), (w, h // 2 + 1)], fill=(120, 124, 130), width=1)
    height = np.zeros((h, w), np.float32)
    height[h // 2 - 1:h // 2 + 1, :] -= 0.28
    shiny = np.full((h, w), 0.22, np.float32)
    return img, height, shiny


# --------------------------------------------------------------------------
#  КАРТА ПАМЯТИ
# --------------------------------------------------------------------------
def memorycard_label(w=256, h=192):
    rng = rng_for("memorycard_label")
    xf = Xform(32.0, 24.0, w, h)
    base = plastic_base(w, h, (48, 52, 57), rng, amp=6.0, freq=30.0, speckle=0.003)
    img = Image.fromarray(base)
    d = ImageDraw.Draw(img, "RGBA")
    height = np.zeros((h, w), np.float32)
    shiny = np.full((h, w), 0.30, np.float32)

    bm = bevel_mask(h, w, xf.s(1.2))
    arr = np.asarray(img).astype(np.float32)
    arr += (bm * 22)[..., None]
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img, "RGBA")
    height -= bm * 0.4

    # наклейка
    lab = (-14.0, -9.0, 13.0, 9.0)
    lb = xf.box(lab[0], lab[2], lab[1], lab[3])
    _i = ibox(lb)
    d.rectangle([lb[0] + 1, lb[1] + 1, lb[2] + 1, lb[3] + 1], fill=(20, 22, 24, 110))
    d.rectangle(lb, fill=(232, 226, 210))
    d.rectangle(lb, outline=(150, 146, 134), width=1)
    d.rectangle(xf.box(-14.0, 13.0, 5.4, 9.0), fill=(38, 96, 150))

    tf, _ = fit_text(d, "8 GB", xf.s(12.0), xf.s(6.4), bold=True, start=44)
    d.text(xf.p(-12.6, 6.2), "8 GB", font=tf, fill=(28, 30, 34))
    sf, _ = fit_text(d, "MUSICRADIO", xf.s(22.0), xf.s(3.0), start=22)
    d.text(xf.p(-12.6, 0.6), "MUSICRADIO", font=sf, fill=(86, 92, 104))
    sf2, _ = fit_text(d, "SD · CLASS 4", xf.s(20.0), xf.s(2.6), start=16)
    d.text(xf.p(-12.6, -3.0), "SD · CLASS 4", font=sf2, fill=(118, 122, 130))
    # логотип-плашка SD
    d.rectangle(xf.box(8.0, 12.4, -8.0, -1.0), fill=(60, 66, 76))
    lf, _ = fit_text(d, "SD", xf.s(3.4), xf.s(3.0), bold=True, start=18)
    d.text(xf.p(8.8, -6.6), "SD", font=lf, fill=(236, 238, 240))

    height[_i[1]:_i[3], _i[0]:_i[2]] += 0.10
    shiny[_i[1]:_i[3], _i[0]:_i[2]] = 0.12

    # метки ёмкости по краю корпуса
    for i in range(3):
        d.rectangle(xf.box(-15.2 + i * 1.6, -14.6 + i * 1.6, 6.0, 9.4),
                    fill=(120, 124, 130))

    scratches(img, rng, n=10, alpha=18)
    return img, height, shiny


def memorycard_contacts(w=256, h=36):
    rng = rng_for("memorycard_contacts")
    img = Image.fromarray(plastic_base(w, h, (44, 46, 50), rng, amp=5.0, freq=8.0))
    d = ImageDraw.Draw(img, "RGBA")
    height = np.zeros((h, w), np.float32)
    shiny = np.full((h, w), 0.20, np.float32)
    pad_w = w / 9.0
    for i in range(9):
        x0 = i * pad_w + pad_w * 0.10
        x1 = (i + 1) * pad_w - pad_w * 0.10
        for k in range(h):
            t = k / max(1, h - 1)
            v = 0.72 + 0.28 * (1 - abs(t - 0.35) * 1.6)
            r = int(np.clip(232 * v, 0, 255))
            g = int(np.clip(190 * v, 0, 255))
            b = int(np.clip(96 * v, 0, 255))
            d.line([(x0, k), (x1, k)], fill=(r, g, b))
        height[2:h - 2, int(x0):int(x1)] += 0.18
        shiny[2:h - 2, int(x0):int(x1)] = 0.85
    return img, height, shiny


def memorycard_side(w=256, h=16):
    rng = rng_for("memorycard_side")
    img = Image.fromarray(plastic_base(w, h, (42, 44, 48), rng, amp=5.0, freq=5.0))
    d = ImageDraw.Draw(img, "RGBA")
    d.line([(0, h // 2), (w, h // 2)], fill=(84, 88, 94), width=1)
    height = np.zeros((h, w), np.float32)
    height[h // 2 - 1:h // 2 + 1, :] -= 0.25
    shiny = np.full((h, w), 0.24, np.float32)
    return img, height, shiny


# --------------------------------------------------------------------------
#  ТАЙЛЫ
# --------------------------------------------------------------------------
def tile_plastic(w=256, h=256):
    rng = rng_for("tile_plastic")
    n = tileable_noise((h, w), 32.0, rng, 4, 0.55)
    fine = tileable_noise((h, w), 128.0, rng, 2, 0.5)
    v = 0.55 + (n - 0.5) * 0.22 + (fine - 0.5) * 0.10
    arr = np.zeros((h, w, 3), np.float32)
    arr[..., 0] = 58 * v * 1.6
    arr[..., 1] = 60 * v * 1.6
    arr[..., 2] = 64 * v * 1.6
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    height = (n - 0.5) * 0.25 + (fine - 0.5) * 0.18
    shiny = np.full((h, w), 0.26, np.float32) + (n - 0.5) * 0.06
    return img, height, shiny


def tile_metal(w=256, h=256):
    rng = rng_for("tile_metal")
    # анизотропный шум: вытянутые вдоль X полосы = шлифовка
    streak = tileable_noise((h, w), 4.0, rng, 3, 0.55, aniso=(16.0, 1.0))
    fine = tileable_noise((h, w), 64.0, rng, 2, 0.5, aniso=(8.0, 1.0))
    v = 0.5 + (streak - 0.5) * 0.55 + (fine - 0.5) * 0.14
    arr = np.zeros((h, w, 3), np.float32)
    arr[..., 0] = 150 + v * 70
    arr[..., 1] = 154 + v * 70
    arr[..., 2] = 160 + v * 72
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    height = (streak - 0.5) * 0.30 + (fine - 0.5) * 0.10
    shiny = np.full((h, w), 0.80, np.float32) + (streak - 0.5) * 0.12
    return img, height, shiny


def tile_paper(w=256, h=256):
    rng = rng_for("tile_paper")
    n = tileable_noise((h, w), 96.0, rng, 3, 0.5)
    blotch = tileable_noise((h, w), 12.0, rng, 3, 0.6)
    v = 0.5 + (n - 0.5) * 0.16 + (blotch - 0.5) * 0.10
    arr = np.zeros((h, w, 3), np.float32)
    arr[..., 0] = 216 + v * 30
    arr[..., 1] = 210 + v * 30
    arr[..., 2] = 192 + v * 32
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    height = (n - 0.5) * 0.16
    shiny = np.full((h, w), 0.06, np.float32)
    return img, height, shiny


def tile_disc(w=256, h=256):
    """Диск MiniDisc: радужные концентрические кольца."""
    rng = rng_for("tile_disc")
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cx = cy = w / 2.0
    r = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / (w / 2.0)
    ang = np.arctan2(yy - cy, xx - cx)
    rings = 0.5 + 0.5 * np.sin(r * 92.0 + np.sin(ang * 5.0) * 1.6)
    swirl = tileable_noise((h, w), 10.0, rng, 3, 0.5, aniso=(2.0, 1.0))
    t = np.clip(rings * 0.7 + swirl * 0.3, 0, 1)
    arr = np.zeros((h, w, 3), np.float32)
    # палитра: золото -> маджента -> зелёный
    arr[..., 0] = 214 * t + 60
    arr[..., 1] = 168 * (1 - np.abs(t - 0.5) * 2.0) + 70
    arr[..., 2] = 120 * (1 - t) + 70
    inside = (r < 1.0).astype(np.float32)
    arr *= inside[..., None]
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    height = (t - 0.5) * 0.10
    shiny = np.full((h, w), 0.92, np.float32) * inside
    return img, height, shiny


# ==========================================================================
#  СБОРКА МАТЕРИАЛОВ
# ==========================================================================
def build_cassette_mat(album=None, dry=False, out=None):
    m = Material("ue_cassette", (52, 54, 58), 0.20)
    img, hgt, shn = cassette_top(album)
    m.paste("top", img, hgt, shn)
    img, hgt, shn = cassette_bottom(album)
    m.paste("bottom", img, hgt, shn)
    img, hgt, shn = cassette_side_long()
    m.paste("side_long", img, hgt, shn)
    img, hgt, shn = cassette_side_short()
    m.paste("side_short", img, hgt, shn)
    img, hgt, shn = cassette_tape()
    m.paste("tape", img, hgt, shn)
    return m.save(out or TEX_SRC, "_" + album["id"] if album else "", dry)


def build_minidisc_mat(album=None, dry=False, out=None):
    m = Material("ue_minidisc", (176, 180, 184), 0.24)
    img, hgt, shn = minidisc_top(album)
    m.paste("top", img, hgt, shn)
    img, hgt, shn = minidisc_bottom(album)
    m.paste("bottom", img, hgt, shn)
    img, hgt, shn = minidisc_side()
    m.paste("side", img, hgt, shn)
    return m.save(out or TEX_SRC, "_" + album["id"] if album else "", dry)


def build_memorycard_mat(dry=False, out=None):
    m = Material("ue_memorycard", (48, 52, 57), 0.28)
    img, hgt, shn = memorycard_label()
    m.paste("label", img, hgt, shn)
    img, hgt, shn = memorycard_contacts()
    m.paste("contacts", img, hgt, shn)
    img, hgt, shn = memorycard_side()
    m.paste("side", img, hgt, shn)
    return m.save(out or TEX_SRC, "", dry)


def build_tile(name, fn, dry=False, out=None):
    m = Material(name, (128, 128, 128), 0.2)
    img, hgt, shn = fn()
    m.paste("tile", img, hgt, shn)
    m.img = img
    m.height = hgt
    m.shiny = shn
    return m.save(out or TEX_SRC, "", dry)


# ==========================================================================
#  КОНТАКТНЫЙ ЛИСТ
# ==========================================================================
def contact_sheet(path, names):
    cols = 4
    cell = 260
    rows = int(math.ceil(len(names) / cols))
    W = cols * cell
    H = rows * (cell + 34)
    sheet = Image.new("RGB", (W, H), (26, 28, 32))
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(names):
        p = os.path.join(TEX_SRC, n + "_co.png")
        if not os.path.exists(p):
            continue
        im = Image.open(p)
        im.thumbnail((cell - 16, cell - 16))
        r, c = divmod(i, cols)
        x = c * cell + (cell - im.width) // 2
        y = r * (cell + 34) + 22
        sheet.paste(im, (x, y))
        d.text((c * cell + 8, r * (cell + 34) + 6), n, font=font(13),
               fill=(210, 214, 220))
    sheet.save(path)
    return path


# ==========================================================================
#  MAIN
# ==========================================================================
def main():
    ap = argparse.ArgumentParser(description="Генератор текстур носителей Uness")
    ap.add_argument("--base", action="store_true", help="только общие материалы")
    ap.add_argument("--albums", action="store_true", help="только обложки альбомов")
    ap.add_argument("--dry-run", action="store_true", help="ничего не писать")
    args = ap.parse_args()

    os.makedirs(TEX_SRC, exist_ok=True)
    os.makedirs(PREVIEW, exist_ok=True)
    do_all = not (args.base or args.albums)
    written = []

    if do_all or args.base:
        written += build_cassette_mat(None, args.dry_run)
        written += build_minidisc_mat(None, args.dry_run)
        written += build_memorycard_mat(args.dry_run)
        written += build_tile("ue_plastic", tile_plastic, args.dry_run)
        written += build_tile("ue_metal", tile_metal, args.dry_run)
        written += build_tile("ue_paper", tile_paper, args.dry_run)
        written += build_tile("ue_disc", tile_disc, args.dry_run)

    if do_all or args.albums:
        for a in ALBUMS:
            written += build_cassette_mat(a, args.dry_run)
            written += build_minidisc_mat(a, args.dry_run)

    verb = "будет записано" if args.dry_run else "записано"
    print("[ok] %s файлов: %d" % (verb, len(written)))
    for f in written:
        print("     %s" % f)

    if not args.dry_run:
        names = sorted({f.rsplit("_", 1)[0] for f in written})
        p = contact_sheet(os.path.join(PREVIEW, "textures_sheet.png"), names)
        print("[ok] контактный лист: %s" % p)


if __name__ == "__main__":
    main()

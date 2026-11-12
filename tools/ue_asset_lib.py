#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ue_asset_lib.py — общая разметка ассетов мода «унесённые» (Uness).

Здесь лежат КОНСТАНТЫ, от которых зависят сразу три вещи:

* geometry  (tools/make_models.py)   — UV-развертка каждого полигона;
* textures  (tools/make_textures.py) — что и в каком прямоугольнике нарисовано;
* materials (data/tex/*.rvmat)       — ссылки на сконвертированные .paa.

Править разметку нужно только здесь: скрипты читают её отсюда и не могут
«разъехаться» друг с другом.

Единицы и оси
-------------
* Геометрия — в МЕТРАХ (1 юнит = 1 м), как и в Object Builder.
* Система координат — как в ODOL/O2: X — вправо, Y — «в глубину», Z — вверх.
  Поэтому у предметов, лежащих на земле, толщина — это размер по Z.
* Прямоугольники в текстурах задаются в ПИКСЕЛЯХ, y считается СВЕРХУ (как в
  PIL). В UV (как в .obj) они переводятся функцией uv_rect(): v=0 — нижняя
  кромка картинки, v=1 — верхняя.
"""

# --------------------------------------------------------------------------
# Размеры железа (метры). Взяты с реальных предметов, чтобы носитель «читался»
# в инвентаре и влезал в слот.
# --------------------------------------------------------------------------
DIM = {
    # компакт-кассета IEC I (TDK D-C60 и аналоги)
    'ue_cassette':   {'w': 0.1000, 'd': 0.0638, 't': 0.0120},
    # MiniDisc-картридж (Sony MDW-74: 68 x 72 x 5 мм)
    'ue_minidisc':   {'w': 0.0680, 'd': 0.0720, 't': 0.0050},
    # SD-карта
    'ue_memorycard': {'w': 0.0320, 'd': 0.0240, 't': 0.0021},
}

# --------------------------------------------------------------------------
# Материалы и их разметка в текстуре.
#   size  — размер PNG-исходника (степени двойки: DXT требует POT)
#   rects — именованные области (x0, y0, x1, y1) в пикселях, y сверху
# --------------------------------------------------------------------------
TEX = {
    'ue_cassette': {
        'size': (1024, 512),
        'rects': {
            'top':        (0, 0, 512, 328),      # лицевая сторона (этикетка)
            'bottom':     (512, 0, 1024, 328),   # оборотная сторона
            'side_long':  (0, 336, 512, 397),    # длинная боковина 100 x 12 мм
            'side_short': (0, 400, 327, 461),    # короткая боковина 63.8 x 12 мм
            'tape':       (336, 400, 416, 470),  # намотанная плёнка (тёмно-коричневая)
        },
    },
    'ue_minidisc': {
        'size': (1024, 512),
        'rects': {
            'top':    (0, 0, 448, 474),          # сторона шторки
            'bottom': (448, 0, 896, 474),        # сторона этикетки
            'side':   (0, 484, 512, 512),        # торцы 2 мм
        },
    },
    'ue_memorycard': {
        'size': (256, 256),
        'rects': {
            'label':    (0, 0, 256, 192),        # верхняя сторона 32 x 24 мм
            'contacts': (0, 198, 256, 234),      # контактная площадка
            'side':     (0, 240, 256, 256),      # торцы 2.1 мм
        },
    },
    # тайловые (бесшовные) материалы: бока корпусов, шторка, плёнка
    'ue_plastic': {'size': (256, 256), 'rects': {'tile': (0, 0, 256, 256)}},
    'ue_metal':   {'size': (256, 256), 'rects': {'tile': (0, 0, 256, 256)}},
    'ue_paper':   {'size': (256, 256), 'rects': {'tile': (0, 0, 256, 256)}},
    'ue_disc':    {'size': (256, 256), 'rects': {'tile': (0, 0, 256, 256)}},
}

# Материал «по умолчанию» для каждой модели (то, что назначается корпусу)
MODEL_MATERIAL = {
    'ue_cassette':   'ue_cassette',
    'ue_minidisc':   'ue_minidisc',
    'ue_memorycard': 'ue_memorycard',
}

# Тайловые материалы подключаются точечно (катушки, шторка, диск, корпус карты)
TILE_MATERIALS = ('ue_plastic', 'ue_metal', 'ue_paper', 'ue_disc')


def tex_size(mat):
    return TEX[mat]['size']


def tex_rect_px(mat, name):
    """Прямоугольник области в пикселях (y сверху)."""
    return TEX[mat]['rects'][name]


def uv_rect(mat, name):
    """Прямоугольник области в UV: (u0, v0, u1, v1), v=0 — низ картинки."""
    W, H = TEX[mat]['size']
    x0, y0, x1, y1 = TEX[mat]['rects'][name]
    return (x0 / W, 1.0 - y1 / H, x1 / W, 1.0 - y0 / H)


class PlanarMapper:
    """
    Отображение «физические координаты (м) -> UV внутри области текстуры».

    Нужно, чтобы этикетка, окно и корпус лежали в одной картинке и не
    «разъезжались»: одна и та же точка модели обязана давать один и тот же UV
    и у корпуса, и у наклеенной сверху пластины.
    """

    def __init__(self, mat, name, x0, x1, y0, y1, flip_u=False, flip_v=False):
        self.rect = uv_rect(mat, name)
        self.x0, self.x1, self.y0, self.y1 = x0, x1, y0, y1
        self.flip_u, self.flip_v = flip_u, flip_v

    def __call__(self, x, y):
        u0, v0, u1, v1 = self.rect
        fx = (x - self.x0) / (self.x1 - self.x0)
        fy = (y - self.y0) / (self.y1 - self.y0)
        if self.flip_u:
            fx = 1.0 - fx
        if self.flip_v:
            fy = 1.0 - fy
        return (u0 + fx * (u1 - u0), v0 + fy * (v1 - v0))


def sub_uv(mapper, x0, x1, y0, y1):
    """UV-прямоугольник под области, заданной в физических координатах."""
    a = mapper(x0, y0)
    b = mapper(x1, y1)
    return (a[0], a[1], b[0], b[1])


class TileMapper:
    """Тайловое отображение: повторяет материал каждые `period` метров."""

    def __init__(self, period=0.05):
        self.period = period

    def __call__(self, x, y):
        return (x / self.period, y / self.period)


# ==========================================================================
#  РАСКЛАДКА КАТАЛОГОВ
# --------------------------------------------------------------------------
#  Один и тот же код должен работать и в репозитории, и внутри раздаточного
#  архива (там папки называются 1_MODELS / 2_TEXTURES / 3_MATERIALS / ...,
#  чтобы Windows-проводник не показывал кракозябры).
# ==========================================================================
import os  # noqa: E402


def project_root(start=None):
    """
    Корень проекта: поднимаемся вверх, пока не найдём характерные папки.
    Обычно это каталог на уровень выше tools/.
    """
    d = os.path.dirname(os.path.abspath(__file__)) if start is None else start
    marks = ("Models", "1_MODELS", "@Uness", "3_MATERIALS", "release")
    for _ in range(5):
        parent = os.path.dirname(d)
        if not parent or parent == d:
            break
        if any(os.path.isdir(os.path.join(parent, m)) for m in marks):
            return parent
        d = parent
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def layout(root=None):
    """
    Словарь путей проекта. Для каждой сущности перечислены варианты:
    сначала раскладка репозитория, потом — раздаточного архива.
    Возвращается первый существующий каталог (или первый вариант, если нет
    ни одного — тогда скрипт его создаст).
    """
    root = root or project_root()

    def pick(*candidates):
        first = candidates[0]
        for c in candidates:
            parts = (c,) if isinstance(c, str) else tuple(c)
            p = os.path.join(root, *parts)
            if os.path.isdir(p):
                return p
        parts = (first,) if isinstance(first, str) else tuple(first)
        return os.path.join(root, *parts)

    cfg = os.path.join(root, "@Uness", "Addons", "Uness_Data", "config.cpp")
    if not os.path.isfile(cfg):
        alt = os.path.join(root, "5_IN_GAME", "config.cpp")
        cfg = alt if os.path.isfile(alt) else cfg

    types_xml = os.path.join(root, "server_files", "mpmissions",
                             "types_uness_assets.xml")
    if not os.path.isfile(types_xml):
        alt = os.path.join(root, "5_IN_GAME", "types_uness_assets.xml")
        types_xml = alt if os.path.isfile(alt) else types_xml

    return {
        "root": root,
        "tools": pick("tools"),
        "models_src": pick(("Models", "src"), "1_MODELS"),
        "tex_src": pick(("Models", "tex_src"), "2_TEXTURES"),
        "preview": pick(("Models", "preview"), "4_PREVIEW"),
        "data": pick(("@Uness", "Addons", "Uness_Data", "data"), "3_MATERIALS"),
        "data_tex": pick(("@Uness", "Addons", "Uness_Data", "data", "tex"),
                         "3_MATERIALS"),
        "config": cfg,
        "types_xml": types_xml,
    }

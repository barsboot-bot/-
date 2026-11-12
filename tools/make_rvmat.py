#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_rvmat.py — генератор материалов (.rvmat) для моделей носителей.

`.rvmat` — это текстовый файл, поэтому он создаётся скриптом: на каждый
материал из Models/tex_src/ пишется файл в @Uness/Addons/Uness_Data/data/tex/,
который ссылается на сконвертированные `.paa` по пути ВНУТРИ PBO.

Схема карт — стандартная для DayZ:
    Stage0 -> _nohq.paa  нормали
    Stage1 -> _co.paa    цвет
    Stage2 -> макро-карта (пустая)
    Stage3 -> AO (белая)
    Stage4 -> _smdi.paa  блики

Если в вашей версии движка материал выглядит «не тем» (цвет вместо рельефа),
поменяйте местами Stage0 и Stage1 — нумерация стадий в разных версиях
DayZ Tools отличается (см. docs/17_ASSETS.md, раздел «Грабли»).

Запуск:
    python3 tools/make_rvmat.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_asset_lib import TEX, TILE_MATERIALS, MODEL_MATERIAL, layout, project_root  # noqa: E402

ROOT = project_root()
L = layout(ROOT)
OUT = L["data_tex"]   # @Uness/Addons/Uness_Data/data/tex (в архиве — 3_MATERIALS)

# плейлисты мода -> обложки альбомов (должны совпадать с make_textures.py)
ALBUMS = ("rock", "pop", "classic", "dance")

PBO = "Uness_Data"

# блики по материалу: металл блестит сильно, бумага — почти нет
SPEC = {
    "ue_cassette": (0.22, 34.0),
    "ue_minidisc": (0.30, 44.0),
    "ue_memorycard": (0.34, 40.0),
    "ue_plastic": (0.26, 28.0),
    "ue_metal": (0.85, 62.0),
    "ue_paper": (0.06, 12.0),
    "ue_disc": (0.95, 78.0),
}

TEMPLATE = """// {name}.rvmat — СГЕНЕРИРИРОВАНО tools/make_rvmat.py, не править руками.
// Материал модели носителя мода «унесённые» (Uness).
// Ссылки на .paa — по пути внутри PBO {pbo}, поэтому имя мода на сервере
// может быть любым, а пути остаются валидными.
//
// PNG-исходники:      Models/tex_src/{name}_*.png
// Конвертация:        TexView 2 -> Save As -> PaC (DirectX)
//                     _co = DXT1, _nohq = DXT5, _smdi = DXT1, mipmaps ВКЛ.

ambient[] = {{1.0, 1.0, 1.0, 1.0}};
diffuse[] = {{1.0, 1.0, 1.0, 1.0}};
forcedDiffuse[] = {{0.0, 0.0, 0.0, 0.0}};
emmisive[] = {{0.0, 0.0, 0.0, 0.0}};
specular[] = {{{spec:.2f}, {spec:.2f}, {spec:.2f}, 1.0}};
specularPower = {power:.1f};

PixelShaderID = "NormalMapSpecularMap";
VertexShaderID = "NormalMap";

class Stage0
{{
\ttexture = "{pbo}\\data\\tex\\{name}_nohq.paa";
\tuvSource = "tex";
\tclass uvTransform
\t{{
\t\taside[] = {{1.0, 0.0, 0.0}};
\t\tup[] = {{0.0, 1.0, 0.0}};
\t\tdir[] = {{0.0, 0.0, 0.0}};
\t\tpos[] = {{0.0, 0.0, 0.0}};
\t}};
}};
class Stage1
{{
\ttexture = "{pbo}\\data\\tex\\{name}_co.paa";
\tuvSource = "tex";
\tclass uvTransform
\t{{
\t\taside[] = {{1.0, 0.0, 0.0}};
\t\tup[] = {{0.0, 1.0, 0.0}};
\t\tdir[] = {{0.0, 0.0, 0.0}};
\t\tpos[] = {{0.0, 0.0, 0.0}};
\t}};
}};
class Stage2
{{
\ttexture = "#(argb,8,8,3)color(0.0,0.0,0.0,0.0)";
\tuvSource = "tex";
}};
class Stage3
{{
\ttexture = "#(argb,8,8,3)color(1.0,1.0,1.0,1.0)";
\tuvSource = "tex";
}};
class Stage4
{{
\ttexture = "{pbo}\\data\\tex\\{name}_smdi.paa";
\tuvSource = "tex";
\tclass uvTransform
\t{{
\t\taside[] = {{1.0, 0.0, 0.0}};
\t\tup[] = {{0.0, 1.0, 0.0}};
\t\tdir[] = {{0.0, 0.0, 0.0}};
\t\tpos[] = {{0.0, 0.0, 0.0}};
\t}};
}};
"""


def spec_for(name):
    for key, val in SPEC.items():
        if name.startswith(key):
            return val
    return (0.25, 30.0)


def write(name):
    spec, power = spec_for(name)
    text = TEMPLATE.format(name=name, pbo=PBO, spec=spec, power=power)
    path = os.path.join(OUT, name + ".rvmat")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)
    return path


def main():
    os.makedirs(OUT, exist_ok=True)
    names = list(TEX.keys())
    for alb in ALBUMS:
        names += ["ue_cassette_%s" % alb, "ue_minidisc_%s" % alb]
    for n in sorted(set(names)):
        print("[ok] %s" % write(n))


if __name__ == "__main__":
    main()

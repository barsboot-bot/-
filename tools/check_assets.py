#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_assets.py — проверка целостности ассетов носителей.

Сверяет четыре вещи, которые обычно «разъезжаются»:

1. `.obj`: индексы в границах, у каждого треугольника есть UV, селекшены
   (`g`) на месте, габарит совпадает с паспортным;
2. текстуры: на каждый материал из `.mtl` есть PNG-исходники (_co/_nohq/_smdi);
3. материалы: на каждый материал есть `.rvmat`, и он ссылается ровно на те
   `.paa`, у которых есть исходник; `.paa` уже сконвертированы — похвалим;
4. `config.cpp`: каждая модель, на которую ссылается класс, существует в
   `Models/src/`, а каждый `.rvmat` из `hiddenSelectionsMaterials[]` — в паке.

Код выхода: 0 — всё чисто, 1 — есть ошибки.

Запуск:
    python3 tools/check_assets.py
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mr_asset_lib import DIM, TEX, MODEL_MATERIAL, layout, project_root  # noqa: E402

ROOT = project_root()
L = layout(ROOT)
SRC = L["models_src"]
TEX_SRC = L["tex_src"]
DATA_TEX = L["data_tex"]
CONFIG = L["config"]
ALBUMS = ("rock", "pop", "classic", "dance")

errors = []
warns = []
oks = []


def err(msg):
    errors.append(msg)


def warn(msg):
    warns.append(msg)


def ok(msg):
    oks.append(msg)


# ---------------------------------------------------------------- 1. OBJ
def check_obj(name):
    path = os.path.join(SRC, name + ".obj")
    if not os.path.exists(path):
        err("нет модели: %s" % path)
        return
    V, VT, faces, groups = [], [], [], set()
    for line in open(path, encoding="utf-8"):
        t = line.strip()
        if not t or t.startswith("#"):
            continue
        p = t.split()
        if p[0] == "v":
            V.append([float(x) for x in p[1:4]])
        elif p[0] == "vt":
            VT.append([float(x) for x in p[1:3]])
        elif p[0] == "g":
            groups.add(p[1])
        elif p[0] == "f":
            for tok in p[1:]:
                a = tok.split("/")
                if len(a) < 2 or not a[1]:
                    err("%s: грань без UV: %s" % (name, tok))
                    continue
                vi, ti = int(a[0]) - 1, int(a[1]) - 1
                faces.append((vi, ti))
                if vi >= len(V) or ti >= len(VT):
                    err("%s: индекс за границей: %s" % (name, tok))
    if not faces:
        err("%s: нет ни одной грани" % name)
        return

    xs = [v[0] for v in V]; ys = [v[1] for v in V]; zs = [v[2] for v in V]
    size = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    want = (DIM[name]["w"], DIM[name]["d"], DIM[name]["t"])
    for i, (got, exp) in enumerate(zip(size, want)):
        if abs(got - exp) > 0.0006:
            err("%s: габарит %.4f м не совпадает с паспортным %.4f м (ось %d)"
                % (name, got, exp, i))
    if abs(min(zs) + max(zs)) > 5e-4:
        warn("%s: модель не отцентрована по Z (низ %.4f, верх %.4f)" % (name, min(zs), max(zs)))
    if "label" not in groups:
        err("%s: нет селекшена `label` — подмена обложки альбома не сработает" % name)

    ok("%-14s %5d верш. %5d граней  %.1f x %.1f x %.1f мм  селекшены: %s"
       % (name, len(V), len(faces) // 3, size[0] * 1000, size[1] * 1000,
          size[2] * 1000, ", ".join(sorted(groups))))

    # материалы из .mtl
    mtl = os.path.join(SRC, name + ".mtl")
    mats = []
    if os.path.exists(mtl):
        mats = re.findall(r"^newmtl\s+(\S+)", open(mtl, encoding="utf-8").read(), re.M)
    else:
        err("нет файла материалов: %s" % mtl)
    for m in mats:
        check_material(m)


# ------------------------------------------------------------ 2. текстуры
def check_material(mat):
    if mat not in TEX:
        err("материал `%s` не описан в tools/mr_asset_lib.py" % mat)
        return
    for suf in ("_co", "_nohq", "_smdi"):
        p = os.path.join(TEX_SRC, mat + suf + ".png")
        if not os.path.exists(p):
            err("нет текстуры: Models/tex_src/%s%s.png (запустите tools/make_textures.py)"
                % (mat, suf))
    rv = os.path.join(DATA_TEX, mat + ".rvmat")
    if not os.path.exists(rv):
        err("нет материала: @MusicRadioCore/Addons/MusicRadioCore/data/tex/%s.rvmat "
            "(запустите tools/make_rvmat.py)" % mat)
    else:
        refs = re.findall(r"(MusicRadioCore\\data\\tex\\[\w]+\.paa)",
                          open(rv, encoding="utf-8", errors="ignore").read())
        if len(refs) != 3:
            err("%s.rvmat: ожидалось 3 ссылки на .paa, найдено %d" % (mat, len(refs)))
        for r in refs:
            base = r.replace("\\", "/").rsplit("/", 1)[-1].replace(".paa", "")
            if not os.path.exists(os.path.join(TEX_SRC, base + ".png")):
                err("%s.rvmat ссылается на %s, но исходника %s.png нет" % (mat, base, base))


# -------------------------------------------------------------- 3. .paa
def check_paa():
    if not os.path.isdir(DATA_TEX):
        err("нет папки материалов: %s" % DATA_TEX)
        return
    paa = [f for f in os.listdir(DATA_TEX) if f.endswith(".paa")]
    png = [f for f in os.listdir(TEX_SRC) if f.endswith(".png")] if os.path.isdir(TEX_SRC) else []
    stray = [f for f in os.listdir(DATA_TEX) if f.lower().endswith(".png")]
    for f in stray:
        err("в data/tex лежит PNG (%s): в пак PNG класть нельзя, движок их не читает" % f)
    if paa:
        ok("сконвертировано .paa: %d из %d (ожидается %d)"
           % (len(paa), len(png) // 3 * 3 if png else 0, len(TEX) * 3))
    else:
        warn(".paa ещё не сконвертированы (%d): модели в игре будут белыми. "
             "Порядок — docs/17_ASSETS.md, шаг 2" % (len(png) // 3))


# ----------------------------------------------------------- 4. config.cpp
def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def check_config():
    if not os.path.exists(CONFIG):
        err("нет конфига: %s" % CONFIG)
        return
    raw = open(CONFIG, encoding="utf-8").read()
    code = strip_comments(raw)

    # баланс скобок — грубая, но полезная проверка синтаксиса
    depth = 0
    for ch in code:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth < 0:
                err("config.cpp: лишняя закрывающая скобка")
                break
    if depth != 0:
        err("config.cpp: не закрыто скобок: %d" % depth)

    # модели: model = "MusicRadioCore\data\<name>.p3d"
    for m in re.finditer(r'model\s*=\s*"MusicRadioCore\\data\\([\w]+)\.p3d"', code):
        name = m.group(1)
        if not os.path.exists(os.path.join(SRC, name + ".obj")):
            err('config.cpp: модель "%s.p3d" не имеет исходника Models/src/%s.obj' % (name, name))
        else:
            ok("config.cpp -> модель %s.p3d (исходник есть)" % name)

    # материалы из hiddenSelectionsMaterials
    for m in re.finditer(r"MusicRadioCore\\data\\tex\\([\w]+)\.rvmat", code):
        rv = m.group(1)
        if not os.path.exists(os.path.join(DATA_TEX, rv + ".rvmat")):
            err("config.cpp: %s.rvmat не найден в data/tex" % rv)
        elif not os.path.exists(os.path.join(TEX_SRC, rv + "_co.png")):
            err("config.cpp: у %s.rvmat нет текстуры-исходника" % rv)

    # классы, которые ждут скрипты
    need = ("MR_Item_Cassette_Rock", "MR_Item_Cassette_Pop",
            "MR_Item_Disk_Classic", "MR_Item_Disk_Dance",
            "MR_Item_MemoryCard")
    for cls in need:
        if not re.search(r"class\s+%s\s*:" % cls, code):
            err("config.cpp: нет класса %s (его ищут скрипты)" % cls)
    ok("config.cpp: классы носителей на месте, скобки сбалансированы")


def main():
    for name in MODEL_MATERIAL:
        check_obj(name)
    check_paa()
    check_config()
    # обложки альбомов
    missing = []
    for a in ALBUMS:
        for pre in ("mr_cassette_%s" % a, "mr_minidisc_%s" % a):
            for suf in ("_co", "_nohq", "_smdi"):
                if not os.path.exists(os.path.join(TEX_SRC, pre + suf + ".png")):
                    missing.append(pre + suf + ".png")
            if not os.path.exists(os.path.join(DATA_TEX, pre + ".rvmat")):
                missing.append(pre + ".rvmat")
    if missing:
        warn("нет обложек/материалов альбомов (%d): запустите "
             "tools/make_textures.py --albums && tools/make_rvmat.py" % len(missing))
    else:
        ok("обложки альбомов: %s — полный комплект" % ", ".join(ALBUMS))

    print("=" * 72)
    for line in oks:
        print("  ok    %s" % line)
    for line in warns:
        print("  !     %s" % line)
    for line in errors:
        print("  ERROR %s" % line)
    print("=" * 72)
    print("итог: %d ok, %d предупреждений, %d ошибок" % (len(oks), len(warns), len(errors)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

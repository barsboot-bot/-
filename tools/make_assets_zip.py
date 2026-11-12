#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
make_assets_zip.py — сборка раздаточного архива с ассетами носителей.

Кладу всё, что нужно художнику/сборщику на Windows, в один ZIP:

    release/MusicRadio_assets_v1.1.0.zip
    └── MusicRadio_assets_v1.1.0/
        ├── README_RU.txt          что где лежит и три шага
        ├── REBUILD_ASSETS.bat     пересборка исходников (Python + Pillow)
        ├── 1_MODELS/              .obj + .mtl  (→ Object Builder)
        ├── 2_TEXTURES/            .png _co/_nohq/_smdi (→ TexView 2)
        ├── 3_MATERIALS/           .rvmat (→ @MusicRadioCore\Addons\MusicRadioCore\data\tex)
        ├── 4_PREVIEW/             чертежи SVG, рендеры, контактный лист
        ├── 5_IN_GAME/             config.cpp (образец), лут types
        ├── Models_README.md       габариты и селекшены
        ├── docs/17_ASSETS.md      полная инструкция
        └── tools/                 генераторы исходников

Имена папок и файлов — ТОЛЬКО ЛАТИНИЦА: иначе Windows-проводник и часть
архиваторов показывают кракозябры (zip-флаг UTF-8 не все читают). Русский
текст — внутри файлов.

Запуск:
    python3 tools/make_assets_zip.py
    python3 tools/make_assets_zip.py --version 1.1.0 --out release
"""
import argparse
import os
import shutil
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RELEASE = os.path.join(ROOT, "release")

MODELS_SRC = os.path.join(ROOT, "Models", "src")
TEX_SRC = os.path.join(ROOT, "Models", "tex_src")
PREVIEW = os.path.join(ROOT, "Models", "preview")
DATA_TEX = os.path.join(ROOT, "@MusicRadioCore", "Addons", "MusicRadioCore", "data", "tex")
CONFIG = os.path.join(ROOT, "@MusicRadioCore", "Addons", "MusicRadioCore", "config.cpp")

GENERATORS = (
    "mr_asset_lib.py",
    "make_models.py",
    "make_textures.py",
    "make_rvmat.py",
    "render_preview.py",
    "check_assets.py",
    "make_assets_zip.py",
)

README_RU = """\
=========================================================================
  MusicRadio ("MusicRadio") — ассеты носителей музыки
  кассета · MiniDisc · карта памяти          версия {version}
=========================================================================

Что лежит в архиве
------------------
1_MODELS/     геометрия: .obj + .mtl. Единицы — МЕТРЫ, оси как в
              Object Builder: X — вправо, Y — в глубину, Z — вверх.
              Импортировать с МАСШТАБОМ 1.0 (не 0.001!).
2_TEXTURES/   исходники текстур: 45 PNG = 15 материалов x 3 карты
              (_co — цвет, _nohq — нормали, _smdi — блики).
              PNG нарисованы скриптом: никаких стоковых картинок,
              лицензия чистая.
3_MATERIALS/  готовые .rvmat (15 шт.) — они уже лежат в паке мода,
              сюда скопированы для справки и для Object Builder.
4_PREVIEW/    чертежи .svg с размерами, контрольные рендеры моделей,
              контактный лист всех текстур.
5_IN_GAME/    config.cpp — образец классов носителей (в репозитории уже
              применён) и types_musicradio_assets.xml — лут для новых предметов.
docs/         полная пошаговая инструкция (17_ASSETS.md).
tools/        генераторы исходников + REBUILD_ASSETS.bat.

Откуда это взялось (пути в репозитории)
---------------------------------------
1_MODELS/    <- Models/src/
2_TEXTURES/  <- Models/tex_src/
3_MATERIALS/ <- @MusicRadioCore/Addons/MusicRadioCore/data/tex/
4_PREVIEW/   <- Models/preview/
5_IN_GAME/   <- @MusicRadioCore/Addons/MusicRadioCore/config.cpp, server_files/mpmissions/
tools/       <- tools/


ТРИ ШАГА (нужны Windows + DayZ Tools из Steam)
----------------------------------------------

ШАГ 1. Текстуры PNG -> .paa (TexView 2)
   Открыть PNG из 2_TEXTURES -> File -> Save As:
     тип файла : PaC (DirectX)      (это и есть .paa)
     имя       : СОХРАНЯЕМ суффикс  (_co / _nohq / _smdi)
     папка     : @MusicRadioCore\\Addons\\MusicRadioCore\\data\\tex\\
     сжатие    : _co = DXT1, _nohq = DXT5, _smdi = DXT1
     mipmaps   : ВКЛЮЧИТЬ
   Всего 45 файлов. Быстрее — пакетный режим: File -> Batch Convert,
   три прохода (сначала все _co, потом _nohq, потом _smdi).

ШАГ 2. Модели .obj -> .p3d (Object Builder)
   File -> Import -> Wavefront (.obj) -> файл из 1_MODELS:
     * масштаб 1.0;
     * проверить габарит по чертежу из 4_PREVIEW;
     * материалы назначать ФАЙЛАМИ .rvmat из 3_MATERIALS (не .paa!),
       тогда нормали и блики подхватятся сами;
     * LOD-схема: Resolution 0.000, Geometry (коробка по габариту),
       View Geometry (копия корпуса), Memory (точка ce_center),
       Shadow Volume 0.000;
     * Save As -> @MusicRadioCore\\Addons\\MusicRadioCore\\data\\mr_cassette.p3d
       (и так же mr_minidisc.p3d, mr_memorycard.p3d — имена ровно как
       в config.cpp).
   Иконка инвентаря — там же: Export -> Icon.

ШАГ 3. Сборка
   powershell -ExecutionPolicy Bypass -File build.ps1
   В игре: #spawnitem MR_Item_Cassette_Rock
           #spawnitem MR_Item_MiniDisc_Classic
           #spawnitem MR_Item_MemoryCard


Классы предметов и габариты
---------------------------
MR_Item_Cassette_Rock, MR_Item_Cassette_Pop     100.0 x 63.8 x 12.0 мм
MR_Item_MiniDisc_Rock, _Pop, _Classic, _Dance    68.0 x 72.0 x  5.0 мм
MR_Item_MemoryCard                               32.0 x 24.0 x  2.1 мм

Селекшены моделей: shell, label, window, cavity, hub_left/right,
tape_left/right (кассета); shell, shutter, disc, hub, label, cavity (MD);
shell, label, contacts, switch (карта). Подробно — Models_README.md.


ВАЖНО
-----
* В архиве НЕТ .p3d и .paa, и это не упущение: ODOL и PaC — бинарные
  форматы Bohemia, их пишут только родные инструменты. Скриптом такой
  файл не создаётся. Всё, что можно подготовить до сохранения, —
  подготовлено.
* PNG внутрь PBO класть нельзя: движок их не читает, только вес растёт.
  В пак идут .paa (из шага 1) и .rvmat.
* У всех кассет ОДИН меш: обложка альбома подменяется материалом через
  селекшен `label` (см. config.cpp, hiddenSelectionsMaterials[]).
  Новый альбом = новая обложка + новый класс, модель не меняется.

Полная инструкция, чек-лист и «грабли» — docs/17_ASSETS.md
"""

REBUILD_BAT = r"""@echo off
rem ============================================================
rem  MusicRadio - rebuild asset sources (models, textures, materials)
rem  Needs Python 3 and Pillow/numpy. Run from the archive root.
rem ============================================================
setlocal
echo == MusicRadio assets rebuild ==

where python >nul 2>nul
if errorlevel 1 (
    echo ERROR: python not found in PATH. Install Python 3 and retry.
    pause
    exit /b 1
)

echo [1/5] installing Pillow and numpy...
python -m pip install --user pillow numpy
if errorlevel 1 (
    echo WARNING: pip install failed - trying to continue anyway.
)

echo [2/5] geometry (.obj + .mtl + drafts)...
python tools\make_models.py
if errorlevel 1 goto fail

echo [3/5] textures (.png: _co/_nohq/_smdi + album covers)...
python tools\make_textures.py
if errorlevel 1 goto fail

echo [4/5] materials (.rvmat)...
python tools\make_rvmat.py
if errorlevel 1 goto fail

echo [5/5] preview renders + integrity check...
python tools\render_preview.py
python tools\check_assets.py
if errorlevel 1 goto fail

echo.
echo DONE. Sources are in 1_MODELS, 2_TEXTURES, 3_MATERIALS.
echo Next: convert PNG to .paa (TexView 2) and OBJ to .p3d (Object Builder).
pause
exit /b 0

:fail
echo.
echo FAILED - see the message above.
pause
exit /b 1
"""


def copy_dir(zf, src, dst_root, arc_root, exts=None):
    """Добавить содержимое папки src в архив по пути arc_root/dst_root."""
    n = 0
    if not os.path.isdir(src):
        print("  ! нет папки: %s" % src)
        return 0
    for name in sorted(os.listdir(src)):
        p = os.path.join(src, name)
        if not os.path.isfile(p):
            continue
        if exts and os.path.splitext(name)[1].lower() not in exts:
            continue
        zf.write(p, "%s/%s/%s" % (arc_root, dst_root, name))
        n += 1
    return n


def write_text(zf, text, arcname, bom=False):
    data = text.encode("utf-8")
    if bom:
        data = b"\xef\xbb\xbf" + data
    data = data.replace(b"\n", b"\r\n")          # CRLF для Windows
    import time
    zi = zipfile.ZipInfo(arcname, date_time=time.localtime()[:6])
    zi.compress_type = zipfile.ZIP_DEFLATED
    zi.external_attr = 0o644 << 16
    zf.writestr(zi, data)


def main():
    ap = argparse.ArgumentParser(description="Сборка архива ассетов MusicRadio")
    ap.add_argument("--version", default="1.1.0")
    ap.add_argument("--out", default=RELEASE)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    arc_root = "MusicRadio_assets_v%s" % args.version
    zip_path = os.path.join(args.out, arc_root + ".zip")
    tmp = tempfile.mkdtemp(prefix="musicradio_assets_")

    try:
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED,
                             compresslevel=9) as zf:
            counts = {}
            counts["1_MODELS"] = copy_dir(zf, MODELS_SRC, "1_MODELS", arc_root,
                                          exts=(".obj", ".mtl"))
            counts["2_TEXTURES"] = copy_dir(zf, TEX_SRC, "2_TEXTURES", arc_root,
                                            exts=(".png",))
            counts["3_MATERIALS"] = copy_dir(zf, DATA_TEX, "3_MATERIALS", arc_root,
                                             exts=(".rvmat",))
            counts["4_PREVIEW"] = copy_dir(zf, PREVIEW, "4_PREVIEW", arc_root,
                                           exts=(".svg", ".png"))

            # ---- 5_IN_GAME: конфиг и лут ----
            for p, name in ((CONFIG, "config.cpp"),
                            (os.path.join(ROOT, "server_files", "mpmissions",
                                          "types_musicradio_assets.xml"),
                             "types_musicradio_assets.xml")):
                if os.path.exists(p):
                    zf.write(p, "%s/5_IN_GAME/%s" % (arc_root, name))
                    counts["5_IN_GAME"] = counts.get("5_IN_GAME", 0) + 1

            # ---- документация ----
            for p, arc in (
                (os.path.join(ROOT, "Models", "README.md"),
                 "Models_README.md"),
                (os.path.join(ROOT, "docs", "17_ASSETS.md"),
                 "docs/17_ASSETS.md"),
                (os.path.join(ROOT, "docs", "НОВАЯ_КАССЕТА.md"),
                 "docs/НОВАЯ_КАССЕТА.md"),
            ):
                if os.path.exists(p):
                    zf.write(p, "%s/%s" % (arc_root, arc))
            counts["docs"] = 2

            # ---- генераторы ----
            for g in GENERATORS:
                p = os.path.join(ROOT, "tools", g)
                if os.path.exists(p):
                    zf.write(p, "%s/tools/%s" % (arc_root, g))
            counts["tools"] = len(GENERATORS)

            # ---- текстовые файлы архива ----
            write_text(zf, README_RU.format(version=args.version),
                       "%s/README_RU.txt" % arc_root, bom=True)
            write_text(zf, REBUILD_BAT, "%s/REBUILD_ASSETS.bat" % arc_root)
            write_text(zf, REBUILD_BAT, "%s/tools/REBUILD_ASSETS.bat" % arc_root)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    size = os.path.getsize(zip_path)
    print("[ok] %s (%.1f МБ)" % (zip_path, size / 1024.0 / 1024.0))
    for k in sorted(counts):
        print("     %-12s %d файл." % (k, counts[k]))
    print("     README_RU.txt, REBUILD_ASSETS.bat")
    total = sum(counts.values()) + 2
    print("     итого %d файлов" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())

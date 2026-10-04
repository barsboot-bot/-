#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
new_cassette.py — генератор нового носителя мода «унесённые».

1 кассета (или 1 диск) = 1 PBO-аддон. Скрипт создаёт каркас аддона
Uness_Tape_<slug> с config.cpp (CfgPatches + CfgVehicles),
заглушкой sound/sample.ogg и meta.txt, а затем упаковывает его
в .pbo (native BI-формат, tools/pack_pbo.py).

Использование:
    python3 tools/new_cassette.py --type tape --name "Ночной Рок" \
        --class MyNightRock --music-dir "<путь к папке с ogg/mp3>" [--out build/out]

После генерации: подпишите PBO через DayZ Tools -> Addon Builder
(подписанные PBO обязательны на серверах с verifySignatures=1).
"""
import argparse
import os
import re
import shutil
import sys
import textwrap

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from pack_pbo import pack_pbo  # noqa: E402

BASE_CLASS = {
    "tape": ("UE_Item_Cassette_Rock", "Кассета"),
    "disk": ("UE_Item_Disk_Classic", "Диск"),
}


def slugify(name: str) -> str:
    ascii_only = name.encode("cp1251", "ignore").decode("cp1251", "ignore")
    s = re.sub(r"[^A-Za-z0-9]+", "_", ascii_only).strip("_")
    return s or "NewTape"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--type", choices=["tape", "disk"], default="tape")
    ap.add_argument("--name", required=True, help="Отображаемое имя, напр. «Ночной Рок»")
    ap.add_argument("--class", dest="cls", default=None,
                    help="Имя класса без префикса UE_Item_ (латиницей)")
    ap.add_argument("--music-dir", default=None,
                    help="Папка с треками (.ogg/.mp3/.wav). Пусто — будет заглушка.")
    ap.add_argument("--addon-root", default=os.path.join(ROOT, "@Uness", "Addons"))
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "out", "@Uness", "Addons"))
    args = ap.parse_args()

    base_class, ru_kind = BASE_CLASS[args.type]
    cls_suffix = args.cls or slugify(args.name)
    cls = f"UE_Item_{'Cassette' if args.type == 'tape' else 'Disk'}_{cls_suffix}"
    addon = f"Uness_{'Tape' if args.type == 'tape' else 'Disc'}_{cls_suffix}"

    src_dir = os.path.join(args.addon_root, addon)
    if os.path.exists(src_dir):
        print(f"!! аддон уже существует: {src_dir}")
        sys.exit(1)
    os.makedirs(os.path.join(src_dir, "sound"))

    cfg = textwrap.dedent(f"""\
        // ============================================================
        //  {addon} — {'аудиокассета' if args.type == 'tape' else 'виниловый диск'}
        //  «{args.name}». 1 носитель = 1 PBO.
        //  Наследует базовый предмет из Uness_Data; добавляет
        //  собственный SoundSource и привязку к плейлисту Music/.
        // ============================================================

        class CfgPatches
        {{
            class {addon}
            {{
                name = "{args.name}";
                author = "KRa Tos (Konstantin)";
                url = "https://github.com/KRaTos/Unessed-DayZ-Mod";
                version = "1.0.0";
                requiredVersion = 0.1;
                requiredAddons[] = {{"DZ_Data", "DZ_Scripts", "Uness_Data", "KRa_TosServerInit"}};
                units[] = {{"{cls}"}};
                weapons[] = {{}};
            }};
        }};

        class CfgVehicles
        {{
            class {base_class};   // базовый класс-носитель из Uness_Data

            class {cls}: {base_class}
            {{
                scope = 2;
                displayName = "{ru_kind}: {args.name}";
                descriptionShort = "{ru_kind} с записью «{args.name}».";
                // ключ плейлиста внешней библиотеки сервера:
                //   Music/Type/{cls_suffix}/  (кассеты)
                //   Music/CD/{cls_suffix}/    (диски)
                uePlaylist = "{cls_suffix}";
            }};
        }};

        // ---------- Звук носителя (штатный SoundSource движка) ----------
        class CfgSounds
        {{
            sounds[] = {{}};
            class {cls}_Sound
            {{
                name = "{cls}_Sound";
                sound[] = {{"sound\\sample.ogg", 0, 1}};
                distance[] = {{0, 150, 160}};   // затухание до 150 м (UE_Config::maxHearDistance)
            }};
        }};
    """)
    with open(os.path.join(src_dir, "config.cpp"), "w", encoding="utf-8") as f:
        f.write(cfg)

    # --- звук: копируем треки или кладём заглушку ---
    if args.music_dir and os.path.isdir(args.music_dir):
        n = 0
        for fn in sorted(os.listdir(args.music_dir)):
            if fn.lower().endswith((".ogg", ".mp3", ".wav")):
                shutil.copy(os.path.join(args.music_dir, fn), os.path.join(src_dir, "sound", fn))
                n += 1
        print(f"  скопировано треков: {n}")
    else:
        with open(os.path.join(src_dir, "sound", "sample.ogg"), "wb") as f:
            f.write(b"OggS")  # заглушка — замените реальным файлом
        print("  вставлена заглушка sound/sample.ogg (замените реальным треком!)")

    # --- meta.txt для внешней библиотеки сервера ---
    lib_kind = "Type" if args.type == "tape" else "CD"
    music_lib = os.path.join(ROOT, "@KRa_TosServer", "Music", lib_kind, cls_suffix)
    os.makedirs(music_lib, exist_ok=True)
    with open(os.path.join(music_lib, "meta.txt"), "w", encoding="utf-8") as f:
        f.write(f"name = {args.name}\n")
    print(f"  библиотека сервера: {music_lib}")

    # --- упаковка в PBO ---
    dst = os.path.join(args.out, addon + ".pbo")
    path, size = pack_pbo(src_dir, dst)
    print(f"  + {os.path.basename(path)}  ({size} bytes)")
    print("\nГотово. Дальнейшие шаги:")
    print(f"  1. Замените sound/*.ogg реальными треками при необходимости.")
    print(f"  2. Положите аудиофайлы в {music_lib} (внешняя библиотека).")
    print(f"  3. Подпишите {addon}.pbo через DayZ Tools -> Addon Builder.")
    print(f"  4. Обновите список юнитов в @Uness/config.cpp (units[]/weapons[]) при желании.")


if __name__ == "__main__":
    main()

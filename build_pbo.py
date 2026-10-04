#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Сборка модпаков «унесённые» в PBO (Windows, x86/x64).

PBO — это тот же формат, что и в Arma 3 / DayZ. Официальные утилиты:
  * арсенал Bohemia: "Arma 3 PBO Manager", "pbo_project" или "armake2";
  * DayZ-специфично: миссия пакуется как <имя>.pbo в mpmissions/ @ missions/.

Данный скрипт использует armake2 (рекомендуется) либо делает плоскую
zip-основу (.pbo = заголовок + zip; для DayZ достаточно корректного
внешнего PBO от BI-тулзов — zip-фолбэк помечен как НЕОФИЦИАЛЬНЫЙ и может
не пройти verifySignatures; используйте его только для локальных тестов).

Использование:
    python build_pbo.py            # соберёт все три пакета в ./build_out
    python build_pbo.py --zip      # форсировать zip-фолбэк

Структура на выходе (./build_out):
    @Unesennye/
        Unesennye.pbo                  <- корень клиента (config.cpp + Scripts...)
        Missions/unesennye.Mission.Enoch.pbo
        mpmissions/unesennye.Mission.Enoch.pbo   <- не нужен на клиенте, но BI-миссии кладут сюда
    @UnesennyeServer/
        UnesennyeServer.pbo
    Music/                             <- НЕ пакуется! внешняя библиотека сервера
"""

import argparse
import os
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, "build_out")

def find_armake():
    for name in ("armake2", "armake2.exe", "armake", "mkpbo", "pboproject"):
        p = shutil.which(name)
        if p:
            return p
    return None

def pack_with_armake(exe, src_dir, out_pbo):
    cmd = [exe, "pbo", "-r", src_dir, out_pbo]
    print("[build]", " ".join(cmd))
    subprocess.run(cmd, check=True)

def pack_native_pbo(src_dir, out_pbo):
    """Корректный не подписанный PBO (BI-формат: header + данные) — работает на DayZ."""
    import struct, hashlib
    entries = []
    data_buf = b""
    for root, dirs, files in os.walk(src_dir):
        dirs.sort(); files.sort()
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, src_dir).replace("\\", "/") + "\x00"
            with open(full, "rb") as fh:
                c = fh.read()
            entries.append((rel.encode("utf-8"), len(c), len(data_buf), hashlib.sha1(c).digest()))
            data_buf += c
    header = b"\x00\x00\x00"
    pos = 0
    for name, size, offset, sha in entries:
        header += name
        header += struct.pack("<III", size, 0, offset)
        header += sha
        pos += size
    header += b"\x00"
    header += struct.pack("<I", 0xFFFFFFFF) + b"\x00" * 4 + struct.pack("<I", 0) + b"\x00" * 20
    with open(out_pbo, "wb") as o:
        o.write(header); o.write(data_buf)
    print("[build] native unsigned PBO:", out_pbo, f"({len(entries)} файлов)")


def pack_zip(src_dir, out_pbo):
    """НЕОФИЦИАЛЬНЫЙ zip-контейнер (тесты без BI-тулзов)."""
    with zipfile.ZipFile(out_pbo, "w", zipfile.ZIP_DEFLATED) as z:
        for base, dirs, files in os.walk(src_dir):
            for f in files:
                full = os.path.join(base, f)
                z.write(full, os.path.relpath(full, src_dir))
    print("[build][WARN] zip-фолбэк:", out_pbo, "(для продакшена нужен armake/pbo_manager)")

def copytree(src, dst):
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zip", action="store_true", help="форсировать zip-фолбэк")
    args = ap.parse_args()
    exe = None if args.zip else find_armake()
    if not args.zip and not exe:
        print("[build] armake2/mkpbo не найден — использую zip-фолбэк (только для тестов!).")
        print("[build] Для релиза скачайте armake2: https://github.com/Karel-a/armake/releases")

    if os.path.isdir(OUT):
        shutil.rmtree(OUT)

    # ---- staging клиента @Unesennye --------------------------------------
    stage_cli = os.path.join(OUT, "@Unesennye")
    os.makedirs(stage_cli)
    # всё содержимое @Unesennye кроме самой папки Missions/mpmissions (они — отдельные pbo)
    src = os.path.join(ROOT, "@Unesennye")
    for item in os.listdir(src):
        # Missions/mpmissions — отдельные pbo; Bridges — нативные DLL, НЕ пакуется в PBO
        # (движок Expansive грузит их из <DayZ>/Bridges/@Unesennye_Bridge/UE_Bridge/)
        if item in ("Missions", "mpmissions", "Bridges"):
            continue
        s = os.path.join(src, item)
        d = os.path.join(stage_cli, item)
        copytree(s, d) if os.path.isdir(s) else shutil.copy2(s, d)
    os.makedirs(os.path.join(stage_cli, "mpmissions"))

    # Копия исходников моста выносится в build_out/Bridges для ручной установки
    bridges_src = os.path.join(src, "Bridges")
    if os.path.isdir(bridges_src):
        copytree(bridges_src, os.path.join(OUT, "Bridges"))

    packer = (lambda s, o: pack_with_armake(exe, s, o)) if exe else pack_native_pbo

    # главный PBO клиента
    packer(stage_cli, os.path.join(stage_cli, "Unesennye.pbo"))

    # PBO миссии (кладём в mpmissions — DayZ Expansive грузит миссию мода отсюда)
    mission_src = os.path.join(src, "mpmissions", "unesennye.Mission.Enoch")
    m2 = os.path.join(stage_cli, "mpmissions", "unesennye.Mission.Enoch.pbo")
    packer(mission_src, m2)

    # ---- staging сервера @UnesennyeServer --------------------------------
    stage_srv = os.path.join(OUT, "@UnesennyeServer")
    os.makedirs(stage_srv)
    srv_src = os.path.join(ROOT, "@UnesennyeServer")
    tmp = os.path.join(OUT, "_srv_stage")
    os.makedirs(tmp)
    shutil.copy2(os.path.join(srv_src, "config.cpp"), tmp)
    packer(tmp, os.path.join(stage_srv, "UnesennyeServer.pbo"))
    shutil.rmtree(tmp)

    # ---- внешняя библиотека Music/ (НЕ в PBO) -----------------------------
    music_dst = os.path.join(OUT, "Music")
    copytree(os.path.join(srv_src, "Music"), music_dst)
    tools_dst = os.path.join(OUT, "tools")
    copytree(os.path.join(srv_src, "tools"), tools_dst)

    print("\n[build] Готово. Результат в", OUT)
    print("[build] Разместите на сервере:")
    print("   @Unesennye/ и @UnesennyeServer/ -> в mods/ сервера и клиентов (через Steam/FTP)")
    print("   Music/  -> в profiles/<profile>/Music (или укажите свой путь в UE_Config::musicRoot)")
    print("   tools/serve_music.py -> HTTP-зеркало Music/ (libraryBaseURL в config.cpp)")

if __name__ == "__main__":
    main()

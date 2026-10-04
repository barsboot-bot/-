#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Сборка аддонов мода «Унесённые | Unessed» в PBO.

Пакует каждую папку @Unessed/Addons/<Addon> в <Addon>.pbo (BI-формат:
заголовок с SHA1 + данные). Для продакшена подписывайте через DayZ Tools
(Addon Builder + Addon Signature) — unsigned PBO проходит только с
verifySignatures=0 (локальные тесты).

Использование:
    python build_unessed.py                 # native PBO -> ./build_out/@Unessed/Addons/
    python build_unessed.py --zip           # zip-фолбэк (тесты без BI-тулзов)
"""
import argparse, os, shutil, struct, hashlib, sys, zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(ROOT, "@Unessed", "Addons")
OUT = os.path.join(ROOT, "build_out", "@Unessed", "Addons")

def pack_native_pbo(src_dir, out_pbo):
    entries, data_buf = [], b""
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
    for name, size, offset, sha in entries:
        header += name + struct.pack("<III", size, 0, offset) + sha
    header += b"\x00"
    header += struct.pack("<I", 0xFFFFFFFF) + b"\x00"*4 + struct.pack("<I", 0) + b"\x00"*20
    os.makedirs(os.path.dirname(out_pbo), exist_ok=True)
    with open(out_pbo, "wb") as o:
        o.write(header); o.write(data_buf)
    print(f"[build] {os.path.basename(out_pbo)} <- {len(entries)} файлов")

def pack_zip(src_dir, out_pbo):
    os.makedirs(os.path.dirname(out_pbo), exist_ok=True)
    with zipfile.ZipFile(out_pbo, "w", zipfile.ZIP_DEFLATED) as z:
        for base, dirs, files in os.walk(src_dir):
            for f in files:
                full = os.path.join(base, f)
                z.write(full, os.path.relpath(full, src_dir))
    print(f"[build][WARN] zip-фолбэк: {os.path.basename(out_pbo)}")

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--zip", action="store_true")
    args = ap.parse_args()
    packer = pack_zip if args.zip else pack_native_pbo
    if os.path.isdir(os.path.join(ROOT, "build_out", "@Unessed")):
        shutil.rmtree(os.path.join(ROOT, "build_out", "@Unessed"))
    for addon in sorted(os.listdir(SRC)):
        src_dir = os.path.join(SRC, addon)
        if not os.path.isdir(src_dir):
            continue
        packer(src_dir, os.path.join(OUT, addon + ".pbo"))
    # Копии README/.gitignore/Keys в сборку
    for item in ("README.md", ".gitignore"):
        s = os.path.join(ROOT, "@Unessed", item)
        if os.path.isfile(s):
            shutil.copy2(s, os.path.join(ROOT, "build_out", "@Unessed", item))
    keys_src = os.path.join(ROOT, "@Unessed", "Keys")
    if os.path.isdir(keys_src):
        shutil.copytree(keys_src, os.path.join(ROOT, "build_out", "@Unessed", "Keys"))
    print("[build] Готово. Результат:", os.path.join(ROOT, "build_out", "@Unessed"))

if __name__ == "__main__":
    main()

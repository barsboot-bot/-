#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pack_pbo.py — упаковка аддонов DayZ в native BI-PBO (fallback,
если DayZ Tools / Addon Builder не установлены).

Формат PBO (Bohemia):
  header: "VBP\0" + 4*0xFF + reserved(4) + size_data(4) + zero(4)
  entries: name\0 + CRC32(4) + sizePacked(4) + sizeUnpacked(4)  (для файлов)
           name\0 + 0xFFFFFFFF                                   (для папок)
  terminator: \0
  data: zlib-сжатые конкатенированные тела файлов.

ВАЖНО: такие PBO не подписываются (.biprivatekey создаёт Addon
Builder из DayZ Tools). Для продакшн-релиза всегда используйте
официальный Addon Builder; этот скрипт — для локальных тестов.

Использование:
    python3 tools/pack_pbo.py <staging_dir> <out_client_dir> [out_server_dir]
"""
import binascii
import os
import struct
import sys
import zlib


def collect(tree_root):
    """Обход дерева: (dirs, files) с относительными путями."""
    dirs, files = [], []
    for root, dnames, fnames in os.walk(tree_root):
        dnames.sort(); fnames.sort()
        rel_root = os.path.relpath(root, tree_root).replace("\\", "/")
        if rel_root != ".":
            dirs.append(rel_root)
        for f in fnames:
            fp = os.path.join(root, f)
            rel = os.path.relpath(fp, tree_root).replace("\\", "/")
            with open(fp, "rb") as fh:
                files.append((rel, fh.read()))
    return dirs, files


def pack_pbo(src_dir, dst_pbo):
    dirs, files = collect(src_dir)
    entries = b""
    body = b""
    # папки первыми — так делает Addon Builder
    for d in sorted(dirs):
        entries += d.encode("cp1250") + b"\x00" + b"\xff\xff\xff\xff"
    for name, data in files:
        comp = zlib.compress(data, 9)
        payload = comp if len(comp) < len(data) else data
        entries += (name.encode("cp1250") + b"\x00"
                    + struct.pack("<III",
                                  binascii.crc32(data) & 0xFFFFFFFF,
                                  len(payload), len(data)))
        body += payload
    hdr = (b"VBP\x00" + b"\xff\xff\xff\xff"
           + struct.pack("<I", len(entries) + 1 + len(body))
           + struct.pack("<I", len(body))
           + b"\x00\x00\x00\x00")
    os.makedirs(os.path.dirname(dst_pbo), exist_ok=True)
    with open(dst_pbo, "wb") as f:
        f.write(hdr + entries + b"\x00" + body)
    return dst_pbo, len(hdr) + len(entries) + 1 + len(body)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    staging = sys.argv[1]
    out_client = sys.argv[2]
    out_server = sys.argv[3] if len(sys.argv) > 3 else out_client
    made = 0
    for addon in sorted(os.listdir(staging)):
        src = os.path.join(staging, addon)
        if not os.path.isdir(src):
            continue
        out_dir = out_server if "Server" in addon else out_client
        dst = os.path.join(out_dir, addon + ".pbo")
        path, size = pack_pbo(src, dst)
        print(f"  + {os.path.basename(path)}  ({size} bytes)")
        made += 1
    print(f"Упаковано аддонов: {made}")


if __name__ == "__main__":
    main()

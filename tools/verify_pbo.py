#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_pbo.py — валидатор формата BI-PBO (DayZ / ArmA).

Проверяет:
  * магический заголовок "VBP\\0" + 0xFFFFFFFF;
  * поля reserved(0x08)=0, sizeHeader(0x0C), sizeData(0x10);
  * таблицу записей начиная с 0x170 (папки: CRC=0xFFFFFFFF,size=0,0);
  * что сумма packed-размеров файлов == sizeData;
  * полный размер файла = 0x170 + sizeHeader + sizeData + digest(460);
  * CRC32 каждого файла после zlib-декомпрессии.

Использование:
    python3 tools/verify_pbo.py release/@Uness/Addons/*.pbo ...
Возвращает код 0, если все файлы корректны.
"""
import binascii
import os
import struct
import sys
import zlib

HEADER_START = 0x170   # фиксированный размер шапки PBO у Bohemia
DIGEST_LEN = 0x1CC     # блок подписи/digest в конце unsigned PBO


def verify(path):
    data = open(path, "rb").read()
    errors = []
    if data[:4] != b"VBP\x00":
        print(f"BAD {path}: magic != VBP\\0")
        return False
    reserved, size_header, size_data = struct.unpack("<III", data[8:20])
    if reserved != 0:
        errors.append(f"reserved должен быть 0, получен {reserved}")

    table = data[HEADER_START:HEADER_START + size_header]
    i = 0
    files = []
    dirs = 0
    while i < len(table):
        if table[i] == 0:            # терминатор таблицы "\0"
            break
        z = table.index(b"\x00", i)
        name = table[i:z].decode("cp1250")
        i = z + 1
        crc, packed, unpacked = struct.unpack("<III", table[i:i + 12])
        i += 12
        if packed == 0 and unpacked == 0 and crc == 0xFFFFFFFF:
            dirs += 1
            continue
        files.append((name, crc, packed, unpacked))

    off = HEADER_START + size_header
    for name, crc, packed, unpacked in files:
        raw = data[off:off + packed]
        off += packed
        try:
            dec = zlib.decompress(raw) if packed != unpacked else raw
        except Exception:
            errors.append(f"{name}: не декомпрессируется")
            continue
        if binascii.crc32(dec) & 0xFFFFFFFF != crc:
            errors.append(f"{name}: CRC MISMATCH")

    expected_len = HEADER_START + size_header + size_data + DIGEST_LEN
    if len(data) != expected_len:
        errors.append(f"размер файла {len(data)} != ожидаемый {expected_len}")
    if sum(f[2] for f in files) != size_data:
        errors.append(f"sizeData={size_data} != сумма packed={sum(f[2] for f in files)}")

    ok = not errors
    print(("OK  " if ok else "BAD ") +
          f"{os.path.basename(path):28s} size={len(data):7d}B "
          f"hdr={size_header} data={size_data} files={len(files)} dirs={dirs}")
    for e in errors:
        print("   !!", e)
    return ok


def main():
    paths = sys.argv[1:]
    if not paths:
        print(__doc__)
        sys.exit(1)
    results = [verify(p) for p in paths]
    print("=== ALL PBO VALID ===" if all(results) else "=== ERRORS FOUND ===")
    sys.exit(0 if all(results) else 2)


if __name__ == "__main__":
    main()

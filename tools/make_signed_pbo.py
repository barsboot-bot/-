#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_signed_pbo.py — добавляет корректный SHA1 digest к unsigned PBO,
чтобы Addon Builder / dllsigned принимали файл при подписи.

BI-PBO digest-блок (460 байт в конце файла):
  0x000  uint32   = 0x1CC (размер блока)
  0x004  20 bytes = SHA1(header + entries + data)  <-- Addon Builder сверяет его
  0x018  ...      = padding нулями (остаток до 460)

Использование:  python3 tools/make_signed_pbo.py <файл.pbo> [ещё.pbo ...]
"""
import hashlib
import struct
import sys

DIGEST_LEN = 0x1CC


def fix(path):
    data = open(path, "rb").read()
    if data[:4] != b"VBP\x00":
        print(f"SKIP {path}: не PBO"); return False
    size_header, size_data = struct.unpack("<II", data[0xC:0x14])
    body_end = 0x170 + size_header + size_data
    content = data[:body_end]
    old_digest = data[body_end:]
    if len(old_digest) != DIGEST_LEN:
        print(f"BAD  {path}: digest-блок {len(old_digest)} != 460"); return False
    sha1 = hashlib.sha1(content).digest()          # 20 байт
    block = struct.pack("<I", DIGEST_LEN) + sha1 + b"\x00" * (DIGEST_LEN - 4 - 20)
    with open(path, "wb") as f:
        f.write(content + block)
    print(f"OK   {path}: SHA1={sha1.hex()}")
    return True


if __name__ == "__main__":
    ok = all(fix(p) for p in sys.argv[1:])
    sys.exit(0 if ok else 1)

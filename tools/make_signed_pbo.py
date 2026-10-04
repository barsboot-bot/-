#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_signed_pbo.py — приводит digest-блок unsigned PBO к каноническому виду.

ОШИБКА ПРЕДЫДУЩИХ ВЕРСИЙ: сюда вставлялся SHA1(header+entries+data).
Addon Builder/dllsigned для НЕподписанного PBO ожидает digest-блок из
460 НУЛЕЙ; непустой блок он трактует как повреждённую подпись => "Failed to sign".

Теперь скрипт: заменяет любой digest-блок на 460 нулей (или добавляет его).

Использование:  python3 tools/make_signed_pbo.py <файл.pbo> [...]
"""
import struct, sys

DIGEST_LEN = 0x1CC

def fix(path):
    data = open(path, "rb").read()
    if data[:4] != b"VBP\x00":
        print(f"SKIP {path}: не PBO"); return False
    size_header, size_data = struct.unpack_from("<II", data, 0xC)
    body_end = 0x170 + size_header + size_data
    content = data[:body_end]
    block = b"\x00" * DIGEST_LEN          # unsigned PBO: 460 нулей
    with open(path, "wb") as f:
        f.write(content + block)
    print(f"OK   {path}: digest -> 460 zero bytes (total {len(content)+DIGEST_LEN} B)")
    return True

if __name__ == "__main__":
    ok = all(fix(p) for p in sys.argv[1:])
    sys.exit(0 if ok else 1)

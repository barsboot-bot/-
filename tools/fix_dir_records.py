#!/usr/bin/env python3
"""Fix directory records in PBOs packed by tools/pack_pbo.py.

Bug: pack_pbo.py wrote directory entries as name + 4 bytes (0xFFFFFFFF),
but the BI-PBO format requires name + 12 bytes: CRC=0xFFFFFFFF, packedSize=0,
unpackedSize=0. Addon Builder / DayZ misparses the table after that
("Data file too short ... Expected -369286256 B").

This tool rewrites affected PBOs in place (header block stays 0x170 bytes).
Usage: python3 tools/fix_dir_records.py <pbo> [<pbo> ...]
"""
import struct, sys


def fix(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'VBP\x00', path + ': not a VBP PBO'
    reserved, sizeHeader, sizeData = struct.unpack_from('<III', d, 8)
    off = 0x170
    end = 0x170 + sizeHeader
    out_entries = bytearray()
    ndirs = nfiles = 0
    while off < end:
        z = d.index(b'\x00', off)
        name = d[off:z]
        off = z + 1
        if name == b'':
            break
        # detect directory record: exactly 4 bytes 0xFFFFFFFF follow the name,
        # and the next byte starts another name (not part of a plausible 12-byte record)
        marker = d[off:off + 4]
        if marker == b'\xff\xff\xff\xff':
            rest = d[off + 4:off + 12]
            # if it was a real file record, bytes 4..12 would be two sane sizes;
            # check whether treating it as 12-byte record yields valid sizes
            crc, psz, usz = struct.unpack_from('<III', d, off)
            if psz == 0 and usz == 0:
                # already correct 12-byte dir record
                out_entries += name + b'\x00' + struct.pack('<III', crc, psz, usz)
                ndirs += 1
                off += 12
                continue
            # broken 4-byte dir record -> normalize to 12 bytes
            out_entries += name + b'\x00' + struct.pack('<III', 0xFFFFFFFF, 0, 0)
            ndirs += 1
            off += 4
            continue
        crc, psz, usz = struct.unpack_from('<III', d, off)
        off += 12
        out_entries += name + b'\x00' + struct.pack('<III', crc, psz, usz)
        nfiles += 1
    out_entries += b'\x00'
    body = d[end:end + sizeData]
    trailer = d[0x170 + sizeHeader + sizeData:]
    new_sh = len(out_entries)
    hdr = bytearray(d[:0x170])
    struct.pack_into('<I', hdr, 0x0C, new_sh)
    struct.pack_into('<I', hdr, 0x10, len(body))
    with open(path, 'wb') as f:
        f.write(bytes(hdr) + bytes(out_entries) + body + trailer)
    print(f'FIXED {path}: dirs={ndirs} files={nfiles} sizeHeader {sizeHeader}->{new_sh}')


if __name__ == '__main__':
    for p in sys.argv[1:]:
        fix(p)

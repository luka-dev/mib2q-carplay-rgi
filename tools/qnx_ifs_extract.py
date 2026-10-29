#!/usr/bin/env python3
"""Extract files from an uncompressed QNX 6 IFS image (dumpifs -u output).

Read-only: the image is never modified. Extraction writes only to the
requested output directory. Layout follows sys/image.h from the QNX
dumpifs sources (image_header / union image_dirent).
"""
from __future__ import annotations

import hashlib
import os
import struct
import sys
from pathlib import Path

S_IFMT = 0o170000
S_IFDIR = 0o040000
S_IFREG = 0o100000
S_IFLNK = 0o120000


def find_header(raw: bytes) -> int:
    idx = 0
    while True:
        sig = raw.find(b"imagefs", idx)
        if sig < 0:
            raise ValueError("no valid imagefs header found")
        idx = sig + 1
        if sig + 8 + 12 > len(raw):
            continue
        image_size, hdr_dir_size, dir_offset = struct.unpack_from("<III", raw, sig + 8)
        if not (0 < image_size <= len(raw) - sig):
            continue
        if not (0 < dir_offset < image_size and 0 < hdr_dir_size <= image_size):
            continue
        first = struct.unpack_from("<H", raw, sig + dir_offset)[0]
        if 16 <= first <= 4096:
            return sig


def extract(raw: bytes, out: Path, only: set[str] | None = None) -> list[str]:
    sig = find_header(raw)
    image_size, hdr_dir_size, dir_offset = struct.unpack_from("<III", raw, sig + 8)
    pos = sig + dir_offset
    end = sig + dir_offset + hdr_dir_size
    written = []
    while pos < end:
        size, extattr, ino = struct.unpack_from("<HHI", raw, pos)
        if size < 24 or ino == 0:
            break
        mode, gid, uid, mtime = struct.unpack_from("<IIII", raw, pos + 8)
        kind = mode & S_IFMT
        if kind == S_IFDIR:
            name = raw[pos + 24:pos + size].split(b"\0")[0].decode("utf-8", "replace")
            (out / name).mkdir(parents=True, exist_ok=True)
        elif kind == S_IFREG:
            foff, fsize = struct.unpack_from("<II", raw, pos + 24)
            name = raw[pos + 32:pos + size].split(b"\0")[0].decode("utf-8", "replace")
            if only is None or name in only:
                dest = out / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                data = raw[sig + foff:sig + foff + fsize]
                dest.write_bytes(data)
                written.append(f"{name}\t{len(data)}\t{hashlib.sha256(data).hexdigest()}")
        elif kind == S_IFLNK:
            name = raw[pos + 28:pos + size].split(b"\0")[0].decode("utf-8", "replace")
            sym_off, sym_size = struct.unpack_from("<HH", raw, pos + 24)
            # sym_offset is relative to the path field of this dirent
            target = raw[pos + 28 + sym_off:pos + 28 + sym_off + sym_size].split(b"\0")[0]
            dest = out / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            if not dest.exists():
                dest.symlink_to(target.decode("utf-8", "replace"))
            written.append(f"{name} -> {target.decode('utf-8', 'replace')}\tsymlink")
        pos += size
    return written


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        print("usage: qnx_ifs_extract.py <raw-ifs-image> <out-dir> [match-substring ...]")
        return 2
    image, out = Path(sys.argv[1]), Path(sys.argv[2])
    raw = image.read_bytes()
    out.mkdir(parents=True, exist_ok=True)
    only = None
    if len(sys.argv) > 3:
        names = []
        # Match by path substring requires enumerating first; use a two-pass scan.
        names = scan_names(raw, sys.argv[3:])
        only = set(names)
    lines = extract(raw, out, only)
    sys.stdout.write("\n".join(lines) + "\n")
    return 0


def scan_names(raw: bytes, patterns: list[str]) -> list[str]:
    sig = find_header(raw)
    image_size, hdr_dir_size, dir_offset = struct.unpack_from("<III", raw, sig + 8)
    pos = sig + dir_offset
    end = sig + dir_offset + hdr_dir_size
    names = []
    while pos < end:
        size, extattr, ino = struct.unpack_from("<HHI", raw, pos)
        if size < 24 or ino == 0:
            break
        mode = struct.unpack_from("<I", raw, pos + 8)[0]
        kind = mode & S_IFMT
        if kind == S_IFREG:
            name = raw[pos + 32:pos + size].split(b"\0")[0].decode("utf-8", "replace")
            if any(p in name for p in patterns):
                names.append(name)
        pos += size
    return names


if __name__ == "__main__":
    raise SystemExit(main())

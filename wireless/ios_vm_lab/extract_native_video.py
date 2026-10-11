#!/usr/bin/env python3
"""Extract complete bounded decoder-input records from the owned QNX trace."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

MARKER = "[native-video-qemu] "
LIMIT = 2097152


def extract(text: str) -> tuple[bytes, int]:
    output = bytearray()
    pending = None
    records = 0
    for line in text.replace("\r", "").splitlines():
        if MARKER not in line:
            continue
        message = line.split(MARKER, 1)[1]
        begin = re.fullmatch(r"begin=(\d+) bytes=(\d+)", message)
        chunk = re.fullmatch(r"record=(\d+) offset=(\d+) hex=([0-9a-f]+)", message)
        end = re.fullmatch(r"end=(\d+) complete=([01])", message)
        if begin:
            record, size = map(int, begin.groups())
            if pending is not None or record != records+1 or not 1 <= record <= 32 or not 1 <= size <= 262144:
                raise ValueError("invalid native video record start")
            if len(output)+size > LIMIT:
                raise ValueError("native video capture exceeds its byte bound")
            pending = (record, size, bytearray())
        elif chunk:
            record, offset = map(int, chunk.groups()[:2])
            hex_bytes = chunk[3]
            if pending is None or record != pending[0] or offset != len(pending[2]) or len(hex_bytes)%2:
                raise ValueError("missing, duplicate or reordered video chunk")
            if not 2 <= len(hex_bytes) <= 512:
                raise ValueError("native video chunk exceeds its byte bound")
            data = bytes.fromhex(hex_bytes)
            if len(pending[2])+len(data) > pending[1]:
                raise ValueError("native video record exceeds its declared size")
            pending[2].extend(data)
        elif end:
            record, complete = map(int, end.groups())
            if pending is None or record != pending[0] or not complete or len(pending[2]) != pending[1]:
                raise ValueError("incomplete native video record")
            output.extend(pending[2])
            pending = None
            records += 1
        else:
            raise ValueError("malformed native video capture marker")
    if pending is not None or not records:
        raise ValueError("native video capture is missing complete records")
    if not output.startswith((b"\0\0\0\1", b"\0\0\1")):
        raise ValueError("expected Annex B decoder input")
    return bytes(output), records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path, default=Path("build/ios-vm-lab/combined-usb/native-carplay.h264"))
    args = parser.parse_args()
    if args.log.stat().st_size > 33554432:
        parser.error("QNX trace exceeds 32 MiB")
    lab = Path(__file__).resolve().parents[2]/"build/ios-vm-lab"
    target = args.output.resolve()
    if not target.is_relative_to(lab.resolve()) or target.suffix != ".h264":
        parser.error("output must be an .h264 file inside the owned laboratory")
    try:
        data, records = extract(args.log.read_text(errors="replace"))
    except ValueError as error:
        target.unlink(missing_ok=True)
        parser.error(str(error))
    target.write_bytes(data)
    print(json.dumps({"records": records, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                      "output": str(target)}))


if __name__ == "__main__":
    main()

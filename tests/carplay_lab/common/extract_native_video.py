#!/usr/bin/env python3
"""Extract complete bounded decoder-input records from the owned QNX trace."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

MARKER = "[native-video-qemu] "
LIMIT = 16777216
RECORD_LIMIT = 4096


def parse_records(text: str, marker: str = MARKER) -> list[tuple[bytes, int | None]]:
    if marker not in (MARKER,"[native-cluster-qemu] "):
        raise ValueError("unknown capture stream")
    output = bytearray()
    pending = None
    records = 0
    complete_records = []
    for line in text.replace("\r", "").splitlines():
        if marker not in line:
            continue
        message = line.split(marker, 1)[1]
        begin = re.fullmatch(r"begin=(\d+) bytes=(\d+)(?: mono_ns=(\d+))?", message)
        chunk = re.fullmatch(r"record=(\d+) offset=(\d+) hex=([0-9a-f]+)", message)
        end = re.fullmatch(r"end=(\d+) complete=([01])", message)
        if begin:
            record, size = map(int, begin.groups()[:2])
            timestamp = int(begin[3]) if begin[3] is not None else None
            if timestamp is not None and not 0 <= timestamp <= 2**64-1:
                raise ValueError("invalid monotonic timestamp")
            if pending is not None or record != records+1 or not 1 <= record <= RECORD_LIMIT or not 1 <= size <= 262144:
                raise ValueError("invalid native video record start")
            if len(output)+size > LIMIT:
                raise ValueError("native video capture exceeds its byte bound")
            pending = (record, size, bytearray(), timestamp)
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
            complete_records.append((bytes(pending[2]), pending[3]))
            pending = None
            records += 1
        else:
            raise ValueError("malformed native video capture marker")
    if pending is not None or not records:
        raise ValueError("native video capture is missing complete records")
    if not output.startswith((b"\0\0\0\1", b"\0\0\1")):
        raise ValueError("expected Annex B decoder input")
    return complete_records


def extract(text: str, marker: str = MARKER) -> tuple[bytes, int]:
    records = parse_records(text,marker)
    return b"".join(data for data, _ in records), len(records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--cluster", action="store_true", help="extract authenticated stream-111 Annex B records")
    parser.add_argument("--lab-dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.log.stat().st_size > 67108864:
        parser.error("QNX trace exceeds 64 MiB")
    build = Path(__file__).resolve().parents[3]/"build"
    lab = (args.lab_dir or build/"ios-vm-lab").resolve()
    if not lab.is_relative_to(build.resolve()) or lab == build.resolve():
        parser.error("lab directory must be inside the owned build tree")
    default = lab/"native-carplay.h264" if args.lab_dir else lab/"combined-usb/native-carplay.h264"
    target = (args.output or default).resolve()
    if not target.is_relative_to(lab.resolve()) or target.suffix != ".h264":
        parser.error("output must be an .h264 file inside the owned laboratory")
    try:
        data, records = extract(args.log.read_text(errors="replace"),
                                "[native-cluster-qemu] " if args.cluster else MARKER)
    except ValueError as error:
        target.unlink(missing_ok=True)
        parser.error(str(error))
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    print(json.dumps({"records": records, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                      "output": str(target)}))


if __name__ == "__main__":
    main()

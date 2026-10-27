#!/usr/bin/env python3
"""Apply or reverse the experimental MU1316 SI CarPlay re-arm patch."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import sys


STOCK_SHA256 = "a25dc981e5591cb3c6b09f00fd95de23803dd3daa0f562e7e8d60b6d0e1b0d02"
PATCHED_SHA256 = "606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3"
EXPECTED_SIZE = 802_798
PATCH_VA = 0x17FDB4
PATCH_OFFSET = 0x7FDB4
STOCK_BYTES = bytes.fromhex("00 30 a0 e3")  # MOV R3, #0
PATCHED_BYTES = bytes.fromhex("2e 33 09 e0")  # AND R3, R9, LR, LSR #6
CONTEXT_OFFSET = PATCH_OFFSET - 8
STOCK_CONTEXT = bytes.fromhex(
    "31 25 d6 e5 d8 00 84 e2 00 30 a0 e3 08 c0 8c e2 04 e0 8d e5"
)
PATCHED_CONTEXT = bytes.fromhex(
    "31 25 d6 e5 d8 00 84 e2 2e 33 09 e0 08 c0 8c e2 04 e0 8d e5"
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def image_state(data: bytes) -> str:
    if len(data) != EXPECTED_SIZE:
        return "unknown"
    sha256 = digest(data)
    if sha256 == STOCK_SHA256 and data[CONTEXT_OFFSET : CONTEXT_OFFSET + len(STOCK_CONTEXT)] == STOCK_CONTEXT:
        return "stock"
    if sha256 == PATCHED_SHA256 and data[CONTEXT_OFFSET : CONTEXT_OFFSET + len(PATCHED_CONTEXT)] == PATCHED_CONTEXT:
        return "patched"
    return "unknown"


def load_known_image(path: Path, expected_state: str) -> bytearray:
    data = bytearray(path.read_bytes())
    state = image_state(data)
    if state != expected_state:
        raise ValueError(
            f"{path}: expected {expected_state} MU1316 P5087 image, "
            f"found state={state}, size={len(data)}, sha256={digest(data)}"
        )
    return data


def write_new_image(source: Path, output: Path, data: bytes) -> None:
    if source.resolve() == output.resolve():
        raise ValueError("in-place patching is deliberately disabled")
    if output.exists():
        raise ValueError(f"output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, output)
    output.write_bytes(data)
    os.chmod(output, source.stat().st_mode)


def command_check(image: Path) -> int:
    data = image.read_bytes()
    state = image_state(data)
    print(f"state={state} size={len(data)} sha256={digest(data)}")
    if state != "unknown":
        print(f"patch_va=0x{PATCH_VA:x} patch_offset=0x{PATCH_OFFSET:x}")
        return 0
    return 1


def command_apply(source: Path, output: Path) -> int:
    data = load_known_image(source, "stock")
    data[PATCH_OFFSET : PATCH_OFFSET + len(STOCK_BYTES)] = PATCHED_BYTES
    if image_state(data) != "patched":
        raise ValueError("internal verification failed after applying patch")
    write_new_image(source, output, data)
    print(f"wrote patched image: {output}")
    print(f"sha256={digest(data)}")
    return 0


def command_revert(source: Path, output: Path) -> int:
    data = load_known_image(source, "patched")
    data[PATCH_OFFSET : PATCH_OFFSET + len(PATCHED_BYTES)] = STOCK_BYTES
    if image_state(data) != "stock":
        raise ValueError("internal verification failed after reversing patch")
    write_new_image(source, output, data)
    print(f"wrote stock image: {output}")
    print(f"sha256={digest(data)}")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    check = subparsers.add_parser("check", help="identify an input image")
    check.add_argument("image", type=Path)

    apply_cmd = subparsers.add_parser("apply", help="write a patched copy")
    apply_cmd.add_argument("stock_image", type=Path)
    apply_cmd.add_argument("output", type=Path)

    revert = subparsers.add_parser("revert", help="write a stock copy")
    revert.add_argument("patched_image", type=Path)
    revert.add_argument("output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "check":
            return command_check(args.image)
        if args.command == "apply":
            return command_apply(args.stock_image, args.output)
        return command_revert(args.patched_image, args.output)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Extract and validate the embedded ADSP-2148x SPI loader from J525 CodeFlash."""

from __future__ import annotations

import argparse
import hashlib
import struct
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


SOURCE_SIZE = 0x180000
SOURCE_SHA256 = "8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b"
SHARC_OFFSET = 0x041014
SHARC_LENGTH = 0x027764
SHARC_END = SHARC_OFFSET + SHARC_LENGTH
BOOT_KERNEL_SIZE = 0x600
FINAL_IVT_SIZE = 0x600
RAW_SHA256 = "659f55e3b57f7cde8ddeb170a5a58e7a985fa4e43ea6898838d4393753e89154"
BITREV_SHA256 = "9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7"
RAW_NAME = "J525_8W0035465_SW0420_SHARC_SPI_raw.ldr.bin"
BITREV_NAME = "J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin"

TAG_NAMES = {
    0: "FINAL_INIT",
    1: "ZERO_LDATA",
    2: "ZERO_L48",
    3: "INIT_L16",
    4: "INIT_L32",
    5: "INIT_L48",
    6: "INIT_L64",
    7: "ZERO_EXT8",
    8: "ZERO_EXT16",
    9: "INIT_EXT8",
    10: "INIT_EXT16",
}


@dataclass(frozen=True)
class Block:
    offset: int
    tag: int
    count: int
    address: int
    payload_size: int


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def bit_reverse_bytes(data: bytes) -> bytes:
    table = bytes(int(f"{value:08b}"[::-1], 2) for value in range(256))
    return data.translate(table)


def payload_size(tag: int, count: int) -> int:
    if tag in (0, 1, 2, 7, 8):
        return 0
    if tag == 3:
        return ((count + 1) // 2) * 4
    if tag == 4:
        return count * 4
    if tag == 5:
        return ((count + 1) // 2) * 12
    if tag == 6:
        return count * 8
    if tag in (9, 10):
        return count * 4
    raise ValueError(f"unknown loader block tag {tag}")


def parse_blocks(decoded: bytes) -> list[Block]:
    position = BOOT_KERNEL_SIZE
    blocks: list[Block] = []
    while position + 12 <= len(decoded):
        header_offset = position
        tag, count, address = struct.unpack_from("<III", decoded, position)
        position += 12
        size = payload_size(tag, count)
        if position + size > len(decoded):
            raise ValueError(
                f"block at {header_offset:#x} overruns image: "
                f"payload {size:#x}, remaining {len(decoded) - position:#x}"
            )
        blocks.append(Block(header_offset, tag, count, address, size))
        position += size
        if tag == 0:
            if len(decoded) - position != FINAL_IVT_SIZE:
                raise ValueError(
                    f"FINAL_INIT leaves {len(decoded) - position:#x} bytes, "
                    f"expected final IVT size {FINAL_IVT_SIZE:#x}"
                )
            position += FINAL_IVT_SIZE
            break
    if not blocks or blocks[-1].tag != 0:
        raise ValueError("loader has no terminal FINAL_INIT block")
    if position != len(decoded):
        raise ValueError(f"loader parse ended at {position:#x}, image ends at {len(decoded):#x}")
    return blocks


def validate(raw: bytes, decoded: bytes) -> list[Block]:
    if len(raw) != SHARC_LENGTH:
        raise ValueError(f"raw length is {len(raw):#x}, expected {SHARC_LENGTH:#x}")
    digest = sha256(raw)
    if digest != RAW_SHA256:
        raise ValueError(f"raw SHA256 is {digest}, expected {RAW_SHA256}")
    if bit_reverse_bytes(decoded) != raw:
        raise ValueError("decoded image is not the byte-bit-reversed raw SPI stream")
    decoded_digest = sha256(decoded)
    if decoded_digest != BITREV_SHA256:
        raise ValueError(f"bit-reversed SHA256 is {decoded_digest}, expected {BITREV_SHA256}")

    blocks = parse_blocks(decoded)
    tag_counts = Counter(block.tag for block in blocks)
    expected_counts = Counter({0: 1, 1: 279, 2: 1, 3: 37, 4: 240, 5: 2})
    if len(blocks) != 560 or tag_counts != expected_counts:
        raise ValueError(f"unexpected block inventory: {len(blocks)} blocks, tags {dict(tag_counts)}")

    first = blocks[0]
    if (first.offset, first.tag, first.count, first.address) != (0x600, 5, 5, 0x8C100):
        raise ValueError(f"unexpected first block: {first}")
    final = blocks[-1]
    if final.offset != 0x27158:
        raise ValueError(f"FINAL_INIT is at {final.offset:#x}, expected 0x27158")
    return blocks


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Extract the complete SHARC SPI boot/application stream embedded in "
            "8W0035465 SW 0420 V850 CodeFlash and emit an analysis-normalized copy."
        )
    )
    parser.add_argument("image", type=Path, help="CodeFlash_24082026_172027.bin")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("assets/soundamp"),
        help="destination directory (default: assets/soundamp)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source = args.image.read_bytes()
    if len(source) != SOURCE_SIZE:
        raise SystemExit(f"source size is {len(source):#x}, expected {SOURCE_SIZE:#x}")
    source_digest = sha256(source)
    if source_digest != SOURCE_SHA256:
        raise SystemExit(f"source SHA256 is {source_digest}, expected {SOURCE_SHA256}")

    raw = source[SHARC_OFFSET:SHARC_END]
    decoded = bit_reverse_bytes(raw)
    try:
        blocks = validate(raw, decoded)
    except ValueError as error:
        raise SystemExit(f"SHARC loader validation failed: {error}") from error

    args.output_dir.mkdir(parents=True, exist_ok=True)
    raw_path = args.output_dir / RAW_NAME
    decoded_path = args.output_dir / BITREV_NAME
    raw_path.write_bytes(raw)
    decoded_path.write_bytes(decoded)

    tag_counts = Counter(block.tag for block in blocks)
    tag_summary = ", ".join(
        f"{tag}:{TAG_NAMES[tag]}={tag_counts[tag]}" for tag in sorted(tag_counts)
    )
    print(f"source_sha256={source_digest}")
    print(f"source_range={SHARC_OFFSET:#x}..{SHARC_END - 1:#x}")
    print(f"raw={raw_path} size={len(raw):#x} sha256={sha256(raw)}")
    print(f"bitrev={decoded_path} size={len(decoded):#x} sha256={sha256(decoded)}")
    print(f"blocks={len(blocks)} tags={tag_summary}")
    print(
        f"first_header=+{blocks[0].offset:#x} tag={blocks[0].tag} "
        f"count={blocks[0].count:#x} address={blocks[0].address:#x}"
    )
    print(f"final_header=+{blocks[-1].offset:#x} final_ivt={FINAL_IVT_SIZE:#x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

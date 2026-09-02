#!/usr/bin/env python3
"""Extract the V850-managed SHARC input mixer matrices from amplifier A07."""

from __future__ import annotations

import argparse
import csv
import hashlib
import sys
from pathlib import Path


A07_SIGNATURE_OFFSET = 0x100000
A07_SELECTOR_OFFSET = 0x100008
A07_PROFILE_OFFSET = 0x100034
A07_PROFILE_SIZE = 0x370
MATRIX_OFFSET = 0x101BB4
FALLBACK_MATRIX_OFFSET = 0x08F33C
PROFILE_MATRIX_SIZE = 0xC0
BANK_MATRIX_SIZE = 0x40
PROFILE_COUNT = 8
BANK_COUNT = 3
CHANNEL_COUNT = 8
SHARC_MATRIX_BASE = 0xC022A

INPUT_NAMES = ("L", "R", "C", "LFE", "Ls", "Rs", "Ch7", "Ch8")
OUTPUT_NAMES = ("Bus_L", "Bus_R", "Bus_C", "Bus_LFE", "Bus_Ls", "Bus_Rs", "Bus_7", "Bus_8")
BANK_NAMES = ("stereo", "5.1", "7.1")


def signed_byte(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Extract the built-in fallback plus all 8 profiles x 3 banks x 8x8 "
            "signed-dB mixer matrices. CSV is written to stdout."
        )
    )
    parser.add_argument("image", type=Path, help="CodeFlash_*.bin")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    image = args.image.read_bytes()
    required = MATRIX_OFFSET + PROFILE_COUNT * PROFILE_MATRIX_SIZE
    if len(image) < required:
        raise SystemExit(f"image is too short: {len(image):#x}, need at least {required:#x}")
    if image[A07_SIGNATURE_OFFSET : A07_SIGNATURE_OFFSET + 8] != b"AD301A07":
        raise SystemExit("AD301A07 signature is missing at file offset 0x100000")

    digest = hashlib.sha256(image).hexdigest()
    print(f"# image_sha256={digest}")
    print("# gain_db=-100 is clamped to approximately -99 dB by FUN_000a9040")
    writer = csv.writer(sys.stdout, lineterminator="\n")
    writer.writerow(
        (
            "profile",
            "profile_source",
            "bank",
            "bank_role",
            "input_index",
            "input_name",
            "output_index",
            "output_name",
            "gain_db",
            "file_offset",
            "sharc_parameter_address",
        )
    )

    profiles = [(-1, "builtin_fallback", FALLBACK_MATRIX_OFFSET)]
    profiles.extend(
        (profile, "A07", MATRIX_OFFSET + profile * PROFILE_MATRIX_SIZE)
        for profile in range(PROFILE_COUNT)
    )
    for profile, profile_source, profile_base in profiles:
        for bank in range(BANK_COUNT):
            matrix_base = profile_base + bank * BANK_MATRIX_SIZE
            for input_index, input_name in enumerate(INPUT_NAMES):
                for output_index, output_name in enumerate(OUTPUT_NAMES):
                    file_offset = matrix_base + input_index * CHANNEL_COUNT + output_index
                    # FUN_000a9040 transposes the row-major V850 table into
                    # SHARC parameter memory: eight consecutive inputs per output.
                    sharc_address = SHARC_MATRIX_BASE + output_index * CHANNEL_COUNT + input_index
                    writer.writerow(
                        (
                            profile,
                            profile_source,
                            bank,
                            BANK_NAMES[bank],
                            input_index + 1,
                            input_name,
                            output_index + 1,
                            output_name,
                            signed_byte(image[file_offset]),
                            f"0x{file_offset:06x}",
                            f"0x{sharc_address:05x}",
                        )
                    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

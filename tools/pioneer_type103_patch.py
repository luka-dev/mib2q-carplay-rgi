#!/usr/bin/env python3
"""Verify or patch Pioneer's type-103 5.1.2-to-5.1 downmix on a copy.

This is deliberately fingerprint-locked to SPHERA 1.030010.  It never edits
the input file and refuses an existing output path.  ``--check`` performs all
fingerprint and expected-byte checks without writing anything.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
from dataclasses import dataclass
from pathlib import Path


DONOR_SHA256 = "d8ac9aa2247d43627228d6538496ee0757e8f6eaa50526babc44dd697b971790"
PATCHED_SHA256 = "91893c9e201d9c3b3552bb18a42fec484cc70143fc368d91f30c1bcedac3d310"
NOP = bytes.fromhex("1f2003d5")
MATRIX_5_1_VA = 0x4316C8


@dataclass(frozen=True)
class Patch:
    name: str
    offset: int
    expected: bytes
    replacement: bytes


PATCHES = (
    Patch(
        "downmixer_output_channels_primary_4_to_6",
        0x14AFC0,
        bytes.fromhex("820080d2"),  # mov x2, #4
        bytes.fromhex("c20080d2"),  # mov x2, #6
    ),
    Patch(
        "downmixer_output_channels_subwoofer_5_to_6",
        0x14B0D4,
        bytes.fromhex("a20080d2"),  # mov x2, #5
        bytes.fromhex("c20080d2"),  # mov x2, #6
    ),
    Patch(
        "declare_subwoofer_available",
        0x14B020,
        bytes.fromhex("1f140071"),  # cmp w0, #5
        bytes.fromhex("1f00006b"),  # cmp w0, w0 (forces following b.eq)
    ),
    Patch(
        "source_capacity_for_six_output_channels",
        0x14B554,
        bytes.fromhex(
            "001700b0810a148b9502148b00ac45f9"
            "21fc41d3000040391f1400713510959a"
        ),
        bytes.fromhex(
            "a00080d2"  # mov x0, #5
            "957e009b"  # mul x21, x20, x0
            "600080d2"  # mov x0, #3
            "b50ac09a"  # udiv x21, x21, x0
        )
        + NOP * 4,
    ),
    Patch(
        "output_capacity_six_channels_s16",
        0x14B624,
        bytes.fromhex(
            "011700b0a00a158b0000008bb5f27dd3"
            "21ac45f9210040393f140071b512809a"
        ),
        bytes.fromhex(
            "b506158b"  # add x21, x21, x21, lsl #1 (frames * 3)
            "b5f67ed3"  # lsl x21, x21, #2 (frames * 12 bytes)
        )
        + NOP * 6,
    ),
    Patch(
        "primary_matrix_got_4_0_to_5_1",
        0x41DD58,
        (0x431C70).to_bytes(8, "little"),
        MATRIX_5_1_VA.to_bytes(8, "little"),
    ),
    Patch(
        "subwoofer_matrix_got_4_1_to_5_1",
        0x41DB78,
        (0x431BE0).to_bytes(8, "little"),
        MATRIX_5_1_VA.to_bytes(8, "little"),
    ),
    Patch(
        "primary_matrix_rela_addend_4_0_to_5_1",
        0x6A8F8,
        (0x431C70).to_bytes(8, "little"),
        MATRIX_5_1_VA.to_bytes(8, "little"),
    ),
    Patch(
        "subwoofer_matrix_rela_addend_4_1_to_5_1",
        0x6A370,
        (0x431BE0).to_bytes(8, "little"),
        MATRIX_5_1_VA.to_bytes(8, "little"),
    ),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate(data: bytes) -> None:
    digest = sha256(data)
    if digest != DONOR_SHA256:
        raise ValueError(f"unsupported input SHA-256 {digest}; expected {DONOR_SHA256}")
    for patch in PATCHES:
        actual = data[patch.offset : patch.offset + len(patch.expected)]
        if actual != patch.expected:
            raise ValueError(
                f"{patch.name}: bytes at 0x{patch.offset:x} are {actual.hex()}, "
                f"expected {patch.expected.hex()}"
            )
        if len(patch.expected) != len(patch.replacement):
            raise AssertionError(f"{patch.name}: in-place patch changes size")


def apply_patches(data: bytes) -> bytes:
    validate(data)
    result = bytearray(data)
    for patch in PATCHES:
        end = patch.offset + len(patch.replacement)
        result[patch.offset:end] = patch.replacement
    return bytes(result)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("library", type=Path, help="original libSPF_CP_PlugInWrapper.so")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="verify only; write no files")
    mode.add_argument("--output", type=Path, help="write a patched copy to this new path")
    args = parser.parse_args()

    try:
        data = args.library.read_bytes()
        validate(data)
        if args.output:
            if args.output.exists():
                raise ValueError(f"output already exists: {args.output}")
            if args.output.resolve() == args.library.resolve():
                raise ValueError("input and output paths must differ")
            patched = apply_patches(data)
            if sha256(patched) != PATCHED_SHA256:
                raise AssertionError("patched output fingerprint does not match the reviewed recipe")
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(patched)
            shutil.copymode(args.library, args.output)
            print(f"wrote {args.output}")
            print(f"patched_sha256={sha256(patched)}")
        else:
            print(f"verified_sha256={DONOR_SHA256}")
        for patch in PATCHES:
            print(
                f"0x{patch.offset:08x} {patch.name}: "
                f"{patch.expected.hex()} -> {patch.replacement.hex()}"
            )
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

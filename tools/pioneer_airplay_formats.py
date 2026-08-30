#!/usr/bin/env python3
"""Enumerate Pioneer's built-in AirPlay/CarPlay audio format table.

The relevant functions are self-contained AArch64 code in
libSPF_CP_PlugInWrapper.so.  Running them under Unicorn is less error-prone
than manually decoding their compiler-generated switch tables.  The target
library is only read; no firmware files are modified.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from elftools.elf.elffile import ELFFile
from unicorn import UC_ARCH_ARM64, UC_HOOK_CODE, UC_MODE_ARM, UC_PROT_ALL, Uc
from unicorn.arm64_const import (
    UC_ARM64_REG_PC,
    UC_ARM64_REG_SP,
    UC_ARM64_REG_X0,
    UC_ARM64_REG_X1,
    UC_ARM64_REG_X2,
    UC_ARM64_REG_X30,
)


PAGE_SIZE = 0x1000
IMAGE_BASE = 0x0
IMAGE_SIZE = 0x500000
STACK_BASE = 0x10000000
STACK_SIZE = 0x20000
SCRATCH_BASE = 0x20000000
SCRATCH_SIZE = 0x10000
RETURN_SENTINEL = 0x30000000

# Addresses in AirPlay/480.7.4 as shipped in SPHERA 1.030010 (KB122).
FORMAT_TO_ASBD = 0x130790
FORMAT_TO_PCM_EQUIVALENT = 0x130C00
FORMAT_TO_CHANNEL_LAYOUT = 0x130EA0

# The three functions use the same stack-canary GOT slot.  Relocations are
# not applied in the file, so point the slot at controlled scratch memory.
STACK_GUARD_GOT = 0x42CE00
STACK_GUARD_VALUE = SCRATCH_BASE + 0x2000


def align_down(value: int) -> int:
    return value & ~(PAGE_SIZE - 1)


def align_up(value: int) -> int:
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)


def fourcc(value: int) -> str:
    raw = value.to_bytes(4, "big")
    if all(0x20 <= byte < 0x7F for byte in raw):
        return raw.decode("ascii")
    return f"0x{value:08x}"


class PioneerFormatTable:
    def __init__(self, library: Path) -> None:
        self.uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.uc.mem_map(IMAGE_BASE, IMAGE_SIZE - IMAGE_BASE, UC_PROT_ALL)
        self.uc.mem_map(STACK_BASE, STACK_SIZE, UC_PROT_ALL)
        self.uc.mem_map(SCRATCH_BASE, SCRATCH_SIZE, UC_PROT_ALL)
        self._load_elf(library)

        self.uc.mem_write(STACK_GUARD_GOT, struct.pack("<Q", STACK_GUARD_VALUE))
        self.uc.mem_write(STACK_GUARD_VALUE, struct.pack("<Q", 0xA55AA55AA55AA55A))
        self.uc.hook_add(UC_HOOK_CODE, self._stop_at_sentinel)

    def _load_elf(self, library: Path) -> None:
        with library.open("rb") as stream:
            elf = ELFFile(stream)
            for segment in elf.iter_segments():
                if segment["p_type"] != "PT_LOAD":
                    continue
                start = int(segment["p_vaddr"])
                end = start + int(segment["p_memsz"])
                if start < IMAGE_BASE or align_up(end) > IMAGE_SIZE:
                    raise ValueError(
                        f"PT_LOAD 0x{start:x}-0x{end:x} is outside emulator image"
                    )
                self.uc.mem_write(start, segment.data())

    def _stop_at_sentinel(self, uc: Uc, address: int, _size: int, _data: object) -> None:
        if address == RETURN_SENTINEL:
            uc.emu_stop()

    def call(self, address: int, x0: int, x1: int = 0, x2: int = 0) -> int:
        self.uc.reg_write(UC_ARM64_REG_SP, STACK_BASE + STACK_SIZE - 0x10)
        self.uc.reg_write(UC_ARM64_REG_X0, x0)
        self.uc.reg_write(UC_ARM64_REG_X1, x1)
        self.uc.reg_write(UC_ARM64_REG_X2, x2)
        self.uc.reg_write(UC_ARM64_REG_X30, RETURN_SENTINEL)
        self.uc.reg_write(UC_ARM64_REG_PC, address)
        self.uc.emu_start(address, RETURN_SENTINEL, count=10000)
        return self.uc.reg_read(UC_ARM64_REG_X0)

    def describe(self, index: int) -> dict[str, int | str] | None:
        asbd_address = SCRATCH_BASE
        auxiliary_address = SCRATCH_BASE + 0x100
        self.uc.mem_write(asbd_address, bytes(0x100))
        self.uc.mem_write(auxiliary_address, bytes(0x100))
        status = self.call(FORMAT_TO_ASBD, index, asbd_address, auxiliary_address)
        if status & 0xFFFFFFFF:
            return None

        words = struct.unpack("<9I", bytes(self.uc.mem_read(asbd_address, 36)))
        pcm_equivalent = self.call(FORMAT_TO_PCM_EQUIVALENT, index)
        if pcm_equivalent == 0xFFFFFFFFFFFFFFFF:
            pcm_equivalent = -1
        layout = self.call(FORMAT_TO_CHANNEL_LAYOUT, index) & 0xFFFFFFFF
        return {
            "index": index,
            "rate": words[0],
            "codec": fourcc(words[1]),
            "flags": words[2],
            "bytes_per_packet": words[3],
            "frames_per_packet": words[4],
            "bytes_per_frame": words[5],
            "channels": words[6],
            "bits": words[7],
            "layout": layout,
            "pcm_equivalent": pcm_equivalent,
        }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("library", type=Path)
    parser.add_argument("--first", type=int, default=2)
    parser.add_argument("--last", type=int, default=84)
    parser.add_argument(
        "--codec",
        help="only print this FourCC (for example apac or lpcm)",
    )
    args = parser.parse_args()

    table = PioneerFormatTable(args.library)
    print(
        "index codec rate channels bits bytes/frame layout       pcm-equivalent"
    )
    for index in range(args.first, args.last + 1):
        item = table.describe(index)
        if item is None or (args.codec and item["codec"] != args.codec):
            continue
        print(
            f"{item['index']:5d} {item['codec']:>5} {item['rate']:5d} "
            f"{item['channels']:8d} {item['bits']:4d} "
            f"{item['bytes_per_frame']:11d} 0x{item['layout']:08x} "
            f"{item['pcm_equivalent']:14d}"
        )


if __name__ == "__main__":
    main()

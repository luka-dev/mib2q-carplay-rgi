#!/usr/bin/env python3
"""Build a separate stock-libairplay variant advertising CarPlay Control.

The source image is never modified. This changes one ARM MOV immediate in
AirPlayGetFeatures: upper 32-bit feature word 0x01 -> 0x21 (bit 37).
"""

import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / (
    "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
    "advanced/MU1316-appimg/eso/lib/libairplay.so"
)
OUT = Path(__file__).resolve().parent / "out" / "libairplay.feature37.offline.so"
REPORT = OUT.parent / "build_report.json"
EXPECTED_SHA256 = "01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2"
INSTRUCTION_VADDR = 0x20D74
OLD = bytes.fromhex("01 30 a0 e3")  # MOV R3,#1
NEW = bytes.fromhex("21 30 a0 e3")  # MOV R3,#0x21


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def vaddr_to_offset(data: bytes, address: int) -> int:
    if data[:16] != b"\x7fELF\x01\x01\x01" + bytes(9):
        raise ValueError("expected ELF32 little-endian image")
    if struct.unpack_from("<HH", data, 16) != (3, 40):
        raise ValueError("expected ARM ET_DYN")
    phoff = struct.unpack_from("<I", data, 28)[0]
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    for number in range(phnum):
        base = phoff + number * phentsize
        kind, offset, vaddr, _, filesz, _, flags, _ = struct.unpack_from("<IIIIIIII", data, base)
        if kind == 1 and vaddr <= address < vaddr + filesz:
            if flags & 1 == 0:
                raise ValueError("feature instruction is outside executable LOAD")
            return offset + address - vaddr
    raise ValueError("feature instruction is not file-backed")


def main() -> None:
    original = SOURCE.read_bytes()
    if sha256(original) != EXPECTED_SHA256:
        raise ValueError("source libairplay SHA-256 mismatch")
    offset = vaddr_to_offset(original, INSTRUCTION_VADDR)
    if original[offset : offset + 4] != OLD:
        raise ValueError("AirPlayGetFeatures instruction mismatch")
    patched = bytearray(original)
    patched[offset : offset + 4] = NEW
    difference = [index for index, (a, b) in enumerate(zip(original, patched)) if a != b]
    if difference != [offset]:
        raise ValueError(f"unexpected changed offsets: {difference}")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(patched)
    OUT.chmod(SOURCE.stat().st_mode & 0o777)
    REPORT.write_text(json.dumps({
        "source": str(SOURCE),
        "source_sha256": EXPECTED_SHA256,
        "output": str(OUT),
        "output_sha256": sha256(patched),
        "vaddr": hex(INSTRUCTION_VADDR),
        "file_offset": hex(offset),
        "before": OLD.hex(),
        "after": NEW.hex(),
        "changed_offsets": [hex(index) for index in difference],
    }, indent=2) + "\n")
    print(REPORT.read_text(), end="")


if __name__ == "__main__":
    main()

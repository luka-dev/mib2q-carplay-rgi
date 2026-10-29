#!/usr/bin/env python3
"""
Audi B&O J525 Amplifier (8W0035465 SW 0420 / AMP16bAPN) Dataset & SWUP Patcher.

Supports:
1. Extracting & verifying partitions from V850 CodeFlash_24082026_172027.bin:
   - Bootloader (Pbl): 0x000000..0x01FFFF (128 KB, CRC-16/CCITT-FALSE @ 0x01FFFC)
   - Application (Apl): 0x040000..0x0FFFFF (768 KB, CRC-16/CCITT-FALSE @ 0x0FFFFC)
   - Sound Dataset (AD301A07): 0x100000..0x10FFFF (64 KB, UDS 0x000D0000, CRC16 @ 0x10FFFE)
   - 3D Dataset (AD301B01): 0x120000..0x127FFF (32 KB, UDS 0x000E0000, CRC16 @ 0x127FFE)

2. Patching AD301A07 (64 KB dataset for ODIS-E UDS 0x34/0x36 DataTransfer @ 0x000D0000):
   - --flat-loudness : Zeroes all 16x3 gains of C4AB7 (0x102E40..0x102E9F) to eliminate
                       the +6..+20.5 dB 55 Hz bass bloat and -3.5 dB 10 kHz cut.
   - --true-3d-off   : Copies Row 0 (Linear mode: first-B muted to -100 dB) into Row 1
                       (MMI '3D Effect = Off') across all 8 sub-profiles in AD301A07,
                       and copies Bank 0 flat biquads into Bank 1 at 0x102BA0.
   - --clean-51-matrix: Removes the -3 dB L/R -> Ls/Rs/bus6/bus7 bleed in Bank 1 (5.1 PCM6)
                       input mixing matrices so rear channels play pure discrete/upmix rear.

3. Re-signing with exact V850 CRC-16/CCITT-FALSE (FUN_ram_000c4516 @ 0x000C4516) and
   computing MIB2 metainfo2.txt SHA-1 checksums (CheckSum / CheckSum1).
"""

import argparse
import hashlib
import os
import struct
import sys


def crc16_ccitt_false(data: bytes, init: int = 0xFFFF) -> int:
    """V850 FUN_ram_000c4516 (0x000C4516): CRC-16/CCITT-FALSE (poly=0x1021, init=0xFFFF)."""
    crc = init
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if (crc & 0x8000) != 0:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def verify_codeflash(cf: bytes) -> None:
    if len(cf) != 0x180000:
        raise ValueError(f"Expected 0x180000 bytes (1.5 MB), got 0x{len(cf):X}")

    pbl = cf[0x000000:0x020000]
    pbl_shadow = cf[0x020000:0x040000]
    apl = cf[0x040000:0x100000]
    a07 = cf[0x100000:0x110000]
    b01 = cf[0x120000:0x128000]

    pbl_crc_calc = crc16_ccitt_false(pbl[:-4])
    pbl_crc_stored = struct.unpack(">H", pbl[-4:-2])[0]

    apl_crc_calc = crc16_ccitt_false(apl[:-4])
    apl_crc_stored = struct.unpack(">H", apl[-4:-2])[0]

    a07_crc_calc = crc16_ccitt_false(a07[:-2])
    a07_crc_stored = struct.unpack(">H", a07[-2:])[0]

    b01_crc_calc = crc16_ccitt_false(b01[:-2])
    b01_crc_stored = struct.unpack(">H", b01[-2:])[0]

    print("=== CodeFlash Partition & Checksum Verification ===")
    print(f"  PBL (0x000000..0x01FFFF): magic={pbl[-16:-4]!r} CRC16 calc=0x{pbl_crc_calc:04X} stored=0x{pbl_crc_stored:04X} (shadow match={pbl == pbl_shadow})")
    print(f"  APL (0x040000..0x0FFFFF): magic={apl[-16:-4]!r} CRC16 calc=0x{apl_crc_calc:04X} stored=0x{apl_crc_stored:04X}")
    print(f"    -> metainfo2 CheckSum  (first 512K SHA1): {hashlib.sha1(apl[:0x80000]).hexdigest()}")
    print(f"    -> metainfo2 CheckSum1 (last  256K SHA1): {hashlib.sha1(apl[0x80000:]).hexdigest()}")
    print(f"  A07 (0x100000..0x10FFFF): magic={a07[:8]!r} footer={a07[-16:-2]!r} CRC16 calc=0x{a07_crc_calc:04X} stored=0x{a07_crc_stored:04X}")
    print(f"  B01 (0x120000..0x127FFF): magic={b01[:8]!r} footer={b01[-16:-2]!r} CRC16 calc=0x{b01_crc_calc:04X} stored=0x{b01_crc_stored:04X}")


def patch_a07(a07_bytes: bytes, flat_loudness: bool, true_3d_off: bool, clean_51_matrix: bool) -> bytes:
    if len(a07_bytes) != 0x10000 or a07_bytes[:8] != b"AD301A07":
        raise ValueError("Invalid AD301A07 image")

    buf = bytearray(a07_bytes)

    if flat_loudness:
        # C4AB7 16x3 signed 16-bit gains (tenths of dB) at CodeFlash 0x102E40..0x102E9F (A07 +0x2E40..+0x2E9F)
        buf[0x2E40:0x2EA0] = b"\x00" * 0x60
        print("[+] Patched C4AB7 (A07 +0x2E40..+0x2E9F): all 16 loudness banks set to flat 0.0 dB (55 Hz / 10 kHz / 4.5 kHz)")

    if true_3d_off:
        # In each of the 8 sub-profiles (0x100034 + p * 0x370), the 5x20-byte Pair Mixer 1 (0xE0584) gain rows
        # sit at +0x308..+0x36B (for Profile 0: 0x10033C..0x10039F).
        # Row 0 (+0x308..+0x31B) is Linear mode (first-B outputs muted to -100 dB).
        # Row 1 (+0x31C..+0x32F) is MMI "3D Effect = Off" (which stock leaves open at 0 dB on 6 B channels).
        for p in range(8):
            prof_base = 0x0034 + p * 0x370
            row0_off = prof_base + 0x308
            row1_off = prof_base + 0x31C
            buf[row1_off:row1_off + 20] = buf[row0_off:row0_off + 20]
        # Also copy Bank 0 (Linear bypass 16 biquads, 0x80 bytes at gp-0x1284c = A07 +0x2B20) into Bank 1 (+0x2BA0)
        buf[0x2BA0:0x2C20] = buf[0x2B20:0x2BA0]
        print("[+] Patched Row 1 across all 8 sub-profiles & biquad Bank 1: MMI '3D = Off' now uses pure Row 0 (first-B muted to -100 dB)")

    if clean_51_matrix:
        # Input Mixing Matrices: 8 sets of 0xC0 bytes at A07 +0x1BB4..+0x21B3 (3 banks of 8x8 signed dB bytes per set).
        # Bank 1 (+0x40..+0x7F in each 0xC0 set) is the 6-channel PCM6 (DTCP51 / CarPlay) matrix.
        # Rows = 8 buses (busL, busR, busC, busLFE, busLs, busRs, bus6, bus7), Cols = 8 inputs (L, R, C, LFE, Ls, Rs, Ch7, Ch8).
        # Remove -3 dB L/R bleed from busLs (row 4, cols 0..1), busRs (row 5, cols 0..1), bus6 (row 6, cols 0..1), bus7 (row 7, cols 0..1)
        for m in range(8):
            bank1_base = 0x1BB4 + m * 0xC0 + 0x40
            for bus_row in (4, 5, 6, 7):
                buf[bank1_base + bus_row * 8 + 0] = 0x9C  # -100 dB (mute L bleed into surround/effect buses)
                buf[bank1_base + bus_row * 8 + 1] = 0x9C  # -100 dB (mute R bleed into surround/effect buses)
        print("[+] Patched Bank 1 (5.1) 8x8 matrices: muted -3 dB front L/R bleed into busLs/busRs/bus6/bus7")

    new_crc = crc16_ccitt_false(bytes(buf[:-2]))
    struct.pack_into(">H", buf, 0xFFFE, new_crc)
    print(f"[+] Recalculated AD301A07 CRC-16/CCITT-FALSE @ +0xFFFE: 0x{new_crc:04X}")
    return bytes(buf)


def main() -> int:
    parser = argparse.ArgumentParser(description="Audi B&O J525 (8W0035465) Dataset & SWUP Patcher")
    parser.add_argument(
        "--codeflash",
        required=True,
        help="Path to V850 CodeFlash bin",
    )
    parser.add_argument("--out-a07", help="Output path for patched 64 KB AD301A07 dataset binary")
    parser.add_argument("--flat-loudness", action="store_true", help="Zero C4AB7 55 Hz loudness boost across all 16 banks")
    parser.add_argument("--true-3d-off", action="store_true", help="Make MMI '3D = Off' (Row 1) fully mute the first-B processor like Row 0")
    parser.add_argument("--clean-51-matrix", action="store_true", help="Remove -3 dB L/R bleed into surround/effect buses in 5.1 Bank 1 matrix")
    args = parser.parse_args()

    with open(args.codeflash, "rb") as f:
        cf = f.read()

    verify_codeflash(cf)

    if args.out_a07 or args.flat_loudness or args.true_3d_off or args.clean_51_matrix:
        patched_a07 = patch_a07(
            cf[0x100000:0x110000],
            flat_loudness=args.flat_loudness,
            true_3d_off=args.true_3d_off,
            clean_51_matrix=args.clean_51_matrix,
        )
        if args.out_a07:
            os.makedirs(os.path.dirname(os.path.abspath(args.out_a07)), exist_ok=True)
            with open(args.out_a07, "wb") as f:
                f.write(patched_a07)
            print(f"[+] Saved patched AD301A07 (65,536 bytes) to {args.out_a07}")

    return 0


if __name__ == "__main__":
    sys.exit(main())

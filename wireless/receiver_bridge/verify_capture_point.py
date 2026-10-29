#!/usr/bin/env python3
"""Pin the MU1316 dio_manager locations used by capture_probe.c."""

from hashlib import sha256
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[4]
DIO = ROOT / (
    "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
    "advanced/MU1316-appimg/eso/bin/apps/dio_manager"
)
EXPECTED_SHA256 = "e9862680d76ca2e6bb2eed067658f2ca8fa9b71b5446322384b76c9735c0e794"
BASE = 0x100000


def main() -> None:
    data = DIO.read_bytes()
    digest = sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"wrong dio_manager image: {digest}")

    def at(address: int, size: int) -> bytes:
        offset = address - BASE
        return data[offset : offset + size]

    expected_bytes = {
        # CProcessHandler's kill-friendly selector and call/return seam.
        0x1472E8: bytes.fromhex("000058e30500a0e10f10a0130910a003fc2effeb"),
        0x171C10: bytes.fromhex("f0452de90040a0e1"),
        0x171C38: bytes.fromhex("2f0ea0e3037095e70710a0e15f83feeb401084e2"),
        0x171C50: bytes.fromhex("03a1ffeb000056e3386084e5"),
        0x171C60: bytes.fromhex("7000a0e30710a0e15683feeb0060a0e175c8ffeb"),
        0x1675A4: bytes.fromhex(
            "000051e350209fe50030a0e102208fe00800000a010051e30600000a"
            "020051e30020a01300208015042080150100000a0300a0e11eff2fe1"
            "20009fe5811081e0002082e0012182e0042082e2030092e8030083e8"
            "0300a0e1"
        ),
        0x19A190: bytes.fromhex(
            "0000000020030000e001000001000000c00300001c020000"
            "0200000000040000e0010000"
        ),
        # Exact-image checks used by property_handoff.c before replacing
        # AirPlayReceiverServerSetDelegate's copy-property callback.
        0x15C6C0: bytes.fromhex("4c308de5c5d8feeb00c0a0e3"),
        0x1586EC: bytes.fromhex("ca22ffeb0400a0e10010e0e355fbffeb"),
        0x157454: bytes.fromhex("190e80e20020a0e3097c00ea"),
        0x17638C: bytes.fromhex("f0402de9106080e20cd04de2"),
        0x176488: bytes.fromhex("f04f2de90170a0e1c8129fe5"),
        0x160A74: bytes.fromhex(
            "0520a0e10400a0e10130a0e17dcf84e200c08de5"
            "e3fcffebf00184e5"
        ),
        0x15B498: bytes.fromhex("f00194e5000050e30000000a7edcfeebe00294e5"),
    }
    expected_words = {
        0x1A4848: 0x15B314,  # CDIOManager primary vptr first entry
    }
    for address, expected in expected_bytes.items():
        assert at(address, len(expected)) == expected, hex(address)
    for address, expected in expected_words.items():
        assert struct.unpack("<I", at(address, 4))[0] == expected, hex(address)
    print(f"capture point: image SHA and {len(expected_bytes) + len(expected_words)} "
          "code/table/vtable checks OK")


if __name__ == "__main__":
    main()

"""Static MU1316 audit assertions; does not execute or modify firmware."""
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parent
manifest = json.loads((ROOT / "manifest.json").read_text())
images = {}
for name, source in manifest.items():
    data = Path(source["path"]).read_bytes()
    assert hashlib.sha256(data).hexdigest() == source["sha256"], name
    assert data[:6] == b"\x7fELF\x01\x01", name  # ELF32 little endian
    images[name] = data


def at(name, va, size):
    data = images[name]
    phoff = struct.unpack_from("<I", data, 28)[0]
    entsize, count = struct.unpack_from("<HH", data, 42)
    for index in range(count):
        kind, off, start, _, filesz, _, _, _ = struct.unpack_from(
            "<8I", data, phoff + index * entsize
        )
        if kind == 1 and start <= va and va + size <= start + filesz:
            return data[off + va - start:off + va - start + size]
    raise AssertionError((name, hex(va), size))


def instruction(name, va, word):
    assert at(name, va, 4) == struct.pack("<I", word), (name, hex(va))


key = b"OMX.QCOM.index.config.video.DisplayPictureBuffer\0"
assert at("airplay", 0xAFDF0, len(key)) == key
assert at("qcom", 0x229F2, len(key)) == key
instruction("airplay", 0x86EB8, 0xE3A02014)  # 20-byte native payload
instruction("airplay", 0x86F34, 0xE5933018)  # GetParameter slot
instruction("airplay", 0x86F48, 0xE58DA01C)  # payload +12 = enabled
instruction("airplay", 0x86F50, 0x158D3020)  # payload +16 = 1 when enabled
instruction("airplay", 0x86F64, 0xE593301C)  # SetParameter slot
instruction("qcom", 0x17AB4, 0xE593200C)  # handler consumes payload +12
instruction("qcom", 0x17AC0, 0xE5932010)  # handler consumes payload +16
assert at("qcom", 0x192A4, 4) == struct.pack("<I", 0x7F100008)

instruction("airplay", 0x8AF74, 0xE3A02064)  # enqueue wait argument=100
instruction("airplay", 0x8B688, 0xE3A0C000)  # native post flags=0
instruction("airplay", 0x8B78C, 0xE5963018)  # reuse callback after render
instruction("airplay", 0x8BE2C, 0xE3A03001)  # native swap interval=1
instruction("airplay", 0x8BE34, 0xE3A0102D)  # Screen property=45
assert at("airplay", 0x88520, 8) == bytes(8)  # stock OMX timestamp literal

instruction("encoder", 0x10A7C0, 0xE3A00FFA)  # integer numerator=1000
instruction("encoder", 0x10A824, 0xE1A02008)  # quotient -> timer interval
instruction("osal", 0x35958, 0xE304C240)  # 1,000,000 ns per ms, low half
instruction("osal", 0x35960, 0xE340C00F)  # high half
instruction("osal", 0x35984, 0xE003019C)  # remainder milliseconds -> ns

period = Fraction(1000, 30)
for capture_period, expected_repeats in [(Fraction(33), 10), (period, 0)]:
    ids = [int((1 + i * capture_period) / period) for i in range(1000)]
    assert sum(a == b for a, b in zip(ids, ids[1:])) == expected_repeats
print("PASS: six firmware hashes, native decoder ABI, async post/reuse, "
      "33-ms timer instructions and idealized clock model (not a HU measurement)")

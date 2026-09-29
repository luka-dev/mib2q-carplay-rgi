#!/usr/bin/env python3
"""Host-side tests for the pure EIR insertion routine; no head-unit access."""
import ctypes
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
UUID = bytes.fromhex('d3 1f bf 50 5d 57 27 97 a2 40 41 cd 48 43 88 ec')


def entries(data: bytes):
    pos = 0
    while pos < len(data):
        size = data[pos]
        assert size and pos + size + 1 <= len(data)
        yield data[pos + 1], data[pos + 2:pos + size + 1]
        pos += size + 1


def run(fn, original: bytes) -> bytes:
    assert len(original) < 240
    buffer = (ctypes.c_ubyte * 256)()
    buffer[:] = bytes(240) + bytes([0xA5]) * 16
    buffer[:len(original)] = original
    result_len = fn(buffer, len(original))
    assert result_len < 240
    assert bytes(buffer[240:]) == bytes([0xA5]) * 16
    assert buffer[result_len] == 0
    return bytes(buffer[:result_len])


def main():
    with tempfile.TemporaryDirectory(prefix='mhi2q-eir-test-') as temp:
        lib = Path(temp) / 'eir.dylib'
        subprocess.run(['clang', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-dynamiclib', str(ROOT / 'eir.c'), '-o', str(lib)],
                       check=True)
        fn = ctypes.CDLL(str(lib)).eir_append_carplay_uuid
        fn.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_uint]
        fn.restype = ctypes.c_uint
        first = run(fn, b'')
        assert first == bytes([17, 7]) + UUID
        assert run(fn, first) == first
        base = bytes([2, 1, 6, 17, 7]) + bytes(range(16)) + bytes([2, 10, 4])
        added = run(fn, base)
        assert added[:3] == base[:3]
        assert added[3:5] == bytes([33, 7])
        assert added[5:21] == bytes(range(16))
        assert added[21:37] == UUID
        assert added[37:] == base[21:]
        assert run(fn, added) == added
        malformed = bytes([5, 7, 1])
        assert run(fn, malformed) == malformed
        assert run(fn, bytes([2, 1, 6]) * 79) == bytes([2, 1, 6]) * 79
        rng = random.Random(0xC4A2)
        for _ in range(1000):
            ad = bytearray()
            while len(ad) < rng.randrange(5, 180):
                size = rng.randrange(1, 12)
                ad.extend(bytes([size + 1, 0xFF]) + rng.randbytes(size))
            ad = ad[:len(ad)]
            result = run(fn, bytes(ad))
            fields = list(entries(result))
            assert fields[:-1] == list(entries(bytes(ad)))
            assert fields[-1] == (7, UUID)
        print('EIR insertion: 1000 randomized layouts and boundary cases passed')


if __name__ == '__main__':
    main()

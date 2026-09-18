"""Reject any encoder other than the statically audited MU1316 ELF. No writes."""
import hashlib
import json
from pathlib import Path
import struct
import sys

EXPECTED_SHA256 = '88dcf0f5f8785a406a4c82f3041b1941f93e27c62ff4de8692cf0f90629b2386'


def verify(path):
    data = Path(path).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError('unsupported videoencoderservice SHA256: ' + digest)
    assert data[:6] == b'\x7fELF\x01\x01'
    assert struct.unpack_from('<HH', data, 16) == (2, 40)  # ET_EXEC, ARM
    phoff = struct.unpack_from('<I', data, 28)[0]
    entsize, count = struct.unpack_from('<HH', data, 42)

    def at(va, size):
        for i in range(count):
            kind, off, addr, _, length, _, _, _ = struct.unpack_from('<8I', data, phoff + i * entsize)
            if kind == 1 and addr <= va and va + size <= addr + length:
                return data[off + va - addr:off + va - addr + size]
        raise ValueError('unmapped VA')

    # display 4 -> remote 0, event keeps mapping in r8, adjustTimer preserves
    # its remote-display argument in callee-saved r6 through startTimer.
    for va, word in [(0x10a65c, 0xe2540004), (0x10a660, 0x13a00001),
                     (0x106ef4, 0xeb000dcf), (0x106f00, 0xe1a08000),
                     (0x106f74, 0xe1a01008), (0x10a6b0, 0xe1a06001)]:
        assert at(va, 4) == struct.pack('<I', word)
    words = [0xe1a00009, 0xe59d100c, 0xebffe4ff]
    assert at(0x10a850, 12) == struct.pack('<3I', *words)
    assert b'\0_ZN4util10TimerGroup10startTimerEj\0' in data
    assert at(0x10a7c0, 4) == struct.pack('<I', 0xe3a00ffa)  # numerator 1000
    return {'firmware': str(Path(path).resolve()), 'sha256': digest,
            'remote_display': 0, 'physical_display': 4, 'return_pc': '0x10a85c', 'guard_va': '0x10a850',
            'guard_words': [hex(w) for w in words], 'requested_period_ns': 33333333,
            'activation': 'candidate only; not installed; restore 33 ms on interval readback mismatch'}


if __name__ == '__main__':
    print(json.dumps(verify(sys.argv[1]), indent=2))

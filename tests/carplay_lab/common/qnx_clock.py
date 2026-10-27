"""Pinned QNX laboratory bootstrap correction for the QEMU generic timer."""
import hashlib
import struct

STARTUP_SHA256 = 'e7715060be5050ca584b57ec32abc9313a1edbb9028ec483c9884b1097c8d9d1'
FREQUENCY = 62500000


def corrected(data: bytes) -> bytes:
    if hashlib.sha256(data).hexdigest() != STARTUP_SHA256:
        raise ValueError('QNX startup hash changed; timer patch requires new ABI review')
    # ARM32 init_qtime_virt@0x080000bc. Keep its relocations/callouts intact.
    # timer_rate * 10^-15 seconds: 83.333 ns -> 16 ns at explicit 62.5 MHz.
    expected = {0xf4: bytes.fromhex('d50009e3f70440e3'), 0x138: struct.pack('<Q', 24000000)}
    for offset, value in expected.items():
        if data[offset:offset+len(value)] != value:
            raise ValueError('QNX timer instruction/constant guard failed')
    result = bytearray(data)
    rate = 16000000
    lo, hi = rate & 65535, rate >> 16
    movw = 0xe3000000 | ((lo >> 12) << 16) | (lo & 4095)
    movt = 0xe3400000 | ((hi >> 12) << 16) | (hi & 4095)
    struct.pack_into('<II', result, 0xf4, movw, movt)
    struct.pack_into('<Q', result, 0x138, FREQUENCY)
    return bytes(result)

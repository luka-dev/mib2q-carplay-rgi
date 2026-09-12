#!/usr/bin/env python3
"""Execute the original G1952 NSA page adapter; verify preselected nav bytes.

This tests serial-frame -> GDC-packet transport, NOT BAP -> icon selection.
Only the external memcpy call is modeled. No firmware or vehicle is modified.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP,
    UC_ARM_REG_LR, UC_ARM_REG_PC,
)

SHA = '0146bfb0ac3584f3f6fe19f2d38a97c65f3bbd4bbed63d2d43c51db24bf4dc63'
TEXT_FILE, TEXT_SIZE, TEXT_VA = 0xe2000, 0x17ce4, 0x10000
ADAPTER, MEMCPY, STATE_PTR = 0x138fc, 0x1782d3c, 0x2942c
INPUT, OUTPUT, STACK, STOP = 0x200000, 0x201000, 0x300000, 0x400000


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('firmware', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    blob = (args.firmware / 'blocks/FD_61FLASHDATA.bin').read_bytes()
    digest = hashlib.sha256(blob).hexdigest()
    if digest != SHA:
        raise SystemExit('Unsupported FD61: this probe is pinned to G1952')
    assert struct.unpack_from('<III', blob, TEXT_FILE + 0x64) == (TEXT_VA, 0, 0x28000)
    vm = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    vm.mem_map(TEXT_VA, 0x20000)
    vm.mem_write(TEXT_VA, blob[TEXT_FILE:TEXT_FILE + TEXT_SIZE])
    vm.mem_map(INPUT, 0x2000)
    vm.mem_map(STACK, 0x10000)
    vm.mem_map(STOP, 0x1000)
    vm.mem_map(MEMCPY & ~0xfff, 0x1000)
    vm.mem_write(STATE_PTR, struct.pack('<I', OUTPUT))
    copies = []

    def external_call(uc, address, size, data):
        if address != MEMCPY:
            return
        dst, src, length = [uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2)]
        copies.append((dst, src, length))
        uc.mem_write(dst, bytes(uc.mem_read(src, length)))
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    vm.hook_add(UC_HOOK_CODE, external_call)
    cases = 0
    for selector in range(256):
        for group_extension in (0, 1, 255):
            frame = bytearray((i * 37 + selector) & 255 for i in range(256))
            frame[4] = 3
            frame[9] = group_extension
            frame[23] = selector  # main 3D field 9
            vm.mem_write(INPUT, bytes(frame))
            vm.mem_write(OUTPUT, bytes([0xa5]) * 256)
            copies.clear()
            vm.reg_write(UC_ARM_REG_R0, INPUT)
            vm.reg_write(UC_ARM_REG_R1, 3)
            vm.reg_write(UC_ARM_REG_SP, STACK + 0xf000)
            vm.reg_write(UC_ARM_REG_LR, STOP)
            vm.emu_start(ADAPTER, STOP, count=10000)
            assert vm.reg_read(UC_ARM_REG_PC) == STOP
            assert vm.reg_read(UC_ARM_REG_R0) == 0
            state = bytes(vm.mem_read(OUTPUT, 256))
            assert struct.unpack_from('<H', state, 10)[0] == 123
            packet = state[4:127]
            assert packet[8] == 3
            assert packet[9:13] == frame[5:9]
            assert packet[13] == group_extension & 1
            assert packet[14:122] == frame[15:123]
            assert packet[122] == group_extension & 1
            assert packet[22] == selector
            assert packet[24:41] == frame[25:42]  # 17 road fields
            assert packet[102] == frame[103]  # 2D field 89
            assert (OUTPUT + 18, INPUT + 15, 108) in copies
            assert state[127:] == bytes([0xa5]) * 129
            cases += 1
    result = dict(
        firmware='8W6919617G_1952', source_sha256=digest, cases_passed=cases,
        code_file_offset=hex(TEXT_FILE), code_link_address=hex(TEXT_VA),
        function=hex(ADAPTER), mocked_external_calls={hex(MEMCPY): 'memcpy'},
        page=3, packet_length=123, copied_selector_bytes=108,
        main_3d=dict(input_frame_byte=23, gdc_packet_byte=22),
        roads_3d=dict(input_frame_bytes=[25, 41], gdc_packet_bytes=[24, 40]),
        main_2d=dict(input_frame_byte=103, gdc_packet_byte=102),
        conclusion='Preselected bytes are preserved. BAP conversion is upstream and was NOT executed.',
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()

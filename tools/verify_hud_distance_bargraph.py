#!/usr/bin/env python3
"""Offline GDC scene-walker probe for HUD 8W6919617G_1952 / 8W6919617M_2325.

Executes original ARM page/group/element/selector/numeric-glyph code with the
original FD_62 scene. Captures surface submissions instead of executing GLES.
This tests renderer capability, not BAP-to-display packet construction.
Requires Unicorn; input flash blocks are hash-pinned and never modified.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

FIRMWARES = {
 '8W6919617G_1952': {
    'FD_61FLASHDATA.bin': '0146bfb0ac3584f3f6fe19f2d38a97c65f3bbd4bbed63d2d43c51db24bf4dc63',
    'FD_62FLASHDATA.bin': '259409a247a297417d6b398bb42a9f51897514604f320c0b184ccb92b53c35ee',
 },
 '8W6919617M_2325': {
    'FD_61FLASHDATA.bin': '8a71ffd31fdfc17131507e0d259bd216751ec2c7918468b8e18ba58787198fc1',
    'FD_62FLASHDATA.bin': '81a31aa257ef322d3333f2e10770d3edb32179abff75e0bd904689503007449b',
 },
}
GDC = 0x808F5000
GDC_FILE = 0x12DEBC
GDC_SIZE = 0x2329B4
SCENE = 0x50000000
GLOBALS = 0x6E0000
PACKET = 0x70000000
STACK = 0x90000000
STOP = 0x30000000
PAGE_WALKER = 0x808F96E0
ELEMENT_DISPATCH = 0x808F9E68
DRAW_SURFACE = 0x808FA984
MEMCPY = 0x82067D3C
TRACK_SURFACE = 0x809027EC
NUMERIC_NODES = (0x943A8, 0x95524)
BAR_NODE = 0x8F3E4
UNIT_NODES = (0x9465C, 0x957D8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('blocks', type=Path)
    args = parser.parse_args()
    blobs = {name: (args.blocks / name).read_bytes() for name in next(iter(FIRMWARES.values()))}
    hashes = {name: hashlib.sha256(blob).hexdigest() for name, blob in blobs.items()}
    firmware = next((name for name, expected in FIRMWARES.items() if hashes == expected), None)
    if firmware is None:
        raise SystemExit('Wrong firmware fingerprint or mixed flash blocks')
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(GDC, (GDC_SIZE + 4095) & ~4095)
    uc.mem_write(GDC, blobs['FD_61FLASHDATA.bin'][GDC_FILE:GDC_FILE + GDC_SIZE])
    uc.mem_map(SCENE, len(blobs['FD_62FLASHDATA.bin']))
    uc.mem_write(SCENE, blobs['FD_62FLASHDATA.bin'])
    uc.mem_map(GLOBALS, 0x20000)
    uc.mem_map(PACKET, 0x1000)
    uc.mem_map(STACK, 0x10000)
    uc.mem_map(STOP, 0x1000)
    uc.mem_map(MEMCPY & ~4095, 0x1000)
    state = {'element': None, 'surfaces': []}

    def return_zero(vm):
        vm.reg_write(UC_ARM_REG_R0, 0)
        vm.reg_write(UC_ARM_REG_PC, vm.reg_read(UC_ARM_REG_LR))

    def boundary(vm, address, size, unused):
        if address == ELEMENT_DISPATCH:
            state['element'] = vm.reg_read(UC_ARM_REG_R1) - SCENE
        elif address == DRAW_SURFACE:
            desc = bytes(vm.mem_read(vm.reg_read(UC_ARM_REG_R0), 44))
            state['surfaces'].append((state['element'], desc))
            return_zero(vm)
        elif address == MEMCPY:
            dst, src, count = (vm.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2))
            vm.mem_write(dst, bytes(vm.mem_read(src, count)))
            vm.reg_write(UC_ARM_REG_PC, vm.reg_read(UC_ARM_REG_LR))
        elif address == TRACK_SURFACE:
            return_zero(vm)

    uc.hook_add(UC_HOOK_CODE, boundary)
    results = []
    examples = []
    scene = blobs['FD_62FLASHDATA.bin']
    glyph_offsets = {struct.unpack_from('<I', scene, NUMERIC_NODES[0] + 32 + i * 44 + 4)[0]: i for i in range(15)}
    # The three leftmost digits are 1,5,0; other digit selectors are blank=15.
    for group, presentation, bar in itertools.product((9, 14), ('valid', 'blank_digits', 'group_off'), range(18)):
        uc.mem_write(GLOBALS, bytes(0x20000))
        uc.mem_write(0x6F7754, struct.pack('<I', SCENE + 0x1294))
        uc.mem_write(0x6F7750, bytes([2]))  # normal caller's initial state
        packet = bytearray([255] * 128)
        packet[8] = 3
        struct.pack_into('<I', packet, 9, (1 << 2) | ((1 << group) if presentation != 'group_off' else 0))
        packet[13] = 0
        # Internal unit selector 1 = meter; this is not the BAP unit enum.
        packet[41:45] = bytes([0xF1, 0x0F, 0x15, 0xFF])
        if presentation == 'blank_digits':
            packet[41:45] = bytes([0xF1, 0xFF, 0xFF, 0xFF])
        packet[45] = bar
        uc.mem_write(PACKET, bytes(packet))
        state['surfaces'].clear()
        uc.reg_write(UC_ARM_REG_R0, PACKET)
        uc.reg_write(UC_ARM_REG_SP, STACK + 0xF000)
        uc.reg_write(UC_ARM_REG_LR, STOP)
        uc.emu_start(PAGE_WALKER, STOP, count=100000)
        if uc.reg_read(UC_ARM_REG_PC) != STOP or uc.reg_read(UC_ARM_REG_R0) != 0:
            raise AssertionError('page walker did not finish successfully')
        numeric = [d for node, d in state['surfaces'] if node in NUMERIC_NODES]
        units = [d for node, d in state['surfaces'] if node in UNIT_NODES]
        bars = [d for node, d in state['surfaces'] if node == BAR_NODE]
        if len(numeric) != (3 if presentation == 'valid' else 0) or len(bars) != (16 if bar <= 16 else 0):
            raise AssertionError((group, bar, len(numeric), len(bars), [hex(n) for n, d in state['surfaces']]))
        if len(units) != (0 if presentation == 'group_off' else 1):
            raise AssertionError('unit selector was not rendered independently')
        if numeric:
            ordered = sorted(numeric, key=lambda d: struct.unpack_from('<H', d, 30)[0])
            digits = ''.join(str(glyph_offsets[struct.unpack_from('<I', d, 4)[0]]) for d in ordered)
            if digits != '150':
                raise AssertionError('wrong glyph sequence: ' + digits)
            if bar == 8:
                examples.append(dict(group=group, distance=digits, packet_hex=packet.hex(' '),
                    glyph_rects=[struct.unpack_from('<HHHH', d, 30) for d in ordered],
                    unit_rects=[struct.unpack_from('<HHHH', d, 30) for d in units],
                    bar_rects=[struct.unpack_from('<HHHH', d, 30) for d in bars]))
        results.append(dict(numeric_group=group, presentation=presentation, bar_state=bar, numeric_glyphs=len(numeric), bar_surfaces=len(bars)))
    for name, expected in hashes.items():
        if hashlib.sha256((args.blocks / name).read_bytes()).hexdigest() != expected:
            raise AssertionError('input changed: ' + name)
    print(json.dumps(dict(firmware=firmware, sha256=hashes,
        cases_passed=len(results), original_firmware_modified=False,
        scope='original ARM scene walker; surface submissions captured; BAP packet producer not executed',
        examples=examples, results=results), indent=2))


if __name__ == '__main__':
    main()

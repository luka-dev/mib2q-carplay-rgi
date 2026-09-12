#!/usr/bin/env python3
"""Execute the audited AU491 distance widget callback offline; never patch the ELF.

Requires Unicorn in the host Python environment and the original, hash-pinned
hmi_app.elf. Only the data-pool getter and widget lookup are substituted; the
visibility predicate, binding callback and visible setter execute real ARM code.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

EXPECTED_SHA256 = 'd2df7d51e7dc6e9d16f0eb063142cb39212e32148b729041d318489bbbe9dc9b'
BASE = 0x100000
CALLBACK = 0x5F0D58
GET_DP = 0x69ECD0
OBJECT = 0x2000000
WIDGET = OBJECT + 0x1000
VTABLE = OBJECT + 0x2000
STOP = 0x3000000
GET_WIDGET = STOP + 0x100
STACK = 0x3100000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    args = parser.parse_args()
    data = args.elf.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit('ELF fingerprint mismatch; addresses apply only to the audited AU491 image')
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(BASE, (len(data) + 4095) & ~4095)
    uc.mem_write(BASE, data)
    uc.mem_map(OBJECT, 0x3000)
    uc.mem_map(STOP, 0x1000)
    uc.mem_map(STACK, 0x4000)
    uc.mem_write(OBJECT, struct.pack('<I', VTABLE))
    uc.mem_write(VTABLE + 0x70, struct.pack('<I', GET_WIDGET))
    values = {}
    reads = []

    def boundary(vm, address, size, unused):
        if address == GET_DP:
            dpid = vm.reg_read(UC_ARM_REG_R0)
            reads.append(dpid)
            vm.mem_write(vm.reg_read(UC_ARM_REG_R1), struct.pack('<h', values[dpid]))
            vm.reg_write(UC_ARM_REG_R0, 0)
            vm.reg_write(UC_ARM_REG_PC, vm.reg_read(UC_ARM_REG_LR))
        elif address == GET_WIDGET:
            vm.reg_write(UC_ARM_REG_R0, WIDGET)
            vm.reg_write(UC_ARM_REG_PC, vm.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, boundary)
    cases = 0
    examples = []
    for available, rg, bar, valid, unit in itertools.product((0, 1), (0, 1, 2), (0, 1), (0, 1), (0, 1, 2, 3, 4, 5, 255)):
        values.update({0x2000071: available, 0x2000070: rg, 0x2000066: bar, 0x2000069: valid, 0x2000068: unit})
        for initial_visible in (0, 1):
            uc.mem_write(WIDGET, bytes(0x100))
            uc.mem_write(WIDGET + 0xA0, bytes([initial_visible]))
            uc.reg_write(UC_ARM_REG_SP, STACK + 0x3000)
            uc.reg_write(UC_ARM_REG_LR, STOP)
            uc.reg_write(UC_ARM_REG_R0, 0)
            uc.reg_write(UC_ARM_REG_R1, OBJECT)
            reads.clear()
            uc.emu_start(CALLBACK, STOP, count=500)
            if uc.reg_read(UC_ARM_REG_PC) != STOP:
                raise AssertionError('callback did not finish within instruction bound')
            expected = int(available != 0 and rg == 1 and bar == 0 and valid != 0 and unit <= 5)
            actual = uc.mem_read(WIDGET + 0xA0, 1)[0]
            if actual != expected or set(reads) != set(values):
                raise AssertionError((values, expected, actual, reads))
            cases += 1
            if available == 1 and rg == 1 and valid == 1 and unit == 0 and initial_visible == 0:
                examples.append(dict(bargraph_on=bar, distance_valid=valid, distance_widget_visible=actual))
    if hashlib.sha256(args.elf.read_bytes()).hexdigest() != digest:
        raise AssertionError('input ELF changed')
    print(json.dumps(dict(elf_sha256=digest, predicate_va='0x5eca80', callback_va=hex(CALLBACK),
        visible_setter_va='0x654830', excluding_branch_va='0x5ecaf4', excluding_branch_file_offset='0x4ecaf4',
        cases_passed=cases, examples=examples, elf_modified=False), indent=2))


if __name__ == '__main__':
    main()

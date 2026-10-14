#!/usr/bin/env python3
"""Build a review-only MHI2Q btstack copy. Never writes the source image."""
from __future__ import annotations

import hashlib
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile

from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
from elftools.elf.elffile import ELFFile

HERE = Path(__file__).resolve().parent
import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '../..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
SOURCE = Path(_WS + '/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/apps/btstack')
EXPECTED_SHA = '154c17826f0773a57b2004f5b17d818e3437c46fcbb6a857d40c696898c29bc5'
HOOK = 0x11ECD0
RESUME = 0x11ECD4
EIR_HOOK = 0x12A710
EIR_RESUME = 0x12A714
CODE_VA = 0x508000
STATE_VA = 0x510000
STATE_BYTES = 0x20000
PHDR_OFF = 0x1B9978


def check(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def align(value: int, boundary: int) -> int:
    return (value + boundary - 1) & -boundary


def command(*args: str) -> None:
    subprocess.run(args, check=True, cwd=HERE)


def pack_phdr(kind: int, offset: int, va: int, pa: int,
              file_size: int, mem_size: int, flags: int, alignment: int) -> bytes:
    return struct.pack('<8I', kind, offset, va, pa, file_size,
                       mem_size, flags, alignment)


def build(qemu_synthetic: bool = False, qemu_iap2_marker: bool = False) -> None:
    check(not qemu_iap2_marker or qemu_synthetic,
          'QEMU iAP2 marker requires synthetic callbacks')
    original = SOURCE.read_bytes()
    check(hashlib.sha256(original).hexdigest() == EXPECTED_SHA, 'source hash differs')
    check(struct.unpack_from('<I', original, HOOK - 0x100000)[0] == 0xE5D53764,
          'startup instruction changed')
    check(struct.unpack_from('<I', original, EIR_HOOK - 0x100000)[0] == 0xE5C6040C,
          'EIR instruction changed')
    check(original[PHDR_OFF:0x1BA000] == bytes(0x1BA000 - PHDR_OFF),
          'PHDR relocation gap is not zero')
    with tempfile.TemporaryDirectory(prefix='mhi2q-graft-') as temp:
        obj = Path(temp) / 'graft.o'
        eir_obj = Path(temp) / 'eir.o'
        linked = Path(temp) / 'graft.elf'
        command('clang', '-target', 'armv7-none-eabi', '-march=armv7-a',
                '-marm', '-O2', '-ffreestanding', '-fno-builtin',
                '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
                '-fno-stack-protector', '-fno-pic',
                *(['-DMHI2Q_GRAFT_QEMU_SYNTHETIC'] if qemu_synthetic else []),
                *(['-DMHI2Q_GRAFT_QEMU_IAP2_MARKER'] if qemu_iap2_marker else []),
                '-c', str(HERE / 'graft.c'),
                '-o', str(obj))
        command('clang', '-target', 'armv7-none-eabi', '-march=armv7-a',
                '-marm', '-O2', '-ffreestanding', '-fno-builtin',
                '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
                '-fno-stack-protector', '-fno-pic', '-c', str(HERE / 'eir.c'),
                '-o', str(eir_obj))
        command('ld.lld', '-flavor', 'gnu', '-m', 'armelf', '-T',
                str(HERE / 'graft.ld'), '--no-undefined', str(obj), str(eir_obj),
                '-o', str(linked))
        with linked.open('rb') as stream:
            elf = ELFFile(stream)
            section = elf.get_section_by_name('.graft')
            code = section.data()
            check(bytes.fromhex('35 11 1c ec 88 43 48 cd 41 40 a2 97 27 57 5d 50 bf 1f d3') in code,
                  'factory CarPlay SDP value absent from graft')
            check(bytes.fromhex('d3 1f bf 50 5d 57 27 97 a2 40 41 cd 48 43 88 ec') in code,
                  'CarPlay EIR UUID absent from graft')
            symbols = elf.get_section_by_name('.symtab')
            hook_symbols = symbols.get_symbol_by_name('graft_hook')
            eir_symbols = symbols.get_symbol_by_name('graft_eir_hook')
            check(hook_symbols and len(hook_symbols) == 1 and
                  eir_symbols and len(eir_symbols) == 1, 'hook symbol missing')
            hook_va = hook_symbols[0]['st_value']
            eir_hook_va = eir_symbols[0]['st_value']
            check(section['sh_addr'] == CODE_VA and CODE_VA + len(code) < STATE_VA,
                  'code placement invalid')
            check(not any(s['sh_type'] in ('SHT_REL', 'SHT_RELA') for s in elf.iter_sections()),
                  'unresolved relocations')
    old_phoff = struct.unpack_from('<I', original, 28)[0]
    old_phentsize, old_phnum = struct.unpack_from('<HH', original, 42)
    check((old_phoff, old_phentsize, old_phnum) == (0x34, 32, 5), 'original PHDR layout differs')
    new_phnum = old_phnum + 2
    phdr_end = PHDR_OFF + new_phnum * 32
    check(phdr_end <= 0x1BA000, 'PHDR table exceeds unused gap')
    code_off = align(len(original), 0x1000)
    state_off = align(code_off + len(code), 0x1000)
    check((code_off - CODE_VA) % 0x1000 == 0 and
          (state_off - STATE_VA) % 0x1000 == 0, 'LOAD alignment mismatch')
    patched = bytearray(original)
    old = [list(struct.unpack_from('<8I', original, old_phoff + i * 32))
           for i in range(old_phnum)]
    check([x[0] for x in old] == [6, 3, 1, 1, 2], 'unexpected PHDR types')
    old[0][1] = PHDR_OFF
    old[0][2] = 0x100000 + PHDR_OFF
    old[0][4] = old[0][5] = new_phnum * 32
    old[2][4] = old[2][5] = phdr_end
    check(old[3][2] + old[3][5] <= CODE_VA, 'original RW memory overlaps graft')
    headers = [pack_phdr(*values) for values in old]
    headers.append(pack_phdr(1, code_off, CODE_VA, CODE_VA,
                             len(code), len(code), 5, 0x1000))
    headers.append(pack_phdr(1, state_off, STATE_VA, STATE_VA,
                             0, STATE_BYTES, 6, 0x1000))
    patched[PHDR_OFF:phdr_end] = b''.join(headers)
    struct.pack_into('<I', patched, 28, PHDR_OFF)
    struct.pack_into('<H', patched, 44, new_phnum)
    branches = {}
    for source_va, dest_va in ((HOOK, hook_va), (EIR_HOOK, eir_hook_va)):
        delta = dest_va - source_va - 8
        check(delta % 4 == 0 and -(1 << 25) <= delta < (1 << 25),
              'ARM branch out of range')
        branch = 0xEA000000 | ((delta >> 2) & 0xFFFFFF)
        struct.pack_into('<I', patched, source_va - 0x100000, branch)
        branches[source_va] = branch
    patched.extend(bytes(code_off - len(patched)))
    patched.extend(code)
    patched.extend(bytes(state_off - len(patched)))
    allowed = (set(range(28, 32)) | set(range(44, 46)) |
               set(range(HOOK-0x100000, HOOK-0x100000+4)) |
               set(range(EIR_HOOK-0x100000, EIR_HOOK-0x100000+4)) |
               set(range(PHDR_OFF, phdr_end)))
    check(all(index in allowed for index, (before, after) in
              enumerate(zip(original, patched)) if before != after),
          'unexpected change to original file-backed bytes')
    out_dir = HERE / 'out'
    out_dir.mkdir(exist_ok=True)
    variant = 'iap2-qemu' if qemu_iap2_marker else 'synthetic' if qemu_synthetic else None
    result = out_dir / (f'btstack.graft.{variant}.offline' if variant
                        else 'btstack.graft.offline')
    result.write_bytes(patched)
    result.chmod(0o644)
    with result.open('rb') as stream:
        elf = ELFFile(stream)
        loads = [p for p in elf.iter_segments() if p['p_type'] == 'PT_LOAD']
        check(len(loads) == 4, 'patched ELF lacks expected LOAD segments')
        check(loads[2]['p_vaddr'] == CODE_VA and loads[2].data() == code,
              'code LOAD differs from compiled bytes')
        check(loads[3]['p_vaddr'] == STATE_VA and loads[3]['p_memsz'] == STATE_BYTES,
              'state LOAD invalid')
    for source_va, dest_va in ((HOOK, hook_va), (EIR_HOOK, eir_hook_va)):
        instructions = list(Cs(CS_ARCH_ARM, CS_MODE_ARM).disasm(
            patched[source_va-0x100000:source_va-0x100000+4], source_va))
        check(len(instructions) == 1 and instructions[0].mnemonic == 'b' and
              int(instructions[0].op_str[1:], 16) == dest_va,
              'hook does not branch to graft')
    for symbol_va, expected in ((hook_va, RESUME), (eir_hook_va, EIR_RESUME)):
        tramp = list(Cs(CS_ARCH_ARM, CS_MODE_ARM).disasm(
            code[symbol_va-CODE_VA:symbol_va-CODE_VA+32], symbol_va))
        loads = [inst for inst in tramp if inst.mnemonic == 'ldr' and
                 inst.op_str.startswith('pc, [pc')]
        check(len(loads) == 1, 'trampoline return load missing')
        instruction = struct.unpack('<I', loads[0].bytes)[0]
        displacement = instruction & 0xFFF
        literal_va = loads[0].address + 8 + (displacement if instruction & (1 << 23)
                                              else -displacement)
        check(struct.unpack_from('<I', code, literal_va - CODE_VA)[0] == expected,
              'trampoline return literal differs')
    check(hashlib.sha256(SOURCE.read_bytes()).hexdigest() == EXPECTED_SHA,
          'source changed during build')
    report = {
        'status': ('QEMU_SYNTHETIC_ONLY' if qemu_synthetic else
                   'OFFLINE_STATIC_REVIEW_ONLY'), 'source': str(SOURCE),
        'source_sha256': EXPECTED_SHA, 'output': str(result),
        'output_sha256': hashlib.sha256(patched).hexdigest(),
        'hook_va': hex(HOOK), 'hook_branch': hex(branches[HOOK]),
        'graft_hook_va': hex(hook_va), 'resume_va': hex(RESUME),
        'eir_hook_va': hex(EIR_HOOK), 'eir_hook_branch': hex(branches[EIR_HOOK]),
        'graft_eir_hook_va': hex(eir_hook_va), 'eir_resume_va': hex(EIR_RESUME),
        'code_va': hex(CODE_VA), 'code_file_offset': hex(code_off),
        'code_size': len(code), 'state_va': hex(STATE_VA),
        'state_mem_size': STATE_BYTES, 'phdr_offset': hex(PHDR_OFF),
        'phdr_count': new_phnum,
        'runtime_tested': False,
    }
    report_name = f'build_report.{variant}.json' if variant else 'build_report.json'
    (out_dir / report_name).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu-synthetic', action='store_true',
                        help='build a separate QEMU-only image with synthetic RFCOMM events')
    parser.add_argument('--qemu-iap2-marker', action='store_true',
                        help='use an iAP2 marker in the separate QEMU synthetic image')
    options = parser.parse_args()
    build(options.qemu_synthetic, options.qemu_iap2_marker)

#!/usr/bin/env python3
"""Run the real transport adapter and pinned native DSI startService in ARM.

No HMI or hardware is claimed: libc, ASI delivery, window notifications and
the native AirPlay queue boundary are modeled. The display/HID configuration
instructions, adapter guards and vtable callbacks execute unchanged.
"""
import hashlib
import json
import struct
import subprocess
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/si-wireless-offline-re'
SOURCE = ROOT / 'build/si-wireless-stage2/hu-20261021/offline-inputs/mnt/app/eso/bin/apps/dio_manager'
EXPECTED = 'e9862680d76ca2e6bb2eed067658f2ca8fa9b71b5446322384b76c9735c0e794'
IMPORTS = ['_Stderr', '__get_errno_ptr', 'fputs', 'getenv', 'getppid', 'memset', 'strcmp', 'strtoull']
SENTINELS = {name: 0x600000 + i * 16 for i, name in enumerate(IMPORTS)}
END, HEAP, STACK = 0x601000, 0x300000, 0x800000


def compile_adapter():
    OUT.mkdir(parents=True, exist_ok=True)
    compiler = 'arm-unknown-nto-qnx6.5.0eabi-gcc'
    args = [compiler, '-std=c99', '-Os', '-Wall', '-Wextra', '-Werror', '-fno-pic',
            '-fno-builtin', '-nostdlib', '-Wl,-Ttext=0x500000', '-Wl,-Tdata=0x510000',
            '-Wl,--entry=mhi2q_si_transport_install',
            *[f'-Wl,--defsym={name}={address}' for name, address in SENTINELS.items()],
            '/src/wireless/receiver_bridge/si_transport.c', '-o',
            '/src/build/si-wireless-offline-re/si_transport.arm']
    subprocess.run(['docker', 'run', '--rm', '--platform=linux/amd64', '-v',
                    f'{ROOT}:/src', 'qnx65-armv7-toolchain:latest', *args], check=True)
    return OUT / 'si_transport.arm'


class Machine:
    def __init__(self, adapter):
        self.u = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        for address, size in ((0x100000, 0x100000), (HEAP, 0x20000),
                              (0x500000, 0x20000), (0x600000, 0x2000),
                              (STACK - 0x10000, 0x10000)):
            self.u.mem_map(address, size)
        self.symbols = {}
        for path in (SOURCE, adapter):
            with path.open('rb') as stream:
                elf = ELFFile(stream)
                for segment in elf.iter_segments():
                    if segment['p_type'] == 'PT_LOAD':
                        self.u.mem_write(segment['p_vaddr'], segment.data())
                for section in elf.iter_sections():
                    if section['sh_type'] in ('SHT_SYMTAB', 'SHT_DYNSYM'):
                        self.symbols.update({s.name: s['st_value'] for s in section.iter_symbols() if s['st_value']})
                    if isinstance(section, RelocationSection):
                        symbols = elf.get_section(section['sh_link'])
                        for relocation in section.iter_relocations():
                            symbol = symbols.get_symbol(relocation['r_info_sym'])
                            if relocation['r_info_type'] == 21 and symbol['st_value']:
                                self.put(relocation['r_offset'], symbol['st_value'])
        self.env = {'MHI2Q_CP_SI_EPOCH': '1234', 'MHI2Q_CP_SI_SESSION': '7',
                    'MHI2Q_CP_SI_LINK': '8', 'MHI2Q_CP_SI_PARENT_PID': '200',
                    'MHI2Q_DIO_OWNER_MODE': 'wireless'}
        self.parent = 200
        self.states, self.starts, self.calls = [], [], []
        self.manager, self.service, self.dsi, self.device, self.config = [HEAP + n for n in (0, 0x1000, 0x2000, 0x3000, 0x4000)]
        self.put(self.service + 112, self.manager)
        self.put(self.dsi + 232, self.manager)
        self.put(self.device, HEAP + 0x5000)
        self.put(self.device + 4, HEAP + 0x5100)
        for address in (0x1a99f0, 0x1a9a5c):
            self.u.mem_write(address, b'\xff\x7f')  # skip external tracing only
        self.u.hook_add(UC_HOOK_CODE, self.intercept)
        self.u.hook_add(UC_HOOK_MEM_INVALID, self.fault)

    def word(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def put(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def string(self, address):
        value = bytearray()
        while address and (c := self.u.mem_read(address, 1)) != b'\0':
            value += c
            address += 1
        return value.decode()

    def intercept(self, u, address, size, _):
        if address == END:
            u.emu_stop()
            return
        r0, r1, r2 = [u.reg_read(UC_ARM_REG_R0 + i) for i in range(3)]
        result = 0
        if address == SENTINELS['getenv']:
            name = self.string(r0)
            value = self.env.get(name)
            result = HEAP + 0x8000 + list(self.env).index(name) * 256 if value is not None else 0
            if value is not None:
                u.mem_write(result, value.encode() + b'\0')
        elif address == SENTINELS['getppid']:
            result = self.parent
        elif address == SENTINELS['__get_errno_ptr']:
            result = HEAP + 0x7000
        elif address == SENTINELS['strtoull']:
            result = int(self.string(r0), r2)
            self.put(r1, r0 + len(self.string(r0)))
            u.reg_write(UC_ARM_REG_R0 + 1, result >> 32)
        elif address == SENTINELS['strcmp']:
            result = int(self.string(r0) != self.string(r1))
        elif address in (SENTINELS['memset'], 0x112a04):
            u.mem_write(r0, bytes([r1 & 255]) * r2)
            result = r0
        elif address == SENTINELS['fputs']:
            pass
        elif address == 0x113178:  # external SharedPtr assignment; retained object modeled
            u.mem_write(r0, bytes(u.mem_read(r1, 12)))
        elif address == 0x164934:  # observe native ASI client boundary
            self.states.append(r1)
        elif address == 0x15a758:  # native BT list copy, external ownership modeled
            assert r0 == self.manager and r1 == self.config + 68
        elif address == 0x157460:
            self.starts.append({'width': self.word(r0 + 548), 'height': self.word(r0 + 552),
                                'hid': u.mem_read(r0 + 372, 1)[0],
                                'touch': [self.word(r0 + n) for n in (364, 368)],
                                'physical': [self.word(r0 + n) for n in (564, 568)]})
        elif address == 0x167990:
            self.calls.append('invalid_configuration')
        else:
            if 0x112000 <= address < 0x114000:
                raise AssertionError(f'unmodeled external call {address:#x}')
            return
        u.reg_write(UC_ARM_REG_R0, result & 0xffffffff)
        u.reg_write(UC_ARM_REG_PC, u.reg_read(UC_ARM_REG_LR))

    def fault(self, u, access, address, size, value, _):
        raise AssertionError(f'ARM memory fault pc={u.reg_read(UC_ARM_REG_PC):#x} lr={u.reg_read(UC_ARM_REG_LR):#x} address={address:#x}')

    def call(self, function, *args):
        self.u.reg_write(UC_ARM_REG_SP, STACK - 0x100)
        self.u.reg_write(UC_ARM_REG_LR, END)
        for i in range(4):
            self.u.reg_write(UC_ARM_REG_R0 + i, args[i] if i < len(args) else 0)
        self.u.emu_start(function, 0, count=200000)
        assert self.u.reg_read(UC_ARM_REG_PC) == END
        return self.u.reg_read(UC_ARM_REG_R0)

    def configure(self, mode, width, height, hid):
        cfg, shared = self.config, self.config + 0x100
        self.u.mem_write(cfg, bytes(100))
        for offset, value in ((24, mode), (28, width), (32, height), (60, 1024),
                              (64, 1024), (68, cfg + 0x200), (72, cfg + 0x210),
                              (84, 88), (88, 188), (92, hid)):
            self.put(cfg + offset, value)
        self.put(cfg + 0x210, 1)  # one test BT identity; no physical BT claim
        self.put(shared, cfg + 0x220)
        self.put(shared + 4, cfg)
        self.call(0x16a7cc, self.dsi, shared)


def main():
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == EXPECTED
    adapter = compile_adapter()
    cases = []
    for mode, width, height, hid, dimensions in (
            (0, 0, 0, 1, (800, 480)), (1, 0, 0, 9, (960, 540)),
            (2, 0, 0, 9, (1024, 480)), (3, 1280, 720, 0, (1280, 720))):
        m = Machine(adapter)
        assert m.call(m.symbols['mhi2q_si_transport_install'], m.manager) == 1
        init, setup = m.word(0x1a4e18), m.word(0x1a4e1c)
        assert m.call(init, m.service, m.device, HEAP + 0x6000) == 1
        assert m.states == [1] and not m.starts, 'transport setup must not start unconfigured Screen/HID'
        assert m.call(setup, m.service) == 1 and m.states == [1] and not m.starts
        assert m.call(m.symbols['mhi2q_si_transport_initialized'], m.manager) == 1
        m.configure(mode, width, height, hid)
        assert m.starts == [{'width': dimensions[0], 'height': dimensions[1], 'hid': hid,
                             'touch': [1024, 1024], 'physical': [188, 88]}], m.starts
        m.parent += 1
        assert m.call(m.symbols['mhi2q_si_transport_initialized'], m.manager) == 0
        cases.append({'mode': mode, **m.starts[0]})
    m = Machine(adapter)
    m.configure(3, 0, 480, 9)
    assert not m.starts and m.calls == ['invalid_configuration']
    receipt = {'status': 'PASS', 'native_dio_sha256': EXPECTED, 'adapter_sha256': hashlib.sha256(adapter.read_bytes()).hexdigest(),
               'cases': cases, 'setup_starts_airplay': False, 'invalid_dimensions_rejected': True,
               'models': ['libc', 'ASI delivery', 'SharedPtr retention', 'BT list copy', 'AirPlay queue boundary'],
               'not_proven': ['real HMI notification delivery', 'QNX full lifecycle', 'physical inputs/audio/video']}
    (OUT / 'hmi-start-arm.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()

"""Freestanding ARM C policies plus pinned native MU1316 receive/completion.

The native timer/queue/registry boundaries below are OFFLINE adapters. They
exercise the policy instructions and driver instructions without producing a
loadable modified QNX driver or pretending to implement real callback draining.
"""
from __future__ import annotations

import hashlib
import io
from pathlib import Path
import struct
import subprocess

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
)
from .reorder_machine import SAVED_REGISTERS
from .probe_latency_candidates import TxCompletionMachine
from .rx_timer_churn_probe import LazyMachine, signed_delta


def compile_policies(out: Path):
    out.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).with_name('wifi_latency_policy.c')
    header = source.with_suffix('.h')
    obj, elf_path, script = (out / n for n in ('wifi_latency_policy.o', 'wifi_latency_policy.elf', 'policy.ld'))
    script.write_text('SECTIONS { . = 0x800000; .text : { *(.text*) *(.rodata*) } '
                      '. = ALIGN(16); .data : { *(.data*) } .bss : { *(.bss*) } '
                      '/DISCARD/ : { *(.ARM.exidx*) *(.comment*) *(.note*) } }\n')
    subprocess.run(['clang', '--target=armv7-none-eabi', '-march=armv7-a', '-marm', '-O2',
                    '-mfloat-abi=soft', '-fno-vectorize', '-fno-slp-vectorize',
                    '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
                    '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
                    '-Wall', '-Wextra', '-Werror', '-c', str(source), '-o', str(obj)],
                   check=True, capture_output=True, timeout=30)
    subprocess.run(['ld.lld', '-T', str(script), '--no-undefined', '--entry=wl_rx_age_create',
                    str(obj), '-o', str(elf_path)], check=True, capture_output=True, timeout=30)
    data = elf_path.read_bytes()
    elf = ELFFile(io.BytesIO(data))
    undefined = [s.name for s in elf.get_section_by_name('.symtab').iter_symbols()
                 if s.name and s['st_shndx'] == 'SHN_UNDEF']
    if undefined:
        raise ValueError(f'unresolved ARM policy runtime helpers: {undefined}')
    return data, {'elf_sha256': hashlib.sha256(data).hexdigest(),
                  'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                  'header_sha256': hashlib.sha256(header.read_bytes()).hexdigest(),
                  'undefined_symbols': undefined, 'loadable_qnx_driver': False}


class PolicyMachine:
    """Run the actual cross-compiled C functions; no mocked policy decisions."""
    rx, tx, slots, out, ticket = 0x900000, 0x901000, 0x902000, 0x903000, 0x903100
    stack, stop = 0xA00000, 0xB00000

    def __init__(self, data):
        elf = ELFFile(io.BytesIO(data))
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.cpu.mem_map(0x7F0000, 0x30000)
        self.cpu.mem_map(self.rx, 0x10000)
        self.cpu.mem_map(self.stack, 0x10000)
        self.cpu.mem_map(self.stop, 0x1000)
        self.symbols = {s.name: int(s['st_value']) for s in elf.get_section_by_name('.symtab').iter_symbols()
                        if s['st_shndx'] != 'SHN_UNDEF' and s['st_info']['type'] == 'STT_FUNC'}
        for segment in elf.iter_segments():
            if segment['p_type'] == 'PT_LOAD':
                self.cpu.mem_write(int(segment['p_vaddr']), segment.data())
        text = elf.get_section_by_name('.text')
        self.text_address, self.text = int(text['sh_addr']), text.data()
        self.cpu.hook_add(UC_HOOK_CODE, self._bound)
        self.call('wl_policy_abi', self.out)
        self.abi = self.words(self.out, 5)
        assert self.abi == (796, 20, 8, 8, 6), self.abi

    def _bound(self, uc, address, size, user):
        if not self.text_address <= address < self.text_address + len(self.text):
            raise AssertionError(f'policy executed outside reviewed text: {address:#x}')

    def words(self, address, count):
        return struct.unpack('<' + 'I' * count, self.cpu.mem_read(address, count * 4))

    def write_words(self, address, words):
        self.cpu.mem_write(address, struct.pack('<' + 'I' * len(words), *(v & 0xFFFFFFFF for v in words)))

    def call(self, name, *arguments):
        sp = self.stack + 0xF000
        for i, r in enumerate((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)):
            self.cpu.reg_write(r, arguments[i] if i < len(arguments) else 0)
        self.write_words(sp, arguments[4:])
        saved = {r: 0xC1000000 + i * 256 for i, r in enumerate(SAVED_REGISTERS)}
        for r, value in saved.items(): self.cpu.reg_write(r, value)
        self.cpu.reg_write(UC_ARM_REG_SP, sp)
        self.cpu.reg_write(UC_ARM_REG_LR, self.stop)
        self.cpu.emu_start(self.symbols[name], self.stop, timeout=1_000_000, count=100000)
        assert self.cpu.reg_read(UC_ARM_REG_PC) == self.stop
        assert self.cpu.reg_read(UC_ARM_REG_SP) == sp
        assert all(self.cpu.reg_read(r) == v for r, v in saved.items())
        assert bytes(self.cpu.mem_read(self.text_address, len(self.text))) == self.text
        return self.cpu.reg_read(UC_ARM_REG_R0)

    def age_create(self, generation=1):
        return self.call('wl_rx_age_create', self.rx, generation)

    def age_observe(self, generation, now, start, slots, *, incoming=0, sequence=0, arrived=0):
        self.write_words(self.slots, slots)
        before = bytes(self.cpu.mem_read(self.slots, 4 * len(slots)))
        status = self.call('wl_rx_age_observe', self.rx, generation, now & 0xFFFFFFFF,
                           start, self.slots, len(slots), incoming, sequence,
                           arrived & 0xFFFFFFFF, self.out)
        assert bytes(self.cpu.mem_read(self.slots, 4 * len(slots))) == before
        return status, self.words(self.out, 5)

    def tx_request(self, generation=1, stopped=True, free=True):
        result = self.call('wl_tx_request', self.tx, generation, int(stopped), int(free), self.ticket)
        return result, self.words(self.ticket, 2)


class AgeCapMachine(LazyMachine):
    """Original ARM RX + root's ARM C arrival ages + agent's ARM timer policy."""
    def __init__(self, image, *, policy_elf, **kwargs):
        self.age = PolicyMachine(policy_elf)
        assert self.age.age_create() == 0
        self.incoming = None
        super().__init__(image, age_budget_ticks=None, **kwargs)

    def receive(self, sequence):
        self.incoming = (0x500000 + 256 * len(self.ownership), sequence, self.tick())
        try:
            return super().receive(sequence)
        finally:
            self.incoming = None

    def policy_event(self, event, *, progressed=False):
        # Models the same table lock/lifetime exclusion required by native RX.
        snapshot = [self.get(self.slots + i * 4) for i in range(self.window)]
        incoming, sequence, arrived = self.incoming if self.incoming else (0, 0, 0)
        status, result = self.age.age_observe(self.policy.generation, self.tick(),
                                            self.get(self.table + 20), snapshot,
                                            incoming=incoming, sequence=sequence, arrived=arrived)
        assert status == 0
        valid, due, expired, oldest, pending = result
        action = self.policy.invoke(event, self.tick(), bool(pending), progressed,
                                    age_cap=due if valid else None)
        assert not action & 8
        if action & 2:
            self.due_ms = None
            self.events.append({'event': 'stop', 'at_ms': self.now_ms})
        if action & 1:
            target = self.policy.words()[5]
            remaining = signed_delta(target, self.tick() & 0xFFFFFFFF)
            assert 0 < remaining <= 6
            self.due_ms = (self.tick() + remaining) * 8403 / 1000
            self.events.append({'event': 'arm', 'at_ms': self.now_ms, 'due_ms': self.due_ms})
        self.cpu.mem_write(self.table + 36, bytes([int(bool(self.policy.words()[4]))]))
        return action

    def delete(self):
        assert self.age.call('wl_rx_age_retire', self.age.rx, self.policy.generation) == 0
        super().delete()


class EarlyWakeMachine(TxCompletionMachine):
    """Original completion with gate bypassed + actual ARM C coalescing policy.

    The callback enqueue/dispatch is an explicit queue model retaining the native
    owner argument. Registry binding/lifetime are represented by one sidecar.
    """
    def __init__(self, image, *, policy_elf):
        self.policy = PolicyMachine(policy_elf)
        self.policy.call('wl_tx_init', self.policy.tx)
        assert self.policy.call('wl_tx_bind', self.policy.tx, 1) == 0
        self.queue, self.requested, self.fail_next = [], [], False
        super().__init__(image, naive_early_wake=True)

    def _hook(self, uc, address, size, user):
        if address == self.address('stk_context_callback_2'):
            original = tuple(uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2))
            assert original == (0x1234, 0x5678, 0x9ABC)
            action, ticket = self.policy.tx_request()
            if action == 1:
                self.requested.append(ticket)
                if self.fail_next:
                    self.fail_next = False
                    assert self.policy.call('wl_tx_submit_failed', self.policy.tx, *ticket) == 0
                else:
                    assert not self.queue
                    self.queue.append((ticket, original))
                    self.notifications.append(self.get(self.handle + 344))
            self._return()
            return
        super()._hook(uc, address, size, user)

    def consume(self, *, during_run=None):
        ticket, original = self.queue.pop(0)
        action = self.policy.call('wl_tx_begin', self.policy.tx, *ticket)
        if action == 2:
            # The original mrvl_kick_tx call remains an explicit stack/scheduler
            # boundary, not mocked proof that native detach serialization works.
            if during_run is not None: during_run()
            follow = self.policy.call('wl_tx_finish', self.policy.tx, *ticket, self.policy.ticket)
            if follow == 1:
                assert not self.queue
                self.queue.append((self.policy.words(self.policy.ticket, 2), original))
        else:
            assert action == 3
        return action

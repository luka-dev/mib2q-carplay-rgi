"""Pinned io-pkt ARM callouts with explicit mutex/thread/clock boundaries.

Suspension at the original indirect call permits controlled interleaving;
it is not proof that the HU scheduler permits that interleaving.
"""
from __future__ import annotations

import hashlib
import io
import struct

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)

from .reorder_machine import SAVED_REGISTERS

STACK_SHA256 = "baf6e5df318ba3ea22cf669017b04185a4da8abb77adb4cd6e40629a8e80dae7"
EXECUTED = ("callout_startup", "callout_init", "callout_msec", "callout_reset", "callout_stop",
            "callout_hardclock", "softclock")
CAPTURED_CALL = 0x1AFF1C


class CalloutMachine:
    entry, tls_pointer, tls = 0x400000, 0x401000, 0x402000
    ops, threads, thread, context = 0x403000, 0x404000, 0x405000, 0x406000
    lock, unlock, callback_stub, stop = 0x500000, 0x500004, 0x500008, 0x500100
    call_stack, soft_stack = 0x600000, 0x610000

    def __init__(self, data: bytes, *, hz=119, callback=None):
        if hashlib.sha256(data).hexdigest() != STACK_SHA256:
            raise ValueError("only the fingerprinted MU1316 io-pkt is supported")
        if type(hz) is not int or not 1 <= hz <= 10000:
            raise ValueError("invalid synthetic hz")
        elf = ELFFile(io.BytesIO(data))
        if elf["e_type"] != "ET_EXEC":
            raise ValueError("expected the unrelocated io-pkt executable")
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.cpu.mem_map(0x100000, 0x150000)
        for segment in elf.iter_segments():
            if segment["p_type"] == "PT_LOAD":
                self.cpu.mem_write(int(segment["p_vaddr"]), segment.data())
        self.cpu.mem_map(self.entry, 0x10000)
        self.cpu.mem_map(self.lock, 0x1000)
        self.cpu.mem_map(self.call_stack, 0x20000)
        self.symbols = {s.name: (int(s["st_value"]), int(s["st_size"]))
                        for s in elf.get_section_by_name(".symtab").iter_symbols()}
        self.allowed = [range(*[a, a+size]) for a, size in (self.symbols[n] for n in EXECUTED)]
        text = elf.get_section_by_name(".text")
        self.text_address = int(text["sh_addr"])
        self.text = text.data()
        # Native callouts fetch the current tracked thread through cpupage/TLS.
        # The literal 0x23b26c is mislabeled _Stdout.__pad3 by generic IDA types.
        self.put(0x23B26C, self.tls_pointer)
        self.put(self.tls_pointer, self.tls)
        self.put(self.tls + 24, 2)
        self.put(0x24A144, self.threads)
        self.put(self.threads, self.thread)
        self.put(0x24A088, self.ops)
        self.put(self.ops + 12, self.lock)
        self.put(self.ops + 16, self.unlock)
        self.put(self.symbols["hz"][0], hz)
        self.hz = hz
        self.callback = callback
        self.callback_entry = None
        self.lock_depth = 0
        self.calls = []
        self.saved_softclock = None
        self.pause_at_callback = False
        self.steps = 0
        self.cpu.hook_add(UC_HOOK_CODE, self._hook)
        self.call("callout_startup")
        self.call("callout_init", self.entry)

    def put(self, address, value):
        self.cpu.mem_write(address, struct.pack("<I", value & 0xFFFFFFFF))

    def get(self, address):
        return struct.unpack("<I", self.cpu.mem_read(address, 4))[0]

    def _return(self, value=0):
        self.cpu.reg_write(UC_ARM_REG_R0, value)
        self.cpu.reg_write(UC_ARM_REG_PC, self.cpu.reg_read(UC_ARM_REG_LR))

    def _hook(self, uc, address, _size, _user):
        self.steps += 1
        if address == CAPTURED_CALL and self.pause_at_callback:
            if self.lock_depth or self.get(self.thread):
                raise AssertionError("softclock callback still holds exclusion")
            self.pause_at_callback = False
            uc.emu_stop()
        elif address == self.lock:
            if uc.reg_read(UC_ARM_REG_R0) != self.symbols["callout_mtx"][0] or self.lock_depth:
                raise AssertionError("unexpected or recursive native callout lock")
            self.lock_depth = 1
            self._return()
        elif address == self.unlock:
            if self.lock_depth != 1:
                raise AssertionError("unbalanced native callout unlock")
            self.lock_depth = 0
            self._return()
        elif address == self.symbols["pthread_mutex_init"][0]:
            self._return()
        elif address == self.callback_stub or address == self.callback_entry:
            if self.lock_depth or self.get(self.thread):
                raise AssertionError("callback entered with callout exclusion held")
            argument = uc.reg_read(UC_ARM_REG_R0)
            self.calls.append({"argument": argument, "tick": self.get(self.symbols["hardclock_ticks"][0])})
            if self.callback is not None:
                self.callback(argument)
            self._return()
        elif not any(address in region for region in self.allowed):
            raise AssertionError(f"unexpected io-pkt execution at {address:#x}")

    def call(self, name, *arguments, soft=False, pause=False):
        sp = (self.soft_stack if soft else self.call_stack) + 0xFFF0
        for i, register in enumerate((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)):
            self.cpu.reg_write(register, arguments[i] if i < len(arguments) else 0)
        saved = {register: 0xB0000000 + i*256 for i, register in enumerate(SAVED_REGISTERS)}
        for register, value in saved.items():
            self.cpu.reg_write(register, value)
        self.cpu.reg_write(UC_ARM_REG_SP, sp)
        self.cpu.reg_write(UC_ARM_REG_LR, self.stop)
        self.pause_at_callback = pause
        self.steps = 0
        self.cpu.emu_start(self.symbols[name][0], self.stop, timeout=1_000_000, count=20000)
        if pause and self.cpu.reg_read(UC_ARM_REG_PC) == CAPTURED_CALL:
            self.saved_softclock = (self.cpu.context_save(), sp, saved)
            return "captured"
        self._check_return(sp, saved)
        return self.cpu.reg_read(UC_ARM_REG_R0)

    def _check_return(self, sp, saved):
        if (self.cpu.reg_read(UC_ARM_REG_PC) != self.stop or self.cpu.reg_read(UC_ARM_REG_SP) != sp
                or self.lock_depth or self.get(self.thread)
                or any(self.cpu.reg_read(reg) != value for reg, value in saved.items())):
            raise AssertionError("io-pkt call bound, ABI or lock-depth failure")
        if bytes(self.cpu.mem_read(self.text_address, len(self.text))) != self.text:
            raise AssertionError("io-pkt instructions changed")

    def schedule(self, argument, milliseconds=50):
        self.call("callout_msec", self.entry, milliseconds, self.callback_stub, argument)

    def cancel(self):
        return self.call("callout_stop", self.entry)

    def drain(self, *, pause=False):
        if self.saved_softclock is not None:
            raise ValueError("a native softclock continuation is already suspended")
        return self.call("softclock", self.context, soft=True, pause=pause)

    def resume(self):
        if self.saved_softclock is None:
            raise ValueError("no captured native callback")
        context, sp, saved = self.saved_softclock
        self.saved_softclock = None
        self.cpu.context_restore(context)
        self.cpu.emu_start(CAPTURED_CALL, self.stop, timeout=1_000_000, count=20000)
        self._check_return(sp, saved)

    def advance(self, ticks, *, pause=False):
        if not 0 <= ticks <= 1000:
            raise ValueError("tick advance is bounded")
        for _ in range(ticks):
            self.put(self.symbols["hardclock_ticks"][0], self.get(self.symbols["hardclock_ticks"][0]) + 1)
            self.call("callout_hardclock")
            if self.drain(pause=pause) == "captured":
                return "captured"

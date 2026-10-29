"""Pinned original ARM reorder/timer/lifecycle execution with OS boundary stubs.

Optional candidates change only guarded instruction fragments in Unicorn.
ELF relocation models loading at base zero. No driver file is ever patched.
"""
from __future__ import annotations

import hashlib
import io
from pathlib import Path
import struct
import subprocess

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_PROT_NONE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7, UC_ARM_REG_R8,
    UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)

from .probe_rx_reorder import SHA256

PATCH_ADDRESS = 0x47368
ORIGINAL_FRAGMENT = bytes.fromhex("012082e20c0094e5022aa0e1222aa0e1")
WINDOW_PATCH_ADDRESS = 0x46D64
ORIGINAL_WINDOW_FRAGMENT = bytes.fromhex("14a091e502005ae118a091a5030000aa02a06ae0183091e503005ae103a0a0a1")
HARDEN_PATCH_ADDRESS = 0x46ED8
ORIGINAL_HARDEN_FRAGMENT = bytes.fromhex("0500000a")
EXECUTED = ("wlan_flush_data", "wlan_11n_dispatch_pkt_until_start_win", "wlan_11n_scan_and_dispatch",
    "woal_timer_handler", "mlan_11n_rxreorder_pkt", "wlan_11n_get_rxreorder_tbl", "moal_memcmp",
    "moal_start_timer", "moal_stop_timer", "moal_free_timer", "wlan_11n_delete_rxreorder_tbl_entry",
    "wlan_cmd_11n_addba_rspgen", "moal_memcpy")
STUBS = ("wlan_11n_display_tbl_ptr", "wlan_11n_dispatch_pkt", "memcmp", "memcpy", "callout_msec", "callout_stop", "free_bsd")
SAVED_REGISTERS = (UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
                   UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11)


def compile_candidate(directory: Path) -> tuple[bytes, dict]:
    return _compile_fragment(directory, "rx_reorder_expiry_candidate.S", "rx-reorder-expiry-fragment.o",
                             PATCH_ADDRESS, ORIGINAL_FRAGMENT)


def compile_window_candidate(directory: Path) -> tuple[bytes, dict]:
    return _compile_fragment(directory, "rx_reorder_window_candidate.S", "rx-reorder-window-fragment.o",
                             WINDOW_PATCH_ADDRESS, ORIGINAL_WINDOW_FRAGMENT)


def compile_harden_candidate(directory: Path) -> tuple[bytes, dict]:
    return _compile_fragment(directory, "rx_timer_harden_candidate.S", "rx-timer-harden-fragment.o",
                             HARDEN_PATCH_ADDRESS, ORIGINAL_HARDEN_FRAGMENT)


def _compile_fragment(directory, source_name, object_name, address, original):
    source = Path(__file__).with_name(source_name)
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / object_name
    subprocess.run(["clang", "--target=armv7-none-eabi", "-c", str(source), "-o", str(target)],
                   check=True, capture_output=True, timeout=30)
    with target.open("rb") as file:
        fragment = ELFFile(file).get_section_by_name(".text").data()
    if len(fragment) != len(original):
        raise ValueError("candidate must fit the exact reviewed instruction span")
    return fragment, {"lab_only": True, "address": hex(address),
        "original_hex": original.hex(), "candidate_hex": fragment.hex(),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "object_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
        "scope": "in-memory Unicorn fragment; no patched driver file or deployment artifact"}


class DriverImage:
    def __init__(self, data: bytes):
        if (hashlib.sha256(data).hexdigest() != SHA256 or data[PATCH_ADDRESS:PATCH_ADDRESS+16] != ORIGINAL_FRAGMENT
                or data[WINDOW_PATCH_ADDRESS:WINDOW_PATCH_ADDRESS+32] != ORIGINAL_WINDOW_FRAGMENT):
            raise ValueError("only the fingerprinted MU1316 stock driver is supported")
        self.data = data
        elf = ELFFile(io.BytesIO(data))
        table = elf.get_section_by_name(".symtab")
        self.symbols = {s.name: (int(s["st_value"]), int(s["st_size"])) for s in table.iter_symbols()
                        if s["st_shndx"] != "SHN_UNDEF" and s["st_info"]["type"] == "STT_FUNC"}
        self.segments = [(int(s["p_vaddr"]), s.data()) for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]
        # ARM ELF PLT has a 20-byte resolver header and 12-byte import entries.
        relplt = elf.get_section_by_name(".rel.plt")
        dynsym = elf.get_section(relplt["sh_link"])
        plt = int(elf.get_section_by_name(".plt")["sh_addr"])
        for index, relocation in enumerate(relplt.iter_relocations()):
            name = dynsym.get_symbol(relocation["r_info_sym"]).name
            self.symbols.setdefault(name, (plt + 20 + 12*index, 12))
        self.relocations = []
        for section in elf.iter_sections():
            if section["sh_type"] != "SHT_REL":
                continue
            symtab = elf.get_section(section["sh_link"])
            for relocation in section.iter_relocations():
                kind, address = int(relocation["r_info_type"]), int(relocation["r_offset"])
                symbol = symtab.get_symbol(relocation["r_info_sym"])
                value = int(symbol["st_value"])
                if symbol["st_shndx"] == "SHN_UNDEF":
                    value = self.symbols.get(symbol.name, (0, 0))[0]
                self.relocations.append((kind, address, value))


class ReorderMachine:
    """Single-thread deterministic callout model; no concurrent scheduler proof."""
    adapter, handle, timer = 0x200000, 0x209000, 0x208000
    priv, table, slots, peer = 0x210000, 0x220000, 0x230000, 0x231000
    stub_lock, stub_unlock, stub_free = 0x300000, 0x300004, 0x300008
    stop, stack = 0x300100, 0x400000

    def __init__(self, image: DriverImage, *, start: int, window: int = 16, candidate: bytes | None = None,
                 window_candidate: bytes | None = None, harden_candidate: bytes | None = None,
                 callout_bridge=None):
        if type(start) is not int or not 0 <= start < 4096 or type(window) is not int or not 1 <= window <= 64:
            raise ValueError("invalid synthetic reorder window")
        if candidate is not None and len(candidate) != 16:
            raise ValueError("candidate fragment must be exactly 16 bytes")
        if window_candidate is not None and len(window_candidate) != 32:
            raise ValueError("window candidate fragment must be exactly 32 bytes")
        if harden_candidate is not None and len(harden_candidate) != 4:
            raise ValueError("harden candidate fragment must be exactly 4 bytes")
        self.image, self.window = image, window
        self.callout_bridge = callout_bridge
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.cpu.mem_map(0, 0x60000)
        self.cpu.mem_map(self.adapter, 0x40000)
        self.cpu.mem_map(self.stub_lock, 0x1000)
        self.cpu.mem_map(self.stack, 0x10000)
        for address, data in image.segments:
            self.cpu.mem_write(address, data)
        for kind, address, value in image.relocations:
            if kind == 23:  # R_ARM_RELATIVE, load base is zero.
                continue
            if kind == 2:  # R_ARM_ABS32.
                value += self.get(address)
            elif kind not in (21, 22):  # GLOB_DAT, JUMP_SLOT.
                raise ValueError(f"unreviewed ELF relocation kind {kind}")
            self.put(address, value & 0xFFFFFFFF)
        for address, original, fragment in ((PATCH_ADDRESS, ORIGINAL_FRAGMENT, candidate),
                (WINDOW_PATCH_ADDRESS, ORIGINAL_WINDOW_FRAGMENT, window_candidate),
                (HARDEN_PATCH_ADDRESS, ORIGINAL_HARDEN_FRAGMENT, harden_candidate)):
            if fragment is not None:
                if bytes(self.cpu.mem_read(address, len(original))) != original:
                    raise ValueError("candidate instruction guard mismatch after ELF loading")
                self.cpu.mem_write(address, fragment)
        self.initial_code = bytes(self.cpu.mem_read(0x8EE4, 0x4E5B4-0x8EE4))
        self.now_ms = 0
        self.due_ms = None
        self.alive = self.timer_alive = True
        self.ownership = {}
        self.delivered, self.rejected, self.events = [], [], []
        self.lock_depth = 0
        self.instructions = self.total_instructions = 0
        self.executed = set()
        self.helper_callers = set()
        self.head = self.priv + 6120
        self.put(self.priv, self.adapter)
        self.put(self.adapter, self.handle)
        for index, name in ((99, "moal_memcpy"), (101, "moal_memcmp"), (105, "moal_free_timer"), (106, "moal_start_timer"),
                            (107, "moal_stop_timer"), (115, "woal_timer_handler")):
            self.put(self.adapter + 4*index, self.address(name))
        for index, pointer in ((97, self.stub_free), (110, self.stub_lock), (111, self.stub_unlock)):
            self.put(self.adapter + 4*index, pointer)
        self.put(self.head, self.table)
        self.put(self.head + 4, self.table)
        self.put(self.table, self.head)
        self.put(self.table + 4, self.head)
        self.put(self.table + 8, 0)  # TID.
        self.cpu.mem_write(self.table + 12, bytes.fromhex("020102030405"))
        self.cpu.mem_write(self.peer, bytes.fromhex("020102030405"))
        self.put(self.table + 20, start)
        self.put(self.table + 24, window)
        self.put(self.table + 28, self.slots)
        self.put(self.table + 32, self.timer)
        self.put(self.table + 40, self.table)
        self.put(self.table + 44, self.priv)
        self.put(self.timer, self.address("wlan_flush_data"))
        self.put(self.timer + 4, self.table + 32)
        self.put(self.timer + 16, 1)  # Initially stopped.
        self.allowed = [range(address, address + size) for address, size in
                        (image.symbols[name] for name in EXECUTED)]
        self.intercepted = {self.address(name): name for name in STUBS}
        self.cpu.hook_add(UC_HOOK_CODE, self._hook)

    def address(self, name):
        return self.image.symbols[name][0]

    def put(self, address, value):
        self.cpu.mem_write(address, struct.pack("<I", value & 0xFFFFFFFF))

    def get(self, address):
        return struct.unpack("<I", self.cpu.mem_read(address, 4))[0]

    def _return(self, value=0):
        self.cpu.reg_write(UC_ARM_REG_R0, value & 0xFFFFFFFF)
        self.cpu.reg_write(UC_ARM_REG_PC, self.cpu.reg_read(UC_ARM_REG_LR))

    def _deliver(self, pointer):
        packet = self.ownership[pointer]
        if packet["state"] != "pending" or self.lock_depth:
            raise AssertionError("duplicate delivery or delivery under the modeled lock")
        packet["state"] = "delivered"
        self.delivered.append({"sequence": packet["sequence"], "pointer": pointer, "at_ms": self.now_ms})

    def _hook(self, uc, address, _size, _user):
        self.instructions += 1
        self.total_instructions += 1
        r0, r1, r2, r3 = (uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3))
        if address == self.stub_lock:
            self.lock_depth += 1
            self._return()
        elif address == self.stub_unlock:
            self.lock_depth -= 1
            if self.lock_depth < 0:
                raise AssertionError("unbalanced unlock")
            self._return()
        elif address == self.stub_free:
            if r1 not in (self.table, self.slots):
                raise AssertionError("unexpected native reorder allocation free")
            self.events.append({"event": "free", "pointer": r1})
            self.cpu.mem_protect(r1, 0x1000, UC_PROT_NONE)
            self._return()
        elif address in self.intercepted:
            name = self.intercepted[address]
            if name == "wlan_11n_dispatch_pkt":
                self._deliver(r1)
            elif name == "memcmp":
                if r2 != 6:
                    raise AssertionError("unexpected peer comparison length")
                first, second = bytes(uc.mem_read(r0, r2)), bytes(uc.mem_read(r1, r2))
                self._return((first > second) - (first < second))
                return
            elif name == "memcpy":
                if r2 != 6:
                    raise AssertionError("unexpected ADDBA peer copy length")
                uc.mem_write(r0, bytes(uc.mem_read(r1, r2)))
                self._return(r0)
                return
            elif name == "callout_msec":
                if not self.timer_alive or r0 != self.timer + 20 or r1 != 50 or r2 != self.address("wlan_flush_data") or r3 != self.table + 32:
                    raise AssertionError("unexpected callout scheduling contract")
                if self.callout_bridge is not None:
                    self.callout_bridge.arm(r0, r1, r2, r3)
                    self.events.append({"event": "arm_native"})
                else:
                    self.due_ms = self.now_ms + r1
                    self.events.append({"event": "arm", "due_ms": self.due_ms})
            elif name == "callout_stop":
                if r0 != self.timer + 20:
                    raise AssertionError("unexpected timer stop")
                if self.callout_bridge is not None:
                    self.callout_bridge.stop(r0)
                    self.events.append({"event": "stop_native", "at_ms": self.now_ms})
                else:
                    self.due_ms = None
                    self.events.append({"event": "stop", "at_ms": self.now_ms})
            elif name == "free_bsd":
                if r0 != self.timer or not self.timer_alive or self.due_ms is not None:
                    raise AssertionError("timer freed twice or while scheduled")
                self.timer_alive = False
                self.cpu.mem_protect(self.timer, 0x1000, UC_PROT_NONE)
                if self.callout_bridge is not None:
                    self.callout_bridge.timer_freed()
                self.events.append({"event": "free_timer"})
            self._return()
        elif not any(address in region for region in self.allowed):
            raise RuntimeError(f"unexpected execution outside reviewed functions: {address:#x}")
        else:
            for name in EXECUTED:
                if address == self.address(name):
                    self.executed.add(name)
            if address == self.address("wlan_11n_dispatch_pkt_until_start_win"):
                link = uc.reg_read(UC_ARM_REG_LR)
                if link != self.stop:
                    self.helper_callers.add(hex(link - 4))

    def call(self, name, *arguments):
        sp = self.stack + 0xFFF0
        for index, register in enumerate((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)):
            self.cpu.reg_write(register, arguments[index] if index < len(arguments) else 0)
        for index, value in enumerate(arguments[4:]):
            self.put(sp + 4*index, value)
        saved = {register: 0xA0000000 + index*256 for index, register in enumerate(SAVED_REGISTERS)}
        for register, value in saved.items():
            self.cpu.reg_write(register, value)
        self.cpu.reg_write(UC_ARM_REG_SP, sp)
        self.cpu.reg_write(UC_ARM_REG_LR, self.stop)
        self.instructions = 0
        self.cpu.emu_start(self.address(name), self.stop, timeout=1_000_000, count=20_000)
        if self.cpu.reg_read(UC_ARM_REG_PC) != self.stop or self.cpu.reg_read(UC_ARM_REG_SP) != sp:
            raise AssertionError("execution bound or stack ABI failure")
        if self.lock_depth or any(self.cpu.reg_read(register) != value for register, value in saved.items()):
            raise AssertionError("lock balance or callee-saved register ABI failure")
        if bytes(self.cpu.mem_read(0x8EE4, len(self.initial_code))) != self.initial_code:
            raise AssertionError("native code modified during execution")
        return self.cpu.reg_read(UC_ARM_REG_R0)

    def packet(self, sequence):
        pointer = 0x500000 + 256*len(self.ownership)
        self.ownership[pointer] = {"sequence": sequence, "state": "pending"}
        return pointer

    def seed(self, offsets):
        if not self.alive or self.ownership or len(set(offsets)) != len(offsets) or any(not 0 <= n < self.window for n in offsets):
            raise ValueError("invalid initial slot fixture")
        start = self.get(self.table + 20)
        for offset in offsets:
            self.put(self.slots + offset*4, self.packet((start + offset) % 4096))
        self.call("moal_start_timer", self.handle, self.timer, 0, 50)
        self.put(self.table + 36, 1)

    def receive(self, sequence):
        if not self.alive or not 0 <= sequence < 4096:
            raise ValueError("receive requires a live table and a valid sequence")
        pointer = self.packet(sequence)
        result = self.call("mlan_11n_rxreorder_pkt", self.priv, sequence, 0, self.peer, 0, pointer)
        if result:
            if self.ownership[pointer]["state"] != "pending":
                raise AssertionError("rejected packet was already delivered")
            # Caller ownership on failure is explicit; native RX returns -1.
            self.ownership[pointer]["state"] = "rejected"
            self.rejected.append(sequence)
        return result

    def advance(self, until_ms):
        if self.callout_bridge is not None:
            raise ValueError("with native callouts advance ticks via the linked machine")
        if until_ms < self.now_ms:
            raise ValueError("modeled clock cannot move backwards")
        callbacks = 0
        while self.due_ms is not None and self.due_ms <= until_ms:
            if not self.alive or not self.timer_alive or callbacks >= 128:
                raise AssertionError("invalid or unbounded timer lifetime")
            self.now_ms, self.due_ms = self.due_ms, None
            self.call("wlan_flush_data", self.table + 32)
            callbacks += 1
        self.now_ms = until_ms
        return callbacks

    def delete(self):
        if not self.alive:
            raise ValueError("table already deleted")
        self.call("wlan_11n_delete_rxreorder_tbl_entry", self.priv, self.table)
        self.alive = False
        if self.timer_alive or self.due_ms is not None or self.get(self.head) != self.head or self.get(self.head + 4) != self.head:
            raise AssertionError("native timer/free/unlink lifecycle did not complete")
        if any(packet["state"] == "pending" for packet in self.ownership.values()):
            raise AssertionError("native teardown stranded a packet")

    def snapshot(self):
        slots = [self.get(self.slots + 4*n) for n in range(self.window)] if self.alive else []
        pending = [self.ownership[p]["sequence"] for p in slots if p]
        if len(slots) != len(set(p for p in slots if p)) + slots.count(0):
            raise AssertionError("a packet appears in multiple native slots")
        if {p for p in slots if p} != {p for p, state in self.ownership.items() if state["state"] == "pending"}:
            raise AssertionError("packet ownership and native slots disagree")
        return {"start": self.get(self.table + 20) if self.alive else None, "pending": pending,
                "delivered": [row["sequence"] for row in self.delivered], "rejected": self.rejected.copy(),
                "due_ms": self.due_ms, "table_alive": self.alive, "timer_alive": self.timer_alive,
                "events": self.events.copy(), "executed": sorted(self.executed)}

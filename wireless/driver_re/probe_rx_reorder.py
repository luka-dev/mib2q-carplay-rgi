#!/usr/bin/env python3
"""Execute pinned stock ARM RX reorder helpers offline; no driver/firmware I/O.

Requires unicorn and pyelftools. Only the reviewed reorder and timer functions
execute. Locks, delivery and diagnostics are
explicit stubs. This establishes helper behavior, not radio performance.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import struct

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

SHA256 = "d7fbf49b297665cbb8c6ba71b41ac892ccd062e0dcbd8339f3286555db1f394a"
EXECUTED = ("wlan_flush_data", "wlan_11n_dispatch_pkt_until_start_win", "wlan_11n_scan_and_dispatch", "woal_timer_handler")
INTERCEPTED = ("wlan_11n_display_tbl_ptr", "wlan_11n_dispatch_pkt")


def run_case(data: bytes, *, start: int, occupied: list[int], window: int = 16) -> dict:
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError("only the reviewed stock MU1316 driver is supported")
    if not 1 <= window <= 64 or not 0 <= start < 4096 or len(set(occupied)) != len(occupied) or any(not 0 <= n < window for n in occupied):
        raise ValueError("invalid synthetic reorder window")
    elf = ELFFile(io.BytesIO(data))
    symbols = {s.name: (int(s["st_value"]), int(s["st_size"]))
               for s in elf.get_section_by_name(".symtab").iter_symbols()}
    cpu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    cpu.mem_map(0, 0x60000)
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD":
            cpu.mem_write(segment["p_vaddr"], segment.data())
    heap, stubs, stack = 0x200000, 0x300000, 0x400000
    cpu.mem_map(heap, 0x40000)
    cpu.mem_map(stubs, 0x1000)
    cpu.mem_map(stack, 0x10000)
    adapter, priv, table, slots = heap, heap + 0x10000, heap + 0x20000, heap + 0x30000
    stop = stubs + 0x100

    def put(at, value):
        cpu.mem_write(at, struct.pack("<I", value))

    def get(at):
        return struct.unpack("<I", cpu.mem_read(at, 4))[0]

    put(priv, adapter)
    put(adapter, heap + 0x9000)
    put(adapter + 110 * 4, stubs)
    put(adapter + 111 * 4, stubs + 4)
    put(adapter + 115 * 4, symbols["woal_timer_handler"][0])
    put(table + 20, start)
    put(table + 24, window)
    put(table + 28, slots)
    put(table + 32, heap + 0x8000)
    put(table + 36, 1)
    put(table + 40, table)
    put(table + 44, priv)
    timer = heap + 0x8000
    put(timer, symbols["wlan_flush_data"][0])
    put(timer + 4, table + 32)
    put(timer + 8, 50)
    put(timer + 12, 0)
    put(timer + 16, 0)
    for offset in occupied:
        put(slots + offset * 4, 0x500000 + ((start + offset) % 4096) * 4)
    delivered, callbacks, calls = [], [], []
    allowed = [range(address, address + size) for address, size in (symbols[n] for n in EXECUTED)]
    display, dispatch = (symbols[n][0] for n in INTERCEPTED)

    def intercept(uc, address, _size, _user):
        if address in (display, dispatch, stubs, stubs + 4):
            if address == dispatch:
                delivered.append((uc.reg_read(UC_ARM_REG_R1) - 0x500000) // 4)
            uc.reg_write(UC_ARM_REG_R0, 0)
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
            return
        if not any(address in region for region in allowed):
            raise RuntimeError(f"unexpected execution outside reviewed helpers: {address:#x}")
        if address == symbols["woal_timer_handler"][0]:
            callbacks.append("woal_timer_handler_original")
        if address in (symbols[n][0] for n in EXECUTED):
            calls.append(next(n for n in EXECUTED if symbols[n][0] == address))

    cpu.hook_add(UC_HOOK_CODE, intercept)
    cpu.reg_write(UC_ARM_REG_R0, table + 32)
    cpu.reg_write(UC_ARM_REG_SP, stack + 0xFFF0)
    cpu.reg_write(UC_ARM_REG_LR, stop)
    cpu.emu_start(symbols["wlan_flush_data"][0], stop, timeout=1_000_000, count=10_000)
    if cpu.reg_read(UC_ARM_REG_PC) != stop:
        raise RuntimeError("reviewed helper did not finish within its bound")
    remaining = [((get(slots + n * 4) - 0x500000) // 4) for n in range(window) if get(slots + n * 4)]
    return {"start_before": start, "occupied_offsets_before": occupied, "window": window,
            "start_after": get(table + 20), "delivered_sequences": delivered,
            "retained_sequences": remaining, "timer_active_after": cpu.mem_read(table + 36, 1)[0],
            "timer_expired_after": get(timer + 16), "timer_interval_after": get(timer + 8),
            "callbacks": callbacks, "executed_helpers": calls}


def probe(data: bytes) -> dict:
    cases = {
        "single_hole": run_case(data, start=100, occupied=[1, 2, 3]),
        "two_holes": run_case(data, start=100, occupied=[1, 3]),
        "two_leading_holes": run_case(data, start=100, occupied=[2, 3]),
        "empty": run_case(data, start=100, occupied=[]),
        "sequence_wrap": run_case(data, start=4095, occupied=[1, 2, 3]),
        "two_holes_window32": run_case(data, start=100, occupied=[1, 3], window=32),
        "two_holes_window64": run_case(data, start=100, occupied=[1, 3], window=64),
    }
    assert cases["single_hole"]["delivered_sequences"] == [101, 102, 103]
    assert cases["two_holes"]["delivered_sequences"] == [101]
    assert cases["two_holes"]["retained_sequences"] == [103]
    assert cases["two_leading_holes"]["delivered_sequences"] == []
    assert cases["two_leading_holes"]["retained_sequences"] == [102, 103]
    assert cases["empty"]["start_after"] == 100
    assert cases["sequence_wrap"]["delivered_sequences"] == [0, 1, 2]
    for name in ("two_holes_window32", "two_holes_window64"):
        assert cases[name]["delivered_sequences"] == [101]
        assert cases[name]["retained_sequences"] == [103]
    assert all(c["timer_active_after"] == 0 and c["timer_expired_after"] == 1 and
               c["timer_interval_after"] == 0 and c["callbacks"] == ["woal_timer_handler_original"] for c in cases.values())
    return {"driver_sha256": SHA256, "original_instructions": True, "binary_patched": False,
            "scope": "isolated stock ARM helper execution, synthetic reorder tables; no firmware, QNX scheduler or RF",
            "stubs": [*INTERCEPTED, "lock", "unlock"], "cases": cases}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve() == args.binary.resolve():
        parser.error("the output must not replace the analyzed binary")
    result = probe(args.binary.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

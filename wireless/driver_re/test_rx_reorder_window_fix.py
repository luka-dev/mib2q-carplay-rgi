#!/usr/bin/env python3
"""Offline ARM window-distance correction and combined lifecycle regressions."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import random
import struct

from .reorder_machine import (DriverImage, ReorderMachine, compile_candidate,
                              compile_harden_candidate, compile_window_candidate,
                              ORIGINAL_WINDOW_FRAGMENT)
from .test_rx_reorder_fix import run as expiry_checks, require


def check_shift(image, fragment, start, window, target, offsets):
    machine = ReorderMachine(image, start=start, window=window, window_candidate=fragment)
    machine.seed(offsets)
    machine.call("wlan_11n_dispatch_pkt_until_start_win", machine.priv, machine.table, target)
    # Independent serial-space oracle; zero deliberately preserves full drain.
    distance = (target - start) % 4096
    take = min(distance, window) if distance else window
    state = machine.snapshot()
    require(state["delivered"] == [(start + n) % 4096 for n in sorted(offsets) if n < take],
            f"wrong shift delivery: {start=} {window=} {target=} {offsets=} {state=}")
    require(state["pending"] == [(start + n) % 4096 for n in sorted(offsets) if n >= take],
            "wrong retained slot compaction")
    require(state["start"] == target, "target start was not installed")
    machine.delete()
    require(machine.advance(5000) == 0, "deleted shift left a callback")


def check_addba(image, expiry, fragment, start, window, distance):
    machine = ReorderMachine(image, start=start, window=window, candidate=expiry, window_candidate=fragment)
    offsets = sorted({1, window - 1})
    machine.seed(offsets)
    request, response = 0x232000, 0x233000
    data = bytearray(14)
    data[1:7] = bytes.fromhex("020102030405")
    data[7] = 9  # Dialog token.
    data[8:10] = struct.pack("<H", 2 | window << 6)  # Immediate BA, TID 0.
    target = (start + distance) % 4096
    data[12:14] = struct.pack("<H", target)
    machine.cpu.mem_write(request, bytes(data))
    machine.cpu.mem_write(machine.priv + 6096, struct.pack("<H", window))
    require(machine.call("wlan_cmd_11n_addba_rspgen", machine.priv, response, request) == 0,
            "native existing-table ADDBA response failed")
    output = bytes(machine.cpu.mem_read(response, 24))
    require(output[9:15] == data[1:7] and output[15] == 9 and output[16:18] == bytes(2),
            "ADDBA peer/token/acceptance response changed")
    require(struct.unpack_from("<H", output, 18)[0] >> 6 == window, "ADDBA window changed")
    take = min(distance, window) if distance else window
    state = machine.snapshot()
    require(state["start"] == target and state["delivered"] ==
            [(start + n) % 4096 for n in offsets if n < take] and state["pending"] ==
            [(start + n) % 4096 for n in offsets if n >= take], "ADDBA reuse shifted the wrong slots")
    machine.delete()
    machine.advance(5000)
    return machine.helper_callers


def run(image, expiry, fragment, *, random_cases=512, harden_candidate=None):
    evidence = {"matrix_cases": 0, "random_cases": random_cases, "seed": 630, "addba_cases": 0}
    for window in (1, 2, 8, 16, 32, 64):
        for start in (0, 1, 100, 2047, 2048, 4090, 4095):
            for distance in sorted({0, 1, window - 1, window, window + 1, 2047, 2048, 4095}):
                check_shift(image, fragment, start, window, (start + distance) % 4096, list(range(window)))
                evidence["matrix_cases"] += 1
    rng = random.Random(630)
    for _ in range(random_cases):
        start, target, window = rng.randrange(4096), rng.randrange(4096), rng.randrange(1, 65)
        offsets = rng.sample(range(window), rng.randrange(window + 1))
        check_shift(image, fragment, start, window, target, offsets)
    # Both stock and original-fragment controls must fail the wrap oracle.
    for control in (None, ORIGINAL_WINDOW_FRAGMENT):
        try:
            check_shift(image, control, 4090, 64, 0, [1, 16])
        except AssertionError:
            pass
        else:
            raise AssertionError("wrap oracle accepted the original window code")
    for bad in (bytes(31), bytes(33)):
        try:
            ReorderMachine(image, start=0, window_candidate=bad)
        except ValueError:
            pass
        else:
            raise AssertionError("wrong-size window fragment accepted")
    callers = set()
    for start in (100, 4090, 4095):
        for window in (16, 64):
            for distance in (0, 1, 6, window, window + 1):
                callers.update(check_addba(image, expiry, fragment, start, window, distance))
                evidence["addba_cases"] += 1
    # The native BAR path moves the window without owning a data allocation.
    machine = ReorderMachine(image, start=4090, window=64, candidate=expiry, window_candidate=fragment)
    machine.seed([1, 16])
    require(machine.call("mlan_11n_rxreorder_pkt", machine.priv, 0, 0, machine.peer, 231, 0) == 0,
            "native BAR rejected")
    state = machine.snapshot()
    require(state["start"] == 0 and state["delivered"] == [4091] and state["pending"] == [10],
            "wrapped BAR drained beyond its new start")
    machine.advance(50)
    require(machine.snapshot()["delivered"] == [4091, 10] and not machine.snapshot()["pending"],
            "BAR plus expiry lost the retained tail")
    machine.delete()
    machine.advance(5000)
    callers.update(machine.helper_callers)
    # A received data packet also takes the shared window-move call site.
    machine = ReorderMachine(image, start=4090, window=64, candidate=expiry, window_candidate=fragment)
    for sequence in (4091, 10, 63):
        machine.receive(sequence)
    machine.advance(1)
    machine.receive(0)
    require(machine.snapshot()["delivered"] == [4091, 0], "receive released the young tail too soon")
    machine.advance(51)
    require(machine.snapshot()["delivered"] == [4091, 0, 10, 63], "combined receive/expiry order failed")
    machine.delete()
    callers.update(machine.helper_callers)
    require(callers == {"0x46ec0", "0x47378", "0x47608", "0x47e98"}, "native helper call-site coverage changed")
    evidence["native_helper_call_sites"] = sorted(callers)
    evidence["combined_expiry_regressions"] = expiry_checks(image, expiry, window_candidate=fragment,
                                                        harden_candidate=harden_candidate)
    evidence.update(passed=True, bar_cases=1, wrapped_receive_cases=1,
                    candidate_ready_for_hu=False,
                    limitations=["single-thread OS boundary model, no real QNX callout-drain/race validation",
                                 "no firmware/RF/TCP/CarPlay performance or live deployment result"])
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--with-harden", action="store_true", help="include the 4-byte timer-harden fragment")
    args = parser.parse_args()
    for name in ("rx-reorder-expiry-fragment.o", "rx-reorder-window-fragment.o", "rx-reorder-window-tests.json"):
        target = args.out_dir / name
        if target.resolve() == args.binary.resolve() or target.exists() and target.samefile(args.binary):
            parser.error("generated artifacts must not replace or alias the input driver")
    image = DriverImage(args.binary.read_bytes())
    expiry, expiry_manifest = compile_candidate(args.out_dir)
    fragment, window_manifest = compile_window_candidate(args.out_dir)
    harden_fragment, harden_manifest = compile_harden_candidate(args.out_dir) if args.with_harden else (None, None)
    result = run(image, expiry, fragment, harden_candidate=harden_fragment)
    sources = ("reorder_machine.py", "test_rx_reorder_fix.py", "test_rx_reorder_window_fix.py")
    result.update(driver_sha256=hashlib.sha256(image.data).hexdigest(),
                  patches=[expiry_manifest, window_manifest], harden_patch=harden_manifest,
                  source_sha256={name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                                 for name in sources},
                  file_unchanged=args.binary.read_bytes() == image.data)
    require(result["file_unchanged"], "input driver changed")
    (args.out_dir / "rx-reorder-window-tests.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("passed", "matrix_cases", "random_cases", "addba_cases",
                    "native_helper_call_sites", "file_unchanged", "candidate_ready_for_hu")}, indent=2))


if __name__ == "__main__":
    main()

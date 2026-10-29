#!/usr/bin/env python3
"""Bounded offline A/B tests of an in-memory RX timeout candidate.

Run with python3 -m wireless.driver_re.test_rx_reorder_fix BINARY --out-dir build/...
The original driver file is read-only. This creates a fragment object and JSON
evidence, never a patched driver suitable for HU deployment.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import random
from functools import partial

from .reorder_machine import (DriverImage, ReorderMachine, compile_candidate, compile_harden_candidate,
                              ORIGINAL_FRAGMENT)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def timeout_case(image, candidate, start, window, occupied, *, window_candidate=None):
    """Independent policy oracle: deliver each occupied offset in order."""
    machine = ReorderMachine(image, start=start, window=window, candidate=candidate, window_candidate=window_candidate)
    machine.seed(occupied)
    require(machine.advance(49) == 0, "candidate changed the 50ms deadline")
    require(not machine.delivered, "delivered before expiry")
    require(machine.advance(50) == 1, "expiry did not run once")
    state = machine.snapshot()
    wanted = [(start + offset) % 4096 for offset in sorted(occupied)]
    require(state["delivered"] == wanted, (start, window, occupied, state, wanted))
    wanted_start = (start + max(occupied) + 1) % 4096 if occupied else start
    require(state["start"] == wanted_start and state["pending"] == [], "wrong post-expiry sequence boundary")
    require(state["due_ms"] is None and machine.get(machine.timer + 16) == 1 and
            machine.get(machine.timer + 8) == 0 and machine.get(machine.table + 36) == 0,
            "one-shot timer did not finish")
    require(machine.advance(5000) == 0 and machine.snapshot() == state,
            "repeated clock advance changed a completed timer")
    return state


def run(image, candidate, *, random_cases=512, seed=629, window_candidate=None, harden_candidate=None):
    new_machine = partial(ReorderMachine, window_candidate=window_candidate, harden_candidate=harden_candidate)
    check_timeout = partial(timeout_case, window_candidate=window_candidate)
    evidence = {"ab": {}, "matrix_cases": 0, "random_cases": random_cases, "seed": seed,
                "lifecycle_cases": [], "validation_cases": [], "executed_functions": set()}
    # Demonstrate the actual defect, then the candidate, through native input.
    for label, fragment in (("stock", None), ("candidate", candidate)):
        machine = new_machine(image, start=100, candidate=fragment)
        require(machine.receive(101) == machine.receive(103) == 0, "native input was refused")
        machine.advance(50)
        state = machine.snapshot()
        evidence["ab"][label] = {"at_50ms": state}
        if label == "stock":
            require(state["delivered"] == [101] and state["pending"] == [103], "stock regression not reproduced")
        else:
            require(state["delivered"] == [101, 103] and not state["pending"], "candidate did not release retained packet")
        machine.advance(500)
        evidence["ab"][label]["at_500ms"] = machine.snapshot()
        evidence["executed_functions"].update(machine.executed)

    for window in (1, 2, 8, 16, 32, 64):
        patterns = {(), (window - 1,), tuple(range(window)), tuple(range(1, window, 2)),
                    tuple(n for n in range(window) if n % 3 == 2)}
        for start in (0, 1, 100, 2047, 2048, 4094, 4095):
            for occupied in sorted(patterns):
                state = check_timeout(image, candidate, start, window, list(occupied))
                evidence["matrix_cases"] += 1
                evidence["executed_functions"].update(state["executed"])

    rng = random.Random(seed)
    for _ in range(random_cases):
        window, start = rng.choice((1, 2, 8, 16, 32, 64)), rng.randrange(4096)
        occupied = rng.sample(range(window), rng.randrange(window + 1))
        check_timeout(image, candidate, start, window, occupied)

    # Arrival just before expiry: avoid skipping a packet that has arrived.
    for fragment in (None, candidate):
        machine = new_machine(image, start=100, candidate=fragment)
        machine.receive(101)
        machine.receive(103)
        machine.advance(49)
        require(machine.receive(100) == machine.receive(102) == 0, "pre-expiry holes should still be accepted")
        require(machine.snapshot()["delivered"] == [100, 101, 102, 103], "pre-expiry arrival ordering changed")
        machine.advance(99)
        require(machine.snapshot()["delivered"] == [100, 101, 102, 103], "expiry redelivered already released packets")
    evidence["lifecycle_cases"].append("late_before_deadline_matches_stock")

    # The policy tradeoff is explicit: after expiry the skipped hole is old.
    machine = new_machine(image, start=100, candidate=candidate)
    machine.receive(101)
    machine.receive(103)
    require(machine.receive(101) == 0xFFFFFFFF, "queued duplicate not rejected")
    machine.advance(50)
    for late in (100, 102, 103):
        require(machine.receive(late) == 0xFFFFFFFF, "expired hole or delivered duplicate was accepted")
    require(machine.receive(104) == 0, "next sequence refused after expiry")
    machine.advance(100)
    require(machine.snapshot()["delivered"] == [101, 103, 104], "late/duplicate ownership failure")
    evidence["lifecycle_cases"].append("late_after_deadline_and_duplicates_rejected")

    # Real native receive path over the sequence-number wrap.
    machine = new_machine(image, start=4094, candidate=candidate)
    machine.receive(4095)
    machine.receive(1)
    machine.advance(50)
    require(machine.snapshot()["delivered"] == [4095, 1] and machine.snapshot()["start"] == 2,
            "wrapped expiry boundary wrong")
    require(machine.receive(0) == 0xFFFFFFFF and machine.receive(2) == 0,
            "late/current sequence handling across wrap wrong")
    machine.advance(100)
    require(machine.snapshot()["delivered"] == [4095, 1, 2], "wrapped input double delivered")
    evidence["lifecycle_cases"].append("native_input_wrap_and_late_arrival")

    # A fresh burst after an expired burst still obtains its own timer.
    machine = new_machine(image, start=100, candidate=candidate)
    machine.receive(101)
    machine.receive(103)
    machine.advance(50)
    machine.advance(60)
    machine.receive(105)
    machine.receive(107)
    require(machine.snapshot()["due_ms"] == 110, "new burst did not arm a new timer")
    machine.advance(110)
    require(machine.snapshot()["delivered"] == [101, 103, 105, 107], "second burst release wrong")
    evidence["lifecycle_cases"].append("repeated_bursts_rearm_native_timer")

    # Expose the policy cost: a new tail is released on the existing deadline,
    # rather than obtaining its own 50ms grace period for a newer hole.
    machine = new_machine(image, start=100, candidate=candidate)
    machine.receive(101)
    machine.advance(49)
    machine.receive(103)
    machine.advance(50)
    require(machine.snapshot()["delivered"] == [101, 103], "young-tail policy changed")
    evidence["young_tail_policy"] = {"sequence": 103, "arrived_at_ms": 49,
                                    "released_at_ms": 50, "newer_hole": 102}
    evidence["lifecycle_cases"].append("young_tail_uses_existing_deadline_policy")

    # Execute native stop/free/unlink, protect freed pages, then advance time.
    # The callout cancellation boundary is modeled, not a QNX race simulation.
    for window in (16, 64):
        for when in (0, 49, 50, 51):
            machine = new_machine(image, start=4094, window=window, candidate=candidate)
            machine.receive(4095)
            machine.receive(1)
            machine.advance(when)
            machine.delete()
            before = machine.snapshot()
            require(machine.advance(5000) == 0 and machine.snapshot() == before,
                    "modeled callback survived native stop/free")
            evidence["executed_functions"].update(machine.executed)
            evidence["lifecycle_cases"].append(f"native_teardown_window{window}_at{when}ms")

    for window, start in ((0, 100), (65, 100), (16, -1), (16, 4096)):
        try:
            new_machine(image, start=start, window=window, candidate=candidate)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid window or sequence accepted")
    evidence["validation_cases"].append("invalid_window_and_start")
    for bad in (bytes(15), bytes(17)):
        try:
            new_machine(image, start=100, candidate=bad)
        except ValueError:
            pass
        else:
            raise AssertionError("wrong-size fragment accepted")
    evidence["validation_cases"].append("wrong_fragment_size")
    changed = bytearray(image.data)
    changed[0] ^= 1
    try:
        DriverImage(bytes(changed))
    except ValueError:
        pass
    else:
        raise AssertionError("changed driver accepted")
    evidence["validation_cases"].append("changed_driver_fingerprint")
    try:
        check_timeout(image, ORIGINAL_FRAGMENT, 100, 16, [1, 3])
    except AssertionError:
        pass
    else:
        raise AssertionError("policy oracle accepted the original broken fragment")
    evidence["validation_cases"].append("negative_control_original_fragment_fails_policy")
    evidence["executed_functions"] = sorted(evidence["executed_functions"])
    evidence["passed"] = True
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--random-cases", type=int, default=512)
    parser.add_argument("--with-harden", action="store_true", help="include the 4-byte timer-harden fragment")
    args = parser.parse_args()
    if not 0 <= args.random_cases <= 2000:
        parser.error("random cases must be 0..2000")
    for name in ("rx-reorder-expiry-fragment.o", "rx-reorder-candidate.json", "rx-reorder-fix-tests.json"):
        target = args.out_dir / name
        if target.resolve() == args.binary.resolve() or target.exists() and target.samefile(args.binary):
            parser.error("generated artifacts must not replace or alias the input driver")
    image = DriverImage(args.binary.read_bytes())
    original_hash = hashlib.sha256(image.data).hexdigest()
    fragment, manifest = compile_candidate(args.out_dir)
    harden_fragment, harden_manifest = compile_harden_candidate(args.out_dir) if args.with_harden else (None, None)
    (args.out_dir / "rx-reorder-candidate.json").write_text(json.dumps(manifest, indent=2) + "\n")
    result = run(image, fragment, random_cases=args.random_cases, harden_candidate=harden_fragment)
    result.update(driver_sha256=original_hash, patch=manifest, harden_patch=harden_manifest, file_unchanged=
                  hashlib.sha256(args.binary.read_bytes()).hexdigest() == original_hash,
                  limitations=["single-thread deterministic OS boundary stubs; no QNX callback-drain/race proof",
                               "no firmware, RF, TCP performance, live HU speed gain or deployment validation"])
    require(result["file_unchanged"], "original driver file changed")
    (args.out_dir / "rx-reorder-fix-tests.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("passed", "matrix_cases", "random_cases", "lifecycle_cases", "file_unchanged")}, indent=2))


if __name__ == "__main__":
    main()

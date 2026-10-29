#!/usr/bin/env python3
"""Constructed callback-lifetime interleavings over original ARM instructions.

Links the pinned driver to the pinned io-pkt callout engine and executes, on
the original instructions, for stock, for the combined in-memory candidate and
for the hardened candidate (unconditional delete-path stop):

1. native stop before capture (queued entry cancelled, no callback);
2. callback completes before teardown (one-shot flags, clean delete);
3. capture -> teardown -> resumed callback (use-after-free on freed pages;
   not fixable by the harden fragment - callout_stop cannot retract a captured
   callback, so this residual hazard needs a drain mechanism);
4. capture -> RX re-arm -> stale one-shot reset -> delete (stock frees the
   timer block while its callout is queued; the harden fragment cancels the
   queue entry first).

The interleavings are constructed at the QNX callout boundary; this does not
prove the HU scheduler produces them. No driver file is patched and no
deployment artifact is created.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from unicorn import UC_ERR_READ_PROT, UC_ERR_READ_UNMAPPED, UC_ERR_WRITE_PROT, \
    UC_ERR_WRITE_UNMAPPED, UcError

from .linked_callout_machine import LinkedMachine
from .reorder_machine import (DriverImage, compile_candidate, compile_harden_candidate,
                              compile_window_candidate)
from .test_rx_reorder_fix import require

FREED_FAULTS = (UC_ERR_READ_UNMAPPED, UC_ERR_WRITE_UNMAPPED, UC_ERR_READ_PROT, UC_ERR_WRITE_PROT)


def linked(image, io_pkt, candidate, window_candidate, harden_candidate):
    return LinkedMachine(image, io_pkt, start=100, window=16, candidate=candidate,
                         window_candidate=window_candidate, harden_candidate=harden_candidate)


def seed_and_capture(image, io_pkt, candidate, window_candidate, harden_candidate, *, armed=5):
    """Arms the native timer, ticks to just before expiry, suspends softclock
    between the mutex release and the indirect callback."""
    machine = linked(image, io_pkt, candidate, window_candidate, harden_candidate)
    machine.driver.seed([1, 3])
    require(machine.callout_state()["time"] == machine.io.hz * 50 // 1000 + 1,
            "native callout_msec tick conversion changed")
    require(machine.advance_ticks(armed) is None, "timer fired before expiry")
    require(machine.advance_ticks(1, pause=True) == "captured", "softclock did not suspend")
    state = machine.callout_state()
    require(state["running"] and not state["queued"] and not state["freed"],
            "wrong captured-callout state")
    return machine


def scenario_cancel_before_capture(image, io_pkt, candidate, window_candidate, harden):
    machine = linked(image, io_pkt, candidate, window_candidate, harden)
    machine.driver.seed([1, 3])
    require(machine.callout_state()["queued"], "native arm did not queue the entry")
    machine.advance_ticks(2)
    machine.driver.delete()
    require(machine.callout_state()["freed"], "timer not freed by delete")
    require(not machine.callout_state()["queued"] and not machine.callout_state()["running"],
            "native stop left the entry queued")
    machine.advance_ticks(300)
    require(machine.fired == [], "cancelled timer fired after the native stop")
    return {"fired": 0}


def scenario_callback_before_delete(image, io_pkt, candidate, window_candidate, harden):
    machine = linked(image, io_pkt, candidate, window_candidate, harden)
    machine.driver.seed([1, 3])
    require(machine.advance_ticks(5) is None, "timer fired before expiry")
    machine.advance_ticks(1)
    require(machine.fired == [6], "one-shot did not fire exactly once at tick 6")
    state = machine.driver.snapshot()
    require(state["delivered"] == ([101, 103] if candidate is not None else [101]),
            "wrong expiry delivery")
    require(machine.driver.get(machine.driver.timer + 16) == 1
            and machine.driver.get(machine.driver.timer + 8) == 0,
            "one-shot completion flags missing")
    require(not machine.callout_state()["queued"], "one-shot left a queued entry")
    machine.driver.delete()
    machine.advance_ticks(300)
    require(machine.fired == [6], "callback after completed delete")
    return {"fired": 1}


def scenario_capture_delete_resume(image, io_pkt, candidate, window_candidate, harden):
    machine = seed_and_capture(image, io_pkt, candidate, window_candidate, harden)
    machine.driver.delete()
    require(machine.callout_state()["freed"], "timer not freed by delete")
    try:
        machine.resume()
    except UcError as error:
        require(error.errno in FREED_FAULTS, f"unexpected fault class {error}")
        require(machine.faults, "fault not recorded")
        address = machine.faults[-1]["address"]
        require(machine.driver.table <= address < machine.driver.table + 0x1000,
                f"resumed callback faulted outside the freed table: {address:#x}")
    else:
        raise AssertionError("resumed callback ran to completion on freed pages")
    return {"fault_address": hex(machine.faults[-1]["address"])}


def scenario_capture_rearm_stale_reset(image, io_pkt, candidate, window_candidate, harden,
                                       *, dangling):
    machine = seed_and_capture(image, io_pkt, candidate, window_candidate, harden)
    events_before = len(machine.driver.events)
    require(machine.driver.receive(100) == 0, "window-closing packet was refused")
    events = machine.driver.events[events_before:]
    require(any(row["event"] == "stop_native" for row in events)
            and any(row["event"] == "arm_native" for row in events),
            "native RX did not stop and re-arm during the capture")
    require(machine.callout_state()["queued"] and machine.callout_state()["time"] == 12,
            "re-armed entry wrong")
    machine.resume()
    # The old callback completed AFTER the new arm: its one-shot epilogue resets
    # stopped/timeout and wlan_flush_data cleared the active byte, while the
    # io-pkt queue still holds the re-armed entry.
    require(machine.driver.get(machine.driver.timer + 16) == 1
            and machine.driver.get(machine.driver.timer + 8) == 0,
            "stale one-shot reset missing")
    require(machine.driver.get(machine.driver.table + 36) == 0, "active byte not cleared")
    require(machine.callout_state()["queued"] and machine.callout_state()["time"] == 12,
            "divergence not present: queue empty or deadline moved")
    if not dangling:
        machine.advance_ticks(20)
        require(machine.fired == [6, 12], "re-armed entry did not fire once at its deadline")
        require(not machine.callout_state()["queued"], "entry stayed queued after firing")
        events_before = len(machine.driver.events)
        machine.driver.delete()
        skipped = not any(row["event"] == "stop_native"
                          for row in machine.driver.events[events_before:])
        require(machine.driver.snapshot()["pending"] == [], "delete stranded a packet")
        machine.advance_ticks(300)
        require(machine.fired == [6, 12], "callback after delete")
        return {"fired": [6, 12], "delete_skipped_stop": skipped}
    # Dangling tail: stock delete reads the stale inactive byte and skips
    # moal_stop_timer, and moal_free_timer's guard is defeated by the stale
    # stopped flag - the timer block is freed while its callout is queued. The
    # hardened fragment removes the skip, cancelling the queue entry first.
    events_before = len(machine.driver.events)
    machine.driver.delete()
    stop_ran = any(row["event"] == "stop_native"
                   for row in machine.driver.events[events_before:])
    require(stop_ran == bool(harden), "unexpected delete stop behavior for this candidate")
    require(machine.callout_state()["freed"], "timer block not freed")
    require(machine.faults == [], "premature fault before the queued entry fired")
    if harden:
        machine.advance_ticks(6)
        require(machine.fired == [6] and machine.faults == [],
                "hardened free was not clean: extra callback or fault")
        require(machine.driver.snapshot()["pending"] == [], "hardened delete stranded a packet")
        return {"fired": [6], "delete_skipped_stop": False}
    try:
        machine.advance_ticks(6)
    except UcError as error:
        require(error.errno in FREED_FAULTS, f"unexpected fault class {error}")
        require(machine.faults and machine.faults[-1]["address"] // 0x1000 * 0x1000 == machine.shadow,
                f"callout engine fault outside the freed callout block: {machine.faults}")
    else:
        raise AssertionError("callout engine consumed the freed queue entry without a fault")
    return {"fired": [6], "delete_skipped_stop": True,
            "fault_address": hex(machine.faults[-1]["address"]),
            "freed_timer_page": hex(machine.driver.timer)}


def run(image, io_pkt, candidate, window_candidate, harden_candidate):
    scenarios = {
        "cancel_before_capture": scenario_cancel_before_capture,
        "callback_before_delete": scenario_callback_before_delete,
        "capture_delete_resume": scenario_capture_delete_resume,
        "capture_rearm_stale_reset_benign": lambda *args: scenario_capture_rearm_stale_reset(
            *args, dangling=False),
        "capture_rearm_stale_reset_dangling": lambda *args: scenario_capture_rearm_stale_reset(
            *args, dangling=True),
    }
    evidence = {"scenarios": {}, "stock_vs_combined_identical": None,
                "external_serialization": {
                    "softclock_releases_callout_mtx_before_callback": True,
                    "driver_reorder_paths_hold_no_common_lock": True,
                    "io_pkt_threaded_mode_switches_exclusion_to_mp": True,
                    "hu_thread_configuration_verified": False,
                    "note": "Interleavings are constructed at the callout boundary. In "
                            "multi-threaded io-pkt, softclock callbacks and driver RX share "
                            "no lock; a runtime check of the HU io-pkt thread configuration "
                            "is still required before calling the race reachable."}}
    for label, scenario in scenarios.items():
        outcomes = {}
        for tag, expiry, window, harden in (("stock", None, None, None),
                                            ("combined", candidate, window_candidate, None),
                                            ("hardened", candidate, window_candidate, harden_candidate)):
            print(f"callout-link {label} {tag}", flush=True)
            outcomes[tag] = scenario(image, io_pkt, expiry, window, harden)
        require(outcomes["stock"] == outcomes["combined"],
                f"{label}: combined candidate changed the timer lifecycle: "
                f"{outcomes['stock']} vs {outcomes['combined']}")
        if label in ("cancel_before_capture", "callback_before_delete", "capture_delete_resume"):
            require(outcomes["hardened"] == outcomes["stock"],
                    f"{label}: hardened fragment changed an unrelated lifecycle outcome")
        elif label.endswith("benign"):
            require(outcomes["hardened"]["fired"] == outcomes["stock"]["fired"],
                    f"{label}: hardened fragment changed the fire pattern")
            require(outcomes["hardened"]["delete_skipped_stop"] is False,
                    "hardened delete did not run the native stop")
        else:
            require(outcomes["stock"]["fired"] == outcomes["hardened"]["fired"] == [6],
                    f"{label}: fire pattern changed")
            require("fault_address" in outcomes["stock"], "stock dangling scenario lost its fault")
            hardened = outcomes["hardened"]
            require(hardened["delete_skipped_stop"] is False and "fault_address" not in hardened,
                    f"hardened candidate did not remove the dangling free: {hardened}")
        evidence["scenarios"][label] = outcomes
    evidence["stock_vs_combined_identical"] = True
    try:
        LinkedMachine(image, b"foreign", start=100)
    except ValueError:
        pass
    else:
        raise AssertionError("foreign io-pkt image accepted")
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("driver", type=Path)
    parser.add_argument("io_pkt", type=Path)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    out = args.out_dir / "callout-link-tests.json"
    if out.resolve() == args.driver.resolve() or out.resolve() == args.io_pkt.resolve():
        parser.error("generated evidence must not replace an input binary")
    image = DriverImage(args.driver.read_bytes())
    original_hash = hashlib.sha256(image.data).hexdigest()
    candidate, _ = compile_candidate(args.out_dir)
    window_candidate, _ = compile_window_candidate(args.out_dir)
    harden_candidate, harden_manifest = compile_harden_candidate(args.out_dir)
    result = run(image, args.io_pkt.read_bytes(), candidate, window_candidate, harden_candidate)
    result.update(driver_sha256=original_hash,
                  io_pkt_sha256=hashlib.sha256(args.io_pkt.read_bytes()).hexdigest(),
                  hz="synthetic 119 (runtime HU hz unmeasured); 50ms arms 6 ticks",
                  harden_patch=harden_manifest,
                  limitations=["constructed interleavings at the callout boundary; no QNX "
                               "scheduler proof",
                               "capture-vs-delete UAF (scenario 3) remains on all candidates; "
                               "it needs a callout drain, not a driver patch",
                               "no HU deployment or measured throughput"],
                  passed=True)
    require(hashlib.sha256(args.driver.read_bytes()).hexdigest() == original_hash,
            "original driver file changed")
    out.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key]
                      for key in ("passed", "scenarios", "stock_vs_combined_identical")}, indent=2))


if __name__ == "__main__":
    main()

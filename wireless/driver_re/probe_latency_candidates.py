#!/usr/bin/env python3
"""Execute guarded MU1316 latency experiments in Unicorn; never write a driver.

The RX experiment uses original ARM receive/dispatch/lifecycle code with the
three reviewed fixes and an optional timeout-immediate change. Native io-pkt
callout_msec determines tick rounding. Arrival clocks and lock/delivery/OS
boundaries remain explicit models, not RF, TCP, scheduler or CarPlay evidence.

The TX experiment executes the original completion callback and an intentionally
naive early-wake negative control. It counts callback requests; it does not model
callback execution, device completions, or end-to-end transmission latency.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from elftools.elf.elffile import ELFFile

from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3

from .callout_machine import CalloutMachine
from .reorder_machine import (
    DriverImage, ReorderMachine, compile_candidate, compile_harden_candidate,
    compile_window_candidate,
)
from .test_rx_reorder_streams import distribution, make_trace

TIMEOUT_ADDRESS = 0x47780
TIMEOUT_STOCK = bytes.fromhex("3230a0e3")
TX_WAKE_BRANCH = 0xF188
TX_WAKE_STOCK = bytes.fromhex("0700001a")
ARM_NOP = bytes.fromhex("00f020e3")
TICK_US = 8403  # Saved HU kern.clockrate; not a configurable speed claim.
EMPTY_GATE_ADDRESS = 0x47748
EMPTY_GATE_STOCK = bytes.fromhex("2430d5e5")
EMPTY_GATE_LAB_CODE = 0x700000
NO_SLIDE_ADDRESS = 0x4775C
NO_SLIDE_STOCK = bytes.fromhex("0b00000a")


class DeadlineMachine(ReorderMachine):
    """Original RX instructions with one guarded MOV immediate and tick clock."""

    def __init__(self, image, *, timeout_ms, timeout_ticks, empty_gate=None,
                 fixed_deadline=False, **kwargs):
        if timeout_ms not in (10, 20, 30, 50) or timeout_ticks < 1:
            raise ValueError("unreviewed timeout profile")
        self.timeout_ms, self.timeout_ticks = timeout_ms, timeout_ticks
        super().__init__(image, **kwargs)
        if bytes(self.cpu.mem_read(TIMEOUT_ADDRESS, 4)) != TIMEOUT_STOCK:
            raise ValueError("timeout instruction guard mismatch")
        # mov r3,#imm8; upper instruction bytes stay identical.
        self.cpu.mem_write(TIMEOUT_ADDRESS, bytes([timeout_ms]) + TIMEOUT_STOCK[1:])
        if fixed_deadline:
            if bytes(self.cpu.mem_read(NO_SLIDE_ADDRESS, 4)) != NO_SLIDE_STOCK:
                raise ValueError("sliding deadline instruction guard mismatch")
            unconditional = (int.from_bytes(NO_SLIDE_STOCK, "little") & 0x0FFFFFFF) | 0xE0000000
            self.cpu.mem_write(NO_SLIDE_ADDRESS, unconditional.to_bytes(4, "little"))
        self.empty_gate = empty_gate
        if empty_gate is not None:
            if bytes(self.cpu.mem_read(EMPTY_GATE_ADDRESS, 4)) != EMPTY_GATE_STOCK:
                raise ValueError("empty gate entry guard mismatch")
            self.cpu.mem_map(EMPTY_GATE_LAB_CODE, 0x1000)
            self.cpu.mem_write(EMPTY_GATE_LAB_CODE, empty_gate)
            displacement = (EMPTY_GATE_LAB_CODE - EMPTY_GATE_ADDRESS - 8) // 4
            branch = (0xEA000000 | (displacement & 0xFFFFFF)).to_bytes(4, "little")
            self.cpu.mem_write(EMPTY_GATE_ADDRESS, branch)
            self.allowed.append(range(EMPTY_GATE_LAB_CODE, EMPTY_GATE_LAB_CODE + len(empty_gate)))
        self.initial_code = bytes(self.cpu.mem_read(0x8EE4, len(self.initial_code)))

    def call(self, name, *arguments):
        result = super().call(name, *arguments)
        if self.empty_gate is not None:
            assert bytes(self.cpu.mem_read(EMPTY_GATE_LAB_CODE, len(self.empty_gate))) == self.empty_gate
        return result

    def _hook(self, uc, address, size, user):
        if address == self.address("callout_msec"):
            r0, r1, r2, r3 = (uc.reg_read(r) for r in
                             (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3))
            if (not self.timer_alive or r0 != self.timer + 20 or r1 != self.timeout_ms
                    or r2 != self.address("wlan_flush_data") or r3 != self.table + 32):
                raise AssertionError("unexpected deadline scheduling contract")
            # Clock ticks run before an arrival at an equal timestamp. Native
            # callout code supplied timeout_ticks; this boundary models phase.
            now_us = round(self.now_ms * 1000)
            self.due_ms = ((now_us // TICK_US + self.timeout_ticks) * TICK_US) / 1000
            self.events.append({"event": "arm", "at_ms": self.now_ms, "due_ms": self.due_ms})
            self._return()
            return
        super()._hook(uc, address, size, user)


class TxCompletionMachine(ReorderMachine):
    """Native callback; only lookup, atomics, logging and notification are stubs."""

    descriptor, ifnet = 0x232000, 0x233000

    def __init__(self, image, *, naive_early_wake=False):
        super().__init__(image, start=0)
        address, size = image.symbols["moal_send_packet_complete"]
        self.allowed.append(range(address, address + size))
        self.notifications = []
        self.put(self.priv + 200, self.ifnet)
        self.put(self.priv + 192, 0x1234)
        self.put(self.priv + 196, 0x5678)
        self.put(self.priv + 24, 0x9ABC)
        if bytes(self.cpu.mem_read(TX_WAKE_BRANCH, 4)) != TX_WAKE_STOCK:
            raise ValueError("TX wake branch guard mismatch")
        if naive_early_wake:
            self.cpu.mem_write(TX_WAKE_BRANCH, ARM_NOP)
        self.initial_code = bytes(self.cpu.mem_read(0x8EE4, len(self.initial_code)))

    def _hook(self, uc, address, size, user):
        r0, r1, r2 = (uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2))
        if address == self.address("moal_print"):
            self._return()
        elif address == self.address("woal_bss_index_to_priv"):
            if r0 != self.handle or r1 != 0:
                raise AssertionError("unexpected BSS lookup")
            self._return(self.priv)
        elif address == self.address("atomic_sub"):
            if r0 != self.handle + 152 or r1 != 1 or self.get(r0) == 0:
                raise AssertionError("invalid pending counter operation")
            self.put(r0, self.get(r0) - r1)
            self._return()
        elif address == self.address("atomic_clr"):
            if r0 != self.handle + 344 or not self.get(r0) & r1 or r1.bit_count() != 1:
                raise AssertionError("completion must free exactly its occupied slot")
            self.put(r0, self.get(r0) & ~r1)
            self._return()
        elif address == self.address("stk_context_callback_2"):
            if (r0, r1, r2) != (0x1234, 0x5678, 0x9ABC):
                raise AssertionError("wrong callback identity")
            self.notifications.append(self.get(self.handle + 344))
            self._return()
        else:
            super()._hook(uc, address, size, user)

    def fixture(self, occupied, *, stopped=True):
        if not 0 < occupied <= 0xFFFFFFFF:
            raise ValueError("nonempty 32-slot bitmap required")
        self.put(self.handle + 344, occupied)
        self.put(self.handle + 152, occupied.bit_count())
        self.put(self.ifnet + 536, 0x440 if stopped else 0x40)

    def complete(self, slot, status=0):
        self.put(self.descriptor + 28, 2)
        self.cpu.mem_write(self.descriptor + 32, b"\0")
        self.cpu.mem_write(self.descriptor + 56, bytes([slot]))
        self.cpu.mem_write(self.descriptor + 26, (64).to_bytes(2, "little"))
        self.call("moal_send_packet_complete", self.handle, self.descriptor, status)
        if bytes(self.cpu.mem_read(self.descriptor + 56, 1)) != b"\x20":
            raise AssertionError("completion did not retire its descriptor")
        if self.get(self.handle + 152) != self.get(self.handle + 344).bit_count():
            raise AssertionError("pending counter and bitmap disagree")


def timer_rounding(io_pkt):
    rows = []
    for request in (1, 5, 10, 15, 20, 25, 30, 40, 50):
        machine = CalloutMachine(io_pkt, hz=119)
        machine.schedule(123, request)
        ticks = machine.get(machine.entry + 16)
        # Exercise the native queue, not just the scheduled timestamp.
        if ticks > 1:
            machine.advance(ticks - 1)
            assert machine.calls == [], "timer fired before its native deadline"
        machine.advance(1)
        assert len(machine.calls) == 1 and machine.calls[0]["tick"] == ticks
        rows.append({"request_ms": request, "ticks": ticks,
                     "tick_boundary_delay_ms": round(ticks * TICK_US / 1000, 3)})
    return rows


def tx_experiment(image):
    cases = []
    for count in (2, 8, 16, 32):
        # Both slot orders ensure the predicate is about occupancy, not low bits.
        for order in (list(range(count)), list(reversed(range(count)))):
            for naive in (False, True):
                machine = TxCompletionMachine(image, naive_early_wake=naive)
                machine.fixture((1 << count) - 1)
                requests = []
                for i, slot in enumerate(order):
                    before = len(machine.notifications)
                    machine.complete(slot)
                    if len(machine.notifications) != before:
                        requests.append(i + 1)
                expected = list(range(1, count + 1)) if naive else [count - 1, count]
                assert requests == expected, (count, order, naive, requests)
                cases.append({"occupied_initial": count, "slot_order": order,
                              "profile": "naive_gate_removed" if naive else "stock",
                              "notify_at_completion_number": requests})
    # Stock must not request a restart for a running (non-OACTIVE) interface.
    machine = TxCompletionMachine(image)
    machine.fixture(0x80000000, stopped=False)
    machine.complete(31)
    assert machine.notifications == []
    return {"cases": cases, "running_interface_negative_control": "pass",
            "limitations": "callback execution deliberately withheld; no device/scheduler timing modeled"}


def replay(image, fixes, ticks, records, *, timeout, window, start=4090, empty_gate=None,
           fixed_deadline=False):
    machine = DeadlineMachine(image, timeout_ms=timeout, timeout_ticks=ticks,
                              start=start, window=window, empty_gate=empty_gate,
                              fixed_deadline=fixed_deadline, **fixes)
    arrivals = {}
    for row in records:
        machine.advance(row["at_ms"])
        result = machine.receive((start + row["packet"]) % 4096)
        assert result in (0, 0xFFFFFFFF)
        arrivals[next(reversed(machine.ownership))] = row
        machine.snapshot()  # Native slot ownership, including rejected packets.
    machine.advance(records[-1]["at_ms"] + 500)
    machine.snapshot()
    delivery = {row["pointer"]: row["at_ms"] for row in machine.delivered}
    order = [arrivals[row["pointer"]]["packet"] for row in machine.delivered]
    assert order == sorted(set(order)), "dispatch duplicate or order inversion"
    outcomes = [{**row, "state": machine.ownership[p]["state"],
                 "wait_ms": None if p not in delivery else round(delivery[p] - row["at_ms"], 3)}
                for p, row in arrivals.items()]
    assert not any(row["state"] == "pending" for row in outcomes), "stranded quiet tail"
    counts = {state: sum(row["state"] == state for row in outcomes)
              for state in ("delivered", "rejected", "pending")}
    assert sum(counts.values()) == len(records)
    waits = [row["wait_ms"] for row in outcomes if row["wait_ms"] is not None]
    assert all(wait >= 0 for wait in waits)
    machine.delete()
    machine.advance(machine.now_ms + 1000)
    return {"summary": {**counts, "arrivals": len(records), "wait_ms": distribution(waits),
                        "timer_arms": sum(e["event"] == "arm" for e in machine.events),
                        "timer_stops": sum(e["event"] == "stop" for e in machine.events)},
            "outcomes": outcomes}


def deadline_lifecycle(image, fixes, tick_map):
    cases = []
    for timeout in (50, 30, 20, 10):
        for window in (16, 32, 64):
            for phase_us in (0, 8402):
                for boundary in ("immediate", "before_expiry", "after_expiry"):
                    machine = DeadlineMachine(image, timeout_ms=timeout,
                                              timeout_ticks=tick_map[timeout],
                                              start=4095, window=window, **fixes)
                    machine.advance(phase_us / 1000)
                    machine.receive(0)  # Missing 4095, crossing the sequence wrap.
                    deadline = machine.due_ms
                    assert deadline is not None
                    if boundary != "immediate":
                        machine.advance(deadline + (0.001 if boundary == "after_expiry" else -0.001))
                    machine.delete()
                    machine.advance(deadline + 1000)
                    assert len(machine.delivered) == 1
                    assert machine.snapshot()["pending"] == []
                    cases.append({"request_ms": timeout, "window": window,
                                  "phase_us": phase_us, "delete": boundary, "status": "pass"})
    return cases


def rx_experiment(image, out, rounding):
    expiry, expiry_info = compile_candidate(out)
    window_fix, window_info = compile_window_candidate(out)
    harden, harden_info = compile_harden_candidate(out)
    fixes = dict(candidate=expiry, window_candidate=window_fix, harden_candidate=harden)
    tick_map = {row["request_ms"]: row["ticks"] for row in rounding}
    sparse = {
        "quiet_two_holes": [(0, 1), (0, 3)],
        "repair_12ms": [(0, 1), (0, 3), (12, 0), (12, 2)],
        "repair_20ms": [(0, 1), (0, 3), (20, 0), (20, 2)],
        "repair_28ms": [(0, 1), (0, 3), (28, 0), (28, 2)],
        "repair_40ms": [(0, 1), (0, 3), (40, 0), (40, 2)],
        "young_tail": [(0, 1), (24, 3), (28, 2)],
    }
    traces = [(name, [{"id": i, "packet": packet, "at_ms": at, "source_ms": 0,
                       "duplicate": False} for i, (at, packet) in enumerate(rows)])
              for name, rows in sparse.items()]
    traces += [(profile, make_trace(profile, 629, count=192))
               for profile in ("clean", "jitter", "loss_1pct", "retry_20ms",
                               "retry_80ms", "burst_loss", "mixed", "duplicates")]
    results = []
    for window in (16, 32, 64):
        for profile, records in traces:
            for phase_us in (0, 4201, 8402):
                phased = [{**r, "at_ms": r["at_ms"] + phase_us / 1000,
                           "source_ms": r["source_ms"] + phase_us / 1000} for r in records]
                for timeout in (50, 30, 20, 10):
                    row = replay(image, fixes, tick_map[timeout], phased, timeout=timeout, window=window)
                    if profile == "clean":
                        assert row["summary"]["delivered"] == 192
                        assert row["summary"]["wait_ms"]["max"] == 0
                    results.append({"profile": profile, "window": window, "request_ms": timeout,
                                    "phase_us": phase_us, **row})
    baselines = {(r["window"], r["profile"], r["phase_us"]): r
                 for r in results if r["request_ms"] == 50}
    for row in results:
        old = {r["id"]: r for r in baselines[row["window"], row["profile"], row["phase_us"]]["outcomes"]}
        new = {r["id"]: r for r in row["outcomes"]}
        assert old.keys() == new.keys()
        common = [k for k in old if old[k]["state"] == new[k]["state"] == "delivered"]
        row["versus_50ms_same_arrivals"] = {
            "common_delivered": len(common),
            "common_wait_saved_ms": distribution([round(old[k]["wait_ms"] - new[k]["wait_ms"], 3)
                                                    for k in common]),
            "baseline_only_delivered": [k for k in old if old[k]["state"] == "delivered"
                                         and new[k]["state"] != "delivered"],
            "candidate_only_delivered": [k for k in old if new[k]["state"] == "delivered"
                                          and old[k]["state"] != "delivered"],
        }
    return {"cases": results, "lifecycle_cases": deadline_lifecycle(image, fixes, tick_map),
            "fix_fragments": [expiry_info, window_info, harden_info],
            "scope": "native ARM; synthetic MAC arrivals; three tick phases; no TCP/RF gain inferred"}


def empty_gate_experiment(image, out, rounding):
    source = Path(__file__).with_name("rx_empty_timer_gate_candidate.S")
    target = out / "rx-empty-timer-gate-fragment.o"
    subprocess.run(["clang", "--target=armv7-none-eabi", "-c", str(source), "-o", str(target)],
                   check=True, capture_output=True, timeout=30)
    with target.open("rb") as file:
        elf = ELFFile(file)
        if any(s["sh_type"] in ("SHT_REL", "SHT_RELA") and s["sh_size"] for s in elf.iter_sections()):
            raise ValueError("lab trampoline has unresolved relocations")
        gate = elf.get_section_by_name(".text").data()
    assert 0 < len(gate) < 4096
    expiry, _ = compile_candidate(out)
    window_fix, _ = compile_window_candidate(out)
    harden, _ = compile_harden_candidate(out)
    fixes = dict(candidate=expiry, window_candidate=window_fix, harden_candidate=harden)
    ticks = next(r["ticks"] for r in rounding if r["request_ms"] == 50)
    traces = [(p, make_trace(p, 629, count=192)) for p in
              ("clean", "jitter", "loss_1pct", "mixed", "duplicates")]
    # A clean packet at t0, then a first hole at t40: stock's still-running
    # empty-table timer expires near t50, whereas the candidate starts at t40.
    traces.append(("inherited_deadline", [
        {"id": i, "packet": packet, "at_ms": at, "source_ms": at, "duplicate": False}
        for i, (at, packet) in enumerate(((0, 0), (40, 2)))]))
    results = []
    for window in (16, 32, 64):
        for profile, records in traces:
            base = replay(image, fixes, ticks, records, timeout=50, window=window)
            candidate = replay(image, fixes, ticks, records, timeout=50, window=window, empty_gate=gate)
            if profile == "clean":
                assert base["outcomes"] == candidate["outcomes"]
                assert base["summary"]["timer_arms"] == 192
                assert candidate["summary"]["timer_arms"] == 0
            results.append({"profile": profile, "window": window, "baseline": base, "candidate": candidate})
    return {"cases": results, "entry": hex(EMPTY_GATE_ADDRESS),
            "stock_entry_hex": EMPTY_GATE_STOCK.hex(), "synthetic_code_address": hex(EMPTY_GATE_LAB_CODE),
            "fragment_hex": gate.hex(), "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "production_recommendation": "not deployable; timing policy and concurrency require further work"}


def progress_experiment(image, out, rounding):
    expiry, _ = compile_candidate(out)
    window_fix, _ = compile_window_candidate(out)
    harden, _ = compile_harden_candidate(out)
    fixes = dict(candidate=expiry, window_candidate=window_fix, harden_candidate=harden)
    ticks = next(r["ticks"] for r in rounding if r["request_ms"] == 50)
    cases = []
    for window in (16, 32, 64):
        arrivals = [(0, window - 1)] + [(40 * (p + 1), p) for p in range(window - 1)]
        records = [{"id": i, "packet": p, "at_ms": at, "source_ms": 0, "duplicate": False}
                   for i, (at, p) in enumerate(arrivals)]
        baseline = replay(image, fixes, ticks, records, timeout=50, window=window)
        fixed = replay(image, fixes, ticks, records, timeout=50, window=window, fixed_deadline=True)
        old_target = baseline["outcomes"][0]
        new_target = fixed["outcomes"][0]
        assert old_target["wait_ms"] == 40 * (window - 1)
        assert new_target["wait_ms"] == round(ticks * TICK_US / 1000, 3)
        assert baseline["summary"]["delivered"] == window
        assert fixed["summary"]["delivered"] == 2
        assert fixed["summary"]["rejected"] == window - 2
        cases.append({"window": window, "baseline": baseline, "fixed_deadline": fixed})
    return {"cases": cases, "entry": hex(NO_SLIDE_ADDRESS), "stock_hex": NO_SLIDE_STOCK.hex(),
            "candidate_hex": "0b0000ea", "scope": "adverse synthetic progress, not a measured RF trace",
            "production_recommendation": "no deployment; quantify added late rejection/TCP recovery first"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("driver", type=Path)
    parser.add_argument("io_pkt", type=Path)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--section", choices=("all", "timing", "empty-gate", "progress"), default="all")
    args = parser.parse_args()
    probe_source = Path(__file__).read_bytes()
    driver_data, stack_data = args.driver.read_bytes(), args.io_pkt.read_bytes()
    image = DriverImage(driver_data)
    if driver_data[TIMEOUT_ADDRESS:TIMEOUT_ADDRESS + 4] != TIMEOUT_STOCK:
        raise ValueError("stock timeout fingerprint mismatch")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    rounding = timer_rounding(stack_data)
    result = {
        "lab_only": True, "driver_sha256": hashlib.sha256(driver_data).hexdigest(),
        "probe_source_sha256": hashlib.sha256(probe_source).hexdigest(),
        "io_pkt_sha256": hashlib.sha256(stack_data).hexdigest(),
        "clock": {"hz": 119, "tick_us": TICK_US}, "timer_rounding": rounding,
        "patches": [
            {"address": hex(TIMEOUT_ADDRESS), "stock": TIMEOUT_STOCK.hex(),
             "profiles": {str(ms): (bytes([ms]) + TIMEOUT_STOCK[1:]).hex() for ms in (10, 20, 30)}},
            {"address": hex(TX_WAKE_BRANCH), "stock": TX_WAKE_STOCK.hex(),
             "naive_negative_control": ARM_NOP.hex(), "production_recommendation": "do not deploy"},
        ],
    }
    if args.section in ("all", "timing"):
        result["tx_completion"] = tx_experiment(image)
        result["rx_deadlines"] = rx_experiment(image, args.out_dir, rounding)
    if args.section in ("all", "empty-gate"):
        result["empty_timer_gate"] = empty_gate_experiment(image, args.out_dir, rounding)
    if args.section in ("all", "progress"):
        result["progress_deadline"] = progress_experiment(image, args.out_dir, rounding)
    assert args.driver.read_bytes() == driver_data and args.io_pkt.read_bytes() == stack_data
    assert Path(__file__).read_bytes() == probe_source, "probe source changed during execution"
    path = args.out_dir / ("latency-candidates.json" if args.section == "all"
                           else f"latency-{args.section}.json")
    path.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"result": str(path), "rx_cases": len(result.get("rx_deadlines", {}).get("cases", [])),
                      "tx_cases": len(result.get("tx_completion", {}).get("cases", [])),
                      "empty_gate_cases": len(result.get("empty_timer_gate", {}).get("cases", [])),
                      "progress_cases": len(result.get("progress_deadline", {}).get("cases", [])),
                      "lifecycle_cases": len(result.get("rx_deadlines", {}).get("lifecycle_cases", [])),
                      "status": "pass; offline only"}))


if __name__ == "__main__":
    main()

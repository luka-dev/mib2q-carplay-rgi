#!/usr/bin/env python3
"""Synthetic arrival/loss A/B experiment executing the pinned ARM RX code.

No TCP, RF, scheduler or measured CarPlay throughput model is implied. Delays
are integer milliseconds; timers run before arrivals at an equal timestamp.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import sys

from .reorder_machine import (DriverImage, ReorderMachine, compile_candidate, compile_harden_candidate,
                              compile_window_candidate)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def distribution(values):
    """Nearest-rank quantiles; an empty sample is distinct from zero wait."""
    ordered = sorted(values)
    if not ordered:
        return {key: None for key in ("mean", "p50", "p95", "p99", "max")}
    return {"mean": round(sum(ordered) / len(ordered), 3),
            **{f"p{p}": ordered[math.ceil(len(ordered) * p / 100) - 1] for p in (50, 95, 99)},
            "max": ordered[-1]}


def make_trace(profile, seed, count=384):
    """One logical MAC sequence per source packet; duplicates get distinct IDs."""
    rng = random.Random(seed)
    records = []
    for packet in range(count):
        # 1400-byte synthetic packets at a nominal 19 Mbit/s source cadence.
        # Integer-ms grouping is intentional; this is not network throughput.
        at = packet * 1400 * 8 // 19000
        delay = 0
        if profile == "jitter":
            delay = rng.randrange(9)
        elif profile == "loss_1pct":
            if rng.random() < 0.01:
                continue
        elif profile == "retry_20ms":
            delay = 20 if packet % 64 in (0, 2) else 0
        elif profile == "retry_80ms":
            delay = 80 if packet % 64 in (0, 2) else 0
        elif profile == "burst_loss":
            if packet % 64 in (0, 1, 3):
                continue
        elif profile == "mixed":
            draw = rng.random()
            if draw < 0.01:
                continue
            delay = rng.randrange(2, 19) if draw < 0.08 else 0
            if 0.08 <= draw < 0.10:
                delay = rng.randrange(55, 91)
        elif profile not in ("clean", "duplicates"):
            raise ValueError(profile)
        records.append({"id": len(records), "packet": packet, "at_ms": at + delay,
                        "source_ms": at, "duplicate": False})
        if profile == "duplicates" and packet % 31 == 3:
            records.append({"id": len(records), "packet": packet, "at_ms": at + 7,
                            "source_ms": at, "duplicate": True})
    return sorted(records, key=lambda row: (row["at_ms"], row["id"]))


def replay(image, fragment, records, *, window, start=4090, horizon=None, window_candidate=None,
           harden_candidate=None):
    require(bool(records), "empty arrival trace")
    require(len({row["id"] for row in records}) == len(records), "duplicate arrival identity")
    require(all(0 <= row["packet"] < 2048 and 0 <= row["source_ms"] <= row["at_ms"]
                for row in records), "invalid trace fields or ambiguous sequence range")
    require(records == sorted(records, key=lambda row: (row["at_ms"], row["id"])), "unordered trace")
    horizon = records[-1]["at_ms"] + 500 if horizon is None else horizon
    require(horizon >= records[-1]["at_ms"], "horizon precedes last arrival")
    machine = ReorderMachine(image, start=start, window=window, candidate=fragment,
                             window_candidate=window_candidate, harden_candidate=harden_candidate)
    arrivals = {}
    peak_pending = 0
    for record in records:
        machine.advance(record["at_ms"])
        status = machine.receive((start + record["packet"]) % 4096)
        require(status in (0, 0xFFFFFFFF), "unexpected native RX status")
        arrivals[next(reversed(machine.ownership))] = record
        peak_pending = max(peak_pending, len(machine.snapshot()["pending"]))
    machine.advance(horizon)
    machine.snapshot()  # Validate final native slots against ownership.
    delivery = {row["pointer"]: row["at_ms"] for row in machine.delivered}
    outcomes = []
    for pointer, record in arrivals.items():
        at = delivery.get(pointer)
        state = machine.ownership[pointer]["state"]
        outcomes.append({**record, "state": state, "delivered_ms": at,
                         "wait_ms": None if at is None else at - record["at_ms"],
                         "source_to_delivery_ms": None if at is None else at - record["source_ms"],
                         "pending_age_ms": horizon - record["at_ms"] if state == "pending" else None})
    require(sum(row["state"] == "delivered" for row in outcomes) == len(machine.delivered),
            "delivery accounting mismatch")
    logical_delivered = [row["packet"] for row in outcomes if row["state"] == "delivered"]
    require(len(set(logical_delivered)) == len(logical_delivered), "duplicate logical packet delivered")
    require(all(row["wait_ms"] is None or row["wait_ms"] >= 0 for row in outcomes), "negative wait")
    # Check dispatch ordering in native delivery order, including sequence wrap.
    order = [arrivals[row["pointer"]]["packet"] for row in machine.delivered]
    inversions = [{"previous_packet": a, "next_packet": b} for a, b in zip(order, order[1:]) if a > b]
    pending = [row["pending_age_ms"] for row in outcomes if row["state"] == "pending"]
    summary = {state: sum(row["state"] == state for row in outcomes)
               for state in ("delivered", "rejected", "pending")}
    require(sum(summary.values()) == len(records), "arrival conservation failed")
    summary.update(arrivals=len(records), duplicate_arrivals=sum(row["duplicate"] for row in records),
                   order_inversions=inversions,
                   horizon_ms=horizon, peak_pending=peak_pending,
                   delivered_wait_ms=distribution([row["wait_ms"] for row in outcomes if row["wait_ms"] is not None]),
                   source_to_delivery_ms=distribution([row["source_to_delivery_ms"] for row in outcomes
                                                       if row["source_to_delivery_ms"] is not None]),
                   pending_age_ms=distribution(pending),
                   delivered_within_50ms=sum(row["wait_ms"] is not None and row["wait_ms"] <= 50 for row in outcomes))
    delivery_ids = [arrivals[row["pointer"]]["id"] for row in machine.delivered]
    require(len(delivery_ids) == summary["delivered"], "dispatch identity count mismatch")
    # Teardown is deliberately after metrics: deletion would flush stock tails.
    machine.delete()
    machine.advance(horizon + 1000)
    return {"summary": summary, "outcomes": outcomes, "delivery_arrival_ids": delivery_ids}


def compare(stock, candidate):
    old = {row["id"]: row for row in stock["outcomes"]}
    new = {row["id"]: row for row in candidate["outcomes"]}
    require(old.keys() == new.keys(), "A/B arrival identities differ")
    common = [key for key in old if old[key]["state"] == new[key]["state"] == "delivered"]
    gained = [key for key in old if old[key]["state"] != "delivered" and new[key]["state"] == "delivered"]
    lost = [key for key in old if old[key]["state"] == "delivered" and new[key]["state"] != "delivered"]
    deltas = [old[key]["wait_ms"] - new[key]["wait_ms"] for key in common]
    return {"common_delivered": len(common), "candidate_only_delivered": len(gained),
            "stock_only_delivered": len(lost), "candidate_only_arrival_ids": gained,
            "stock_only_arrival_ids": lost, "common_wait_saved_ms": distribution(deltas),
            "common_faster": sum(n > 0 for n in deltas), "common_slower": sum(n < 0 for n in deltas),
            "common_unchanged": sum(n == 0 for n in deltas)}


def run(image, candidate, *, window_candidate=None, harden_candidate=None):
    cases = []
    # Sparse controls isolate timeout policy from window pressure in a fast stream.
    controls = {
        "quiet_tail": [(0, 1), (0, 3)],
        "recovery_20ms": [(0, 1), (0, 3), (20, 0), (20, 2)],
        "late_hole_80ms": [(0, 1), (0, 3), (80, 2)],
        "young_tail": [(0, 1), (49, 3), (51, 2)],
        "deadline_tie": [(0, 1), (0, 3), (50, 2)],
        "deadline_before": [(0, 1), (0, 3), (49, 2)],
        "wrapped_window_move": [(0, 1), (0, 16), (0, 69), (1, 6)],
    }
    traces = [(name, None, [{"id": i, "packet": p, "at_ms": at, "source_ms": 0,
                            "duplicate": False} for i, (at, p) in enumerate(rows)])
              for name, rows in controls.items()]
    for profile in ("clean", "jitter", "loss_1pct", "retry_20ms", "retry_80ms", "burst_loss", "mixed", "duplicates"):
        for seed in ((629, 630, 631) if profile in ("jitter", "loss_1pct", "mixed") else (629,)):
            traces.append((profile, seed, make_trace(profile, seed)))
    for window in (16, 64):
        for profile, seed, records in traces:
            print(f"replay {profile} window={window} seed={seed}", file=sys.stderr, flush=True)
            stock = replay(image, None, records, window=window)
            patched = replay(image, candidate, records, window=window, window_candidate=window_candidate,
                             harden_candidate=harden_candidate)
            paired = compare(stock, patched)
            # Keep inherited stock defects visible; do not call them a fix or
            # silently drop the offending trace from latency comparisons.
            require(all(row in stock["summary"]["order_inversions"]
                        for row in patched["summary"]["order_inversions"]),
                    "candidate introduced a new dispatch-order inversion")
            require(patched["summary"]["pending"] == 0, "candidate left a quiet tail")
            if profile in ("clean", "duplicates", "recovery_20ms", "deadline_before"):
                require(stock == patched, f"loss-free/recovered control changed: {profile}")
            if profile == "clean":
                require(patched["summary"]["delivered"] == 384 and patched["summary"]["delivered_wait_ms"]["max"] == 0,
                        "clean cadence regression")
            if profile == "quiet_tail":
                require(stock["summary"]["pending"] == 1 and paired["candidate_only_delivered"] == 1,
                        "stock tail defect not reproduced")
            if profile in ("late_hole_80ms", "young_tail", "deadline_tie"):
                require(paired["stock_only_delivered"] == 1 and patched["summary"]["rejected"] == 1,
                        "late-hole policy cost was not exposed")
            if profile == "wrapped_window_move" and window == 64:
                expected = [{"previous_packet": 16, "next_packet": 6}]
                require(stock["summary"]["order_inversions"] == expected, "wrapped stock defect not reproduced")
                require(patched["summary"]["order_inversions"] == ([] if window_candidate is not None else expected),
                        "wrapped candidate order mismatch")
            cases.append({"profile": profile, "seed": seed, "window": window,
                          "trace_sha256": hashlib.sha256(json.dumps(records, sort_keys=True).encode()).hexdigest(),
                          "stock": stock, "candidate": patched, "comparison": paired})
    ordering_passed = all(not row["candidate"]["summary"]["order_inversions"] for row in cases)
    if window_candidate is not None:
        require(ordering_passed, "combined candidate violated dispatch order")
    return {"passed": True, "candidate_ordering_check_passed":
            ordering_passed,
            "candidate_ready_for_hu": False, "cases": cases, "case_count": len(cases),
            "arrival_replays": sum(row["stock"]["summary"]["arrivals"] * 2 for row in cases),
            "model": {"start": 4090, "synthetic_packets_per_stream": 384, "payload_bytes_for_cadence": 1400,
                      "nominal_source_mbit_s": 19, "time_unit": "integer milliseconds",
                      "equal_time_order": "timer before arrivals, then increasing arrival ID",
                      "horizon": "last arrival plus 500 ms; metrics before native delete",
                      "quantiles": "nearest rank; delivered samples only; pending reported separately"},
            "limitations": ["synthetic arrivals, not a captured phone/RF distribution",
                            "one peer/TID and one reorder table, single-thread modeled callouts",
                            "no MAC retry, TCP retransmission/congestion, RF, QNX scheduler or video model",
                            "delivered wait percentiles exclude rejected and right-censored pending packets",
                            "no live HU speed, frame pacing or race-safety claim"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--with-window-fix", action="store_true", help="test the combined expiry + wrapped-window candidate")
    parser.add_argument("--with-harden", action="store_true", help="include the 4-byte timer-harden fragment in every replay")
    args = parser.parse_args()
    report_name = "rx-reorder-combined-stream-tests.json" if args.with_window_fix else "rx-reorder-stream-tests.json"
    names = ["rx-reorder-expiry-fragment.o", report_name]
    if args.with_window_fix:
        names.append("rx-reorder-window-fragment.o")
    if args.with_harden:
        names.append("rx-timer-harden-fragment.o")
    for name in names:
        target = args.out_dir / name
        if target.resolve() == args.binary.resolve() or target.exists() and target.samefile(args.binary):
            parser.error("generated artifacts must not replace or alias the input driver")
    image = DriverImage(args.binary.read_bytes())
    fragment, manifest = compile_candidate(args.out_dir)
    window_fragment, window_manifest = compile_window_candidate(args.out_dir) if args.with_window_fix else (None, None)
    harden_fragment, harden_manifest = compile_harden_candidate(args.out_dir) if args.with_harden else (None, None)
    result = run(image, fragment, window_candidate=window_fragment, harden_candidate=harden_fragment)
    result.update(driver_sha256=hashlib.sha256(image.data).hexdigest(), patch=manifest,
                  window_patch=window_manifest, harden_patch=harden_manifest,
                  harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  file_unchanged=args.binary.read_bytes() == image.data)
    require(result["file_unchanged"], "input driver changed")
    (args.out_dir / report_name).write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("passed", "candidate_ordering_check_passed",
                    "candidate_ready_for_hu", "case_count", "arrival_replays", "file_unchanged")}, indent=2))
    for case in result["cases"]:
        old, new, pair = case["stock"]["summary"], case["candidate"]["summary"], case["comparison"]
        print(f"{case['profile']:18} w={case['window']:2} seed={str(case['seed']):4} "
              f"delivered={old['delivered']}->{new['delivered']} rejected={old['rejected']}->{new['rejected']} "
              f"pending={old['pending']}->{new['pending']} gained={pair['candidate_only_delivered']} "
              f"lost={pair['stock_only_delivered']}")


if __name__ == "__main__":
    main()

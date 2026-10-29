"""Offline integration receipt for items 1/2 and the independently built item 3.

Run with python3 -m wireless.driver_re.test_wifi_latency_native DRIVER --out-dir DIR.
The two native instruction worlds deliberately share only explicit ABI snapshots.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from .reorder_machine import (DriverImage, compile_candidate, compile_window_candidate,
                              compile_harden_candidate)
from .probe_latency_candidates import TxCompletionMachine, TICK_US
from .rx_timer_churn_probe import compile_policy, lazy_replay
from .test_rx_reorder_streams import make_trace, distribution
from .wifi_latency_native import compile_policies, PolicyMachine, AgeCapMachine, EarlyWakeMachine


def run_rx(image, fixes, blob, elf, records, window):
    m = AgeCapMachine(image, policy_blob=blob, policy_elf=elf, start=4090, window=window, **fixes)
    arrivals = {}
    for row in records:
        m.advance(row['at_ms'])
        m.receive((4090 + row['packet']) % 4096)
        arrivals[next(reversed(m.ownership))] = row
        m.snapshot()
    m.advance(records[-1]['at_ms'] + 500)
    deliveries = {r['pointer']: r['at_ms'] for r in m.delivered}
    outcomes = [{**r, 'state': m.ownership[p]['state'],
                 'wait_ms': None if p not in deliveries else round(deliveries[p] - r['at_ms'], 3)}
                for p, r in arrivals.items()]
    order = [arrivals[r['pointer']]['packet'] for r in m.delivered]
    assert order == sorted(set(order))
    assert all(r['state'] != 'pending' for r in outcomes)
    summary = {state: sum(r['state'] == state for r in outcomes) for state in ('delivered', 'rejected')}
    summary['wait_ms'] = distribution([r['wait_ms'] for r in outcomes if r['wait_ms'] is not None])
    summary['timer_arms'] = sum(e['event'] == 'arm' for e in m.events)
    m.delete()
    m.advance(m.now_ms + 1000)
    summary['timer_stops'] = sum(e['event'] == 'stop' for e in m.events)
    return dict(summary=summary, outcomes=outcomes)


def age_guards(elf):
    p = PolicyMachine(elf)
    assert p.age_create() == 0
    assert p.age_observe(1, 0, 0, [0, 0, 123])[1] == (1, 6, 0, 123, 1)
    assert p.age_observe(1, 4, 1, [0, 123])[1] == (1, 6, 0, 123, 1)
    assert p.age_observe(1, 6, 1, [0, 123])[1][2] == 1
    before = bytes(p.cpu.mem_read(p.rx, p.abi[0]))
    assert p.age_observe(2, 6, 1, [0, 123])[0] == 1
    assert bytes(p.cpu.mem_read(p.rx, p.abi[0])) == before
    assert p.age_observe(1, 5, 1, [0, 123])[0] == 3
    assert p.age_observe(1, 8, 1, [0, 123])[0] == 3
    assert p.call('wl_rx_age_retire', p.rx, 1) == 0
    assert p.age_create(2) == 0
    # First call-entry timestamp survives processing time, uint32 tick wrap and
    # 4095 -> 0 sequence wrap. Same allocator address can become a new packet.
    assert p.age_observe(2, 0xfffffffe, 4095, [0, 123], incoming=123,
                         sequence=0, arrived=0xfffffffc)[1] == (1, 2, 0, 123, 1)
    assert p.age_observe(2, 2, 4095, [0, 123])[1] == (1, 2, 1, 123, 1)
    assert p.age_observe(2, 3, 0, [0, 123], incoming=123,
                         sequence=1, arrived=3)[1] == (1, 9, 0, 123, 1)
    assert p.age_observe(2, 4, 0, [0, 0])[1] == (0, 0, 0, 0, 0)
    return dict(status='pass', cases=['oldest_survives_progress', 'expired', 'stale_generation',
                                      'backwards_clock_latched', 'tick_and_sequence_wrap',
                                      'allocator_reuse', 'empty_reset'])


def tx_cases(image, elf):
    rows = []
    for count in (2, 8, 16, 32):
        for reverse in (False, True):
            order = list(range(count))[::(-1 if reverse else 1)]
            old = TxCompletionMachine(image)
            new = EarlyWakeMachine(image, policy_elf=elf)
            old.fixture((1 << count) - 1)
            new.fixture((1 << count) - 1)
            first_old = first_new = None
            for n, slot in enumerate(order, 1):
                old.complete(slot)
                new.complete(slot)
                if old.notifications and first_old is None: first_old = n
                if new.notifications and first_new is None: first_new = n
            assert first_old == count - 1 and first_new == 1
            assert len(new.queue) == 1 and len(new.notifications) == 1
            assert new.consume() == 2 and not new.queue
            rows.append(dict(occupied=count, reverse=reverse, stock_first=first_old,
                             early_first=first_new, callbacks=1))
    m = EarlyWakeMachine(image, policy_elf=elf)
    m.fixture(0xffffffff)
    m.complete(0)
    def while_running():
        for slot in range(1, 32): m.complete(slot)
        assert not m.queue
    assert m.consume(during_run=while_running) == 2
    assert len(m.queue) == 1  # one follow-up, no missed wake or callback flood
    assert m.consume() == 2 and not m.queue
    rows.append(dict(case='completion_during_running_callback', callbacks=2))

    m = EarlyWakeMachine(image, policy_elf=elf)
    m.fixture(0xff)
    m.fail_next = True
    m.complete(0)
    assert not m.queue
    m.complete(1)
    assert len(m.queue) == 1
    old_ticket = m.queue[0][0]
    assert m.policy.call('wl_tx_retire', m.policy.tx, 1) == 4
    assert m.policy.call('wl_tx_reclaimable', m.policy.tx) == 0
    assert m.policy.call('wl_tx_bind', m.policy.tx, 2) == 4
    assert m.consume() == 3  # retired target must not be called
    assert m.policy.call('wl_tx_reclaimable', m.policy.tx) == 1
    assert m.policy.call('wl_tx_bind', m.policy.tx, 2) == 0
    action, ticket = m.policy.tx_request(generation=2)
    assert action == 1
    before = bytes(m.policy.cpu.mem_read(m.policy.tx, 8))
    assert m.policy.call('wl_tx_begin', m.policy.tx, *old_ticket) == 0
    assert m.policy.call('wl_tx_submit_failed', m.policy.tx, *old_ticket) == 1
    assert bytes(m.policy.cpu.mem_read(m.policy.tx, 8)) == before
    assert m.policy.call('wl_tx_submit_failed', m.policy.tx, *ticket) == 0
    rows.append(dict(case='enqueue_failure_retire_rebind_stale_ticket', status='pass'))

    m = EarlyWakeMachine(image, policy_elf=elf)
    m.fixture(3)
    m.complete(0)
    def retire_running():
        assert m.policy.call('wl_tx_retire', m.policy.tx, 1) == 4
        assert m.policy.call('wl_tx_reclaimable', m.policy.tx) == 0
    assert m.consume(during_run=retire_running) == 2
    assert not m.queue and m.policy.call('wl_tx_reclaimable', m.policy.tx) == 1
    rows.append(dict(case='running_callback_drains_before_reclaim', status='pass'))
    m = EarlyWakeMachine(image, policy_elf=elf)
    m.fixture(3, stopped=False)
    m.complete(0)
    assert not m.queue
    rows.append(dict(case='running_interface_no_spurious_wake', status='pass'))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('driver', type=Path)
    parser.add_argument('--out-dir', required=True, type=Path)
    parser.add_argument('--quick', action='store_true')
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    data = args.driver.read_bytes()
    image = DriverImage(data)
    elf, policy_info = compile_policies(args.out_dir)
    blob, timer_info = compile_policy(args.out_dir)
    fixes = dict(candidate=compile_candidate(args.out_dir)[0],
                 window_candidate=compile_window_candidate(args.out_dir)[0],
                 harden_candidate=compile_harden_candidate(args.out_dir)[0])
    traces = [(name, make_trace(name, 629, count=192)) for name in
              ('clean', 'jitter', 'loss_1pct', 'retry_20ms', 'retry_80ms', 'burst_loss', 'mixed', 'duplicates')]
    for name, pairs in [('inherited_deadline', [(0, 0), (40, 2)]),
                        ('expired_empty_deadline', [(0, 0), (55, 2)]),
                        ('lazy_progress', [(0, 3), (40, 0), (80, 1)])]:
        traces.append((name, [dict(id=i, packet=p, at_ms=t, source_ms=t, duplicate=False)
                              for i, (t, p) in enumerate(pairs)]))
    guards = age_guards(elf)
    tx = tx_cases(image, elf)
    cases = []
    for window in ((32,) if args.quick else (16, 32, 64)):
        for name, records in traces:
            for phase in ((0,) if args.quick else (0, 4201, 8402)):
                phased = [{**r, 'at_ms': r['at_ms'] + phase/1000} for r in records]
                expected = lazy_replay(image, fixes, blob, phased, window, age_budget=6)
                actual = run_rx(image, fixes, blob, elf, phased, window)
                assert expected['outcomes'] == actual['outcomes'], (window, name, phase)
                for key in ('delivered', 'rejected', 'wait_ms', 'timer_arms', 'timer_stops'):
                    assert expected['summary'][key] == actual['summary'][key], (window, name, phase, key)
                cases.append(dict(window=window, trace=name, phase_us=phase, **actual['summary']))
        print(f'RX window {window} pass ({len(cases)} cases)', flush=True)
    # Prolonged sliding deadline: oldest packet may never be given a fresh age.
    progress = []
    for window in (16, 32, 64):
        pairs = [(0, window-1)] + [(40*(n+1), n) for n in range(window-1)]
        records = [dict(id=i, packet=p, at_ms=t, source_ms=t, duplicate=False)
                   for i, (t, p) in enumerate(pairs)]
        result = run_rx(image, fixes, blob, elf, records, window)
        oldest = result['outcomes'][0]
        assert oldest['state'] == 'delivered' and oldest['wait_ms'] == 6*TICK_US/1000
        progress.append(dict(window=window, oldest_wait_ms=oldest['wait_ms'], **result['summary']))
    # Softclock delayed beyond the cap; receive itself must request exactly one flush.
    m = AgeCapMachine(image, policy_blob=blob, policy_elf=elf, start=4090, window=32, **fixes)
    m.receive(4092)
    old_epoch, old_gen = m.policy.words()[6], m.policy.generation
    m.now_ms = 60
    m.receive(4093)
    assert not m.pending_pointers() and m.due_ms is None
    assert [r['sequence'] for r in m.delivered] == [4092, 4093]
    assert m.policy.invoke(1, m.tick(), True, generation=old_gen, callback_epoch=old_epoch) == 8
    m.delete()
    assert args.driver.read_bytes() == data
    source_names = ('wifi_latency_policy.c', 'wifi_latency_policy.h', 'wifi_latency_native.py',
                    'test_wifi_latency_policy.c', 'test_wifi_latency_native.py',
                    'rx_timer_churn_policy.S', 'rx_timer_churn_policy.h', 'rx_timer_churn_probe.py')
    fingerprints = {name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                    for name in source_names}
    result = dict(status='pass; offline only', driver_sha256=hashlib.sha256(data).hexdigest(),
                  policies=policy_info, timer=timer_info, source_sha256=fingerprints,
                  rx_age_guards=guards, rx=cases, progress=progress, tx=tx,
                  delayed_rx_flush='pass', limitations=[
                      'No modified loadable QNX driver, trampoline, registry or physical measurement.',
                      'Timer/callback scheduling, native-target lifetime and exclusion are modeled boundaries.',
                      'Six ticks bound policy deadlines; scheduler delay can exceed this wall-clock duration.',
                      'Age cap can reject late repairs and is not a loss-free throughput improvement.'])
    path = args.out_dir / 'wifi_latency_native_results.json'
    path.write_text(json.dumps(result, indent=2) + '\n')
    print(f'PASS: {len(cases)} RX + {len(progress)} progress + {len(tx)} TX; receipt {path}', flush=True)


if __name__ == '__main__':
    main()

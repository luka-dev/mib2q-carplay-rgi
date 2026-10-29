#!/usr/bin/env python3
"""Offline native ARM RX plus standalone ARM lazy deadline policy.

No source driver writes, networking, HU access or deployable ELF is produced.
The sidecar lock/lifetime and exact-tick callout operation are explicit OS
boundary models; none is claimed as an integrated production hook.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R4,
    UC_ARM_REG_R6, UC_ARM_REG_R10, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)
from .reorder_machine import DriverImage, compile_candidate, compile_window_candidate, compile_harden_candidate
from .probe_latency_candidates import DeadlineMachine, TICK_US, replay
from .test_rx_reorder_streams import make_trace, distribution

ARM, STOP, FLUSH, IGNORE = 1, 2, 4, 8
RX, EXPIRY, DELETE, CREATE = range(4)


def signed_delta(a, b):
    return ((a - b + 0x80000000) & 0xffffffff) - 0x80000000


class ArmPolicy:
    code, state, inputs, stack, stop = 0x1000, 0x3000, 0x3100, 0x4ff0, 0x5000

    def __init__(self, blob):
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.cpu.mem_map(0x1000, 0x5000)
        self.cpu.mem_write(self.code, blob)
        self.blob = blob
        self.invoke(CREATE)

    def words(self):
        return struct.unpack('<7I', self.cpu.mem_read(self.state, 28))

    @property
    def generation(self):
        return self.words()[0]

    def invoke(self, event, now=0, pending=False, progressed=False, generation=None, age_cap=None, callback_epoch=None):
        args = (event, now & 0xffffffff, int(pending), int(progressed),
                self.generation if generation is None else generation,
                int(age_cap is not None), 0 if age_cap is None else age_cap & 0xffffffff,
                self.words()[6] if callback_epoch is None else callback_epoch)
        self.cpu.mem_write(self.inputs, struct.pack('<8I', *args))
        self.cpu.reg_write(UC_ARM_REG_R0, self.state)
        self.cpu.reg_write(UC_ARM_REG_R1, self.inputs)
        self.cpu.reg_write(UC_ARM_REG_SP, self.stack)
        self.cpu.reg_write(UC_ARM_REG_LR, self.stop)
        saved = {r: 0xa1230000 + r for r in range(UC_ARM_REG_R4, UC_ARM_REG_R10 + 1)}
        for r, v in saved.items(): self.cpu.reg_write(r, v)
        self.cpu.emu_start(self.code, self.stop, count=500)
        assert self.cpu.reg_read(UC_ARM_REG_PC) == self.stop
        assert self.cpu.reg_read(UC_ARM_REG_SP) == self.stack
        assert all(self.cpu.reg_read(r) == v for r, v in saved.items())
        assert bytes(self.cpu.mem_read(self.code, len(self.blob))) == self.blob
        return self.cpu.reg_read(UC_ARM_REG_R0)


class LazyMachine(DeadlineMachine):
    def __init__(self, image, *, policy_blob, age_budget_ticks=None, **kwargs):
        self.policy = ArmPolicy(policy_blob)
        self.age_budget_ticks = age_budget_ticks
        self.arrival_ticks = {}
        self.deferred_native_flush = False
        super().__init__(image, timeout_ms=50, timeout_ticks=6, **kwargs)
        # Exact guard for the entry whose policy/timer tail is replaced in the model.
        assert bytes(self.cpu.mem_read(0x47748, 4)) == bytes.fromhex('2430d5e5')

    def tick(self):
        return round(self.now_ms * 1000) // TICK_US

    def pending_pointers(self):
        return [p for i in range(self.window) if (p := self.get(self.slots + 4*i))]

    def policy_event(self, event, *, progressed=False):
        pending = self.pending_pointers()
        age_cap = None
        if pending and self.age_budget_ticks is not None:
            age_cap = min(self.arrival_ticks[p] for p in pending) + self.age_budget_ticks
        action = self.policy.invoke(event, self.tick(), bool(pending), progressed, age_cap=age_cap)
        assert not action & IGNORE
        if action & STOP:
            self.due_ms = None
            self.events.append({'event': 'stop', 'at_ms': self.now_ms})
        if action & ARM:
            target = self.policy.words()[5]
            remaining = signed_delta(target, self.tick() & 0xffffffff)
            assert 0 < remaining <= 6
            # Exact native ticks are modeled. An actual hook must call/verify an
            # absolute tick primitive rather than naively round ms a second time.
            self.due_ms = (self.tick() + remaining) * TICK_US / 1000
            self.events.append({'event': 'arm', 'at_ms': self.now_ms, 'due_ms': self.due_ms})
        self.cpu.mem_write(self.table + 36, bytes([int(bool(self.policy.words()[4]))]))
        return action

    def receive(self, sequence):
        pointer = 0x500000 + 256*len(self.ownership)
        self.arrival_ticks[pointer] = self.tick()
        result = super().receive(sequence)
        # RX policy runs inside original receive. Its FLUSH continuation executes
        # only after that invocation returns, never by re-entering Unicorn/native
        # RX while its original frame is active. Lifecycle exclusion is modeled.
        if self.deferred_native_flush:
            self.deferred_native_flush = False
            self.call('wlan_flush_data', self.table + 32)
        return result

    def _hook(self, uc, address, size, user):
        if address == 0x47748:
            progressed = self.get(self.table + 20) != uc.reg_read(UC_ARM_REG_R6)
            action = self.policy_event(RX, progressed=progressed)
            if action & FLUSH:
                assert not self.deferred_native_flush
                self.deferred_native_flush = True
            uc.reg_write(UC_ARM_REG_PC, 0x47790)
            return
        super()._hook(uc, address, size, user)

    def advance(self, until_ms):
        assert until_ms >= self.now_ms
        callbacks = 0
        while self.due_ms is not None and self.due_ms <= until_ms:
            assert self.alive and self.timer_alive and callbacks < 128
            self.now_ms, self.due_ms = self.due_ms, None
            action = self.policy_event(EXPIRY)
            if action & FLUSH:
                self.call('wlan_flush_data', self.table + 32)
            callbacks += 1
        self.now_ms = until_ms
        return callbacks

    def delete(self):
        action = self.policy.invoke(DELETE, self.tick())
        if action & STOP:
            self.due_ms = None
            self.events.append({'event': 'stop', 'at_ms': self.now_ms})
        super().delete()


def compile_policy(out):
    src = Path(__file__).with_name('rx_timer_churn_policy.S')
    obj = out / 'rx_timer_churn_policy.o'
    subprocess.run(['clang', '--target=armv7-none-eabi', '-c', str(src), '-o', str(obj)],
                   check=True, capture_output=True, timeout=30)
    with obj.open('rb') as f:
        elf = ELFFile(f)
        assert not any(s['sh_type'] in ('SHT_REL', 'SHT_RELA') and s['sh_size'] for s in elf.iter_sections())
        blob = elf.get_section_by_name('.text').data()
    return blob, {'bytes': len(blob), 'source_sha256': hashlib.sha256(src.read_bytes()).hexdigest(),
                  'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest()}


def policy_tests(blob):
    result = []
    for origin in (0, 0xfffffffc):
        p = ArmPolicy(blob)
        assert p.invoke(RX, origin, progressed=True) == 0
        assert p.invoke(RX, origin + 4, pending=True) == ARM
        assert p.words()[5] == (origin + 6) & 0xffffffff
        assert p.invoke(RX, origin + 5, pending=True, progressed=True) == 0
        assert p.words()[3] == (origin + 11) & 0xffffffff
        assert p.invoke(EXPIRY, origin + 6, pending=True) == ARM
        assert p.words()[5] == (origin + 11) & 0xffffffff
        assert p.invoke(EXPIRY, origin + 11, pending=True) == FLUSH
        result.append({'case': 'inherited_anchor_lazy_rearm_wrap', 'origin': origin, 'status': 'pass'})
    p = ArmPolicy(blob)
    assert p.invoke(RX, 0, pending=True) == ARM
    assert p.invoke(RX, 1, pending=True, progressed=True, age_cap=3) == ARM | STOP
    assert p.words()[5] == 3
    assert p.invoke(EXPIRY, 3, pending=True, age_cap=3) == FLUSH
    result.append({'case': 'earlier_age_cap_replaces_timer', 'status': 'pass'})
    for captured_before_cancel in (False, True):
        p = ArmPolicy(blob)
        old = p.generation
        assert p.invoke(RX, 0, pending=True) == ARM
        if not captured_before_cancel:
            assert p.invoke(RX, 1) == STOP
            assert p.invoke(EXPIRY, 6, pending=True) == IGNORE
        assert p.invoke(DELETE, 2) == (STOP if captured_before_cancel else 0)
        assert p.invoke(EXPIRY, 6, pending=True, generation=old) == IGNORE
        assert p.invoke(CREATE) == 0
        new = p.generation
        assert new != old
        assert p.invoke(RX, 7, pending=True) == ARM
        before = p.words()
        assert p.invoke(EXPIRY, 8, pending=True, generation=old) == IGNORE
        assert p.words() == before
        assert p.invoke(DELETE, generation=old) == IGNORE
        assert p.words() == before
        result.append({'case': 'cancel_capture_delete_reuse', 'captured': captured_before_cancel,
                       'old_generation': old, 'new_generation': new, 'status': 'pass'})
    p = ArmPolicy(blob)
    assert p.invoke(RX, 0, pending=True) == ARM
    epoch = p.words()[6]
    assert p.invoke(RX, 1) == STOP
    assert p.invoke(RX, 2, pending=True) == ARM
    before = p.words()
    assert p.invoke(EXPIRY, 6, pending=True, callback_epoch=epoch) == IGNORE
    assert p.words() == before
    result.append({'case': 'captured_cancel_then_rearm_same_table', 'status': 'pass'})
    p = ArmPolicy(blob)
    before = p.words()
    assert p.invoke(CREATE) == IGNORE and p.words() == before
    assert p.invoke(99) == IGNORE and p.words() == before
    result.append({'case': 'invalid_event_and_double_create_fail_closed', 'status': 'pass'})
    return result


def lazy_replay(image, fixes, blob, records, window, age_budget=None):
    machine = LazyMachine(image, policy_blob=blob, start=4090, window=window,
                          age_budget_ticks=age_budget, **fixes)
    arrivals = {}
    for row in records:
        machine.advance(row['at_ms'])
        machine.receive((4090 + row['packet']) % 4096)
        arrivals[next(reversed(machine.ownership))] = row
        machine.snapshot()
    machine.advance(records[-1]['at_ms'] + 500)
    delivery = {r['pointer']: r['at_ms'] for r in machine.delivered}
    outcomes = [{**r, 'state': machine.ownership[p]['state'],
                 'wait_ms': None if p not in delivery else round(delivery[p] - r['at_ms'], 3)}
                for p, r in arrivals.items()]
    order = [arrivals[r['pointer']]['packet'] for r in machine.delivered]
    assert order == sorted(set(order))
    assert not any(r['state'] == 'pending' for r in outcomes)
    counts = {s: sum(r['state'] == s for r in outcomes) for s in ('delivered', 'rejected', 'pending')}
    summary = {**counts, 'arrivals': len(records),
               'wait_ms': distribution([r['wait_ms'] for r in outcomes if r['wait_ms'] is not None]),
               'timer_arms': sum(e['event'] == 'arm' for e in machine.events),
               'timer_stops': sum(e['event'] == 'stop' for e in machine.events)}
    summary['timer_stops_runtime'] = summary['timer_stops']
    machine.delete()
    machine.advance(machine.now_ms + 1000)
    summary['timer_stops'] = sum(e['event'] == 'stop' for e in machine.events)
    return {'summary': summary, 'outcomes': outcomes}



def delayed_expiry_rx_test(image, fixes, blob):
    machine = LazyMachine(image, policy_blob=blob, start=4090, window=32,
                          age_budget_ticks=6, **fixes)
    machine.receive(4092)
    old_generation, old_epoch = machine.policy.generation, machine.policy.words()[6]
    old_due = machine.due_ms
    assert old_due == 50.418
    # Deliberately defer softclock while RX work runs after the age deadline.
    # This is a scheduler boundary model, not an assertion about actual QNX order.
    machine.now_ms = 60
    machine.receive(4093)
    assert not machine.deferred_native_flush
    assert machine.snapshot()['pending'] == []
    assert [r['sequence'] for r in machine.delivered] == [4092, 4093]
    assert machine.due_ms is None
    before = machine.policy.words()
    assert machine.policy.invoke(EXPIRY, machine.tick(), True,
                                 generation=old_generation, callback_epoch=old_epoch) == IGNORE
    assert machine.policy.words() == before
    delivered = machine.delivered.copy()
    machine.advance(500)
    assert machine.delivered == delivered
    machine.delete()
    machine.advance(1000)
    return {'case': 'overdue_rx_flush_before_delayed_softclock', 'status': 'pass',
            'previous_deadline_ms': old_due, 'rx_at_ms': 60,
            'delivered_once': [4092, 4093], 'stale_callback': 'ignored'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('driver', type=Path)
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--quick', action='store_true', help='small smoke matrix')
    args = parser.parse_args()
    data = args.driver.read_bytes()
    image = DriverImage(data)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    blob, info = compile_policy(args.out_dir)
    fixes = dict(candidate=compile_candidate(args.out_dir)[0],
                 window_candidate=compile_window_candidate(args.out_dir)[0],
                 harden_candidate=compile_harden_candidate(args.out_dir)[0])
    traces = [(p, make_trace(p, 629, count=192)) for p in
              ('clean', 'jitter', 'loss_1pct', 'retry_20ms', 'retry_80ms', 'burst_loss', 'mixed', 'duplicates')]
    for name, pairs in [('inherited_deadline', [(0, 0), (40, 2)]),
                        ('expired_empty_deadline', [(0, 0), (55, 2)]),
                        ('lazy_progress', [(0, 3), (40, 0), (80, 1)])]:
        traces.append((name, [dict(id=i, packet=p, at_ms=t, source_ms=t, duplicate=False)
                              for i, (t,p) in enumerate(pairs)]))
    cases = []
    for window in ((32,) if args.quick else (16, 32, 64)):
        for name, records in traces:
            for phase in ((0,) if args.quick else (0, 4201, 8402)):
                phased = [{**r, 'at_ms': r['at_ms'] + phase/1000} for r in records]
                baseline = replay(image, fixes, 6, phased, timeout=50, window=window)
                lazy = lazy_replay(image, fixes, blob, phased, window)
                assert baseline['outcomes'] == lazy['outcomes'], (window, name, phase)
                capped = lazy_replay(image, fixes, blob, phased, window, age_budget=6)
                cases.append(dict(window=window, trace=name, phase_us=phase,
                                  baseline=baseline['summary'], lazy=lazy['summary'],
                                  age_capped=capped['summary'], identical_outcomes=True))
    assert args.driver.read_bytes() == data
    result = dict(status='pass; offline only', driver_sha256=hashlib.sha256(data).hexdigest(),
                  policy=info, unit_cases=policy_tests(blob), native_cases=cases,
                  delayed_expiry_case=delayed_expiry_rx_test(image, fixes, blob),
                  limitations=['tick scheduling is an explicit OS model',
                               'serialized lifetime modeled, concurrency not proven',
                               'no production code cave or callback/lifecycle integration',
                               'timer_stops includes final native teardown stop for both profiles'])
    target = args.out_dir / 'rx_timer_churn_results.json'
    target.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(result=str(target), native_cases=len(cases), unit_cases=len(result['unit_cases']),
                          status=result['status'])))


if __name__ == '__main__': main()

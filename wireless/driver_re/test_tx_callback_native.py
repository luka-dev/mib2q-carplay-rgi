"""Execute pinned io-pkt enqueue/owner-clean/dispatch with ARM wake tickets.

OS mutex, malloc/free, pulse delivery and target execution are explicit stubs.
The capture point is a constructed interleaving, not proof of HU scheduling.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                               UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)
from .callout_machine import CalloutMachine
from .reorder_machine import SAVED_REGISTERS
from .wifi_latency_native import PolicyMachine, compile_policies


class CallbackMachine(CalloutMachine):
    head, tail, mutex = 0x24a42c, 0x24a430, 0x2393b4
    capture = 0x1d5ca0
    node, cookie, process, pulse, errno = 0x407000, 0x408000, 0x409000, 0x40a000, 0x40b000

    def __init__(self, data, elf):
        self.qdepth, self.allocated, self.freed = 0, set(), []
        self.malloc_fail, self.pulse_fail, self.capture_next = False, False, False
        self.saved_dispatch = None
        self.target_calls, self.callback_actions = 0, []
        self.policy = PolicyMachine(elf)
        self.policy.call('wl_tx_init', self.policy.tx)
        assert self.policy.call('wl_tx_bind', self.policy.tx, 1) == 0
        super().__init__(data)
        self.symbols['tx_dispatch'] = (0x1d5a80, 0x28c)
        for name in ('stk_context_callback_2', 'stk_context_callback_2_clean', 'tx_dispatch'):
            addr, size = self.symbols[name]
            self.allowed.append(range(addr, addr+size))
        self.put(self.head, 0)
        self.put(self.tail, self.head)
        self.put(0x24a0dc, self.process)
        self.put(self.process+116, 0)
        self.put(self.process+168, self.pulse)
        self.cpu.mem_write(self.pulse+4, b'\x06')
        self.put(self.pulse+8, 0)

    def _hook(self, uc, address, size, user):
        r0 = uc.reg_read(UC_ARM_REG_R0)
        if address == self.capture and self.capture_next:
            assert self.qdepth == 0
            self.capture_next = False
            uc.emu_stop()
        elif address == self.symbols.get('malloc', (0,))[0]:
            assert r0 == 20
            if self.malloc_fail:
                self.malloc_fail = False
                self._return(0)
            else:
                assert self.node not in self.allocated
                self.allocated.add(self.node)
                self._return(self.node)
        elif address == self.symbols.get('free', (0,))[0]:
            assert r0 in self.allocated
            self.allocated.remove(r0)
            self.freed.append(r0)
            self._return()
        elif address == self.symbols.get('__get_errno_ptr', (0,))[0]:
            self._return(self.errno)
        elif address == self.symbols.get('MsgSendPulse', (0,))[0]:
            assert uc.reg_read(UC_ARM_REG_R2) == 6
            self._return(0xffffffff if self.pulse_fail else 0)
        elif address == self.symbols['pthread_mutex_lock'][0]:
            assert r0 == self.mutex and not self.qdepth
            self.qdepth = 1
            self._return()
        elif address == self.symbols['pthread_mutex_unlock'][0]:
            assert r0 == self.mutex and self.qdepth == 1
            self.qdepth = 0
            self._return()
        elif address == self.callback_stub:
            assert r0 == self.cookie and not self.qdepth
            ticket = (self.get(self.cookie), self.get(self.cookie+4))
            action = self.policy.call('wl_tx_begin', self.policy.tx, *ticket)
            self.callback_actions.append(action)
            if action == 2:
                self.target_calls += 1
                assert self.policy.call('wl_tx_finish', self.policy.tx, *ticket, self.policy.ticket) == 0
            else:
                assert action == 3
            self._return()
        else:
            super()._hook(uc, address, size, user)

    def dispatch(self, capture=False):
        sp = self.soft_stack+0xfff0
        saved = {r: 0xb1000000+i*256 for i, r in enumerate(SAVED_REGISTERS)}
        for reg, value in saved.items(): self.cpu.reg_write(reg, value)
        self.cpu.reg_write(UC_ARM_REG_SP, sp)
        self.cpu.reg_write(UC_ARM_REG_LR, self.stop)
        self.capture_next = capture
        self.cpu.emu_start(0x1d5a80, self.stop, timeout=1_000_000, count=20000)
        if capture:
            assert self.cpu.reg_read(UC_ARM_REG_PC) == self.capture
            self.saved_dispatch = self.cpu.context_save(), sp, saved
        else:
            self._check_return(sp, saved)
        assert not self.qdepth

    def resume_dispatch(self):
        context, sp, saved = self.saved_dispatch
        self.saved_dispatch = None
        self.cpu.context_restore(context)
        self.cpu.emu_start(self.capture, self.stop, timeout=1_000_000, count=20000)
        self._check_return(sp, saved)

    def enqueue(self, owner=0x9abc):
        action, ticket = self.policy.tx_request()
        assert action == 1
        self.put(self.cookie, ticket[0]); self.put(self.cookie+4, ticket[1])
        result = self.call('stk_context_callback_2', self.callback_stub, self.cookie, owner)
        assert not self.qdepth
        return result, ticket


def run(data, elf):
    cases = []
    m = CallbackMachine(data, elf)
    assert m.enqueue()[0] == 0
    assert m.get(m.node+8) == 0x9abc
    m.dispatch()
    assert m.target_calls == 1 and not m.allocated and not m.get(m.head)
    cases.append('enqueue_dispatch_preserves_owner')

    m = CallbackMachine(data, elf)
    m.malloc_fail = True
    result, ticket = m.enqueue()
    assert result == 0xffffffff and m.get(m.errno) == 12 and not m.allocated
    assert m.policy.call('wl_tx_submit_failed', m.policy.tx, *ticket) == 0
    cases.append('allocation_failure_releases_gate')

    m = CallbackMachine(data, elf)
    m.pulse_fail = True
    result, ticket = m.enqueue()
    assert result == 0xffffffff and not m.allocated and not m.get(m.head)
    # Only this serialized, non-captured failure is a definite cancellation ACK.
    assert m.policy.call('wl_tx_submit_failed', m.policy.tx, *ticket) == 0
    cases.append('serialized_pulse_failure_unlinks_node')

    m = CallbackMachine(data, elf)
    result, ticket = m.enqueue()
    assert result == 0
    assert m.policy.call('wl_tx_retire', m.policy.tx, 1) == 4
    m.call('stk_context_callback_2_clean', 0x1111)
    assert m.allocated  # wrong owner cannot clean this queue node
    m.call('stk_context_callback_2_clean', 0x9abc)
    assert not m.allocated and not m.callback_actions
    # Adapter explicitly acknowledges the removed queued ticket. Stock clean
    # does not call our wrapper and cannot clear its gate automatically.
    assert m.policy.call('wl_tx_submit_failed', m.policy.tx, *ticket) == 0
    assert m.policy.call('wl_tx_reclaimable', m.policy.tx) == 1
    cases.append('owner_clean_requires_explicit_ticket_ack')

    m = CallbackMachine(data, elf)
    m.enqueue()
    m.dispatch(capture=True)
    assert m.allocated and not m.get(m.head)
    assert m.policy.call('wl_tx_retire', m.policy.tx, 1) == 4
    m.call('stk_context_callback_2_clean', 0x9abc)
    assert m.allocated and not m.callback_actions
    assert m.policy.call('wl_tx_reclaimable', m.policy.tx) == 0
    m.resume_dispatch()
    assert m.callback_actions == [3] and m.target_calls == 0 and not m.allocated
    assert m.policy.call('wl_tx_reclaimable', m.policy.tx) == 1
    cases.append('captured_callback_survives_clean_then_discards_retired_target')
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('io_pkt', type=Path)
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    elf, info = compile_policies(args.out_dir)
    data = args.io_pkt.read_bytes()
    cases = run(data, elf)
    result = dict(status='pass; offline only', cases=cases, policies=info,
                  io_pkt_sha256=hashlib.sha256(data).hexdigest(),
                  source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  limitations='Explicit OS boundary stubs and constructed capture, no production detach proof.')
    (args.out_dir/'tx_callback_native_results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f'PASS: {len(cases)} native callback queue/lifetime cases')


if __name__ == '__main__':
    main()

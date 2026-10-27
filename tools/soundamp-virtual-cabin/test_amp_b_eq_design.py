"""Decoded outer designer oracle with explicit host math-call substitutes."""
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import unittest

import numpy as np
from numpy.testing import assert_allclose
from scipy.signal import freqz, sosfilt

from amp_b_eq_design import design_b_eq
from amp_b_filters import LibraryFilterCascade, stable_section

OUT = Path(__file__).resolve().parents[2]/'assets/soundamp/remaining-blocks-20260919'


def bits(value): return struct.unpack('<I', struct.pack('<f', value))[0]
def flt(value): return struct.unpack('<f', struct.pack('<I', value & 0xffffffff))[0]


def decoded_design(record, sample_rate=48000):
    """Execute12AB1F caller math/branches/stores, float32 at every operation.

Division/sin/cos/pow/sinh/sqrt and stability calls are host stand-ins, not
bit-exact implementations of the firmware library. CJUMP frame prologue/return
is an explicit ABI bridge; CALL delay instructions really execute first.
"""
    code = {}; raw = []
    for name in ('first_b_eq_identity', 'first_b_eq_design'):
        for line in (OUT/(name+'.asm')).read_text().splitlines():
            m = re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)', line)
            if m:
                words = [int(w, 16) for w in m[2].split()]; raw.extend(words)
                code[int(m[1], 16)] = len(words), m[3]
    assert hashlib.sha256(struct.pack(f'<{len(raw)}H', *raw)).hexdigest() == 'ddb418950c281e25fbd759923641e253b58582764d56887f89c96b40b19b9ea4'
    regs = {f'R{i}': 0 for i in range(16)} | {f'I{i}': 0 for i in range(16)}
    regs.update(I6=10000, I7=9998, M5=0, M6=1, M7=-1, M14=1)
    memory = {9999: 0, 10006: 20000, 10007: bits(sample_rate)}
    memory.update({10001+i: bits(v) for i, v in enumerate(record)})
    pc = 0x12ab1f; flag = 0; sz = False; pending = None; errors = []; calls = []
    def call(target):
        calls.append(target)
        x, y = flt(regs['R4']), flt(regs['R8'])
        if target == 0x12ab0a:
            memory.update({regs['R4']+i: bits(v) for i, v in enumerate([1, 0, 0, 0, 0])})
        elif target == 0x124354: regs['R0'] = bits(x/y)
        elif target == 0x125c2b: regs['R0'] = bits(math.sin(x))
        elif target == 0x125c1c: regs['R0'] = bits(math.cos(x))
        elif target == 0x1257e6: regs['R0'] = bits(x**y)
        elif target == 0x12440b: regs['R0'] = bits(math.sinh(x))
        elif target == 0x1244a6: regs['R0'] = bits(math.sqrt(x))
        elif target == 0x13163d:
            # ABI takes a2 then a1 after stored negated-feedback operands.
            regs['R0'] = int(abs(x) < 1 and abs(y) < 1+x)
        elif target == 0x12b6a7:
            errors.append((regs['R4'], regs['R8'], regs['R12'])); regs['R0'] = 1
        else: raise AssertionError(f'unsupported call {target:x}')
    for _ in range(1000):
        size, op = code[pc]
        if op.startswith('JUMP (M14,I12)'):
            return [flt(memory[20000+i]) for i in range(5)], errors, calls
        if op.startswith('CJUMP '):
            call(int(op.split()[1], 16)); pc += size
            for _ in range(2): pc += code[pc][0]
            continue
        if op.startswith('CALL '):
            pending = [2, int(op.split()[1], 16), True]; pc += size; continue
        old = regs.copy(); oldflag = flag; oldsz = sz
        writes = {}; stores = {}; nextpc = pc+size; newpending = None
        def value(v):
            if v.startswith('F'): return flt(old['R'+v[1:]])
            return old[v] if v in old else int(v, 0)
        def address(spec):
            a, b = spec.split(',')
            if a.startswith('I'):
                result = value(a); writes[a] = result+value(b); return result
            return value(a)+value(b)
        # Multifunction compute clauses also read the old register set.
        for part in re.split(r' \|\| |, (?=[FR]\d+=)', op):
            if part.startswith('IF '):
                m = re.match(r'IF (NOT AV|GE|EQ|NE|SZ|LT) (.*)', part)
                assert m, part
                cond, part = m.groups()
                if not {'NOT AV': True, 'GE': oldflag >= 0, 'EQ': oldflag == 0,
                        'NE': oldflag != 0, 'SZ': oldsz, 'LT': oldflag < 0}[cond]: continue
            if part == 'NOP': continue
            if part.startswith('JUMP '):
                target = int(part.split()[1], 16)
                if '(DB)' in part: newpending = [2, target, False]
                else: nextpc = target
                continue
            m = re.fullmatch(r'COMP\((\w+),(\w+)\)', part)
            if m:
                a, b = value(m[1]), value(m[2]); flag = (a > b)-(a < b); continue
            part = re.sub(r'^(\w+)([+*\-])=(\w+)$', r'\1=\1\2\3', part)
            dest, expr = part.split('=', 1)
            if dest.startswith('DM('): stores[address(dest[3:-1])] = value(expr); continue
            if expr.startswith('DM('): out = memory[address(expr[3:-1])]
            elif expr.startswith('MODIFY('):
                a, b = expr[7:-1].split(','); out = value(a)+value(b)
            elif expr.startswith('PASS '):
                out = value(expr[5:]); flag = (out > 0)-(out < 0)
            elif expr.startswith('TRUNC '): out = math.trunc(value(expr[6:]))
            elif expr.startswith('MANT '): out = math.frexp(value(expr[5:]))[0]
            elif expr.startswith('BTGL '):
                a, b = expr[5:].split(' BY '); out = value(a)^(1 << value(b)); sz = out == 0
            elif re.fullmatch(r'\w+[+*\-]\w+', expr):
                a, operator, b = re.split(r'([+*\-])', expr); a, b = value(a), value(b)
                out = {'+': lambda: a+b, '-': lambda: a-b, '*': lambda: a*b}[operator]()
                flag = (out > 0)-(out < 0)
            elif expr.startswith('-F'): out = -value(expr[1:])
            else: out = value(expr)
            writes['R'+dest[1:] if dest.startswith('F') else dest] = bits(out) if dest.startswith('F') else out
        regs.update(writes); memory.update(stores)
        if pending:
            pending[0] -= 1
            if pending[0] == 0:
                _, target, is_call = pending; pending = None
                if is_call: call(target)
                else: nextpc = target
        if newpending: pending = newpending
        pc = nextpc
    raise AssertionError('designer execution did not terminate')


class BEqualizerDesignerTests(unittest.TestCase):
    def test_zero_gain_exact_identity_all_three_types(self):
        for kind in range(3):
            record = [1000, 100, 0, kind, 1]
            self.assertEqual(design_b_eq(record), [1, 0, 0, 0, 0])
            actual, errors, _ = decoded_design(record)
            self.assertEqual(actual, [1, 0, 0, 0, 0]); self.assertFalse(errors)

    def test_nonzero_paths_match_decoded_float32_outer_designer(self):
        for kind in range(3):
            for f in (20, 80, 1000, 8000, 16000):
                for width in (50, 100, 150):
                    for gain in (-20, -6, 6, 20):
                        record = [f, width, gain, kind, 1]
                        with self.subTest(record=record):
                            actual, errors, _ = decoded_design(record)
                            self.assertFalse(errors)
                            assert_allclose(design_b_eq(record), actual, rtol=2e-5, atol=3e-6)

    def test_reserved_word_is_not_read_and_type_rounds(self):
        for kind in range(3):
            for other in (-17, 0, 9e10):
                record = [1000, 100, 6, kind+.49, other]
                assert_allclose(design_b_eq(record), design_b_eq([1000, 100, 6, kind, 1]), atol=0)
                actual, errors, _ = decoded_design(record)
                self.assertFalse(errors)
                assert_allclose(actual, design_b_eq(record), rtol=2e-5, atol=3e-6)

    def test_gain_at_center_and_shelf_endpoints(self):
        for gain in (-20, -6, 6, 20):
            for kind in range(3):
                r = design_b_eq([1000, 100, gain, kind, 1])
                _, h = freqz(r[:3], [1, -r[3], -r[4]], worN=[0, 2*math.pi*1000/48000, math.pi])
                db = 20*np.log10(np.abs(h))
                expected = [0, gain, 0] if kind == 0 else [gain, gain/2, 0] if kind == 1 else [0, gain/2, gain]
                assert_allclose(db, expected, atol=.0006)

    def test_nonzero_coefficients_run_through_library_cascade(self):
        records = [design_b_eq([1000, 100, 6, k, 1]) for k in range(3)]
        x = np.random.default_rng(920).normal(size=(211, 2))
        sos = [[*r[:3], 1, -r[3], -r[4]] for r in records]
        assert_allclose(LibraryFilterCascade(records, 2).process(x), sosfilt(sos, x, axis=0), atol=1e-12)

    def test_rejects_unsupported_domain_and_nonreal_shelf(self):
        for record in ([0, 100, 6, 0, 1], [24000, 100, 6, 0, 1], [1000, 0, 6, 0, 1],
                [1000, 100, 21, 0, 1], [1000, 100, -101, 0, 1], [1000, 100, 6, 3, 1],
                [1000, 100, 6, 0, float('nan')], [1000, 1000, 20, 1, 1]):
            with self.assertRaises(ValueError): design_b_eq(record)

    def test_recovered_deep_gain_clamp_against_decoded_oracle(self):
        for kind in range(3):
            for frequency in (20, 1000, 20000):
                for gain in (-100, -60, -30):
                    record = [frequency, 100, gain, kind, 1]
                    actual, errors, _ = decoded_design(record)
                    self.assertFalse(errors)
                    assert_allclose(design_b_eq(record), actual, rtol=3e-5, atol=2e-5)

    def test_opt_in_replay_nonzero_eq_and_default_fail_closed(self):
        from amp_b_bound_scenario import replay_bound_scalar_targets
        report = json.loads((OUT/'b_config_control.json').read_text())
        controls = {19: 0, 18: 0, 22: 0, 21: 0, 23: 0, 24: 0, 25: 0, 27: 0, 29: 0}
        slots = {d: [0]*9 for d in range(44, 59)}; slots[55][8] = 6
        args = dict(source='b01', bank=1, effective_values=controls, contributions=slots,
                    shadow_selectors={16: 2, 20: 1})
        with self.assertRaisesRegex(ValueError, 'zero-gain'):
            replay_bound_scalar_targets(report, **args)
        got = replay_bound_scalar_targets(report, **args, allow_nonzero_eq=True)
        expected = design_b_eq([20, 100, 6, 0, 1])
        assert_allclose([got['config_updates'][295+i] for i in range(5)], expected, atol=0)
        self.assertTrue(got['eq_design']['designer_math_reference_not_bitexact'])
        self.assertFalse(got['actual_vehicle_state_proven'])
        with self.assertRaises(ValueError): replay_bound_scalar_targets(report, **args, allow_nonzero_eq=1)


if __name__ == '__main__': unittest.main()

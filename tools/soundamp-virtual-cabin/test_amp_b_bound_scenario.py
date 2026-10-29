import copy
import hashlib
import json
import re
import struct
import sys
import unittest

import numpy as np
from numpy.testing import assert_allclose
from scipy.signal import sosfilt

from amp_b_bound_scenario import (retained_config, bind_effective_output, build_effective_finish,
    bind_effective_mode, replay_output_target_slice, api21_22_bindings,
    api21_22_parameter_targets, replay_bound_scalar_targets)
from test_amp_b_control import OUT, decoded_curve

sys.path.insert(0, str(OUT.parents[3]/'tools'))
from audit_amp_b_pointer_aliases import (analyze_instructions, audit as audit_pointer_aliases,
                                        smoothing_caller_domains)


def decoded_api_calls(api, wanted_hash):
    """Bounded caller execution with success-return stand-ins for12B00F.

No expected descriptor/record maps are consulted. Explicit host ABI bridge
captures arguments before the call and skips its prologue/return delay words.
Actual curve arithmetic is checked separately by the decoded kernel oracle.
"""
    code = {}; raw = []
    for line in (OUT/f'first_b_gain_slot{2 if api == 22 else 3}.asm').read_text().splitlines():
        match = re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)', line)
        if match:
            words = [int(w, 16) for w in match[2].split()]; raw.extend(words)
            code[int(match[1], 16)] = (len(words), match[3])
    assert hashlib.sha256(struct.pack(f'<{len(raw)}H', *raw)).hexdigest() == wanted_hash
    regs = {f'R{i}': 0 for i in range(16)} | {f'I{i}': 0 for i in range(16)}
    regs.update(R4=10000, R8=struct.unpack('<I', struct.pack('<f', 73))[0],
                I6=40000, I7=39999, M5=0, M6=1, M7=-1, M14=1)
    memory = {11180: 20000, 39999: 0} | {10000+d: 30000+d for d in range(44, 59)}
    pc = min(code); calls = []; flag = 0; pending = None
    for _ in range(500):
        size, op = code[pc]
        if op.startswith('JUMP (M14,I12)'): return calls
        if op.startswith('CJUMP '):
            assert op.startswith('CJUMP 0x12b00f '), op
            pointer = memory[regs['I7']+4]
            calls.append({'descriptor': regs['R4']-30000, 'slot': regs['R8'],
                'records': [memory[pointer]-20000, memory[pointer+1]-20000],
                'count': memory[regs['I7']+3],
                'clamp': [struct.unpack('<f', struct.pack('<I', memory[regs['I7']+j]))[0] for j in (1, 2)]})
            regs['R0'] = 0; pc += size
            for _ in range(2): pc += code[pc][0]
            continue
        old = regs.copy(); oldflag = flag; writes = {}; stores = {}; nextpc = pc+size; newpending = None
        def value(v): return old[v] if v in old else int(v, 0)
        def address(spec):
            a, b = spec.split(',')
            if a.startswith('I'):
                result = value(a); writes[a] = result+value(b); return result
            return value(a)+value(b)
        for part in op.split(' || '):
            if part.startswith('IF EQ '):
                if oldflag != 0: continue
                part = part[6:]
            if part == 'NOP': continue
            if part.startswith('JUMP '):
                target = int(part.split()[1], 16)
                if '(DB)' in part: newpending = [2, target]
                else: nextpc = target
                continue
            dest, expr = part.split('=', 1)
            if dest.startswith('DM('): stores[address(dest[3:-1])] = value(expr); continue
            if expr.startswith('DM('): out = memory[address(expr[3:-1])]
            elif expr.startswith('MODIFY('):
                a, b = expr[7:-1].split(','); out = value(a)+value(b)
            elif expr.startswith('PASS '):
                out = value(expr[5:]); flag = (out > 0)-(out < 0)
            else: out = value(expr)
            writes[dest] = out
        regs.update(writes); memory.update(stores)
        if pending:
            pending[0] -= 1
            if pending[0] == 0: nextpc = pending[1]; pending = None
        if newpending: pending = newpending
        pc = nextpc
    raise AssertionError('bounded caller did not terminate')


def decoded_filter_smoothing_addresses(count, base):
    """Execute the five-call coefficient loop12DC79..12DCD1.

Smoothing calls captureR4 and return; stability returns success. This is an
address-domain oracle, not coefficient/filter audio or reset/error emulation.
"""
    code = {}
    for line in (OUT/'first_b_filter_wrapper.asm').read_text().splitlines():
        m = re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)', line)
        if m: code[int(m[1], 16)] = len(m[2].split()), m[3]
    regs = {f'R{i}': 0 for i in range(16)} | {'I5': 0, 'M5': 0, 'M6': 1}
    regs.update(R8=base, R13=count); pc = 0x12dc79; flag = 0; sz = False; addresses = []
    for _ in range(10000):
        if pc == 0x12dcd1: return addresses
        n, text = code[pc]
        if text.startswith('CJUMP '):
            target = int(text.split()[1], 16)
            if target == 0x12faf2: addresses.append(regs['R4']); regs['R0'] = regs['R1'] = regs['R2'] = 0
            elif target == 0x13163d: regs['R0'] = 1
            else: raise AssertionError(text)
            pc += n
            for _ in range(2): pc += code[pc][0]
            continue
        old = regs.copy(); oldflag = flag; oldsz = sz; writes = {}; nextpc = pc+n
        def val(v): return old[v] if v in old else int(v, 0)
        for part in text.split(' || '):
            if part.startswith('IF '):
                m = re.match(r'IF (NOT SZ|NE) (.*)', part); assert m, part
                cond, part = m.groups()
                if not {'NOT SZ': not oldsz, 'NE': oldflag != 0}[cond]: continue
            if part.startswith('JUMP '): nextpc = int(part.split()[1], 16); continue
            dest, expr = part.split('=', 1)
            if expr.startswith('DM('):
                a, b = expr[3:-1].split(',')
                if a.startswith('I'): writes[a] = val(a)+val(b)
                out = 0  # stable feedback placeholders; no address-domain dependency.
            elif dest.startswith('F'): out = 0
            elif expr.startswith('BTGL '):
                a, b = expr[5:].split(' BY '); out = val(a)^(1 << val(b)); sz = out == 0
            elif re.fullmatch(r'\w+[+\-]\w+', expr):
                a, op, b = re.split(r'([+\-])', expr); out = val(a)+val(b) if op == '+' else val(a)-val(b)
                flag = (out > 0)-(out < 0)
            else: out = val(expr)
            writes['R'+dest[1:] if dest.startswith('F') else dest] = out
        regs.update(writes); pc = nextpc
    raise AssertionError('bounded filter-address loop did not terminate')


def decoded_smoothing_prefix(offset):
    """Execute address guard/read prefix through12FB0D, no float update."""
    code = {}
    for line in (OUT/'first_b_coefficient_smoothing.asm').read_text().splitlines():
        m = re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)', line)
        if m: code[int(m[1], 16)] = len(m[2].split()), m[3]
    regs = {'I6': 40000, 'I7': 39999, 'I5': 17, 'R4': 100000+offset, 'M5': 0}
    memory = {0xb903a: 100299, 0xb903b: 200000}; reads = []; pc = 0x12faf2; flag = 0
    for _ in range(30):
        if pc in (0x12fb0d, 0x12fb36): return reads
        n, text = code[pc]; old = regs.copy(); oldflag = flag; writes = {}; stores = {}; nextpc = pc+n
        def val(v): return old[v] if v in old else int(v, 0)
        def addr(spec):
            if ',' not in spec: return val(spec)
            a, b = spec.split(','); return val(a)+val(b)
        for part in text.split(' || '):
            if part.startswith('COMPU('):
                a, b = part[6:-1].split(','); a, b = val(a), val(b); flag = (a > b)-(a < b); continue
            if part.startswith('IF GT JUMP '):
                if oldflag > 0: nextpc = int(part.split()[-1], 16)
                continue
            dest, expr = part.split('=', 1)
            if dest.startswith('DM('): stores[addr(dest[3:-1])] = val(expr); continue
            if expr.startswith('DM('):
                address = addr(expr[3:-1]); reads.append(address); out = memory.get(address, 0)
            elif expr.startswith('MODIFY('):
                a, b = expr[7:-1].split(','); out = val(a)+val(b)
            elif re.fullmatch(r'\w+-\w+', expr):
                a, b = expr.split('-'); out = val(a)-val(b)
            else: out = val(expr)
            writes[dest] = out
        regs.update(writes); memory.update(stores); pc = nextpc
    raise AssertionError('smoothing prefix did not terminate')


class BoundBScenarioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads((OUT/'b_config_control.json').read_text())
        cls.mode = json.loads((OUT/'b_mode_control.json').read_text())

    def config(self, source='b01', bank=1):
        return retained_config(self.report, source=source, bank=bank)

    def replay(self, bank=1, source='b01', controls=None, slots=None):
        values = {19: 0, 18: 0, 22: 0, 21: 0, 23: 0, 24: 0, 25: 0, 27: 0, 29: 0}
        values.update(controls or {})
        return replay_output_target_slice(self.report['control_bindings_by_bank'][bank-1],
            source=source, effective_values=values,
            contributions=slots or {d: [0]*9 for d in range(44, 59)}, shadow_selectors={16: 2, 20: 1})

    def test_all_eight_retained_banks_bind_inline_counts_and_gains(self):
        for source in ('loader', 'b01'):
            for bank in range(1, 5):
                cfg = self.config(source, bank)
                result = bind_effective_output(effective_config=cfg, library_mode=2, successful_filter_path=True)
                args = result['finish_parameters']
                self.assertEqual(len(args['six_eq']), 2); self.assertEqual(len(args['two_eq']), 2)
                self.assertEqual(len(args['final_eq']), 2 if source == 'loader' else 4)
                for name, offset in [('four_gain', 122), ('six_gain', 293), ('two_gain', 377), ('final_gain', 420)]:
                    self.assertEqual(args[name], cfg[offset])
                self.assertFalse(result['actual_vehicle_state_proven'])

    def test_audio_against_independent_scipy_all_banks(self):
        rng = np.random.default_rng(920)
        six, four, two = [rng.normal(size=(79, n)) for n in (6, 4, 2)]
        def filter_at(x, cfg, count):
            rows = np.array([cfg[count+1+5*i:count+6+5*i] for i in range(round(cfg[count]))])
            sos = np.column_stack((rows[:, :3], np.ones(len(rows)), -rows[:, 3:]))
            return sosfilt(sos, x, axis=0)
        for source in ('loader', 'b01'):
            for bank in range(1, 5):
                cfg = self.config(source, bank)
                model = build_effective_finish(effective_config=cfg, library_mode=5, successful_filter_path=True)
                expected = six*cfg[293]; expected[:, :4] += four*cfg[122]
                expected = np.column_stack((filter_at(expected, cfg, 294), filter_at(two*cfg[377], cfg, 378)))
                expected = filter_at(expected, cfg, 399)*cfg[420]
                assert_allclose(model.process(six, four, two), expected, atol=3e-13, rtol=3e-13)

    def test_streaming_keeps_filter_history(self):
        cfg = self.config(); rng = np.random.default_rng(921)
        inputs = [rng.normal(size=(111, n)) for n in (6, 4, 2)]
        def model(): return build_effective_finish(effective_config=cfg, library_mode=2, successful_filter_path=True)
        a, b = model(), model()
        blocks = np.concatenate([a.process(*(v[:13] for v in inputs)), a.process(*(v[13:] for v in inputs))])
        assert_allclose(blocks, b.process(*inputs), atol=0, rtol=0)

    def test_no_pointer_tail_gain_is_mistaken_for_inline_branch_gain(self):
        cfg = self.config(); cfg[488] = 17; cfg[489] = 19; cfg[490] = 23
        args = bind_effective_output(effective_config=cfg, library_mode=2, successful_filter_path=True)['finish_parameters']
        self.assertEqual(args['four_gain'], 1); self.assertEqual(args['six_gain'], 1)
        self.assertAlmostEqual(args['two_gain'], 1.6)

    def test_pointer_history_is_explicit_and_unknown_consumers_omitted(self):
        pointers = {int(k): v for k, v in self.mode['pointer_maps']['20:default'].items()}
        pointers.update({int(k): v for k, v in self.mode['pointer_maps']['16:default'].items()})
        result = bind_effective_mode(self.mode, effective_config=self.config(), pointers=pointers)
        self.assertEqual(result['unresolved_pointer_words'], [1162, 1163, 1164, 1165])
        self.assertAlmostEqual(result['parameters']['post_crossfeed'], .25)
        with self.assertRaises(ValueError): bind_effective_mode(self.mode, effective_config=self.config(), pointers={})

    def test_replay_gain_and_delay_values_for_all_calibrations(self):
        for bank, wanted in [(1, 3.2), (2, 3.2), (3, 2.5), (4, 1.0)]:
            result = self.replay(bank=bank, controls={18: 9, 27: 180})
            self.assertAlmostEqual(result['branch_gain_targets'][50], wanted, places=5)
            self.assertNotIn(420, result['config_updates'])
            self.assertNotIn('effective_config', result)
            self.assertEqual(result['excluded_replay_descriptors'], [47, 48, 54])
        self.assertEqual(self.replay(controls={19: -50})['two_channel_delays_samples'], [162, 162])
        self.assertEqual(self.replay(bank=2, controls={19: -50})['two_channel_delays_samples'], [140, 140])

    def test_no_silent_zeroing_of_untouched_slot(self):
        slots = {d: [0]*9 for d in range(44, 59)}; slots[51][1] = .25
        saved = copy.deepcopy(slots)
        result = self.replay(slots=slots)
        self.assertEqual(slots, saved)
        self.assertAlmostEqual(result['branch_gain_targets'][51], 1.85)
        self.assertEqual(result['contributions_for_covered_helpers_only'][51][1], .25)

    def test_unimplemented_nonzero_eq_design_fails_closed(self):
        slots = {d: [0]*9 for d in range(44, 59)}; slots[55][8] = 1
        with self.assertRaisesRegex(ValueError, 'zero-gain'): self.replay(slots=slots)

    def test_modes_counts_unstable_filters_and_scope_are_not_guessed(self):
        args = dict(effective_config=self.config(), library_mode=2, successful_filter_path=True)
        for key, value in [('library_mode', 6), ('library_mode', 2.0), ('successful_filter_path', False),
                           ('effective_config', [0]*828)]:
            with self.assertRaises(ValueError): bind_effective_output(**(args | {key: value}))
        for offset, value in [(294, 0), (294, 3), (399, 5), (399, float('nan')), (303, 2)]:
            config = self.config(); config[offset] = value
            with self.assertRaises(ValueError): bind_effective_output(**(args | {'effective_config': config}))

    def test_constructor_does_not_mutate_or_alias_snapshot(self):
        cfg = self.config(); saved = cfg.copy()
        model = build_effective_finish(effective_config=cfg, library_mode=2, successful_filter_path=True)
        cfg[420] = 99
        self.assertEqual(model.final_gain, saved[420])
        self.assertEqual(cfg[:420], saved[:420])

    def test_retained_word_mutation_is_rejected(self):
        report = copy.deepcopy(self.report)
        report['sources']['b01']['banks'][0]['words'][420] ^= 1
        with self.assertRaisesRegex(ValueError, 'fingerprint'):
            retained_config(report, source='b01', bank=1)

    def test_consumer_immediates_match_decoded_listing(self):
        # Independent extraction from instruction text, not this builder's
        # config-offset constants. Existing extraction tests pin code hashes.
        code = {}
        for name in ('first_b_algorithm_dispatch', 'first_b_eight_channel_finish'):
            for line in (OUT/(name+'.asm')).read_text().splitlines():
                match = re.match(r'([0-9a-f]{8})\s+(?:(?:[0-9a-f]{4})\s+)+(\S.*)', line)
                if match: code[int(match[1], 16)] = match[2]
        for pc, register, offset in [(0x131383, 'R1', 294), (0x1313df, 'R2', 378),
                (0x131487, 'R1', 293), (0x1314d3, 'R1', 122), (0x131565, 'R2', 377),
                (0x12b88b, 'R1', 399), (0x12b8e9, 'R2', 420)]:
            self.assertEqual(code[pc], f'{register}={299+offset:#x}')

    def test_all_api21_22_calls_against_decoded_caller(self):
        binding = api21_22_bindings(self.report, source='b01', bank=1)
        for api, row in binding.items():
            expected = [{'descriptor': p['descriptor'], 'slot': row['slot'], 'records': p['record_offsets'],
                         'count': 2, 'clamp': p['clamp']} for p in row['parameters']]
            self.assertEqual(decoded_api_calls(api, row['range_sha256']), expected)

    def test_api21_22_all_banks_against_decoded_curve_kernel(self):
        for source in ('loader', 'b01'):
            for bank in range(1, 5):
                binding = api21_22_bindings(self.report, source=source, bank=bank)
                for api, row in binding.items():
                    for coordinate in (-200, -73, 0, 73, 200):
                        state = {d: [.003*i for i in range(9)] for d in range(44, 59)}
                        got = api21_22_parameter_targets(binding, api=api, coordinate=coordinate, contributions=state)
                        for p in row['parameters']:
                            # Kernel oracle writes slot6; identical initial slots
                            # let us exchange slots6/helper-slot before execution.
                            slots = state[p['descriptor']].copy(); slot = row['slot']
                            slots[6], slots[slot] = slots[slot], slots[6]
                            value, _ = decoded_curve(coordinate, p['records'], p['base'], slots)
                            self.assertAlmostEqual(got['targets'][p['descriptor']], value, places=5)

    def test_composed_all15_scalar_targets_include_final_gain_and_keep_state(self):
        state = {d: [0]*9 for d in range(44, 59)}; state[54][8] = .1
        controls = {19: 40, 18: -6, 22: 73, 21: -73, 23: 0, 24: 0, 25: 0, 27: 0, 29: 0}
        saved = copy.deepcopy(state)
        got = replay_bound_scalar_targets(self.report, source='b01', bank=1, effective_values=controls,
            contributions=state, shadow_selectors={16: 2, 20: 1})
        self.assertEqual(state, saved); self.assertEqual(set(got['targets']), set(range(44, 59)))
        self.assertEqual(got['config_updates'][420], got['targets'][54])
        self.assertEqual(got['final_gain_target'], got['targets'][54])
        self.assertNotIn('effective_config', got); self.assertFalse(got['actual_vehicle_state_proven'])
        # API21 doesn't touch47: its slot3 survives explicitly.
        self.assertEqual(got['contributions'][47][3], 0)

    def test_new_binding_rejects_missing_state_or_outside_coordinate(self):
        binding = api21_22_bindings(self.report, source='b01', bank=1)
        for api, coordinate, slots in [(20, 0, {}), (21, 201, {}), (22, 0, {}), (21, float('nan'), {})]:
            with self.assertRaises(ValueError):
                api21_22_parameter_targets(binding, api=api, coordinate=coordinate, contributions=slots)

    def test_api21_final_gain_real_calibration_examples(self):
        for source in ('loader', 'b01'):
            for bank in range(1, 5):
                binding = api21_22_bindings(self.report, source=source, bank=bank)
                for coordinate in (-9, 0, 9):
                    result = api21_22_parameter_targets(binding, api=21, coordinate=coordinate,
                        contributions={d: [0]*9 for d in range(44, 59)})
                    base = self.config(source, bank)[420]
                    delta = 0
                    if source == 'b01' and bank in (1, 2) and coordinate != 0: delta = -.4
                    if source == 'b01' and bank == 3 and coordinate < 0: delta = -1
                    self.assertAlmostEqual(result['targets'][54], base+delta, places=6)

    def test_new_nonzero_curve_selection_and_slot_replacement(self):
        binding = api21_22_bindings(self.report, source='loader', bank=1)
        changed = copy.deepcopy(binding)
        p = next(p for p in changed[22]['parameters'] if p['descriptor'] == 47)
        p['records'] = [[-100, -2, 1], [100, 2, 2]]
        state = {d: [0]*9 for d in range(44, 59)}; state[47][2] = 1.7; state[47][8] = .1
        result = api21_22_parameter_targets(changed, api=22, coordinate=25, contributions=state)
        self.assertAlmostEqual(result['targets'][47], 2.1)
        self.assertEqual(result['contributions'][47][8], .1)
        again = api21_22_parameter_targets(changed, api=22, coordinate=25, contributions=result['contributions'])
        self.assertEqual(again, result)

    def test_unresolved_pointer_pointees_are_known_delay_curve_records(self):
        # Classify destination records, NOT an indirect consumer/read of these
        # pointer words. Effective pointer map and target/shadow control lookup
        # are independently recovered domains and may disagree at runtime.
        variants = [('16:default', 'default'), ('20:default', 'default'),
                    ('16:5', 'shadow16_eq5'), ('20:3', 'shadow20_eq3')]
        for binding in self.report['control_bindings_by_bank']:
            params = {p['descriptor']: p for p in binding['parameter_controls']['apis'][0]['parameters']}
            for mode_name, variant in variants:
                record_offsets = [params[d]['variants'][variant]['record_offsets'][side]
                                  for side in (0, 1) for d in (52, 53)]
                pointers = self.mode['pointer_maps'][mode_name]
                self.assertEqual([pointers[str(p)] for p in range(1162, 1166)], record_offsets)


class PointerAliasAuditTests(unittest.TestCase):
    def analyze(self, *instructions):
        return analyze_instructions([(0x100+i, 1, text, None) for i, text in enumerate(instructions)])

    def test_computed_offset_and_postincrement_find_hidden_field_read(self):
        result = self.analyze('I5=R4', 'I5=MODIFY(I5,+1160)',
            'R1=DM(I5,M6)', 'R2=DM(I5,M6)', 'R3=DM(I5,M6)')
        self.assertEqual([r['address']['offset'] for r in result['reads']], [1160, 1161, 1162])
        self.assertTrue(any(r['address']['offset'] == 1162 for r in result['candidate_table_relative_reads']))

    def test_integer_add_and_stack_spill_recover_alias(self):
        result = self.analyze('R2=0x48a', 'R3=R4+R2', 'DM(-2,I6)=R3', 'R3=R8',
                              'I5=DM(-2,I6)', 'R0=DM(M5,I5)')
        self.assertEqual(result['reads'][-1]['address']['offset'], 1162)
        self.assertEqual(result['reads'][-1]['address']['base'], 'block:100:R4')

    def test_parallel_uses_old_value_and_conditional_update_is_unknown(self):
        result = self.analyze('I5=R4', 'I5=MODIFY(I5,+1162)',
                              'I5=R8 || R0=DM(M5,I5)', 'IF EQ I5=MODIFY(I5,+2)', 'R1=DM(M5,I5)')
        self.assertEqual(result['reads'][0]['address']['offset'], 1162)
        self.assertIsNone(result['reads'][1]['address'])

    def test_conditional_postincrement_cannot_be_treated_unconditionally(self):
        result = self.analyze('I5=R4', 'IF EQ R0=DM(I5,M6)', 'R1=DM(M5,I5)')
        self.assertIsNone(result['reads'][-1]['address'])

    def test_call_cut_does_not_claim_register_survival(self):
        result = self.analyze('I5=R4', 'I5=MODIFY(I5,+1162)', 'CJUMP 0x200 (DB)',
                              'NOP', 'NOP', 'R0=DM(M5,I5)')
        self.assertEqual(result['reads'][-1]['address']['offset'], 0)
        self.assertEqual(len(result['cuts']), 1)
        self.assertFalse(result['dead_field_proven'])

    def test_dynamic_index_remains_an_explicit_unresolved_read(self):
        result = self.analyze('I5=R4', 'R0=DM(M4,I5)')
        self.assertEqual(len(result['non_affine_reads']), 1)
        self.assertEqual(result['unresolved_pointer_consumers'], [1162, 1163, 1164, 1165])

    def test_full_image_audit_and_mutation_rejection(self):
        loader = (OUT.parent/'J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        result = audit_pointer_aliases(loader)
        self.assertEqual(result['instructions'], 16093)
        self.assertEqual(len(result['candidate_table_relative_reads']), 14)
        self.assertFalse(result['dead_field_proven'])
        changed = bytearray(loader); changed[-1] ^= 1
        with self.assertRaises(ValueError): audit_pointer_aliases(bytes(changed))

    def test_filter_loop_address_domain_against_decoded_instructions(self):
        for count in range(1, 9):
            expected = [698+5*section+i for section in range(count) for i in (5, 4, 3, 2, 1)]
            self.assertEqual(decoded_filter_smoothing_addresses(count, 698), expected)
        # No pretend upper bound: an invalidly enlarged count can reach the
        # pointer cells. This does not claim such state occurs in the vehicle.
        self.assertIn(1162, decoded_filter_smoothing_addresses(93, 698))

    def test_smoothing_retained_domains_and_callsite_closure(self):
        loader = (OUT.parent/'J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        report = audit_pointer_aliases(loader)
        configs = json.loads((OUT/'b_config_control.json').read_text())
        modes = json.loads((OUT/'b_mode_control.json').read_text())
        result = smoothing_caller_domains(report, configs, modes)
        self.assertFalse(result['helper_has_upper_bound'])
        self.assertTrue(result['conditional_smoothing_path_excludes_1162_1165'])
        self.assertEqual(len(result['scenarios']), 8)
        for s in result['scenarios']:
            self.assertEqual(s['argument_offsets_min_max'], [97, 807])
            self.assertEqual(s['pointer_cell_collisions'], [])
        changed = copy.deepcopy(report)
        changed['call_arguments'].append({'pc': '0x123456', 'target': '0x12faf2'})
        with self.assertRaises(ValueError): smoothing_caller_domains(changed, configs, modes)

    def test_synthetic_mutated_filter_count_invalidates_exclusion(self):
        loader = (OUT.parent/'J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        report = audit_pointer_aliases(loader)
        configs = json.loads((OUT/'b_config_control.json').read_text())
        modes = json.loads((OUT/'b_mode_control.json').read_text())
        bank = configs['sources']['b01']['banks'][0]
        bank['words'][399] = struct.unpack('<I', struct.pack('<f', 93))[0]
        bank['sha256'] = hashlib.sha256(struct.pack('<829I', *bank['words'])).hexdigest()
        result = smoothing_caller_domains(report, configs, modes)
        self.assertFalse(result['conditional_smoothing_path_excludes_1162_1165'])
        self.assertEqual(result['scenarios'][4]['pointer_cell_collisions'], [1162, 1163])

    def test_decoded_smoothing_lower_bound_does_not_check_upper_boundary(self):
        self.assertEqual(decoded_smoothing_prefix(298), [0xb903a])
        for offset in (299, 1127, 1128, 1162, 1165):
            self.assertEqual(decoded_smoothing_prefix(offset)[-2:], [100000+offset, 200000+offset-299])


if __name__ == '__main__': unittest.main()

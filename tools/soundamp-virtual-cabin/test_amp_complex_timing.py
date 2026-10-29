import json
from pathlib import Path
import tempfile
import unittest

import numpy as np

from amp_complex_modes import CalibrationNoiseCore, CalibrationSequence
from amp_complex_timing import (CalibrationShapedNoise, CalibrationExcitationReference,
                                load_builtin_records, timing_inventory, second_b_binding, ROOT)


CORE = dict(seed=1, shuffle=list(range(32)), previous=0, alusat=False)
RECORDS = np.array([[.6,-.2,.3,.1,-.05],[.2,-.1,.8,.2,.03]])


def noise(kind=9, state=None, records=RECORDS):
    return CalibrationShapedNoise(noise_type=kind, core_state=CORE,
        builtin_records=records, custom_records=records,
        initial_filter_state=np.zeros((2,2)) if state is None else state)


def scalar_cascade(x, records, initial):
    """Independent literal scalar recurrence, not scipy/biquad_sos."""
    result=x.copy();state=initial.copy()
    for section, (feedback1,feedback2,b0,b1,b2) in enumerate(records):
        z1,z2=state[section]
        for i,value in enumerate(result):
            out=b0*value+z1
            z1,z2=b1*value+feedback1*out+z2,b2*value+feedback2*out
            result[i]=out
        state[section]=z1,z2
    return result,state


class ComplexTimingTests(unittest.TestCase):
    def test_two_cascaded_sections_match_independent_scalar_with_nonzero_history(self):
        initial=np.array([[.2,-.1],[-.3,.15]])
        reference=CalibrationNoiseCore(**CORE).process(1100)
        expected,state=scalar_cascade(reference, RECORDS, initial)
        shaped=noise(state=initial)
        np.testing.assert_allclose(shaped.process(1100),expected,atol=1e-15)
        np.testing.assert_allclose(shaped.state,state,atol=1e-15)

    def test_block_partition_preserves_both_noise_and_filter_state(self):
        a=noise();b=noise()
        expected=a.process(2048)
        actual=np.concatenate([b.process(n) for n in (0,1,63,64,7,1913)])
        np.testing.assert_array_equal(actual,expected)
        np.testing.assert_array_equal(a.state,b.state)
        self.assertEqual(a.core.shuffle,b.core.shuffle)

    def test_raw_modes_ignore_filter_records_without_altering_history(self):
        for kind in (0,2,8,10,0xffffffff):
            initial=np.ones((2,2))
            a=noise(kind,initial,records=None)
            np.testing.assert_array_equal(a.process(65),CalibrationNoiseCore(**CORE).process(65))
            np.testing.assert_array_equal(a.state,initial)

    def test_type1_selects_builtins_not_type9_records(self):
        a=CalibrationShapedNoise(noise_type=1, core_state=CORE,builtin_records=RECORDS,
            custom_records=np.zeros((2,5)),initial_filter_state=np.zeros((2,2)))
        np.testing.assert_array_equal(a.process(64),noise(9).process(64))

    def test_required_parameters_reject_unproved_shaping_state(self):
        for kind in (1,9):
            with self.assertRaises(ValueError):noise(kind,records=None)
        with self.assertRaises(ValueError):noise(state=np.zeros(4))
        with self.assertRaises(ValueError):noise(kind=-1)

    def test_configure_wait_measure_and_completion_audio_timing(self):
        a=CalibrationExcitationReference(sequence=CalibrationSequence(1,64,64),
                                          generator=noise(),excitation_gain=.3)
        seed=a.generator.core.seed
        for reason in ('configure','wait'):
            r=a.process_block()
            self.assertEqual(r['sequence']['reason'],reason)
            np.testing.assert_array_equal(r['audio'],0)
            self.assertEqual(a.generator.core.seed,seed)
        expected=noise().process(128)*.3
        # One measurement block AND one completion fall-through block.
        first=a.process_block();last=a.process_block()
        np.testing.assert_array_equal(np.r_[first['audio'][:,0],last['audio'][:,0]],expected)
        np.testing.assert_array_equal(first['audio'][:,1:],0)
        self.assertTrue(a.sequence.complete)
        done_seed=a.generator.core.seed
        r=a.process_block()
        self.assertEqual(r['sequence']['reason'],'complete')
        np.testing.assert_array_equal(r['audio'],0)
        self.assertEqual(a.generator.core.seed,done_seed)

    def test_gate_channel_nine_and_zero_gain_do_not_invent_extra_delay(self):
        for gain in (0.,-.5):
            seq=CalibrationSequence(512,0,64,phase=2,selected_bit=512,active_gates=512)
            a=CalibrationExcitationReference(sequence=seq,generator=noise(),excitation_gain=gain)
            r=a.process_block()['audio']
            np.testing.assert_array_equal(r[:,:9],0)
            np.testing.assert_array_equal(r[:,9],noise().process(64)*gain)
            self.assertNotEqual(a.generator.core.seed,CORE['seed'])

    def test_loader_pinning_and_builtin_recurrence(self):
        records=load_builtin_records()
        self.assertEqual(records.shape,(2,5))
        self.assertEqual(records[0,0],float.fromhex('0x1.7d0a2ap-3'))
        expected,_=scalar_cascade(CalibrationNoiseCore(**CORE).process(512),records,np.zeros((2,2)))
        np.testing.assert_allclose(noise(1,records=records).process(512),expected,atol=1e-15)
        with tempfile.TemporaryDirectory() as tmp:
            bad=Path(tmp)/'bad-loader';bad.write_bytes(b'not the audited loader')
            with self.assertRaises(ValueError):load_builtin_records(bad)

    def test_inventory_separates_delay_cadence_and_no_write(self):
        audit=json.loads((ROOT/'output/r2/soundamp/remaining-blocks-20260919/audit.json').read_text())
        result=timing_inventory(audit)
        for p in result['profiles']:
            self.assertEqual(p['full_mode_pair_delay_samples'],[1023]*5)
            self.assertEqual(p['target_update_interval_audio_samples'],1024)
        for mode in result['modes_after_enable_and_binding']:
            self.assertEqual(mode['stage8_delay_active'],bool(mode['mode_word']&4))
        self.assertIn('no output write',result['master_disabled_or_first_binding'])
        self.assertFalse(result['live_state_proven'])

    def test_second_b_schedule_reuse_does_not_create_music_edge(self):
        binding=json.loads((ROOT/'output/r2/soundamp/output_binding_audit.json').read_text())
        result=second_b_binding(binding)
        self.assertEqual(result['source_roots'],[8,9,10])
        self.assertEqual(result['slot_possible_input_roots'],[[8,9,10]]*16)
        self.assertEqual(result['stored_nonzero_b_slots'],[1,3,5,7,12,13])
        self.assertEqual(result['ordered_b_ports'],[27,26,25,24,23,22,21,36,37,50,51,52,53,54,55,73])
        binding['graph_nodes'][55]['port_ids'][16]=56
        with self.assertRaises(ValueError):second_b_binding(binding)


if __name__ == '__main__':unittest.main()

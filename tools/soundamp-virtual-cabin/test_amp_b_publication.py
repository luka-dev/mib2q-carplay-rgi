import copy
import json
import unittest

import numpy as np
from numpy.testing import assert_array_equal,assert_allclose

from amp_b_bound_scenario import retained_config
from amp_b_publication import (ROOT,BConfigPublicationReference,initial_interpolation_steps,
                               publication_evidence,replay_retained_bank_publication)


class PublicationTests(unittest.TestCase):
    def state(self):
        vector=np.zeros(829);vector[2]=4
        return BConfigPublicationReference(effective_config=vector,target_config=vector,
                   steps=initial_interpolation_steps(assume_successful_initialization=True))

    def test_instruction_mask_is_not_all829_ramping(self):
        expected=[35,79,122,147,198,249,293,316,317,377,420,*range(295,305),*range(379,389)]
        self.assertEqual(publication_evidence()['initial_interpolated_config_offsets'],expected)
        steps=initial_interpolation_steps(assume_successful_initialization=True)
        self.assertEqual(np.count_nonzero(steps==0),31)
        assert_array_equal(steps[expected],0)
        self.assertEqual(steps[3],1) # spatial coefficients are immediate copies.

    def test_publication_flag_equality_not_bit_test_and_steps_preserved_copy(self):
        target=np.full(829,2.);target[2]=4
        a=self.state();prior=a.steps.copy();a.publish(target,publication_flag=1)
        assert_array_equal(a.effective,target);assert_array_equal(a.steps,prior)
        b=self.state();b.publish(target,publication_flag=3)
        self.assertEqual(b.effective[122],0);self.assertEqual(b.steps[122],.5)
        self.assertEqual(b.effective[123],2);self.assertEqual(b.steps[123],1)
        self.assertFalse(b.snapshot()['all_current_words_equal_target'])

    def test_explicit_calls_preserve_order_duplicate_access_and_unvisited_fields(self):
        state=self.state();target=state.target.copy();target[[122,293]]=[2,-2]
        state.publish(target,publication_flag=0)
        state.advance_offsets([122,122,-202,293])
        self.assertEqual(state.effective[122],1)
        self.assertEqual(state.effective[293],-.5)
        self.assertEqual(state.effective[420],0)
        state.advance_offsets([122,122]);self.assertEqual(state.effective[122],2)
        self.assertEqual(state.steps[122],0)
        self.assertEqual(state.effective[293],-.5) # no invented frame tick.

    def test_duration_is_truncated_and_zero_maps_one(self):
        for duration,expected in [(0,1),(.99,1),(1.99,1),(4.99,4)]:
            state=self.state();target=state.target.copy();target[2]=duration;target[122]=2
            state.publish(target,publication_flag=0)
            self.assertEqual(state.duration,expected)
            self.assertEqual(state.steps[122],2/expected)

    def test_step_one_collision_and_republish_recompute_from_current(self):
        state=self.state();target=state.target.copy();target[122]=4
        state.publish(target,publication_flag=0)
        self.assertEqual(state.steps[122],1)
        state.advance_offsets([122]);self.assertEqual(state.effective[122],1)
        target[122]=8;state.publish(target,publication_flag=0)
        self.assertEqual(state.effective[122],8) # literal previous step==1 sentinel.
        self.assertEqual(state.steps[122],1)
        target[293]=2;state.publish(target,publication_flag=0);state.advance_offsets([293])
        target[293]=-1;state.publish(target,publication_flag=0)
        self.assertEqual(state.effective[293],.5);self.assertEqual(state.steps[293],-.375)

    def test_snap_equality_preserves_existing_step_and_zero_step_can_stall(self):
        state=self.state();state.target[122]=.25;state.steps[122]=.5
        state.advance_offsets([122]);self.assertEqual(state.effective[122],.25)
        self.assertEqual(state.steps[122],0)
        state.steps[122]=.8;state.advance_offsets([122])
        self.assertEqual(state.steps[122],.8) # equal-target earlyreturn before clearing.
        state.target[293]=10;state.steps[293]=0
        state.advance_offsets([293]*100);self.assertEqual(state.effective[293],0)

    def test_replay_whole_bank_immediate_publication_and_untouched_words(self):
        report=json.loads((ROOT/'assets/soundamp/remaining-blocks-20260920-v3/b_config_control.json').read_text())
        controls={api:0. for api in (19,18,22,21,23,24,25,27,29)}
        controls[19]=-50
        contributions={d:[0.]*9 for d in range(44,59)};contributions[51][1]=.25
        original=copy.deepcopy(contributions)
        current=retained_config(report,source='loader',bank=1)
        for bank,delay in [(1,162),(2,140)]:
            result=replay_retained_bank_publication(report,source='b01',bank=bank,
                effective_values=controls,contributions=contributions,shadow_selectors={16:2,20:1},
                current_effective=current,prior_steps=initial_interpolation_steps(assume_successful_initialization=True),
                publication_flag=1,assume_successful_validated_bank=True)
            self.assertTrue(result['all_current_words_equal_target'])
            assert_array_equal(result['effective_config'],result['target_config'])
            self.assertEqual(round(result['effective_config'][316]),delay)
            self.assertEqual(result['contributions'][51][1],.25)
            retained=retained_config(report,source='b01',bank=bank)
            changed=set(result['metadata']['updated_config_offsets'])
            untouched=[i for i in range(829) if i not in changed]
            assert_array_equal(np.asarray(result['effective_config'])[untouched],np.asarray(retained)[untouched])
            self.assertFalse(result['metadata']['actual_vehicle_state_proven'])
        self.assertEqual(contributions,original)

    def test_mutation_free_failclosed_and_snapshot_copy(self):
        state=self.state();before=state.snapshot()
        for flag in (-1,True,2**32):
            with self.assertRaises(ValueError):state.publish(state.target,publication_flag=flag)
        for sequence in ([829],[1.],[122,829]):
            with self.assertRaises(ValueError):state.advance_offsets(sequence)
        target=state.target.copy();target[2]=-1
        with self.assertRaises(ValueError):state.publish(target,publication_flag=0)
        self.assertEqual(state.snapshot(),before)
        snapshot=state.snapshot();snapshot['effective_config'][122]=99
        self.assertEqual(state.effective[122],0)
        with self.assertRaises(ValueError):initial_interpolation_steps(assume_successful_initialization=False)


if __name__=='__main__':unittest.main()

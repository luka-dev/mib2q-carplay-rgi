import copy
import json
import unittest

import numpy as np
from numpy.testing import assert_array_equal

from amp_b_bound_scenario import retained_config
from amp_b_mode5_schedule import ROOT,mode5_frame_schedule,advance_mode5_frame
from amp_b_publication import BConfigPublicationReference,initial_interpolation_steps
from test_amp_b_bound_scenario import decoded_filter_smoothing_addresses


class Mode5ScheduleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        d=ROOT/'output/r2/soundamp/remaining-blocks-20260920-v3'
        cls.report=json.loads((d/'b_config_control.json').read_text())
        cls.maps=json.loads((d/'b_mode_control.json').read_text())['pointer_maps']

    def args(self,bank=1,variant='20:default'):
        return dict(effective_config=retained_config(self.report,source='b01',bank=bank),
            pointers={int(k):v for name in (variant,'16:5') for k,v in self.maps[name].items()},
            assume_successful_active_path=True)

    def test_all31_visited_once_and_actual_nested_call_order(self):
        s=mode5_frame_schedule(**self.args());e=s['events']
        self.assertEqual(len(e),313)
        self.assertEqual(set(s['metadata']['initial31_interpolated_visits'].values()),{1})
        self.assertEqual(s['offsets'][:5],[-202,492,316,317,318])
        scalar=[v['consumer'] for v in e if '.inline_gain' in v['consumer']]
        self.assertEqual(scalar,['two.inline_gain','six.inline_gain','four.inline_gain'])
        self.assertEqual(e[-1]['consumer'],'finish.final_gain')
        final=[v for v in e if v['consumer'].startswith('finish.')]
        self.assertEqual([v['offset'] for v in final[:10]],
                         [299,298,297,296,295,304,303,302,301,300])

    def test_eq_offsets_against_independent_decoded_register_loop(self):
        args=self.args();s=mode5_frame_schedule(**args)
        for name,countoffset in [('two.eq',319),('six.pair145.eq',148),
            ('six.pair196.eq',199),('six.pair247.eq',250),('four.pair33.eq',36),
            ('four.pair77.eq',80),('finish.six_eq',294),('finish.two_eq',378),('finish.all_eq',399)]:
            actual=[v['offset'] for v in s['events'] if v['consumer']==name]
            # Existing oracle executes decoded12DC79..12DCD1, not this builder.
            expected=decoded_filter_smoothing_addresses(int(args['effective_config'][countoffset]),countoffset)
            self.assertEqual(actual,expected)

    def test_retained_banks_and_pointer_variants_have_successful_coverage(self):
        for bank in range(1,5):
            for variant in ('20:default','20:3'):
                s=mode5_frame_schedule(**self.args(bank,variant))
                self.assertEqual(set(s['metadata']['initial31_interpolated_visits'].values()),{1})
                self.assertEqual(s['offsets'][1],492) #16:5 applied last.
                self.assertFalse(s['metadata']['actual_vehicle_state_proven'])

    def test_advancement_one_per_callback_not_channels_or_samples(self):
        a=self.args();cfg=a['effective_config'];target=cfg.copy();target[2]=4
        target[122]=cfg[122]+.4;target[377]=cfg[377]+.8;target[316]=cfg[316]+4
        state=BConfigPublicationReference(effective_config=cfg,target_config=cfg,
            steps=initial_interpolation_steps(assume_successful_initialization=True))
        state.publish(target,publication_flag=0)
        initial=state.effective.copy();steps=state.steps.copy()
        result=advance_mode5_frame(state,pointers=a['pointers'],assume_successful_active_path=True)
        for offset in (122,377,316):
            self.assertEqual(state.effective[offset],float(np.float32(initial[offset]+steps[offset])))
            events=[e for e in result['accesses'] if e['offset']==offset]
            self.assertEqual(len(events),1)
            self.assertEqual(events[0]['before'],initial[offset])
            self.assertEqual(events[0]['after'],state.effective[offset])
        self.assertFalse(result['all_current_words_equal_target'])

    def test_identical_values_stay_bitwise_and_no_history_mutation(self):
        a=self.args();cfg=a['effective_config'];before=copy.deepcopy(a)
        state=BConfigPublicationReference(effective_config=cfg,target_config=cfg,
            steps=initial_interpolation_steps(assume_successful_initialization=True))
        prior=state.snapshot()
        advance_mode5_frame(state,pointers=a['pointers'],assume_successful_active_path=True)
        self.assertEqual(state.snapshot(),prior);self.assertEqual(a,before)

    def test_missing_path_premise_or_inactive_filter_rejected(self):
        a=self.args();a['assume_successful_active_path']=False
        with self.assertRaises(ValueError):mode5_frame_schedule(**a)
        a=self.args();a['effective_config'][319]=0
        with self.assertRaises(ValueError):mode5_frame_schedule(**a)


if __name__=='__main__':unittest.main()

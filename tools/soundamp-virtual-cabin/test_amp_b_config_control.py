import copy
import json
import unittest
from amp_b_control import b_config_request,replay_branch_gain_targets,branch_gain_targets,api18_19_parameter_targets
from test_amp_b_control import OUT


class BConfigReferenceTests(unittest.TestCase):
    def setUp(self):
        self.report=json.loads((OUT/'b_config_control.json').read_text())

    def test_all_shipped_selector_entries(self):
        table=self.report['v850_selector']['table']
        for b in range(2):
            for a in range(21):
                r=b_config_request(table,state_a=a,state_b=b)
                self.assertEqual(r['api17_candidate'],a if a in (1,2) else 0)
                self.assertEqual(r['api17_accepted'],a in (1,2))

    def test_selector_domains_are_explicit(self):
        for a,b in [(21,0),(1,2),(-1,0),(1.0,0)]:
            with self.assertRaises(ValueError):b_config_request(self.report['v850_selector']['table'],state_a=a,state_b=b)

    def test_bank2_api19_changes_delay_and_two_gain(self):
        binding=self.report['control_bindings_by_bank'][1]
        for x,delay in [(-50,140),(0,96),(100,42)]:
            r=api18_19_parameter_targets(binding['parameter_controls'],source='b01',api=19,coordinate=x,
                contributions={d:[0]*9 for d in range(44,59)},shadow_selectors={16:5,20:3})
            self.assertEqual(r['targets'][52],delay)
            if x==-50:self.assertAlmostEqual(r['targets'][51],1.45,places=6)

    def test_profile_specific_replay_does_not_assume_first_bank(self):
        targets=[]
        for binding in self.report['control_bindings_by_bank']:
            coords={s['api']:0 for s in binding['slots']};coords[27]=180;coords[18]=9
            r=replay_branch_gain_targets(binding,source='b01',effective_values=coords,
                                         contributions={d:[0]*9 for d in (46,50,51)})
            targets.append(r['targets'][50])
        for a,b in zip(targets,[3.2,3.2,2.5,1]):self.assertAlmostEqual(a,b,places=6)

    def test_replay_uses_raw_api25_not_baseline_adjusted(self):
        binding=copy.deepcopy(self.report['control_bindings_by_bank'][0])
        # Synthetic nonzero level curve exposes the caller difference; all
        # shipped banks have zero baseline/curves, so no audible bug is claimed.
        branch=binding['slots'][6]['branches'][0]
        for point,values in zip(branch['records'],[(0,0,0),(100,1,0)]):
            for word,value in zip(point,values):word['b01']=value
        effective={s['api']:0 for s in binding['slots']};effective[25]=100
        state={d:[0]*9 for d in (46,50,51)}
        replay=replay_branch_gain_targets(binding,source='b01',effective_values=effective,contributions=state)
        adjusted=dict(effective);adjusted[25]-=100
        single=branch_gain_targets(binding,source='b01',coordinates=adjusted,contributions=state)
        self.assertEqual(replay['targets'][46],2);self.assertEqual(single['targets'][46],1)

    def test_replay_retains_untouched_slot51_1(self):
        binding=self.report['control_bindings_by_bank'][1]
        state={d:[0]*9 for d in (46,50,51)};state[51][1]=.3
        r=replay_branch_gain_targets(binding,source='b01',effective_values={s['api']:0 for s in binding['slots']},contributions=state)
        self.assertEqual(r['contributions'][51][1],.3)
        self.assertAlmostEqual(r['targets'][51],1.9,places=6)


if __name__=='__main__':unittest.main()

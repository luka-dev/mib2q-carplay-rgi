import copy
import unittest

import numpy as np

from amp_second_b_control import load_control_inventory, target_overlay, MASTER, FANOUT
from amp_second_b import SecondBAuxReference, load_static_inventory
from test_amp_second_b import synthetic_scenario


class SecondBControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.parameters = load_control_inventory()

    def overlay(self, **overrides):
        args = dict(source_levels=[0,0,0], common_addition=0, table1_positions=[0,0,0],
                    table2_positions=[0,0,0], sub_enabled=True, sub_position=0,
                    scalar_filter_selector=0, assume_loader_input_eq_counts=False)
        args.update(overrides)
        return target_overlay(self.parameters,**args)

    def test_input_eq_holes_are_skipped_without_packed_gap(self):
        records = self.parameters['input_eq']['records']
        self.assertEqual([r['design_index'] for r in records],list(range(12))+list(range(16,20)))
        self.assertEqual([int(r['target'],16) for r in records],list(range(0xE05C0,0xE0610,5)))
        self.assertEqual(self.parameters['input_eq']['loader_counts'],[8,4,4])
        self.assertEqual(len(self.overlay()['stages']['0xc4b96']['records']),16)
        self.assertNotIn('counts',self.overlay()['stages']['0xc4b96'])
        self.assertEqual(self.overlay(assume_loader_input_eq_counts=True)['stages']['0xc4b96']['counts'],[8,4,4])

    def test_stored_gain_delay_and_scalar_targets_exact(self):
        stages=self.overlay()['stages']
        self.assertEqual(stages['0xc4b7f']['target'],[1.,1.,1.])
        self.assertEqual(stages['0xc4cf9']['samples'],[67,57,0,57,67,57,0,57,96,96,0,0,0,0,0,0])
        self.assertEqual(stages['0xc4c2b']['gain'],0.4466835856437683)
        self.assertNotIn('history',stages['0xc4cf9'])
        self.assertNotIn('current',stages['0xc4b7f'])

    def test_master_target_formula_and_clamping(self):
        neutral=self.overlay()['target_tenths_db']
        self.assertEqual([neutral[d][0] for d in MASTER],[-34,-157,-157])
        adjusted=self.overlay(source_levels=[1,2,3],common_addition=7)['target_tenths_db']
        self.assertEqual([adjusted[d][0] for d in MASTER],[-17,-130,-120])
        high=self.overlay(source_levels=[127]*3,common_addition=255)
        low=self.overlay(source_levels=[-128]*3)
        for d in MASTER:
            self.assertEqual(high['stages'][d]['target'],[100.])
            self.assertEqual(low['stages'][d]['target'],[0.])

    def test_fanout_neutral_and_sub_exact_mute(self):
        neutral=self.overlay(); muted=self.overlay(sub_enabled=False)
        for d in FANOUT:
            self.assertEqual(neutral['target_tenths_db'][d],[0]*16)
            self.assertEqual(muted['target_tenths_db'][d],([0]*14)+[-1000,-1000])
            self.assertEqual(muted['stages'][d]['target'][14:],[0.,0.])

    def test_fanout_source_specific_table_indices(self):
        p1,p2=[-9,0,9],[9,-3,2]
        o=self.overlay(table1_positions=p1,table2_positions=p2,sub_position=4)
        g=self.parameters['gain_control']
        for i,d in enumerate(FANOUT):
            expected=[]
            for row in g['slot_to_table_row']:
                v=g['table1'][row][9+p1[i]]+g['table2'][row][9+p2[i]]
                if row==3:v+=g['sub_table'][13]
                expected.append(max(-1000,min(400,v)))
            self.assertEqual(o['target_tenths_db'][d],expected)

    def test_sidefilter_explicit_selector(self):
        bypass=self.overlay()['stages']['0xc4c24']['record']
        self.assertEqual(bypass,[0.,0.,1.,0.,0.])
        self.assertEqual(self.overlay(scalar_filter_selector=255)['stages']['0xc4c24']['record'],bypass)
        for selector in (1,2):
            self.assertNotEqual(self.overlay(scalar_filter_selector=selector)['stages']['0xc4c24']['record'],bypass)

    def test_mode_writes_are_not_populated_as_effective_state(self):
        writes=self.parameters['requested_gain_control_writes']
        self.assertEqual(writes['master']['mode'],1)
        self.assertEqual(writes['fanout']['mode'],3)
        self.assertEqual(writes['fanout']['multiplicative_rates'],
                         [[1.1659144163131714,0.8576958775520325]]*16)
        for desc in MASTER+FANOUT:
            self.assertNotIn('mode',self.overlay()['stages'][desc])

    def test_partial_overlay_requires_explicit_effective_state(self):
        inv=load_static_inventory(); overlay=self.overlay(assume_loader_input_eq_counts=True)
        with self.assertRaises(ValueError):
            SecondBAuxReference(scenario={'label':'Not complete', 'stages':overlay['stages']},inventory=inv)
        scenario=synthetic_scenario(inv)
        for d,p in overlay['stages'].items():scenario['stages'][d].update(copy.deepcopy(p))
        for d in ('0xc4b96','0xc4ce8'):
            scenario['stages'][d]['history']=np.zeros((sum(scenario['stages'][d]['counts']),2)).tolist()
        scenario['stages']['0xc4cf9']['history']=[([0.]*n) for n in scenario['stages']['0xc4cf9']['samples']]
        scenario['label']='Synthetic initial effective state plus explicit stored/control target hypothesis'
        y=SecondBAuxReference(scenario=scenario,inventory=inv).process(np.ones((256,3))*.001)
        self.assertTrue(np.isfinite(y).all())
        self.assertFalse(overlay['effective_state_populated'])
        self.assertFalse(overlay['live_state_proven'])

    def test_out_of_range_or_implicit_controls_rejected(self):
        for kwargs in ({'source_levels':[128,0,0]},{'common_addition':-1},
                       {'table1_positions':[10,0,0]},{'sub_enabled':1},
                       {'sub_position':True},{'scalar_filter_selector':256},
                       {'assume_loader_input_eq_counts':1}):
            with self.assertRaises(ValueError):self.overlay(**kwargs)


if __name__=='__main__':unittest.main()

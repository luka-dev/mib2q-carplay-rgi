"""Synthetic explicit-state fixtures; never factory or current car settings."""
import copy
import unittest

import numpy as np

from amp_second_b import SecondBAuxReference, load_static_inventory, mode1_gain_update


def synthetic_scenario(inventory):
    stages = {}
    for node in inventory['nodes']:
        h = node['process_visa_address_candidate']; n = node['parameters'].get('channel_count')
        if h == '0x12879c':
            p = {'current':[1.]*n, 'target':[1.]*n, 'mode':0, 'rates':[[1.,1.]]*n}
        elif h == '0x126559':
            p = {'records':[[0.,0.,1.,0.,0.]], 'history':np.zeros((3,1,2)).tolist()}
        elif h == '0x126a95': p = {'counts':[0]*n, 'records':[], 'history':[]}
        elif h == '0x126092': p = {'record':[0.,0.,1.,0.,0.], 'history':[0.,0.]}
        elif h == '0x1269bc': p = {'coefficients':[1.,1.,1.]}
        elif h == '0x127a4d': p = {'gain':1.}
        elif h == '0x124517': p = {'samples':[0]*16, 'history':[[] for _ in range(16)], 'lifecycle':'settled_dma'}
        else: raise AssertionError(h)
        stages[node['descriptor']] = p
    return {'label':'SYNTHETIC UNIT TEST, not live/stock', 'stages':stages}


class SecondBTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.inventory = load_static_inventory()

    def ref(self, scenario=None):
        return SecondBAuxReference(scenario=synthetic_scenario(self.inventory) if scenario is None else scenario,
                                   inventory=self.inventory)

    def test_inventory_direct_loader_ports_and_partial_calibration(self):
        a = self.inventory
        self.assertEqual(len(a['nodes']),30)
        self.assertEqual(a['boundary']['source_roots'],[8,9,10])
        self.assertEqual(a['boundary']['ordered_b_ports'],[27,26,25,24,23,22,21,36,37,50,51,52,53,54,55,73])
        targets = a['stored_writer_targets']
        self.assertEqual(sum(targets['0xc4ce8']['counts']),35)
        self.assertEqual(len(targets['0xc4ce8']['records']),35)
        self.assertEqual(targets['0xc4b70']['records'],[[0.,0.,1.,0.,0.]])
        self.assertNotIn('0xc4b7f',targets)
        self.assertNotIn('0xc4cf9',targets)
        self.assertEqual(a['nodes'][-1]['parameters']['capacity_samples'],2880)

    def test_three_separate_sources_and_fanout_order(self):
        scenario = synthetic_scenario(self.inventory)
        s = scenario['stages']
        for desc,gain in [('0xc4ba7',2.),('0xc4bd5',3.),('0xc4c03',4.)]: s[desc]['target'] = [gain]
        a,b,c = np.arange(1,17),np.arange(17,33),np.arange(33,49)
        for desc,g in zip(('0xc4bbe','0xc4bec','0xc4c31'),(a,b,c)): s[desc]['target'] = g.tolist()
        x = np.random.default_rng(1).normal(size=(137,3))
        expected = 2*x[:,0,None]*a+3*x[:,1,None]*b+4*x[:,2,None]*c
        np.testing.assert_allclose(self.ref(scenario).process(x),expected,atol=1e-12)

    def test_dead_scalar_branch_does_not_leak_into_output(self):
        scenario = synthetic_scenario(self.inventory)
        scenario['stages']['0xc4c2b']['gain'] = -99.
        x = np.ones((64,3))
        trace = self.ref(scenario).process(x,capture_stages=True)
        np.testing.assert_array_equal(trace['out'],np.full((64,16),3.))
        np.testing.assert_array_equal(trace['stages']['0xc4c2b'],np.full((64,1),-297.))

    def test_nonzero_history_can_emit_with_silent_auxiliary(self):
        scenario = synthetic_scenario(self.inventory)
        scenario['stages']['0xc4b70']['history'][0][0][0] = 2.
        y = self.ref(scenario).process(np.zeros((3,3)))
        np.testing.assert_array_equal(y[0],np.full(16,2.))
        np.testing.assert_array_equal(y[1:],np.zeros((2,16)))

    def test_unequal_delays_and_explicit_history(self):
        scenario = synthetic_scenario(self.inventory)
        delays = list(range(16))
        scenario['stages']['0xc4cf9'].update(samples=delays, history=[[-1.]*d for d in delays])
        x = np.zeros((96,3)); x[0,0] = 1.
        y = self.ref(scenario).process(x)
        for i,d in enumerate(delays):
            np.testing.assert_array_equal(y[:d,i],np.full(d,-1.))
            self.assertEqual(y[d,i],1.)
            self.assertEqual(np.count_nonzero(y[d+1:,i]),0)

    def test_streaming_partition_preserves64_sample_gain_cadence(self):
        scenario = synthetic_scenario(self.inventory)
        scenario['stages']['0xc4b7f'].update(current=[0.]*3,target=[1.]*3,mode=2,rates=[[.1,-.1]]*3)
        x = np.ones((151,3))
        full = self.ref(scenario).process(x)
        ref = self.ref(scenario)
        split = np.concatenate([ref.process(x[:1]),ref.process(x[1:63]),ref.process(x[63:65]),ref.process(x[65:])])
        np.testing.assert_array_equal(full,split)
        np.testing.assert_allclose(full[[0,63,64,127,128],0],[.3,.3,.6,.6,.9],atol=1e-15)

    def test_shared_prefilter_histories_are_independent(self):
        scenario = synthetic_scenario(self.inventory)
        scenario['stages']['0xc4b70']['records'] = [[.5,0.,1.,0.,0.]]
        x = np.zeros((16,3)); x[0,1] = 1.
        trace = self.ref(scenario).process(x,capture_stages=True)['stages']['0xc4b70']
        np.testing.assert_allclose(trace[:,1],.5**np.arange(16))
        np.testing.assert_array_equal(trace[:,[0,2]],np.zeros((16,2)))

    def test_packed_eq_counts_and_records_have_channel_specific_effect(self):
        scenario = synthetic_scenario(self.inventory)
        scenario['stages']['0xc4ce8'] = {'counts':[0,1]+[0]*14,
                                        'records':[[0.,0.,2.,0.,0.]],'history':[[0.,0.]]}
        y = self.ref(scenario).process(np.ones((1,3)))
        expected = np.full((1,16),3.); expected[0,1] = 6.
        np.testing.assert_array_equal(y,expected)

    def test_effective_stored_posteq_runs_with_explicit_zero_history(self):
        scenario = synthetic_scenario(self.inventory)
        p = copy.deepcopy(self.inventory['stored_writer_targets']['0xc4ce8'])
        p['history'] = np.zeros((35,2)).tolist(); scenario['stages']['0xc4ce8'] = p
        y = self.ref(scenario).process(np.random.default_rng(4).normal(size=(512,3))*.01)
        self.assertTrue(np.isfinite(y).all())
        self.assertEqual(y.shape,(512,16))

    def test_missing_or_unsupported_state_rejected(self):
        for desc,key,value in [('0xc4b7f','mode',1),('0xc4cf9','lifecycle','startup'),
                               ('0xc4cf9','samples',[2880]*16)]:
            scenario = synthetic_scenario(self.inventory); scenario['stages'][desc][key] = value
            with self.assertRaises(ValueError): self.ref(scenario)
        scenario = synthetic_scenario(self.inventory); del scenario['stages']['0xc4c1a']
        with self.assertRaises(ValueError): self.ref(scenario)
        with self.assertRaises(ValueError): self.ref().process(np.zeros((64,10)))

    def test_empty_calls_do_not_advance_and_deepcopy_isolated(self):
        a = self.ref(); b = copy.deepcopy(a)
        self.assertEqual(a.process(np.empty((0,3))).shape,(0,16))
        self.assertEqual(a.phase,0)
        a.process(np.ones((1,3)))
        self.assertEqual(b.phase,0)
        self.assertFalse(b.metadata()['live_state_proven'])
        self.assertEqual(b.metadata()['input_graph_ids'],[8,9,10])

    def test_mode1_changed_target_divides_by_time_and_scale(self):
        r=mode1_gain_update([0.],[1.],previous_mode=1,previous_target=[0.],
                            time_parameters=[4.],scale=.75,steps=[[.9,-.9]])
        np.testing.assert_allclose(r['current'],[1/3])
        np.testing.assert_allclose(r['steps'],[[1/3,-1/3]])
        self.assertEqual(r['previous_mode'],1)
        np.testing.assert_array_equal(r['previous_target'],[1.])
        self.assertTrue(r['recomputed'][0])

    def test_mode1_equal_magnitude_reuses_old_step_and_fixes_down_sign(self):
        r=mode1_gain_update([.2],[-1.],previous_mode=1,previous_target=[1.],
                            time_parameters=[100.],scale=.75,steps=[[.125,99.]])
        np.testing.assert_array_equal(r['steps'],[[.125,-.125]])
        np.testing.assert_array_equal(r['current'],[-.325])
        self.assertFalse(r['recomputed'][0])

    def test_mode1_entry_overwrites_previous_target_before_comparison(self):
        same=dict(current=[.2],target=[1.],previous_target=[1.],time_parameters=[4.],
                  scale=.75,steps=[[.125,-.125]])
        stay=mode1_gain_update(previous_mode=1,**same)
        enter=mode1_gain_update(previous_mode=3,**same)
        self.assertFalse(stay['recomputed'][0]); self.assertTrue(enter['recomputed'][0])
        np.testing.assert_allclose(enter['steps'],[[.8/3,-.8/3]])

    def test_mode1_nonpositive_denominator_only_is_replaced(self):
        r=mode1_gain_update([0.]*3,[1.]*3,previous_mode=1,previous_target=[0.]*3,
                            time_parameters=[0.,-4.,.5],scale=.75,steps=[[0.,0.]]*3)
        np.testing.assert_allclose(r['steps'][:,0],[1.,1.,1/.375])
        np.testing.assert_array_equal(r['current'],[1.,1.,1.])

    def test_mode1_nonzero_old_step_is_retained_until_target_magnitude_changes(self):
        r=mode1_gain_update([.8],[.2],previous_mode=1,previous_target=[.2],
                            time_parameters=[4.],scale=.75,steps=[[.05,-.05]])
        np.testing.assert_allclose(r['current'],[.75])
        np.testing.assert_array_equal(r['steps'],[[.05,-.05]])

    def test_mode1_process_is_same_under_uneven_chunking(self):
        scenario=synthetic_scenario(self.inventory)
        scenario['stages']['0xc4b7f']={'current':[0.]*3,'target':[1.]*3,'mode':1,
                                       'previous_mode':3,'previous_target':[9.]*3,
                                       'time_parameters':[4.]*3,'scale':.75,'steps':[[0.,0.]]*3}
        x=np.ones((193,3)); full=self.ref(scenario).process(x)
        ref=self.ref(scenario)
        split=np.concatenate([ref.process(x[:1]),ref.process(x[1:65]),ref.process(x[65:128]),ref.process(x[128:])])
        np.testing.assert_array_equal(full,split)
        np.testing.assert_allclose(full[[0,63,64,127,128,192],0],[1.,1.,2.,2.,3.,3.])

    def test_mode1_requires_full_explicit_prior_state(self):
        scenario=synthetic_scenario(self.inventory); scenario['stages']['0xc4b7f']['mode']=1
        with self.assertRaises(ValueError):self.ref(scenario)
        with self.assertRaises(ValueError):
            mode1_gain_update([0.],[1.],previous_mode=True,previous_target=[0.],
                              time_parameters=[4.],scale=.75,steps=[[0.,0.]])


if __name__ == '__main__': unittest.main()

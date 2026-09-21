"""Synthetic aux provenance is explicit; no car-state or acoustic claim."""
import copy
import json
import tempfile
import unittest

import numpy as np

from amp_dsp_chain import AmpChain, reference_fingerprint, ROOT
from amp_second_b import SecondBAuxReference, load_static_inventory
from test_amp_second_b import synthetic_scenario
import test_amp_cabin_render as cabin_fixtures
from amp_cabin_render import render


class SecondBIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.inventory=load_static_inventory()

    def scenario(self):
        params=synthetic_scenario(self.inventory)
        return {'reference':SecondBAuxReference(scenario=params,inventory=self.inventory),
                'label':'SYNTHETIC integration; no current auxiliary source/state inference'}

    def test_internal_matches_direct_boundary_injection(self):
        scenario=self.scenario();c=AmpChain(second_b_scenario=scenario)
        rng=np.random.default_rng(702);x=rng.normal(0,.01,(1500,6));aux=rng.normal(0,.01,(1500,3))
        boundary=copy.deepcopy(scenario['reference']).process(aux)
        expected=AmpChain().process(x,second_b=boundary)['out']
        actual=c.process(x,second_b_auxiliary=aux)
        np.testing.assert_array_equal(actual['second_b16'],boundary)
        np.testing.assert_array_equal(actual['out'],expected)
        self.assertIn('v9',c.metadata()['version'])
        self.assertFalse(c.metadata()['second_b_scenario']['live_state_proven'])

    def test_auxiliary_is_independent_of_music_and_common_input(self):
        from test_amp_common_input import upstream
        c=AmpChain(upstream_scenario=upstream(10),second_b_scenario=self.scenario())
        x=np.random.default_rng(703).normal(0,.01,(2048,6))
        silent=c.process(x,second_b_auxiliary=np.zeros((len(x),3)))
        plain=AmpChain(upstream_scenario=upstream(10)).process(x)
        np.testing.assert_array_equal(silent['out'],plain['out'])
        aux=np.zeros((len(x),3));aux[0,0]=.01
        audible=c.process(np.zeros_like(x),second_b_auxiliary=aux)
        self.assertGreater(np.max(abs(audible['out'])),0)
        np.testing.assert_array_equal(audible['bus'],0)

    def test_caller_state_copy_and_repeated_reset(self):
        scenario=self.scenario();before=reference_fingerprint(scenario['reference'])
        c=AmpChain(second_b_scenario=scenario);x=np.zeros((513,6));aux=np.ones((513,3))*.001
        a=c.process(x,second_b_auxiliary=aux)['out']
        np.testing.assert_array_equal(a,c.process(x,second_b_auxiliary=aux)['out'])
        self.assertEqual(reference_fingerprint(scenario['reference']),before)
        scenario['reference'].process(np.ones((13,3)))
        np.testing.assert_array_equal(a,c.process(x,second_b_auxiliary=aux)['out'])

    def test_missing_conflicting_and_malformed_auxiliary_rejected(self):
        x=np.zeros((65,6));c=AmpChain(second_b_scenario=self.scenario())
        for aux in (None,np.zeros((64,3)),np.zeros((65,6)),np.full((65,3),np.nan),np.zeros((65,3),complex)):
            with self.assertRaises(ValueError):c.process(x,second_b_auxiliary=aux)
        with self.assertRaises(ValueError):c.process(x,second_b_auxiliary=np.zeros((65,3)),second_b=np.zeros((65,16)))
        with self.assertRaises(ValueError):AmpChain().process(x,second_b_auxiliary=np.zeros((65,3)))
        with self.assertRaises(ValueError):c.response([55])

    def test_source_and_port_mismatch_rejected(self):
        for field,value in (('source_sha256','bad'),('boundary',dict(self.inventory['boundary'],ordered_b_ports=[0]*16))):
            scenario=self.scenario();scenario['reference'].inventory[field]=value
            with self.assertRaises(ValueError):AmpChain(second_b_scenario=scenario)

    def test_empty_short_and_capture_false(self):
        c=AmpChain(second_b_scenario=self.scenario())
        for n in (0,1,63,64,65,513):
            result=c.process(np.zeros((n,6)),second_b_auxiliary=np.zeros((n,3)),capture_stages=False)
            self.assertEqual(set(result),{'out'});self.assertEqual(result['out'].shape,(n,16))
            np.testing.assert_array_equal(result['out'],0)

    def test_bridge_accepts_explicit_auxiliary_and_records_hash(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,_=cabin_fixtures.CabinBridgeTests().fixture(tmp,'post_amp_config_outputs')
            x=np.zeros((128,6));aux=np.zeros((256,3));aux[0,1]=.01
            y,report=render(x,path,chain=AmpChain(second_b_scenario=self.scenario()),
                            electronic_tail_frames=128,stage_inputs={'second_b_auxiliary':aux})
            self.assertGreater(np.max(abs(y)),0)
            self.assertEqual(report['boundary_inputs']['second_b_auxiliary']['shape'],[256,3])
            self.assertIn('second_b_scenario',report['model'])

    def test_default_metadata_matches_completed_tuning_report(self):
        report=json.loads((ROOT/'output/upmix-tuning-grid-20260920/report.json').read_text())
        self.assertEqual(AmpChain().metadata(),report['model'])


if __name__=='__main__':unittest.main()

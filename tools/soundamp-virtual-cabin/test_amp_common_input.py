"""Integration tests with explicit synthetic B configuration, NOT a live preset."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import numpy as np
from amp_dsp_chain import AmpChain, AUDIT, reference_fingerprint
from amp_b_upstream import BInputPathFixedReference
from amp_b_pipeline import BMode5FixedReference
import test_amp_b_pipeline as bfixtures
from amp_music_report import load_upstream_scenario

PATH=Path(__file__).resolve().parents[2]/'output/r2/soundamp/remaining-blocks-20260919/audit.json'


def upstream(bank=7,delay=0):
    return dict(audit_path=str(PATH),graph_input_sources=[0,1,2,3,4,5,None,None],
                eq_family='tone',shared_gains_tenths_db=[0]*4,bank_index=bank,delay_samples=delay)


def b_scenario(delay=0):
    # Deliberately synthetic, independently tested branch gains1.5/.5/.25.
    base=bfixtures.Mode2CompositionTests().model()
    ref=BMode5FixedReference(input_mix_gain=.5,spectral=base.spectral,
        six_tail=base.six_tail,four_tail=base.four_tail,two_tail=base.two_tail,finish=base.finish)
    return dict(reference=ref,wrapper_delay_samples=delay,label='synthetic-neutral-tail-routing-test')


class CommonInputTests(unittest.TestCase):
    def test_neutral_upstream_reproduces_legacy_a_audio_and_response(self):
        old=AmpChain();new=AmpChain(upstream_scenario=upstream())
        x=np.random.default_rng(151).normal(0,.1,(6000,6))
        np.testing.assert_allclose(new.process(x)['out'],old.process(x)['out'],atol=2e-11)
        frequencies=[30,55,120,1000,10000]
        np.testing.assert_allclose(new.response(frequencies),old.response(frequencies),atol=2e-11)
        self.assertEqual(old.metadata()['version'],'bank1-known-electronics-v7')
        self.assertIn('v8',new.metadata()['version'])

    def test_one_common_filter_for_a_and_b_and_correct_mixer_slots(self):
        c=AmpChain(upstream_scenario=upstream(10),first_b_scenario=b_scenario(13))
        x=np.random.default_rng(152).normal(0,.03,(2500,6))
        s=c.process(x);common=s['common_input8']
        np.testing.assert_allclose(s['bus'],common @ c.M8.T,atol=0)
        prepared=common[:,:2]+.5*common[:,2,None]
        b=np.zeros((len(x),8))
        b[525:]=np.tile(prepared[:-525],(1,4))*[1.5,1.5,1.5,1.5,.5,.5,.25,.25]
        np.testing.assert_allclose(s['first_b8'],b,atol=2e-14)
        slots=[0,1,4,5,6,7,8,9]
        np.testing.assert_array_equal(s['first_b10'][:,slots],s['first_b8'])
        np.testing.assert_array_equal(s['first_b10'][:,[2,3]],0)
        wanted=np.zeros((len(x),10));wanted[:,:8]=s['bus_eq']*c.pair_a[:8]
        wanted+=s['first_b10']*c.pair_b
        np.testing.assert_array_equal(s['ten'],wanted)

    def test_composed_outputs_match_separate_common_filter_then_injected_b(self):
        spec=b_scenario();c=AmpChain(upstream_scenario=upstream(10,17),first_b_scenario=spec)
        x=np.random.default_rng(153).normal(0,.01,(3500,6));s=c.process(x)
        # With explicit first-six mapping, legacy A accepts common first6.
        injected=AmpChain().process(s['common_input8'][:,:6],first_b=s['first_b10'])
        np.testing.assert_allclose(s['out'],injected['out'],atol=1e-14)

    def test_reset_and_copy_of_caller_b_parameters_and_state(self):
        spec=b_scenario();before=reference_fingerprint(spec['reference'])
        c=AmpChain(upstream_scenario=upstream(8),first_b_scenario=spec)
        x=np.random.default_rng(154).normal(0,.1,(1234,6))
        a=c.process(x)['out'];b=c.process(x)['out']
        np.testing.assert_array_equal(a,b)
        self.assertEqual(reference_fingerprint(spec['reference']),before)
        spec['reference'].input_mix_gain=9
        np.testing.assert_array_equal(c.process(x)['out'],a)
        self.assertNotEqual(reference_fingerprint(spec['reference']),before)
        self.assertEqual(c.metadata()['first_b_scenario']['parameter_history_sha256'],before)

    def test_short_empty_and_partial_blocks_and_capture_false(self):
        c=AmpChain(upstream_scenario=upstream(),first_b_scenario=b_scenario())
        for n in (0,1,511,512,513,1025):
            x=np.zeros((n,6));s=c.process(x,capture_stages=False)
            self.assertEqual(set(s),{'out'});self.assertEqual(s['out'].shape,(n,16))
            np.testing.assert_array_equal(s['out'],0)

    def test_conflicting_or_incomplete_scenarios_are_rejected(self):
        with self.assertRaises(ValueError):AmpChain(first_b_scenario=b_scenario())
        c=AmpChain(upstream_scenario=upstream(),first_b_scenario=b_scenario())
        with self.assertRaises(ValueError):c.process(np.zeros((512,6)),first_b=np.zeros((512,10)))
        with self.assertRaises(ValueError):c.response([55])
        for field in ('wrapper_delay_samples','label','reference'):
            spec=b_scenario();del spec[field]
            with self.assertRaises(ValueError):AmpChain(upstream_scenario=upstream(),first_b_scenario=spec)
        for mapping in ([0]*6,[True]*8,[6]*8):
            spec=upstream();spec['graph_input_sources']=mapping
            with self.assertRaises(ValueError):AmpChain(upstream_scenario=spec)

    def test_explicit_mapping_applies_to_all_eight_matrix_columns(self):
        spec=upstream();spec['graph_input_sources']=[1,0,2,3,4,5,0,1]
        c=AmpChain(upstream_scenario=spec)
        x=np.zeros((700,6));x[0]=[1,2,3,4,5,6]
        common=c.process(x)['common_input8']
        np.testing.assert_allclose(common[0],[2,1,3,4,5,6,1,2],atol=1e-12)
        np.testing.assert_allclose(c.process(x)['bus'],common @ c.M8.T,atol=0)
        # Do not silently treat mapping as physically proven.
        self.assertFalse(c.metadata()['upstream_scenario']['physical_mapping_proven'])

    def test_upstream_response_and_a_delay_metadata(self):
        legacy=AmpChain();c=AmpChain(upstream_scenario=upstream(10,65))
        np.testing.assert_array_equal(c.total_a_delay-legacy.total_a_delay,
                                     np.where(np.array(c.out_src)<8,65,0))
        f=np.array([30,55,120,1000])
        params=json.loads(PATH.read_text())['input_path_profiles'][0]
        common=BInputPathFixedReference(params,eq_family='tone',shared_gains_tenths_db=[0]*4,
                                       bank_index=10,delay_samples=65).response(f)[:,0]
        np.testing.assert_allclose(c.response(f),legacy.response(f)*common,atol=2e-12)
        np.testing.assert_allclose(c.response(f,False),legacy.response(f,False)*common*np.exp(2j*np.pi*f*65/48000),atol=2e-12)
        json.dumps(c.metadata(),allow_nan=False)

    def test_injected_b_is_not_filtered_twice(self):
        c=AmpChain(upstream_scenario=upstream(15));plain=AmpChain()
        x=np.zeros((3000,6));b=np.random.default_rng(155).normal(0,.01,(3000,10))
        np.testing.assert_array_equal(c.process(x,first_b=b)['out'],plain.process(x,first_b=b)['out'])
        b2=np.random.default_rng(156).normal(0,.01,(3000,16))
        np.testing.assert_array_equal(c.process(x,second_b=b2)['out'],plain.process(x,second_b=b2)['out'])

    def test_scenario_json_resolves_relative_audit_and_hash_mismatch_rejected(self):
        # Temporary files are test fixtures, not a vehicle write or preset.
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);spec=upstream();spec['audit_path']='audit.json'
            (p/'scenario.json').write_text(json.dumps(spec))
            loaded=load_upstream_scenario(p/'scenario.json')
            self.assertEqual(Path(loaded['audit_path']), (p/'audit.json').resolve())
            data=json.loads(PATH.read_text());data['source_sha256']='bad'
            (p/'audit.json').write_text(json.dumps(data))
            with self.assertRaises(ValueError):AmpChain(upstream_scenario=loaded)


if __name__=='__main__':unittest.main()

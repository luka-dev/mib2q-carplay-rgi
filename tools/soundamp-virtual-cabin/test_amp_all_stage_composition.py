"""All-stage wiring fixture: synthetic controls/aux/spectral, NOT a car preset."""
import copy
import json
import unittest

import numpy as np

from amp_dsp_chain import AmpChain, reference_fingerprint
from amp_b_bound_scenario import retained_config
from amp_b_tail_binding import ROOT, build_effective_mode5
from amp_second_b import SecondBAuxReference, load_static_inventory
from amp_model_readiness import audit
from test_amp_b_pipeline import pipeline
from test_amp_common_input import upstream
from test_amp_second_b import synthetic_scenario


def explicit_synthetic_scenario():
    path=ROOT/'output/r2/soundamp/remaining-blocks-20260920-v3'
    report=json.loads((path/'b_config_control.json').read_text())
    mode=json.loads((path/'b_mode_control.json').read_text())
    cfg=retained_config(report,source='b01',bank=1)
    pointers={int(k):v for name in ('20:default','16:default') for k,v in mode['pointer_maps'][name].items()}
    first=build_effective_mode5(effective_config=cfg,pointers=pointers,library_mode=5,
        assume_successful_settled_allocations=True,zero_histories=True,
        spectral=pipeline(),input_mix_gain=.5)
    inventory=load_static_inventory()
    second_state=synthetic_scenario(inventory)
    for descriptor in ('0xc4ba7','0xc4bd5','0xc4c03'):
        # Explicit synthetic entry into mode1; NOT actual gain/current state.
        second_state['stages'][descriptor]=dict(current=[0.],target=[1.],mode=1,
            previous_mode=3,previous_target=[.7],time_parameters=[4.],scale=.75,steps=[[0.,0.]])
    second=SecondBAuxReference(scenario=second_state,inventory=inventory)
    limiter=dict(threshold=[.01]*16,attack=[.5]*16,release=[1.001]*16,
                 minimum=1e-6,maximum=1,initial=1,linked=False)
    complex_scenario=dict(profile=json.loads((path/'audit.json').read_text())['complex_control_profiles'][0],
        fir_coefficients=np.zeros((10,512)),adaptive_enable=1,sideband=0,
        power_x_db=[0]*5,power_y_db=[0]*5,power_floor_db=[-200]*5,rounding='nearest')
    return dict(upstream_scenario=upstream(10,17),
        first_b_scenario=dict(reference=first,wrapper_delay_samples=1024,
            label='SYNTHETIC spectral/controls + retained B01bank1 tails; zero history; on-time wrapper'),
        second_b_scenario=dict(reference=second,label='SYNTHETIC aux8/9/10 stages, not live state'),
        complex_block='full_mode_reference',complex_scenario=complex_scenario,
        runtime=dict(pre_complex_gain=[.7]*10,post_complex_gain=[.6]*10,limiters=[limiter,copy.deepcopy(limiter)]))


def composition_smoke(frames=4097):
    scenario=explicit_synthetic_scenario();chain=AmpChain(**scenario)
    rng=np.random.default_rng(790)
    x=rng.normal(0,.02,(frames,6));aux2=rng.normal(0,.003,(frames,3))
    aux=np.sin(np.arange(frames)*2*np.pi*63/48000)*.01
    result=chain.process(x,complex_auxiliary=aux,second_b_auxiliary=aux2)
    injection={k:v for k,v in scenario.items() if k not in ('first_b_scenario','second_b_scenario')}
    separate=AmpChain(**injection).process(x,complex_auxiliary=aux,
        first_b=result['first_b10'],second_b=result['second_b16'])
    error=float(np.max(np.abs(result['out']-separate['out']))) if frames else 0.
    return chain, scenario, (x,aux,aux2), result, error


class AllStageCompositionTests(unittest.TestCase):
    def test_full_wiring_matches_separate_boundary_injection_with_active_limiters(self):
        chain,scenario,inputs,result,error=composition_smoke()
        self.assertEqual(error,0)
        for stage in ('common_input8','first_b8','first_b10','second_b16','pre_complex_gain',
                      'complex','ten','mixed16','limiter_1','limiter_2'):
            self.assertIn(stage,result)
            self.assertTrue(np.isfinite(result[stage]).all())
        self.assertGreater(np.max(abs(result['first_b8'])),0)
        self.assertGreater(np.max(abs(result['second_b16'])),0)
        self.assertGreater(np.max(abs(result['post_eq_1']-result['limiter_1'])),0)
        self.assertEqual(chain.metadata()['first_b_scenario']['tail_binding']['library_mode'],5)
        self.assertFalse(chain.metadata()['first_b_scenario']['tail_binding']['actual_vehicle_state_proven'])

    def test_scenario_presence_does_not_pass_fidelity_or_state_gates(self):
        chain=AmpChain(**explicit_synthetic_scenario());report=audit(chain.metadata())
        self.assertTrue(all(report['electrical']['gates'].values()))
        self.assertTrue(report['claims']['all_electrical_scenarios_explicit'])
        self.assertFalse(report['claims']['full_cabin_fidelity_established'])
        self.assertFalse(report['claims']['actual_vehicle_state_verified'])
        self.assertFalse(report['claims']['subjective_optimality_established'])

    def test_reset_is_repeatable_and_caller_histories_are_unchanged(self):
        chain,scenario,(x,aux,aux2),result,_=composition_smoke(2051)
        before=reference_fingerprint(scenario)
        repeated=chain.process(x,complex_auxiliary=aux,second_b_auxiliary=aux2)
        np.testing.assert_array_equal(repeated['out'],result['out'])
        self.assertEqual(reference_fingerprint(scenario),before)
        with self.assertRaises(ValueError):chain.process(x,complex_auxiliary=aux)
        with self.assertRaises(ValueError):chain.process(x,second_b_auxiliary=aux2)


if __name__=='__main__':unittest.main()

"""Reusable diagnostic factories must not become implicit vehicle presets."""
import ast
import copy
import hashlib
import unittest
from pathlib import Path

import numpy as np

from amp_dsp_chain import AmpChain, reference_fingerprint
from amp_model_readiness import audit
from amp_test_scenarios import make_test_scenario, scenario_manifest, make_neutral_diagnostic_spectral


class TestScenarioFactoryTests(unittest.TestCase):
    def test_factory_has_no_imports_from_test_modules(self):
        source=Path(__file__).with_name('amp_test_scenarios.py').read_text()
        for node in ast.walk(ast.parse(source)):
            modules=[v.name for v in node.names] if isinstance(node,ast.Import) else (
                [node.module or ''] if isinstance(node,ast.ImportFrom) else [])
            for module in modules:self.assertFalse(module.split('.')[-1].startswith('test_'))

    def test_neutral_spectral_branches_and_explicit_disabled_features(self):
        spectral=make_neutral_diagnostic_spectral()
        rng=np.random.default_rng(807)
        x=rng.normal(size=(2,513))+1j*rng.normal(size=(2,513))
        for branch in spectral.spectral_process(*x,mode=5).values():
            np.testing.assert_allclose(branch,x,atol=2e-15)
        manifest=scenario_manifest(make_test_scenario(input_eq_bank=7))
        self.assertEqual(manifest['disabled_spectral_features'],['spatial','smoothing','pre_subtraction'])

    def test_defaults_are_labelled_neutral_runtime_not_old_stress_fixture(self):
        scenario=make_test_scenario(input_eq_bank=7)
        manifest=scenario_manifest(scenario)
        self.assertTrue(manifest['diagnostic_only'])
        self.assertFalse(manifest['actual_vehicle_state_proven'])
        self.assertGreaterEqual(len(manifest['unknowns']),8)
        self.assertEqual(manifest['adaptive_enable'],0)
        self.assertEqual(manifest['input_mix_gain_object97_hypothesis'],.5)
        self.assertEqual(scenario['upstream_scenario']['delay_samples'],0)
        self.assertEqual(scenario['runtime']['pre_complex_gain'],[1.]*10)
        self.assertEqual(scenario['runtime']['post_complex_gain'],[1.]*10)
        for limiter in scenario['runtime']['limiters']:
            self.assertEqual(limiter['threshold'],[100.]*16);self.assertEqual(limiter['initial'],1)
        for item in ('first_b_scenario','second_b_scenario'):
            self.assertIn('SYNTHETIC E2E DIAGNOSTIC',scenario[item]['label'])
        chain=AmpChain(**scenario)
        self.assertEqual(chain.metadata()['first_b_scenario']['tail_binding']['diagnostic_test_scenario'],manifest)
        report=audit(chain.metadata())
        self.assertTrue(report['claims']['all_electrical_scenarios_explicit'])
        for claim in ('full_cabin_fidelity_established','actual_vehicle_state_verified','subjective_optimality_established'):
            self.assertFalse(report['claims'][claim])

    def test_full_shape_zero_aux_repeatability_no_limiter_reduction_and_external_state_unchanged(self):
        scenario=make_test_scenario(input_eq_bank=7)
        before=reference_fingerprint(scenario);chain=AmpChain(**scenario)
        frames=4097;x=np.random.default_rng(808).normal(0,.005,(frames,6))
        aux=np.zeros(frames);aux2=np.zeros((frames,3))
        a=chain.process(x,complex_auxiliary=aux,second_b_auxiliary=aux2)
        b=chain.process(x,complex_auxiliary=aux,second_b_auxiliary=aux2)
        self.assertEqual(a['out'].shape,(frames,16));self.assertTrue(np.isfinite(a['out']).all())
        self.assertGreater(np.max(abs(a['out'])),0)
        np.testing.assert_array_equal(a['out'],b['out'])
        np.testing.assert_array_equal(a['second_b16'],0)
        np.testing.assert_array_equal(a['limiter_1'],a['post_eq_1'])
        self.assertEqual(reference_fingerprint(scenario),before)
        with self.assertRaises(ValueError):chain.process(x,complex_auxiliary=aux)
        with self.assertRaises(ValueError):chain.process(x,second_b_auxiliary=aux2)

    def test_mode1_settled_targets_have_no_synthetic_fade_and_match_mode0(self):
        scenarios=[make_test_scenario(input_eq_bank=7,auxiliary_gain_mode=v) for v in (0,1)]
        refs=[s['second_b_scenario']['reference'] for s in scenarios]
        aux=np.random.default_rng(809).normal(0,.002,(137,3))
        a,b=[ref.process(aux) for ref in refs]
        np.testing.assert_array_equal(a,b)
        for descriptor in ('0xc4ba7','0xc4bd5','0xc4c03'):
            params=refs[1].scenario['stages'][descriptor]
            self.assertEqual(params['current'],params['target'])
            self.assertEqual(params['previous_target'],params['target'])
            self.assertEqual(params['steps'],[[0.,0.]])
        self.assertGreater(np.max(abs(a)),0)

    def test_stored_second_b_coefficients_and_initial_fir_are_not_erased(self):
        scenario=make_test_scenario(input_eq_bank=7);manifest=scenario_manifest(scenario)
        stages=scenario['second_b_scenario']['reference'].scenario['stages']
        self.assertEqual(stages['0xc4b96']['counts'],[8,4,4])
        self.assertEqual(len(stages['0xc4b96']['records']),16)
        self.assertEqual(sum(stages['0xc4ce8']['counts']),35)
        self.assertEqual(len(stages['0xc4ce8']['records']),35)
        self.assertGreater(max(stages['0xc4cf9']['samples']),0)
        for samples,history in zip(stages['0xc4cf9']['samples'],stages['0xc4cf9']['history']):
            self.assertEqual(history,[0.]*samples)
        self.assertEqual([manifest['second_b_target_tenths_db'][d][0]
                          for d in ('0xc4ba7','0xc4bd5','0xc4c03')],[-34,-157,-157])
        self.assertEqual(manifest['second_b_control_inputs']['source_levels'],[0,0,0])
        self.assertTrue(manifest['second_b_state_assumptions']['chosen_targets_delivered_and_settled'])
        fir=scenario['complex_scenario']['fir_coefficients']
        self.assertGreater(np.count_nonzero(fir),0)
        self.assertEqual(hashlib.sha256(fir.astype('<f4').tobytes()).hexdigest(),
                         manifest['complex_initial_fir']['bank_sha256'])

    def test_probe_and_pointer_variants_explicit_manifest_and_fresh_objects(self):
        a=make_test_scenario(input_eq_bank=10,adaptive_enable=1,first_b_pointer_variant='20:3')
        b=make_test_scenario(input_eq_bank=10,adaptive_enable=1,first_b_pointer_variant='20:3')
        m=scenario_manifest(a)
        self.assertEqual(m['adaptive_enable'],1);self.assertEqual(m['first_b_pointer_maps'],['20:3','16:default'])
        self.assertEqual(a['complex_scenario']['adaptive_enable'],1)
        self.assertEqual(reference_fingerprint(a),reference_fingerprint(b))
        m['unknowns'].clear();self.assertGreater(len(scenario_manifest(a)['unknowns']),0)
        a['runtime']['limiters'][0]['threshold'][0]=2
        self.assertEqual(b['runtime']['limiters'][0]['threshold'][0],100)
        self.assertEqual(a['runtime']['limiters'][1]['threshold'][0],100)

    def test_invalid_choices_fail_closed(self):
        cases=[dict(input_eq_bank=v) for v in (-1,16,True,7.,'7')]
        cases += [dict(input_eq_bank=7,auxiliary_gain_mode=v) for v in (-1,2,True,'1')]
        cases += [dict(input_eq_bank=7,adaptive_enable=v) for v in (-1,2,True,'1')]
        cases += [dict(input_eq_bank=7,first_b_pointer_variant=v) for v in ('3','live','16:5',1,None)]
        cases += [dict(input_eq_bank=7,first_b_spectral=v) for v in ('live','unity',1,None,True)]
        for kwargs in cases:
            with self.assertRaises(ValueError):make_test_scenario(**kwargs)
        with self.assertRaises(ValueError):scenario_manifest({})

    def test_retained_spectral_uses_common_header_and_consistent_mode5_maps(self):
        for pointer,expected_offset in [('default',3),('20:3',439)]:
            scenario=make_test_scenario(input_eq_bank=7,first_b_spectral='retained_calibration',
                                        first_b_pointer_variant=pointer)
            manifest=scenario_manifest(scenario)
            spectral=scenario['first_b_scenario']['reference'].spectral
            self.assertEqual(manifest['first_b_pointer_maps'],
                             ['20:default' if pointer=='default' else '20:3','16:5'])
            self.assertEqual(manifest['first_b_mode_control_history']['apply_order'][-1],[16,5])
            self.assertAlmostEqual(manifest['input_mix_gain_object97_hypothesis'],.5011872053146362)
            self.assertEqual(manifest['spectral_binding']['crossfeed_config_offset'],493)
            self.assertEqual(manifest['spectral_binding']['spatial']['parameter_config_offset'],expected_offset)
            self.assertEqual(spectral.spatial.controls.gate,1)
            self.assertEqual(spectral.level_sum.params[14:16].tolist(),[1.,.5])
            self.assertTrue(spectral.pre_subtraction['enabled'])
            self.assertTrue(spectral.spatial_enabled)
            self.assertFalse(np.array_equal(spectral.spatial.edges,np.linspace(0,513,75,dtype=int)))
            self.assertFalse(manifest['actual_vehicle_state_proven'])

    def test_retained_spectral_real_composition_runs_and_does_not_mutate_scenario(self):
        scenario=make_test_scenario(input_eq_bank=7,first_b_spectral='retained_calibration')
        before=reference_fingerprint(scenario)
        n=4096
        x=np.random.default_rng(811).normal(0,.002,(n,6))
        result=AmpChain(**scenario).process(x,complex_auxiliary=np.zeros(n),
                                          second_b_auxiliary=np.zeros((n,3)))
        self.assertEqual(result['out'].shape,(n,16))
        self.assertTrue(np.isfinite(result['out']).all())
        self.assertGreater(np.max(abs(result['out'])),0)
        self.assertEqual(reference_fingerprint(scenario),before)


if __name__=='__main__':unittest.main()

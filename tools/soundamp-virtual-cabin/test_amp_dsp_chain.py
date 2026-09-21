"""Routing, delay, transfer-function and proof-boundary regressions for AmpChain."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from amp_dsp_chain import AUDIT, AmpChain, SR, design_to_sos
from digital_stages import PackedOutputEQ, pair_mix, select
from amp_complex_path import ComplexFullModeReference


class AmpChainTests(unittest.TestCase):
    def complex_scenario(self, sideband=0):
        path=Path(__file__).resolve().parents[2]/'output/r2/soundamp/remaining-blocks-20260919/audit.json'
        return dict(profile=json.loads(path.read_text())['complex_control_profiles'][0],
            fir_coefficients=np.zeros((10,512)),adaptive_enable=0,sideband=sideband,
            power_x_db=[0]*5,power_y_db=[0]*5,power_floor_db=[-200]*5,rounding='nearest')

    def test_full_complex_requires_explicit_auxiliary_and_matching_profile(self):
        scenario=self.complex_scenario()
        with self.assertRaises(ValueError):AmpChain(complex_block='full_mode_reference')
        with self.assertRaises(ValueError):AmpChain(complex_scenario=scenario)
        c=AmpChain(complex_block='full_mode_reference',complex_scenario=scenario)
        with self.assertRaises(ValueError):c.process(np.zeros((64,6)))
        with self.assertRaises(ValueError):c.process(np.zeros((64,6)),complex_auxiliary=np.zeros((64,1)))
        with self.assertRaises(ValueError):AmpChain().process(np.zeros((64,6)),complex_auxiliary=np.zeros(64))
        with self.assertRaises(ValueError):c.response([100])
        scenario['profile']['profile_index']=1
        with self.assertRaises(ValueError):AmpChain(complex_block='full_mode_reference',complex_scenario=scenario)
        meta=c.metadata()
        self.assertEqual(meta['complex_block'],'full_mode_reference')
        self.assertEqual(len(meta['complex_scenario']['initial_fir_float32_sha256']),64)
        np.testing.assert_array_equal(c.total_a_delay,AmpChain().total_a_delay+1023)
        json.dumps(meta)

    def test_full_complex_sideband_exact_path_delay_and_reset(self):
        c=AmpChain(complex_block='full_mode_reference',complex_scenario=self.complex_scenario(1))
        x=np.random.default_rng(41).normal(0,.01,(4099,6))
        plain=AmpChain().process(x)['out']
        expected=np.zeros_like(plain);expected[1023:]=plain[:-1023]
        y=c.process(x,complex_auxiliary=np.zeros(len(x)))['out']
        np.testing.assert_allclose(y,expected,atol=1e-14)
        np.testing.assert_array_equal(y,c.process(x,complex_auxiliary=np.zeros(len(x)))['out'])
        for n in (0,1,63,64,65):
            np.testing.assert_array_equal(c.process(x[:n],complex_auxiliary=np.zeros(n))['out'],np.zeros((n,16)))

    def test_full_complex_pre_post_gain_order_and_independent_composition(self):
        params=dict(threshold=[100]*16,attack=[.5]*16,release=[2]*16,
                    minimum=1e-6,maximum=1,initial=1,linked=False)
        runtime=dict(pre_complex_gain=[.5]*10,post_complex_gain=[.3]*10,limiters=[params,params])
        scenario=self.complex_scenario()
        c=AmpChain(complex_block='full_mode_reference',complex_scenario=scenario,runtime=runtime)
        x=np.random.default_rng(42).normal(0,.01,(4096,6));aux=np.ones(len(x))*.02
        stages=c.process(x,complex_auxiliary=aux)
        boundary=AmpChain().process(x)['ten']*.5
        reference=ComplexFullModeReference(**scenario)
        expected=np.concatenate([reference.process_block(boundary[i:i+64],aux[i:i+64])['audio']
                                 for i in range(0,len(x),64)])
        np.testing.assert_array_equal(stages['pre_complex_gain'],boundary)
        np.testing.assert_array_equal(stages['complex'],expected)
        np.testing.assert_array_equal(stages['ten'],expected*.3)
        self.assertEqual(set(c.process(x,complex_auxiliary=aux,capture_stages=False)),{'out'})

    def test_output_mixer_calibration_and_matrix_floor(self):
        c = AmpChain()
        gains = 20*np.log10(abs(c.output_pair_a[c.route]))
        np.testing.assert_allclose(gains[[0,1,8,9,10,11,12,13,14,15]],
                                   [3.8,3.8,0,0,.2,-8.8,9.4,9.4,-.8,-.8], atol=1e-6)
        self.assertAlmostEqual(c.M[0,3], 10**(-99/20), places=11)
        self.assertAlmostEqual(c.M[3,0],10**(-8/20),places=7)
        self.assertEqual(c.pair_a[8], 0)
        self.assertGreater(c.pair_b[8], 0)  # -99 dB is not the -100 mute sentinel.
        np.testing.assert_array_equal(c.total_a_delay[[0,1,8,9,10,11,12,13,14,15]],
                                      [1786,1789,2035,2035,1757,1774,1719,1719,1836,1836])

    def test_exact_impulse_routing_delays_and_short_clips(self):
        c = AmpChain()
        c.bus_sos = [np.zeros((0,6)) for _ in range(8)]
        c.out_sos = [np.zeros((0,6)) for _ in range(16)]
        for source in range(6):
            x = np.zeros((2200,6)); x[0,source] = .1
            y = c.process(x, capture_stages=False)['out']
            expected = np.zeros_like(y)
            for output, bus in enumerate(c.out_src):
                if bus < 8:
                    expected[c.total_a_delay[output],output] = (
                        .1*c.M[bus,source]*c.pair_a[bus]*c.output_pair_a[c.route[output]]*c.out_gain[output])
            np.testing.assert_allclose(y, expected, atol=1e-16)
        for length in (0,1,17,100):
            np.testing.assert_array_equal(c.process(np.zeros((length,6)))['out'], np.zeros((length,16)))

    def test_response_matches_time_domain_for_every_input(self):
        c = AmpChain()
        n = 65536
        bins = np.array([41,86,171,341,683,1365,5461,13653])
        expected = c.response(bins*SR/n)
        for source in range(6):
            x = np.zeros((n,6)); x[0,source] = 1
            actual = np.fft.rfft(c.process(x, capture_stages=False)['out'],axis=0)[bins].T
            np.testing.assert_allclose(actual,expected[:,source],atol=2e-7,rtol=2e-6)
        np.testing.assert_allclose(abs(c.response(bins*SR/n,include_delay=False)),abs(expected),atol=1e-12)

    def test_b_inputs_join_at_the_correct_stage(self):
        c = AmpChain()
        c.bus_sos = [np.zeros((0,6)) for _ in range(8)]
        c.out_sos = [np.zeros((0,6)) for _ in range(16)]
        x = np.zeros((128,6))
        first = np.zeros((128,10)); first[0,0] = 1
        y = c.process(x,first_b=first)['out']
        self.assertAlmostEqual(y[67,0], c.pair_b[0]*c.output_pair_a[1])
        self.assertAlmostEqual(y[0,12], c.pair_b[0]*c.output_pair_a[3])
        second = np.zeros((128,16)); second[0,3] = 1
        y = c.process(x,second_b=second)['out']
        expected = np.zeros_like(y); expected[0,12] = 10**(6/20)
        np.testing.assert_allclose(y,expected,atol=1e-7)
        with self.assertRaises(ValueError):
            c.process(x,second_b=np.zeros((128,10)))

    def test_packed_eq_and_selector_composition(self):
        c = AmpChain()
        d = json.loads(AUDIT.read_text())
        x = np.random.default_rng(4).normal(0,.01,(4096,6))
        stages = c.process(x)
        expanded = select(stages['ten'], d['split_modes'][0])
        mixed = pair_mix(np.concatenate([expanded,np.zeros_like(expanded)],axis=1),
                         np.column_stack([c.output_pair_a,c.output_pair_b]).ravel())
        routed = select(mixed,d['normal_route'])[:,d['sharc_slot_to_v850_config']]
        records = [r for ch in sorted(d['channels'],key=lambda v:v['sharc_processing_slot'])
                   for r in ch['packed_eq_records']]
        sos = np.array([design_to_sos(r,c.zeta) for r in records])
        packed = np.column_stack([-sos[:,4],-sos[:,5],sos[:,:3]])
        y = PackedOutputEQ(d['output_eq_counts'],packed).process(routed)[:,d['v850_config_to_sharc_slot']]
        np.testing.assert_allclose(y,stages['output_eq'],atol=1e-11)

    def test_no_silent_unknown_stage_or_legacy_audit_fallback(self):
        with self.assertRaises(ValueError):
            AmpChain(complex_block='fully_emulated')
        with self.assertRaises(ValueError):
            AmpChain(row=-1)
        d = json.loads(AUDIT.read_text())
        for kind in ('legacy','nonunity'):
            changed = copy.deepcopy(d)
            if kind == 'legacy':
                del changed['known_electronic_stages']
            else:
                r=changed['known_electronic_stages']['row_eq']['rows'][1][0]
                r['gain_tenths_db']=30; r['gain_linear']=10**(3/20)
            with tempfile.TemporaryDirectory() as temp:
                p=Path(temp)/'audit.json';p.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):
                    AmpChain(p)
        meta=AmpChain().metadata()
        self.assertEqual(meta['verified_unity_eq_records'],32)
        self.assertEqual(meta['complex_block'],'identity_hypothesis')
        self.assertIn('second_mixer_B',meta['default_auxiliary_audio'])

    def test_post_eq_nonunity_binding_and_frequency_response(self):
        d = json.loads(AUDIT.read_text())
        r = d['known_electronic_stages']['post_output_eq'][0]['records'][4]
        r['gain_tenths_db'] = 30; r['gain_linear'] = 10**(3/20)
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp)/'audit.json'; p.write_text(json.dumps(d))
            c = AmpChain(p)
        # Source records 4/5 bind to cfg14 even though DSP slots place cfg15 first.
        ratio = abs(c.response([100])) / np.maximum(abs(AmpChain().response([100])), 1e-30)
        np.testing.assert_allclose(ratio[14,3,0],10**(3/20),atol=1e-6)
        np.testing.assert_allclose(ratio[[12,13,15],3,0],1,atol=1e-12)
        x = np.zeros((65536,6)); x[0,3] = 1
        bins = np.array([86,137,341])
        actual = np.fft.rfft(c.process(x)['out'],axis=0)[bins].T
        np.testing.assert_allclose(actual,c.response(bins*SR/len(x))[:,3],atol=2e-7)

    def test_explicit_runtime_scenario_and_nonlinear_response_guard(self):
        params = dict(threshold=[100]*16,attack=[.5]*16,release=[2]*16,
                      minimum=1e-6,maximum=1,initial=1,linked=False)
        runtime = dict(pre_complex_gain=[.5]*10,post_complex_gain=[.5]*10,
                       limiters=[params,params])
        c = AmpChain(runtime=runtime)
        x = np.random.default_rng(8).normal(0,.01,(4096,6))
        np.testing.assert_allclose(c.process(x)['out'], AmpChain().process(x)['out']*.25,atol=1e-14)
        with self.assertRaises(ValueError): c.response([100])
        with self.assertRaises(ValueError): AmpChain(runtime={})
        json.dumps(c.metadata())


if __name__ == '__main__':
    unittest.main()

import unittest

import numpy as np

from amp_complex_control import (ComplexControl, adaptive_target, shape_powers,
                                 log_reference, exp_reference, P, Q, AdaptiveResidual)


class ComplexControlTests(unittest.TestCase):
    def options(self, **changes):
        # Deliberately explicit synthetic scenario, not claimed live state.
        out = dict(helper_parameters=[[1e-10, 1.5848932e-9, .00420727, 15.848932]]*5,
                   y_floor_db=[-200]*5, cap_code=10, common_code=24,
                   adaptive_enable=1, inhibit=0, cap_enable=1, freeze=0,
                   link_upper=0, other_base=[1]*5, other_scale=1,
                   adaptive_rates=[1.0144952535629272,.9536192417144775],
                   other_rates=[1.0072215795516968,.9928302764892578],
                   force_smoothing=0)
        out.update(changes)
        return out

    def test_log_polynomial_matches_independent_math(self):
        x = np.geomspace(1e-20, 1e20, 1001)
        np.testing.assert_allclose([log_reference(v) for v in x], np.log(x), atol=1e-9, rtol=0)
        self.assertEqual(log_reference(0), 0)
        self.assertEqual(log_reference(-1), 0)

    def test_exp_polynomial_matches_independent_math(self):
        x = np.linspace(-80,80,1001)
        np.testing.assert_allclose([exp_reference(v) for v in x], np.exp(x), rtol=3e-10)
        self.assertEqual(exp_reference(0), 1)

    def test_helper_threshold_equality_returns_unity(self):
        self.assertEqual(adaptive_target(.01, .5, [.01,.02,0,20]), 1)
        self.assertEqual(adaptive_target(.5, .02, [.01,.02,0,20]), 1)

    def test_helper_matches_closed_form_and_caps(self):
        for x in [.0001,.01,.5,1,2]:
            for y in [.0001,.01,.5,1,2]:
                expected = max(1,min((1+(min(y,1)**P-.004)*min(x,1)**(-P))**Q, 15))
                self.assertAlmostEqual(adaptive_target(x,y,[0,0,.004,15]), expected, places=7)
        self.assertEqual(adaptive_target(1e-9,1,[0,0,0,4]),4)

    def test_prefix_minimum_and_db_scaling(self):
        x,y=shape_powers([1]*5,[3,2,4,1,5],x_db=[10,0,0,0,0],y_db=[0]*5,cumulative_min=True)
        np.testing.assert_allclose(x,[np.sqrt(10),1,1,1,1])
        np.testing.assert_array_equal(y,[3,2,2,1,1])

    def test_disabled_neutral_start_is_neutral(self):
        s=ComplexControl().update([.001]*5,[.5]*5,**self.options(adaptive_enable=0))
        np.testing.assert_array_equal(s['endpoint'],1)
        self.assertEqual(s['common_endpoint'],1)

    def test_inhibit_or_cap_disable_forces_band_endpoint_unity(self):
        for change in (dict(inhibit=1),dict(cap_enable=0),dict(cap_code=0)):
            s=ComplexControl().update([.001]*5,[.5]*5,**self.options(**change))
            np.testing.assert_array_equal(s['endpoint'],1)

    def test_cap_and_1024_sample_endpoint(self):
        s=ComplexControl().update([1e-8]*5,[1]*5,**self.options())
        np.testing.assert_allclose(s['endpoint'],np.sqrt(10))
        self.assertAlmostEqual(s['common_endpoint'],10**(-24/20))
        np.testing.assert_allclose(s['gains']+1024*s['steps'],s['endpoint'])
        self.assertFalse(s['smoothing_applied'])  # cap/enable changed

    def test_power_floor_also_uses_firmware_half_db_exponent(self):
        s=ComplexControl().update([.001]*5,[0]*5,**self.options(y_floor_db=[-40]*5,cap_code=40))
        expected=adaptive_target(.001,.01,self.options()['helper_parameters'][0])
        np.testing.assert_allclose(s['adaptive'],expected)

    def test_upper_link_set_not_clear_selects_link_branch(self):
        args=([.001]*5,[.1,.2,.3,.4,.5])
        linked=ComplexControl().update(*args,**self.options(link_upper=1))
        free=ComplexControl().update(*args,**self.options(link_upper=0))
        self.assertEqual(linked['adaptive'][4],1)
        self.assertEqual(linked['adaptive'][3],(linked['adaptive'][2]+1)/2)
        self.assertGreater(free['adaptive'][4],1)

    def test_steady_mode_rate_clamps_but_changed_mode_skips(self):
        c=ComplexControl()
        c.update([.01]*5,[.01]*5,**self.options())
        previous=c.adaptive.copy()
        s=c.update([1e-8]*5,[1]*5,**self.options())
        self.assertTrue(s['smoothing_applied'])
        np.testing.assert_allclose(s['adaptive'],previous*self.options()['adaptive_rates'][0])
        s=c.update([1e-8]*5,[1]*5,**self.options(cap_code=11))
        self.assertFalse(s['smoothing_applied'])

    def test_disable_can_decay_when_forced_smoothing(self):
        c=ComplexControl()
        c.update([.001]*5,[.1]*5,**self.options())
        old=c.adaptive.copy()
        s=c.update([.001]*5,[.1]*5,**self.options(adaptive_enable=0,force_smoothing=1))
        np.testing.assert_allclose(s['adaptive'],old*self.options()['adaptive_rates'][1])
        self.assertTrue((s['endpoint']>1).all())

    def test_sideband_disabled_smoothing_stage(self):
        c=ComplexControl()
        c.update([.001]*5,[.1]*5,**self.options())
        s=c.update([0]*5,[0]*5,**self.options(force_smoothing=1,smoothing_stage=False))
        self.assertFalse(s['smoothing_applied'])
        np.testing.assert_array_equal(s['adaptive'],1)

    def test_freeze_copies_previous_targets_and_common(self):
        c=ComplexControl()
        c.update([.01]*5,[.01]*5,**self.options())
        previous=c.adaptive.copy(); common=c.common
        s=c.update([1e-8]*5,[1]*5,**self.options(freeze=1))
        np.testing.assert_array_equal(s['adaptive'],previous)
        self.assertEqual(s['common_endpoint'],common)

    def test_other_factor_is_independent_of_adaptive_enable(self):
        s=ComplexControl().update([0]*5,[0]*5,
              **self.options(adaptive_enable=0,other_base=[2]*5,smoothing_stage=False))
        np.testing.assert_array_equal(s['endpoint'],2)

    def test_invalid_power_rejected(self):
        with self.assertRaises(ValueError):
            ComplexControl().update([-1]*5,[1]*5,**self.options())

    def residual(self, coefficients=None, **kwargs):
        opts=dict(reference_gains=[1]*10,auxiliary_gain=1,step=.002,rounding='nearest')
        opts.update(kwargs)
        return AdaptiveResidual(np.zeros((10,512)) if coefficients is None else coefficients,**opts)

    def test_residual_zero_reference_preserves_auxiliary_and_coefficients(self):
        c=self.residual(auxiliary_gain=.5)
        x=np.zeros((4,10));y=np.array([1,2,3,4.])
        np.testing.assert_array_equal(c.process(x,y),y*.5)
        np.testing.assert_array_equal(c.coefficients,0)

    def test_fir_prediction_matches_independent_convolution(self):
        from scipy.signal import lfilter
        rng=np.random.default_rng(29)
        h=rng.normal(0,.01,(10,512)); x=rng.normal(0,.01,(1024,10))
        predicted=sum(lfilter(h[i],[1],x[:,i]) for i in range(10))
        c=self.residual(h,step=0)
        np.testing.assert_allclose(c.process(x,predicted),0,atol=1e-16)
        np.testing.assert_array_equal(c.coefficients,h)

    def test_lms_updates_after_residual_not_before(self):
        c=self.residual(step=.1)
        x=np.zeros((1,10));x[0,0]=.5
        np.testing.assert_array_equal(c.process(x,[1]),[1])
        self.assertAlmostEqual(c.coefficients[0,0],.05)
        self.assertEqual(np.count_nonzero(c.coefficients),1)
        np.testing.assert_allclose(c.process(x,[1]),[.975])

    def test_power_attack_immediate_release_smoothed_and_step_zero_at_ten(self):
        c=self.residual(step=.1)
        c.process(np.ones((4,10)),np.ones(4))
        self.assertEqual(c.power_state,10)
        np.testing.assert_array_equal(c.coefficients,0)
        c.process(np.zeros((4,10)),np.ones(4))
        self.assertAlmostEqual(c.power_state,9.821999669075012)

    def test_weighted_reference_power_and_gain(self):
        c=self.residual(step=0,reference_gains=[2]+[0]*9)
        c.process(np.ones((4,10)),np.zeros(4))
        self.assertEqual(c.power_state,4)

    def test_fix_rounding_is_explicit_not_confused_with_alusat(self):
        x=np.zeros((1,10));x[0,0]=np.sqrt(.6)
        a=self.residual(rounding='nearest');b=self.residual(rounding='floor')
        a.process(x,[1]);b.process(x,[1])
        np.testing.assert_allclose(a.coefficients,b.coefficients*.5)


if __name__ == '__main__': unittest.main()

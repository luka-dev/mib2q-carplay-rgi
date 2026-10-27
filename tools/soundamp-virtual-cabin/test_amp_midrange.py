import unittest
import numpy as np
from amp_dsp_chain import AmpChain
from audit_amp_midrange import (attribution, blend_front, delayed_front_reference,
                               output_contributions, power_terms)


class MidrangeAttributionTests(unittest.TestCase):
    def test_decomposition_matches_unfactored_renderer_all_outputs(self):
        c = AmpChain()
        x = np.random.default_rng(510).normal(0, .01, (8192, 6))
        y = c.process(x, capture_stages=False)['out']
        for o in range(16):
            contributions = output_contributions(c, x, o)
            np.testing.assert_allclose(contributions.sum(axis=1), y[:, o], atol=1e-13)

    def test_each_input_route_including_delays(self):
        c = AmpChain()
        x = np.zeros((4096, 6)); x[0] = .2
        for o in (0, 1, 10):
            parts = output_contributions(c, x, o)
            for i in range(6):
                isolated = np.zeros_like(x); isolated[:, i] = x[:, i]
                y = c.process(isolated, capture_stages=False)['out'][:, o]
                np.testing.assert_allclose(parts[:, i], y, atol=1e-13)

    def test_cross_term_sign_and_exact_power_closure(self):
        t = np.arange(65536) / 48000
        s = np.sin(2*np.pi*1000*t)
        dry = np.zeros((len(s), 6)); dry[:, 0] = s; dry[:, 1] = s
        up = dry.copy(); up[:, 0] *= .4; up[:, 1] *= -.2
        d, u = power_terms(dry), power_terms(up)
        self.assertGreater(d['lr_cross_power'][4], 0)
        self.assertLess(u['lr_cross_power'][4], 0)
        a = attribution(d, u)
        self.assertAlmostEqual(a['total_change_db'][4], -20, places=10)
        self.assertLess(a['closure_max_abs_db'], 1e-12)

    def test_gain_only_is_not_summation_loss(self):
        x = np.random.default_rng(2).normal(size=(32768, 6)); x[:, 2:] = 0
        a = attribution(power_terms(x), power_terms(.5*x))
        np.testing.assert_allclose(a['self_power_change_db'], 20*np.log10(.5), atol=1e-12)
        np.testing.assert_allclose(a['lr_summation_change_db'], 0, atol=1e-12)

    def test_front_blends_preserve_other_channels_and_originals(self):
        dry = np.ones((8, 2)); x = np.arange(48).reshape(8, 6).astype(float)
        ref = delayed_front_reference(dry, 8, 2, .75)
        np.testing.assert_array_equal(ref[:2], 0)
        np.testing.assert_array_equal(ref[2:], .75)
        original = x.copy()
        for alpha in (0., .5, 1.):
            y = blend_front(x, ref, alpha)
            np.testing.assert_array_equal(y[:, 2:], x[:, 2:])
            np.testing.assert_allclose(y[:, :2], (1-alpha)*x[:, :2]+alpha*ref)
        np.testing.assert_array_equal(x, original)
        with self.assertRaises(ValueError): blend_front(x, ref, -1)
        with self.assertRaises(ValueError): delayed_front_reference(dry, 8, -1, .5)

    def test_scenario_guard_and_invalid_input(self):
        c = AmpChain(); c.upstream_scenario = {}
        with self.assertRaises(ValueError): output_contributions(c, np.zeros((10, 6)), 0)
        with self.assertRaises(ValueError): output_contributions(AmpChain(), np.zeros((10, 2)), 0)


if __name__ == '__main__':
    unittest.main()

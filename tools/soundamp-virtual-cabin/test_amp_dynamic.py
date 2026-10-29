import unittest
import json
from pathlib import Path
import numpy as np
from amp_dynamic import PeakLimiter, gain_step, ComplexFiveBand, ComplexStereoDelay


class DynamicTests(unittest.TestCase):
    def test_complex_ring_delay_is_not_ring_length(self):
        x=np.zeros((1030,10));x[0]=np.arange(1,11)
        y=ComplexStereoDelay([1023]*5,[1023]*5).process(x)
        np.testing.assert_array_equal(y[1023],x[0])
        self.assertEqual(np.count_nonzero(y),10)
        np.testing.assert_array_equal(ComplexStereoDelay([0]*5,[1023]*5).process(x),x)

    def test_complex_ring_clamps_and_retains_state(self):
        a=ComplexStereoDelay([-1,2,3,4,9],[9,9,9,9,5])
        b=ComplexStereoDelay([-1,2,3,4,9],[9,9,9,9,5])
        np.testing.assert_array_equal(a.delay,[0,2,3,4,5])
        x=np.random.default_rng(23).normal(size=(127,10))
        np.testing.assert_array_equal(a.process(x),np.concatenate([b.process(x[:7]),b.process(x[7:])]))

    def five_band(self, gains=None, steps=None):
        p = Path(__file__).resolve().parents[2]/'output/r2/soundamp/output_binding_audit.json'
        d = json.loads(p.read_text())['dynamic_stage_research']['complex_five_band_kernel']
        return ComplexFiveBand(d['coefficients'], gains=[1]*5 if gains is None else gains,
                               steps=[0]*5 if steps is None else steps)

    def test_five_band_recombination_neutral_and_common_ramp(self):
        x = np.random.default_rng(10).normal(size=(4096,10))
        # Extracted float32 complementary sums differ from exact unity <1e-7.
        np.testing.assert_allclose(self.five_band().process(x),x,atol=1e-6)
        y = self.five_band([.5]*5,[.0001]*5).process(x)
        np.testing.assert_allclose(y,x*(.5+.0001*np.arange(1,len(x)+1))[:,None],atol=1e-6)

    def test_five_band_partition_invariance_nonuniform_gain(self):
        x = np.random.default_rng(11).normal(size=(512,10))
        a=self.five_band([.5,.8,1,1.2,.3],[.0001]*5)
        b=self.five_band([.5,.8,1,1.2,.3],[.0001]*5)
        np.testing.assert_allclose(a.process(x),np.concatenate([b.process(x[:32]),b.process(x[32:])]),atol=1e-14)
        self.assertFalse(np.allclose(self.five_band([.5,1,1,1,1]).process(x),x))

    def limiter(self, linked=False):
        return PeakLimiter([1., 1.], [.5, .25], [2., 2.], minimum=.01,
                           maximum=1., initial=1., linked=linked)

    def test_independent_uses_previous_gain_and_strict_threshold(self):
        d = self.limiter()
        np.testing.assert_array_equal(d.process([[2, .5], [2, .5], [2, .5]]),
                                      [[2, .5], [1, .5], [2, .5]])

    def test_linked_uses_channel_zero_rates(self):
        d = self.limiter(True)
        np.testing.assert_array_equal(d.process([[.5, 2], [.5, 2]]), [[.5, 2], [.25, 1]])

    def test_partition_invariance_both_modes(self):
        x = np.random.default_rng(7).normal(size=(513, 2)) * 2
        for linked in (False, True):
            a, b = self.limiter(linked), self.limiter(linked)
            expected = a.process(x)
            actual = np.concatenate([b.process(x[:17]), b.process(x[17:500]), b.process(x[500:])])
            np.testing.assert_array_equal(actual, expected)
            np.testing.assert_array_equal(a.gain, b.gain)

    def test_silence_releases_and_gain_never_exceeds_maximum(self):
        d = self.limiter()
        d.gain[:] = .25
        np.testing.assert_array_equal(d.process(np.zeros((5, 2))), 0)
        np.testing.assert_array_equal(d.gain, 1)

    def test_no_hard_clip_first_overload_sample(self):
        d = self.limiter()
        np.testing.assert_array_equal(d.process([[10, -10]]), [[10, -10]])
        np.testing.assert_array_equal(d.gain, [.5, .25])

    def test_finite_audio_and_shape_required(self):
        for x in ([[float('nan'), 0]], [[1]], [1, 2]):
            with self.assertRaises(ValueError): self.limiter().process(x)

    def test_gain_direct_and_additive(self):
        np.testing.assert_array_equal(gain_step([0, 1], [1, -.5], mode=0), [1, -.5])
        np.testing.assert_allclose(gain_step([.2, 1], [1, -.5], mode=2,
                                            rates=[[.1, -.1], [.1, -.1]]), [.3, -.9])

    def test_gain_multiplicative_start_from_zero_and_clamp(self):
        y = gain_step([0, 1, .99], [1, -.9, 1], mode=3, rates=[[2, .5]]*3)
        self.assertAlmostEqual(y[0], 2 * float.fromhex('0x1.4f8b58p-17'))
        np.testing.assert_array_equal(y[1:], [-.9, 1])

    def test_zero_target_is_asymptotic_in_mode3(self):
        np.testing.assert_array_equal(gain_step([1, 0], [0, 0], mode=3,
                                               rates=[[2, .5]]*2), [.5, 0])
        with self.assertRaises(ValueError): gain_step([1], [0], mode=1)


if __name__ == '__main__': unittest.main()

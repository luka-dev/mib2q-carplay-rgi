import unittest
import numpy as np
from numpy.testing import assert_allclose
from amp_b_spatial import (SpatialStatisticsReference, SpatialControlsReference,
    SpatialDiffuseReference, coherence, fixed_mean, spatial_ramp, frequency_blend,
    COHERENCE_GATE, COHERENCE_EPSILON, BAND_POWER_MIN, RAMP_EPSILON, spatial_finish,
    separator_coefficients, spatial_separator, SpatialFrameReference)
from amp_b_spectral import POWER_MIN


def params():
    p = np.zeros(28)
    p[[13, 15, 17, 19, 22, 23, 25]] = 1
    return p


class SpatialStatisticsTests(unittest.TestCase):
    def model(self, **overrides):
        args = dict(bins=4, edges=[0, 2, 4], direct=1, feedback=0,
                    left_history=[0, 0], right_history=[0, 0], cross_history=[0j, 0j])
        args.update(overrides)
        return SpatialStatisticsReference(**args)

    def test_band_sum_complex_sign_and_not_mean(self):
        a = np.array([1+2j, 3-1j, -2+4j, 2j]); b = np.array([3j, 2+1j, 1, -3j])
        got = self.model().process(a, b)
        expected = [[], [], []]
        for lo, hi in ((0, 2), (2, 4)):
            expected[0].append(sum(v.real**2+v.imag**2 for v in a[lo:hi]))
            expected[1].append(sum(v.real**2+v.imag**2 for v in b[lo:hi]))
            expected[2].append(sum(complex(x.real*y.real+x.imag*y.imag,
                                           x.imag*y.real-x.real*y.imag)
                                   for x, y in zip(a[lo:hi], b[lo:hi])))
        for result, reference in zip(got, expected): assert_allclose(result, reference)

    def test_history_updated_before_output_clamp(self):
        model = self.model(direct=0, feedback=.5, left_history=[-4, 1e32],
                           right_history=[-2, 0], cross_history=[2+4j, -6j])
        a, b, c = model.process(np.zeros(4), np.zeros(4))
        assert_allclose(model.left, [-2, 5e31])
        self.assertEqual(a[0], BAND_POWER_MIN)
        self.assertLess(a[1], model.left[1])
        assert_allclose(c, [1+2j, -3j])
        model.process(np.zeros(4), np.zeros(4))
        assert_allclose(model.left, [-1, 2.5e31])

    def test_floor_before_band_sum_and_empty_bands(self):
        model = self.model(edges=[0, 0, 4])
        a, b, c = model.process(np.zeros(4), np.zeros(4))
        assert_allclose(model.left, [0, 4*POWER_MIN], atol=0)
        assert_allclose(a, BAND_POWER_MIN, atol=0)
        assert_allclose(c, 0)

    def test_multiple_calls_against_independent_covariance(self):
        model = self.model(direct=.3, feedback=.8)
        rng = np.random.default_rng(130658); previous = np.zeros((2, 2, 2), complex)
        for _ in range(5):
            z = rng.normal(size=(2, 4))+1j*rng.normal(size=(2, 4))
            expected = np.array([z[:, :2]@z[:, :2].conj().T, z[:, 2:]@z[:, 2:].conj().T])
            previous = .3*expected+.8*previous
            a, b, c = model.process(*z)
            assert_allclose(a, previous[:, 0, 0].real)
            assert_allclose(b, previous[:, 1, 1].real)
            assert_allclose(c, previous[:, 0, 1])

    def test_coherence_uses_magnitude_not_real_part(self):
        a, b, c = self.model().process(np.ones(4), 1j*np.ones(4))
        assert_allclose(coherence(a, b, c), 1)
        _, _, cancel = self.model().process(np.ones(4), [1, -1, 1j, -1j])
        assert_allclose(coherence(a, b, cancel), np.sqrt(POWER_MIN))

    def test_coherence_floor_epsilon_and_upper_clamp(self):
        got = coherence([0, 1, 1], [0, 1, 1], [0, .5j, 4])
        assert_allclose(got, [np.sqrt(POWER_MIN), .5/(1+COHERENCE_EPSILON), 1])

    def test_invalid_statistics_and_coherence(self):
        with self.assertRaises(ValueError): self.model(cross_history=[1j])
        with self.assertRaises(ValueError): self.model().process([1j], [2j])
        with self.assertRaises(ValueError): coherence([-1], [1], [1])


class SpatialControlTests(unittest.TestCase):
    def test_minimum_drives_gate_not_fixed_mean(self):
        model = SpatialControlsReference(params=params(), minimum_history=0,
                                         mean_history=0, gate_history=0)
        x = np.ones(74); x[0] = .1  # outside fixed mean, inside minimum
        result = model.process(x)
        self.assertAlmostEqual(result['minimum'], .1)
        self.assertAlmostEqual(result['fixed_mean'], 1)
        self.assertEqual(result['target'], 1)
        self.assertEqual(result['gate'], 1)
        self.assertEqual(model.process(np.ones(74))['target'], 0)

    def test_strict_float32_threshold_and_asymmetric_smoothing(self):
        p = params(); p[23:27] = [.2, .7, .4, .3]
        model = SpatialControlsReference(params=p, minimum_history=0,
                                         mean_history=0, gate_history=.5)
        result = model.process(np.full(74, COHERENCE_GATE))
        self.assertEqual(result['target'], 1)
        self.assertAlmostEqual(result['gate'], .2+.7*.5)
        result = model.process(np.ones(74))
        self.assertEqual(result['target'], 0)
        self.assertAlmostEqual(result['gate'], .3*(.2+.7*.5))

    def test_minimum_and_mean_temporal_coefficients_not_normalized(self):
        p = params(); p[13:15] = [.2, .4]
        model = SpatialControlsReference(params=p, minimum_history=.6,
                                         mean_history=.8, gate_history=0)
        result = model.process(np.full(74, .5))
        self.assertAlmostEqual(result['minimum'], .2*.5+.4*.6)
        self.assertAlmostEqual(result['fixed_mean'], .2*.5+.4*.8)

    def test_fixed_mean_exact_range(self):
        x = np.arange(74, dtype=float)
        expected = .25*np.mean(x[9:60])+.7*8
        self.assertAlmostEqual(fixed_mean(x, previous=8, direct=.25, feedback=.7), expected)
        x[:9] = -10000; x[60:] = 10000
        self.assertAlmostEqual(fixed_mean(x, previous=8, direct=.25, feedback=.7), expected)

    def test_ramp_supplied_step_and_additive_floor(self):
        out = spatial_ramp(bins=8, start=-1, end=0, low_bin=2, high_bin=5, step=.2)
        assert_allclose(out, [-1+RAMP_EPSILON]*2+
                        [-.8+RAMP_EPSILON, -.6+RAMP_EPSILON, -.4+RAMP_EPSILON]+
                        [RAMP_EPSILON]*3)
        self.assertLess(out[0], 0)  # not max(start, epsilon)

    def test_ramp_zero_and_equal_bounds(self):
        assert_allclose(spatial_ramp(bins=4, start=1, end=2, low_bin=0, high_bin=0, step=99),
                        [1, 2, 2, 2])
        assert_allclose(spatial_ramp(bins=4, start=1, end=2, low_bin=2, high_bin=2, step=99),
                        [1, 1, 2, 2])

    def test_reject_invalid_control_inputs(self):
        with self.assertRaises(ValueError): fixed_mean(np.ones(59), previous=0, direct=1, feedback=0)
        with self.assertRaises(ValueError): spatial_ramp(bins=4, start=0, end=1, low_bin=3, high_bin=2, step=0)
        model = SpatialControlsReference(params=params(), minimum_history=0, mean_history=0, gate_history=0)
        with self.assertRaises(ValueError): model.process(np.full(74, 1.1))


class SpatialDiffuseTests(unittest.TestCase):
    def model(self, **overrides):
        args = dict(bins=4, edges=[0, 2, 4], params=params(), history=[0, 0])
        args.update(overrides)
        return SpatialDiffuseReference(**args)

    def test_mask_is_sqrt_one_minus_coherence_not_one_minus_square(self):
        left = np.array([1+2j, 3j, -1, 2]); right = 2j*left
        a, b, mask = self.model().process(left, right, [.75, 1])
        assert_allclose(mask, [.5, .5, 0, 0])
        assert_allclose(a, left*mask); assert_allclose(b, right*mask)

    def test_history_before_expansion_and_clamp(self):
        p = params(); p[19:23] = [.3, .8, .2, .4]
        model = self.model(params=p, history=[1, 2])
        _, _, mask = model.process(np.ones(4), np.ones(4), [0, 1])
        assert_allclose(model.history, [1.1, 1.6])
        assert_allclose(mask, .4)
        model.process(np.ones(4), np.ones(4), [1, 1])
        assert_allclose(model.history, [.88, 1.28])

    def test_frequency_blend_endpoint_and_parameter_order(self):
        # Feedback0: each bidirectional smoother is direct^2, not direct.
        p = params(); p[15:19] = [.25, 0, .5, 0]
        _, _, mask = self.model(params=p).process(np.ones(4), np.ones(4), [0, 0])
        weight = np.array([1, .75, .5, .25])
        assert_allclose(mask, weight*.5**2+(1-weight)*.25**2)
        self.assertNotEqual(mask[-1], .25**2)

    def test_frequency_blend_with_independent_recursive_passes(self):
        x = np.arange(1, 9, dtype=float)
        def smooth(g, h):
            y = []; old = x[0]
            for v in x: old = g*v+h*old; y.append(old)
            old = y[-1]; out = []
            for v in y[::-1]: old = g*v+h*old; out.append(old)
            return np.array(out[::-1])
        w = (len(x)-np.arange(len(x)))/len(x)
        assert_allclose(frequency_blend(x, low_direct=.3, low_feedback=.7,
                                       high_direct=.8, high_feedback=.2),
                        w*smooth(.3, .7)+(1-w)*smooth(.8, .2))

    def test_silence_floor_is_not_generated_audio(self):
        stats = SpatialStatisticsReference(bins=4, edges=[0, 2, 4], direct=1, feedback=0,
                      left_history=[0, 0], right_history=[0, 0], cross_history=[0, 0])
        c = coherence(*stats.process(np.zeros(4), np.zeros(4)))
        a, b, mask = self.model().process(np.zeros(4), np.zeros(4), c)
        self.assertTrue((mask > 0).all())
        assert_allclose(a, 0); assert_allclose(b, 0)

    def test_invalid_diffuse_inputs(self):
        p = params(); p[21:23] = [2, 1]
        with self.assertRaises(ValueError): self.model(params=p)
        with self.assertRaises(ValueError): self.model().process(np.ones(4), np.ones(4), [-.1, 0])
        with self.assertRaises(ValueError): self.model().process(np.ones(3), np.ones(3), [0, 0])


class SpatialFinishTests(unittest.TestCase):
    def test_residual_sign_blend_and_gate_scope(self):
        a = np.array([2+3j, -4j]); b = np.array([1j, 4+3j])
        c, d = .25*a, .5*b; e, f = .6*a, .8*b
        out = spatial_finish(a, b, c, d, e, f, gate_selector=1, gate=.2, mix=.3)
        assert_allclose(out[0], .7*.2*(a-c)+.3*e)
        assert_allclose(out[1], .7*.2*(b-d)+.3*f)

    def test_gate_selector_requires_exact_one(self):
        for selector in (0, 2, 3, 0xFFFFFFFF):
            out = spatial_finish([2j], [4], [1j], [3], [9], [8],
                                 gate_selector=selector, gate=0, mix=0)
            assert_allclose(out[0], [1j]); assert_allclose(out[1], [1])
        out = spatial_finish([2j], [4], [1j], [3], [9], [8],
                             gate_selector=1, gate=0, mix=0)
        assert_allclose(out, 0)

    def test_mix_is_not_clipped_to_zero_one(self):
        out = spatial_finish([2], [3], [1], [1], [4], [5],
                             gate_selector=0, gate=1, mix=2)
        assert_allclose(out[0], [7]); assert_allclose(out[1], [8])


class SpatialSeparatorTests(unittest.TestCase):
    def matrices(self, a, b, c, ramp, **overrides):
        args = dict(eigen_selector=0, regularization=.3, fallback_scale=2e-6,
                    balance_enabled=False, mean_coherence=.5, balance_slope=1, balance_offset=0)
        args.update(overrides)
        return separator_coefficients(a, b, c, ramp, **args)

    def test_known_complex_matrix_and_amplitude(self):
        # D=(4+1)*2-2+10*.3=11; factor=.6/11, not a unity normalization.
        got = self.matrices([4], [1], [1+2j], [.6])[0]
        expected = .6/11*np.array([[7, 2+4j], [2-4j, 1]])
        assert_allclose(got, expected)

    def test_matrix_matches_literal_register_pipeline(self):
        # Independently retain names/old operands from130086..1300C0, then
        #1300E8..130145, finite branch with p2==0. Not an ISA emulator.
        for a, b, c, ramp in ((4, 1, 1+2j, .6), (3, 9, -1+1j, 1.2)):
            f6 = 2.; f0 = a; f1 = b
            f10 = f0*f6
            f3, f9 = f0+f1, f0-f1
            f8 = f3*f6
            f5, f8 = c.real*f6, f8-2
            f8 += 3  # 10*regularization
            f0 = 1/f8
            f10, f15 = f1*f6, f10-1
            f0, f1 = f0*ramp, f10-1
            f7 = f5*f0; f14 = f15*f0; f13 = f1*f0
            f5, f6 = -c.imag*f6*f0, c.imag*f6*f0
            f15, f12 = f7, f13
            x, y = 2+3j, -4+5j
            lr, li, rr, ri = x.real, x.imag, y.real, y.imag
            # Final scalar products / additions extracted from kernel.
            left = complex(f14*lr+f7*rr-f6*ri, f14*li+f7*ri+f6*rr)
            right = complex(f12*rr+f15*lr-f5*li, f12*ri+f15*li+f5*lr)
            got = spatial_separator([x], [y], edges=[0, 1],
                                    matrices=self.matrices([a], [b], [c], [ramp]))
            assert_allclose([got[0][0], got[1][0]], [left, right])

    def test_eigen_estimate_ignores_imaginary_cross(self):
        real = self.matrices([4], [4], [2], [1], eigen_selector=1, regularization=1)
        complex_cross = self.matrices([4], [4], [2+5j], [1], eigen_selector=1, regularization=1)
        # lambda=.5*(8-sqrt(4*2^2))=2, scale=.5, denominator12.
        assert_allclose(complex_cross[0], np.array([[1, 1+2.5j], [1-2.5j, 1]])/12)
        assert_allclose(real[:, [0, 1], [0, 1]], complex_cross[:, [0, 1], [0, 1]])

    def test_eigen_floor_fallback_and_exact_selector(self):
        args = ([1], [1], [1], [1])
        fallback = self.matrices(*args, eigen_selector=0)
        assert_allclose(self.matrices(*args, eigen_selector=1), fallback)
        assert_allclose(self.matrices(*args, eigen_selector=2), fallback)
        # Positive lambda: only ==1 changes scale.
        a = self.matrices([4], [4], [2], [1], eigen_selector=0)
        assert_allclose(self.matrices([4], [4], [2], [1], eigen_selector=2), a)
        self.assertFalse(np.allclose(self.matrices([4], [4], [2], [1], eigen_selector=1), a))

    def test_balance_weights_and_identity_complement(self):
        plain = self.matrices([1], [4], [1j], [.8])[0]
        got = self.matrices([1], [4], [1j], [.8], balance_enabled=True)[0]
        g = 1/(1+.6); weight = .5*g*g+.5*g
        expected = plain.copy()
        expected[0, 0] = 1+weight*(plain[0, 0]-1)
        expected[0, 1] *= weight
        assert_allclose(got, expected)
        # Mirror channels: opposite row is modified, not same weight on both.
        mirrored = self.matrices([4], [1], [-1j], [.8], balance_enabled=True)[0]
        assert_allclose(mirrored, got[::-1, ::-1])

    def test_balance_blend_clamps_and_input_powers_not_mutated(self):
        a = np.array([0.]); b = np.array([1.])
        zero = self.matrices(a, b, [0], [1], balance_enabled=True, balance_offset=-100)
        same = self.matrices(a, b, [0], [1], balance_enabled=True, balance_slope=0, balance_offset=0)
        assert_allclose(zero, same); assert_allclose(a, [0])

    def test_half_open_bands_empty_band_and_complex_sign(self):
        matrices = np.array([[[1, 2j], [-2j, 3]], [[99, 0], [0, 99]], [[4, 1], [1, 5]]])
        a, b = spatial_separator([1, 2, 3], [4, 5, 6], edges=[0, 1, 1, 3], matrices=matrices)
        assert_allclose(a, [1+8j, 13, 18]); assert_allclose(b, [12-2j, 27, 33])

    def test_zero_denominator_is_not_silently_regularized(self):
        with self.assertRaises(ValueError):
            self.matrices([1], [1], [0], [1], regularization=0, fallback_scale=1e-6)


class SpatialFrameTests(unittest.TestCase):
    def model(self, **overrides):
        p = params(); p[4:6] = 1; p[9:12] = [.3, 2e-6, 1]; p[27] = .35
        args = dict(bins=148, edges=np.arange(0, 149, 2), params=p, eigen_selector=0,
                    balance_enabled=False, gate_selector=0, low_band=0, high_band=73,
                    balance_slope=1, balance_offset=0, left_history=np.zeros(74),
                    right_history=np.zeros(74), cross_history=np.zeros(74, complex),
                    diffuse_history=np.zeros(74), minimum_history=0, mean_history=0, gate_history=0)
        args.update(overrides)
        return SpatialFrameReference(**args)

    def test_composition_against_independent_covariance_formula(self):
        rng = np.random.default_rng(130771)
        z = rng.normal(size=(2, 148))+1j*rng.normal(size=(2, 148))
        expected = np.empty_like(z)
        for band in range(74):
            sl = slice(2*band, 2*band+2); block = z[:, sl]
            covariance = block@block.conj().T
            c = min(1, abs(covariance[0, 1])/(np.sqrt(
                covariance[0, 0].real*covariance[1, 1].real)+COHERENCE_EPSILON))
            matrix = (2*covariance-np.eye(2))/(2*np.trace(covariance).real+1)
            expected[:, sl] = .65*(block-matrix@block)+.35*np.sqrt(1-c)*block
        assert_allclose(self.model().process(*z, enabled=True), expected, atol=2e-14)

    def test_disabled_is_copy_and_keeps_histories(self):
        model = self.model()
        x = np.ones(148)+2j; y = -3*x
        got = model.process(x, y, enabled=False)
        assert_allclose(got, [x, y])
        assert_allclose(model.statistics.left, 0)
        assert_allclose(model.diffuse.history, 0)
        self.assertEqual(model.controls.gate, 0)
        self.assertFalse(np.shares_memory(got[0], x))

    def test_repeated_frames_preserve_three_independent_histories(self):
        model = self.model()
        model.statistics.direct = .25; model.statistics.feedback = .75
        model.diffuse.params[19:21] = [.3, .7]
        model.controls.params[13:15] = [.4, .6]
        x = np.tile([1., 1.], 74); y = np.tile([1., -1.], 74)
        for step in range(1, 4):
            model.process(x, y, enabled=True)
            assert_allclose(model.statistics.left, 2*(1-.75**step))
            expected_coh = np.sqrt(POWER_MIN)
            assert_allclose(model.diffuse.history, np.sqrt(1-expected_coh)*(1-.7**step))
            self.assertAlmostEqual(model.controls.minimum, expected_coh*(1-.6**step))

    def test_reject_short_band_map_even_if_bypass_requested_later(self):
        with self.assertRaises(ValueError): self.model(edges=[0, 148])


if __name__ == '__main__': unittest.main()

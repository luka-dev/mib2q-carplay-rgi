import unittest
import numpy as np
from numpy.testing import assert_allclose
from amp_b_transform import forward_real, inverse_real, sine_window, NeutralOverlapReference


class TransformTests(unittest.TestCase):
    def test_forward_against_independent_real_fft(self):
        rng = np.random.default_rng(901)
        for n in (8, 16, 64, 1024):
            x = rng.normal(size=n)
            assert_allclose(forward_real(x), np.fft.rfft(x), atol=5e-14)

    def test_inverse_independent_spectrum(self):
        rng = np.random.default_rng(902)
        for n in (8, 16, 64, 1024):
            x = rng.normal(size=n//2+1)+1j*rng.normal(size=n//2+1)
            assert_allclose(inverse_real(x), np.fft.irfft(x), atol=1e-15)

    def test_dc_nyquist_quarter_and_impulse_scales(self):
        n = 1024; t = np.arange(n)
        for x in (np.ones(n), (-1.)**t, np.cos(np.pi*t/2),
                  np.sin(np.pi*t/2), np.eye(1, n)[0]):
            assert_allclose(forward_real(x), np.fft.rfft(x), atol=1e-12)
            assert_allclose(inverse_real(forward_real(x)), x, atol=1e-14)
        self.assertEqual(forward_real(np.ones(n))[0], n)

    def test_small_direct_dft(self):
        x = np.arange(16)**2
        expected = np.exp(-2j*np.pi*np.outer(np.arange(9), np.arange(16))/16)@x
        assert_allclose(forward_real(x), expected, atol=4e-12)

    def test_window_squared_overlap_is_unity(self):
        w = sine_window(1024)
        assert_allclose(w, w[::-1], atol=0)
        assert_allclose(w[:512]**2+w[512:]**2, 1, atol=5e-16)
        self.assertGreater(np.max(w[:512]+w[512:]), 1.4)

    def test_neutral_stream_unity_and_one_hop_latency(self):
        model = NeutralOverlapReference()
        x = np.random.default_rng(903).normal(size=512*7)
        y = np.concatenate([model.process(a) for a in
                            np.concatenate((x, np.zeros(512))).reshape(-1, 512)])
        assert_allclose(y, np.concatenate((np.zeros(512), x)), atol=3e-15)

    def test_no_frequency_selective_boost(self):
        for frequency in (30, 60, 120, 1000, 10000):
            model = NeutralOverlapReference()
            x = np.sin(2*np.pi*frequency*np.arange(512*8)/48000)
            y = np.concatenate([model.process(a) for a in
                                np.concatenate((x, np.zeros(512))).reshape(-1, 512)])[512:]
            assert_allclose(y, x, atol=2e-15)

    def test_reset_and_nonunity_window_are_not_hidden(self):
        model = NeutralOverlapReference(16, window=2*sine_window(16))
        model.process(np.ones(8))
        assert_allclose(model.process(np.zeros(8)), 4, atol=3e-15)
        model.reset()
        assert_allclose(model.process(np.zeros(8)), 0, atol=0)

    def test_explicit_float32_twiddles(self):
        n = 1024; x = np.random.default_rng(904).normal(size=n)
        w = np.exp(-2j*np.pi*np.arange(n//4)/n).astype(np.complex64)
        assert_allclose(inverse_real(forward_real(x, twiddles=w), twiddles=w), x, atol=3e-7)

    def test_reject_invalid_shapes_and_sizes(self):
        for x in (np.zeros(6), np.zeros(4), np.zeros((8, 1)),
                  np.full(8, np.nan), np.ones(8, dtype=complex)):
            with self.assertRaises(ValueError): forward_real(x)
        with self.assertRaises(ValueError): NeutralOverlapReference().process(np.zeros(1024))
        with self.assertRaises(ValueError): forward_real(np.zeros(8), twiddles=np.ones(3))


if __name__ == '__main__':
    unittest.main()

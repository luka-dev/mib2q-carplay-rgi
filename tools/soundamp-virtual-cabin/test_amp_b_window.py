import json
import tempfile
from pathlib import Path
import unittest

import numpy as np

from amp_b_window import load_spectral_tables, LOADER
from amp_b_transform import sine_window, NeutralOverlapReference


class FirmwareWindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tables = load_spectral_tables()

    def test_stored_polynomial_against_independent_monomial_sum(self):
        theta = (np.arange(512)+.5)*float(np.float32(np.pi))/1024
        # Independently decode recorded source words, not the helper's evaluator.
        words = np.asarray(self.tables['metadata']['polynomial_words'], dtype='<u4')
        coefficients = words.view('<f4').astype(float)
        expected = theta.copy()
        for coefficient, exponent in zip(coefficients, (15,13,11,9,7,5,3)):
            expected += coefficient*theta**exponent
        expected = expected.astype(np.float32).astype(float)
        np.testing.assert_array_equal(self.tables['window'][:512], expected)
        np.testing.assert_array_equal(self.tables['window'][512:], expected[::-1])

    def test_window_scale_phase_and_no_silent_normalization(self):
        window = self.tables['window']
        self.assertEqual(window.shape, (1024,))
        self.assertGreater(window[0], 0)
        self.assertLess(window[0], .00154)
        self.assertGreater(window[511], .99999)
        delta = abs(window-sine_window(1024)).max()
        self.assertGreater(delta, 1e-8)
        self.assertLess(delta, 5e-8)
        overlap = window[:512]**2+window[512:]**2
        self.assertGreater(abs(overlap-1).max(), 1e-8)
        self.assertLess(abs(overlap-1).max(), 2e-7)

    def test_explicit_tables_used_by_overlap_reference(self):
        model = NeutralOverlapReference(window=self.tables['window'], twiddles=self.tables['twiddles'])
        np.testing.assert_array_equal(model.window, self.tables['window'])
        np.testing.assert_array_equal(model.twiddles, self.tables['twiddles'])
        signal = np.random.default_rng(125).normal(size=1024)
        result = np.concatenate([model.process(block) for block in
            np.concatenate((signal,np.zeros(512))).reshape(-1,512)])
        expected = np.concatenate((np.zeros(512),signal))
        self.assertGreater(abs(result-expected).max(), 1e-8)
        np.testing.assert_allclose(result,expected,rtol=0,atol=8e-7)

    def test_metadata_preserves_numerical_and_live_boundaries(self):
        metadata = self.tables['metadata']
        json.dumps(metadata, allow_nan=False)
        self.assertFalse(metadata['hardware_bit_exact'])
        self.assertFalse(metadata['live_allocated_window_readback'])
        self.assertEqual(metadata['normalization'], 'none')
        self.assertEqual(metadata['twiddle_complex_entries_used'], 256)
        self.assertEqual(len(metadata['code_ranges']), 3)
        self.assertIn('NumPy complex FFT', metadata['complex_fft_boundary'])

    def test_wrong_loader_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'invalid.ldr'
            data = bytearray(LOADER.read_bytes());data[-1] ^= 1
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'exact audited'):
                load_spectral_tables(path)


if __name__ == '__main__':
    unittest.main()

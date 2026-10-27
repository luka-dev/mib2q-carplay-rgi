import unittest
import numpy as np
from amp_dsp_chain import AmpChain
from audit_amp_retention_candidates import peak_metrics


class RetentionMetricTests(unittest.TestCase):
    def test_correct_matrix_and_full_clip_chunk_boundary(self):
        x = np.zeros((65538, 6)); x[-1, 0] = 1.; x[-1, 4] = .4
        m = AmpChain().M
        report = peak_metrics(x, m)
        np.testing.assert_allclose(report['matrix_bus_peak'], abs(m @ x[-1]))
        self.assertGreater(report['matrix_bus_peak'][6], 1.8)
        self.assertEqual(report['pcm_rail_samples'], 1)

    def test_negative_rail_and_zero_centre(self):
        x = np.array([[0, -1, 0, 0, 0, 0], [0, 0, 0, -.125, 0, 0]])
        report = peak_metrics(x, AmpChain().M)
        self.assertEqual(report['pcm_rail_samples'], 1)
        self.assertEqual(report['pcm_peak'][2], 0)


if __name__ == '__main__':
    unittest.main()

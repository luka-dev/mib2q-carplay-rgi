import unittest
import numpy as np
from scipy.signal import sosfilt
from amp_b_input_eq import BInputBankEQReference


class BInputBankEQTests(unittest.TestCase):
    def test_fixed_coefficients_match_independent_scipy_with_history(self):
        rng = np.random.default_rng(20260920)
        records = rng.uniform(-.15, .15, (8, 3, 4))
        gains = np.linspace(.3, 1.4, 8)
        history = rng.normal(0, .03, (8, 3, 2))
        audio = rng.normal(size=(1024, 8))
        ref = BInputBankEQReference(records=records, gains=gains, history=history)
        out = ref.process(audio)
        for ch in range(8):
            c = records[ch]
            sos = np.column_stack((np.ones(3), c[:, 3], c[:, 2],
                                   np.ones(3), -c[:, 1], -c[:, 0]))
            expected, state = sosfilt(sos, audio[:, ch]*gains[ch], zi=history[ch])
            np.testing.assert_allclose(out[:, ch], expected, atol=2e-15)
            np.testing.assert_allclose(ref.state[ch], state, atol=2e-15)

    def test_each_input_remains_in_its_own_channel(self):
        records = np.tile([-.12, .5, .03, .15], (8, 3, 1))
        for channel in range(8):
            audio = np.zeros((128, 8)); audio[0, channel] = 1
            ref = BInputBankEQReference(records=records, gains=np.ones(8))
            result = ref.process(audio)
            np.testing.assert_array_equal(np.delete(result, channel, axis=1), 0)
            self.assertGreater(np.linalg.norm(result[:, channel]), 1)

    def test_record_order_and_gain_are_not_swapped_or_applied_per_section(self):
        c = np.array([[[-.2, .6, .1, .3], [0, 0, 0, 0]]])
        ref = BInputBankEQReference(records=c, gains=[2])
        x = np.zeros((8, 1)); x[0] = 1
        out = ref.process(x)
        self.assertEqual(out[0, 0], 2)  # gain once; no clipping
        self.assertAlmostEqual(out[1, 0], 1.8)
        self.assertAlmostEqual(out[2, 0], .88)

    def test_stream_partition_and_empty_input_preserve_state(self):
        c = np.tile([-.12, .5, .03, .15], (2, 3, 1))
        x = np.random.default_rng(4).normal(size=(1001, 2))
        full = BInputBankEQReference(records=c, gains=[1, .5])
        split = BInputBankEQReference(records=c, gains=[1, .5])
        a = split.process(x[:117]); saved = split.state.copy()
        self.assertEqual(split.process(x[:0]).shape, (0, 2))
        np.testing.assert_array_equal(split.state, saved)
        np.testing.assert_array_equal(np.r_[a, split.process(x[117:])], full.process(x))

    def test_constructor_copies_configuration_and_history(self):
        c = np.zeros((2, 1, 4)); g = np.ones(2); h = np.zeros((2, 1, 2))
        ref = BInputBankEQReference(records=c, gains=g, history=h)
        c[:] = 3; g[:] = 0; h[:] = 7
        x = np.ones((10, 2)); old = x.copy()
        np.testing.assert_array_equal(ref.process(x), old)
        np.testing.assert_array_equal(x, old)

    def test_invalid_input_does_not_advance_state(self):
        ref = BInputBankEQReference(records=np.zeros((2, 3, 4)), gains=[1, 1])
        saved = ref.state.copy()
        for x in [np.zeros((8, 3)), np.full((8, 2), np.nan),
                  np.ones((8, 2), complex), np.zeros(8)]:
            with self.assertRaises(ValueError): ref.process(x)
            np.testing.assert_array_equal(ref.state, saved)

    def test_invalid_configuration_is_rejected(self):
        for c, g, h in [(np.zeros((2, 0, 4)), [1, 1], None),
                        (np.zeros((2, 3, 5)), [1, 1], None),
                        (np.zeros((2, 3, 4)), [1], None),
                        (np.zeros((2, 3, 4)), [1, np.inf], None),
                        (np.zeros((2, 3, 4)), [1, 1], np.zeros((2, 3, 3)))]:
            with self.assertRaises(ValueError):
                BInputBankEQReference(records=c, gains=g, history=h)


if __name__ == '__main__': unittest.main()

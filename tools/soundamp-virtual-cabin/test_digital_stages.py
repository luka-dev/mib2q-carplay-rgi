import json
from pathlib import Path
import sys
import unittest
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from digital_stages import select, pair_mix, pair_gains, PairState, PackedOutputEQ, run_ordered


class DigitalStagesTests(unittest.TestCase):
    def test_selector_invalid_indices_mute_not_wrap(self):
        np.testing.assert_array_equal(select(np.array([[1, 2, 3]]), [2, -1, 3, 0xFFFFFFFF, 0]),
                                      [[3, 0, 0, 0, 1]])
        with self.assertRaises(ValueError):
            select(np.array([[1, 2]]), [0.5])

    def test_pair_layout_and_no_implicit_normalization(self):
        np.testing.assert_array_equal(pair_mix(np.array([[1, 2, 10, 20]]), [1, 2, 3, 4]),
                                      [[21, 86]])

    def test_off_row_and_mute_are_not_matrix_floor(self):
        path = HERE.parents[1] / "output/r2/soundamp/output_binding_audit.json"
        audit = json.loads(path.read_text())
        row, gains = pair_gains(audit, PairState(0, True, True, 0xFC))
        self.assertEqual(row, 1)
        self.assertEqual(gains[16], 0)
        self.assertAlmostEqual(gains[17], 10**(-99/20))
        self.assertEqual(pair_gains(audit, PairState(0, False, True, 0xFC))[0], 0)
        with self.assertRaises(ValueError):
            pair_gains(audit, PairState(0, True, True, 0xC0))
        _, adjusted = pair_gains(audit, PairState(0, True, True, 0xC0, 0))
        self.assertGreater(adjusted[5], 0)  # C0 odd sentinel clamps to -99 dB
        self.assertEqual(gains[5], 0)       # FC retains exact mute

    def test_packed_zero_count_and_sign_order(self):
        # y[n] = x[n] + 0.5*y[n-1]; next channel has a x2 stage.
        eq = PackedOutputEQ([1, 0, 1], [[.5, 0, 1, 0, 0], [0, 0, 2, 0, 0]])
        source = np.zeros((5, 3)); source[0] = [1, 3, 4]
        result = eq.process(source)
        np.testing.assert_array_equal(result[:, 0], [1, .5, .25, .125, .0625])
        np.testing.assert_array_equal(result[:, 1], source[:, 1])
        np.testing.assert_array_equal(result[:, 2], source[:, 2] * 2)
        eq.reset()
        split = np.concatenate([eq.process(source[:2]), eq.process(source[2:])])
        np.testing.assert_array_equal(split, result)
        with self.assertRaises(ValueError):
            PackedOutputEQ([145], np.zeros((145, 5)))

    def test_schedule_uses_prior_values_before_replacing_ids(self):
        signals = {1: np.array([[10.]]), 2: np.array([[20.]])}
        stages = [dict(kind="selector", inputs=[1, 2], outputs=[2, 1], indices=[0, 1]),
                  dict(kind="pair_mixer", inputs=[1, 2], outputs=[3], gains=[1, 2])]
        result = run_ordered(signals, stages)
        self.assertEqual(result[3][0, 0], 40)
        self.assertEqual(signals[1][0, 0], 10)
        with self.assertRaisesRegex(ValueError, "unmodelled"):
            run_ordered(signals, [dict(kind="complex", inputs=[1], outputs=[2])])


if __name__ == "__main__":
    unittest.main()

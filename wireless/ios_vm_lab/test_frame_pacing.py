import random
import unittest

from extract_native_video import parse_records
from frame_pacing import intervals, report
from test_extract_native_video import record


class FramePacingTests(unittest.TestCase):
    def test_configuration_not_counted_as_picture_and_legacy_has_no_clock(self):
        text = record(1, b"\0\0\0\1\x67abc")+record(2, b"\0\0\1\x65xyz")
        value = report(text)
        self.assertEqual(value["picture_buffers"], 1)
        self.assertIsNone(value["qnx_decoder_input"])

    def test_separate_clocks_and_source_completion(self):
        text = "".join(record(i, b"\0\0\0\1\x65abc").replace("bytes=8", f"bytes=8 mono_ns={10000000000+i*40000000}") for i in range(1, 4))
        source = "".join(f"[native-source-qemu] frame={i} elapsed_ms={i*1000:.3f} render_ms=2.500\n" for i in range(1, 4))
        source += "[mhi2-screen-framebuffer] finished result=0 submitted=3\n"
        value = report(text, source)
        self.assertEqual(value["qnx_decoder_input"]["observed_fps"], 25)
        self.assertEqual(value["native_source"]["observed_fps"], 1)
        self.assertEqual(value["native_source"]["render_ms"]["median"], 2.5)
        for malformed in (text.replace("mono_ns=10120000000", "mono_ns=10000000000"),):
            with self.assertRaises(ValueError): report(malformed)
        for malformed in (source.replace("frame=2", "frame=1"), source.replace("submitted=3", "submitted=4")):
            with self.assertRaises(ValueError): report(text, malformed)

    def test_zero_clock_is_unavailable_and_corrupted_capture_is_rejected(self):
        text = record(1, b"\0\0\0\1\x65abc").replace("bytes=8", "bytes=8 mono_ns=0")
        self.assertFalse(report(text)["qnx_timestamps_available"])
        with self.assertRaises(ValueError): parse_records(text.replace("complete=1", "complete=0"))

    def test_random_intervals_match_independent_span_reference(self):
        rng = random.Random(6030)
        for _ in range(1000):
            values = [0]
            for _ in range(rng.randint(1, 80)): values.append(values[-1]+rng.randint(1, 3000))
            result = intervals(values)
            self.assertEqual(result["span_ms"], sum(b-a for a, b in zip(values, values[1:])))
            self.assertAlmostEqual(result["observed_fps"], (len(values)-1)*1000/values[-1])

    def test_unusable_clock_values_do_not_produce_numeric_claims(self):
        for bad in (float('nan'),float('inf'),float('-inf')):
            with self.assertRaises(ValueError): intervals([0,bad])


if __name__ == "__main__":
    unittest.main()

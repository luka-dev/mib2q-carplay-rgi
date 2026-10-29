import plistlib
import struct
import unittest

from .screen_pair_pacing import compare


def capture(streams, end_ms=2000):
    records = []
    for port, pictures, telemetry in streams:
        sequence = 100
        def tcp(ms, payload=b"", flags=0x18):
            nonlocal sequence
            segment = struct.pack(">HHIIBBHHH", 49152+port%2, port, sequence, 0, 0x50, flags, 65535, 0, 0)+payload
            ip = struct.pack(">IHBB", 0x60000000, len(segment), 6, 0)+bytes(32)
            records.append((ms, bytes(12)+b"\x86\xdd"+ip+segment))
            sequence += len(payload)+(1 if flags & 2 else 0)
        tcp(0, flags=2)
        for ms, opcode, body in sorted([(ms, 0, b"ciphertext") for ms in pictures]+
                                      [(ms, 5, plistlib.dumps(value)) for ms, value in telemetry]):
            tcp(ms, struct.pack("<I", len(body))+bytes([opcode])+bytes(123)+body)
        tcp(end_ms, flags=0x10)
    data = struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)
    for ms, frame in sorted(records, key=lambda item: item[0]):
        data += struct.pack("<IIII", ms//1000, ms%1000*1000, len(frame), len(frame))+frame
    return data


class ScreenPairTests(unittest.TestCase):
    def test_smooth_primary_can_coexist_with_alt_sink_overflow(self):
        data = capture([(6030, [10+i*33 for i in range(60)], [(500, {"submitSurfaceFPS": 30, "sinkOverflowDropFPS": 0})]),
                        (6031, [10+i*66 for i in range(30)], [(500, {"submitSurfaceFPS": 30, "sinkOverflowDropFPS": 15})])])
        result = compare(data, 0, 2000, 1000)["screens"]
        self.assertEqual(result["primary"]["window_au_per_second"], 30)
        self.assertEqual(result["alt"]["window_au_per_second"], 15)
        self.assertEqual(result["alt"]["sender_counters"]["sinkOverflowDropFPS"]["mean"], 15)
        self.assertIsNone(result["alt"]["sender_counters"]["encoderQueueDropFPS"])

    def test_brief_thirty_fps_burst_is_not_sustained_thirty(self):
        data = capture([(6030, [10, 43], []), (6031, [10+i*66 for i in range(30)], [])])
        primary = compare(data, 0, 2000, 1000)["screens"]["primary"]
        self.assertGreater(primary["wire_interval_cadence"]["observed_fps"], 30)
        self.assertEqual(primary["window_au_per_second"], 1)
        self.assertEqual(primary["bins_without_video"], 1)

    def test_partial_capture_is_rejected_and_boundaries_are_counted_once(self):
        data = capture([(6030, [0, 1000, 2000], []), (6031, [], [])])
        result = compare(data, 0, 2000, 1000)["screens"]
        self.assertEqual([b["video_aus"] for b in result["primary"]["bins"]], [1, 1])
        self.assertEqual(result["primary"]["video_aus"], 2)
        self.assertFalse(result["alt"]["flow_available"])
        with self.assertRaisesRegex(ValueError, "complete comparison window"):
            compare(data, 0, 2500)

    def test_bad_windows_and_same_ports_are_rejected(self):
        data = capture([(6030, [10], [])])
        for a, b, width in [(0, float("inf"), 1000), (1, 1, 1000), (0, 2000, 0), (0, 2000, 1)]:
            with self.assertRaises(ValueError):
                compare(data, a, b, width)
        with self.assertRaises(ValueError):
            compare(data, 0, 2000, primary_port=6031)


if __name__ == "__main__":
    unittest.main()

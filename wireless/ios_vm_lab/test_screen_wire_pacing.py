import random
import struct
import unittest

from screen_wire_pacing import report


def frame(seq, data=b"", flags=0x18):
    tcp = struct.pack(">HHIIBBHHH", 49152, 6030, seq, 0, 0x50, flags, 65535, 0, 0)+data
    ip = struct.pack(">IHBB", 0x60000000, len(tcp), 6, 64)+bytes(range(16))+bytes(range(16, 32))
    return bytes(12)+b"\x86\xdd"+ip+tcp


def pcap(items):
    data = struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)
    for ms, payload in items:
        data += struct.pack("<IIII", ms//1000, ms%1000*1000, len(payload), len(payload))+payload
    return data


def picture(opcode=0, data=b"encrypted bytes"):
    return struct.pack("<I", len(data))+bytes([opcode])+bytes(123)+data


class ScreenWireTests(unittest.TestCase):
    def test_fragmented_and_repeated_payload_keeps_first_completion_time(self):
        a, b = picture(), picture()
        items = [(0, frame(100, flags=2)), (10, frame(101, a[:70])),
                 (40, frame(171, a[70:])), (50, frame(101+len(a), b)),
                 (90, frame(101, a))]
        result = report(pcap(items))["connections"][0]
        self.assertEqual(result["screen_access_units"]["events"], 2)
        self.assertEqual(result["screen_access_units"]["interval_ms"]["median"], 10)
        self.assertEqual(result["repeated_tcp_payload_bytes"], len(a))
        self.assertEqual(result["bytes_after_sequence_gap"], 0)

    def test_out_of_order_headers_gate_later_frames_and_wrap_sequence(self):
        a, b = picture(1), picture()
        origin = 0xfffffff0
        items = [(0, frame(origin-1, flags=2)), (10, frame((origin+len(a)) & 0xffffffff, b)),
                 (30, frame(origin, a)), (40, frame((origin+len(a)+len(b)) & 0xffffffff, b))]
        result = report(pcap(items))["connections"][0]
        self.assertEqual(result["opcodes"], {"1": 1, "0": 2})
        self.assertEqual(result["screen_access_units"]["span_ms"], 10)

    def test_bad_capture_or_conflicting_overlap_rejected_and_gap_reported(self):
        a = picture()
        good = pcap([(0, frame(100, flags=2)), (10, frame(101, a))])
        for bad in (good[:-1], good+b"x", pcap([(0, frame(101, a))]),
                    pcap([(0, frame(100, flags=2)), (10, frame(101, a)), (20, frame(101, b"bad"))])):
            with self.assertRaises(ValueError): report(bad)
        result = report(pcap([(0, frame(100, flags=2)), (10, frame(103, a))]))["connections"][0]
        self.assertGreater(result["bytes_after_sequence_gap"], 0)
        self.assertEqual(result["screen_access_units"]["events"], 0)

    def test_random_tcp_boundaries_and_reordering_preserve_exact_header_count(self):
        rng = random.Random(110)
        for _ in range(1000):
            count = rng.randint(1, 15)
            data = b"".join(picture(data=rng.randbytes(rng.randint(1, 100))) for _ in range(count))
            chunks, at = [], 0
            while at < len(data):
                end = min(at+rng.randint(1, 200), len(data))
                chunks.append(frame(101+at, data[at:end]))
                at = end
            rng.shuffle(chunks)
            items = [(0, frame(100, flags=2))]+[(i+1, chunk) for i, chunk in enumerate(chunks)]
            result = report(pcap(items))["connections"][0]
            self.assertEqual(result["screen_access_units"]["events"], count)
            self.assertEqual(result["unparsed_contiguous_bytes"], 0)


if __name__ == "__main__":
    unittest.main()

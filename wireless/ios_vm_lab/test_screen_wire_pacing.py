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
    def test_buffered_other_direction_does_not_invalidate_screen_clock(self):
        a = picture()
        other = bytearray(frame(1))
        struct.pack_into(">H", other, 14+40+2, 5000)
        result = report(pcap([(0, frame(100, flags=2)), (100, bytes(other)),
                              (10, frame(101, a)), (40, frame(101+len(a), a))]))
        self.assertEqual(result["connections"][0]["screen_access_units"]["span_ms"], 30)

    def test_regression_within_measured_screen_flow_is_rejected(self):
        a = picture()
        with self.assertRaisesRegex(ValueError, "clock moved backwards"):
            report(pcap([(0, frame(100, flags=2)), (40, frame(101, a)),
                         (10, frame(101+len(a), a))]))

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

    def test_reset_window_is_not_classified_as_receive_buffer_backpressure(self):
        def reply(window,flags):
            data=bytearray(frame(20,flags=flags))
            data[22:54]=bytes(range(16,32))+bytes(range(16))
            struct.pack_into('>HH',data,54,6030,49152)
            struct.pack_into('>H',data,68,window)
            return bytes(data)
        result=report(pcap([(0,frame(100,flags=2)),(10,frame(101,picture())),
                           (20,reply(40000,16)),(30,reply(0,4)),(40,reply(0,16))]))['connections'][0]
        windows=result['receiver_advertised_window']
        self.assertEqual(windows['samples'],2)
        self.assertEqual(windows['zero_window_samples'],1)
        self.assertEqual(windows['resets'],1)
        self.assertEqual(windows['raw_max'],40000)

    def test_window_filters_picture_pairs_after_tcp_reassembly(self):
        a=picture()
        data=pcap([(0,frame(100,flags=2)),(10,frame(101,a)),(40,frame(101+len(a),a)),(80,frame(101+2*len(a),a))])
        result=report(data,window=(30,90))["connections"][0]
        self.assertEqual(result["screen_access_units"]["events"],2)
        self.assertEqual(result["screen_access_units"]["span_ms"],40)
        for window in ((0,float("inf")),(40,10),(1,1)):
            with self.assertRaises(ValueError): report(data,window=window)

    def test_sender_target_gap_is_distinguished_from_delivery_stall(self):
        def timed(ms):
            data = bytearray(picture())
            struct.pack_into("<Q", data, 8, (1 << 32)+round(ms*(1 << 32)/1000))
            return bytes(data)
        a,b,c = timed(0),timed(33),timed(533)
        result = report(pcap([(0, frame(100, flags=2)), (10, frame(101,a)),
                              (510,frame(101+len(a),b)), (543,frame(101+len(a)+len(b),c))]))["connections"][0]
        self.assertTrue(result["sender_timestamps_available"])
        self.assertAlmostEqual(result["long_gaps"][0]["sender_gap_ms"], 33, places=4)
        self.assertAlmostEqual(result["long_gaps"][0]["transit_delta_ms"], 467, places=4)
        self.assertAlmostEqual(result["long_gaps"][1]["sender_gap_ms"], 500, places=4)
        self.assertAlmostEqual(result["long_gaps"][1]["transit_delta_ms"], -467, places=4)

    def test_unusable_sender_timestamps_do_not_invalidate_delivery_measurement(self):
        a=picture()
        result=report(pcap([(0,frame(100,flags=2)), (10,frame(101,a))]))["connections"][0]
        self.assertFalse(result["sender_timestamps_available"])
        self.assertIsNone(result["sender_timestamp_intervals"])


if __name__ == "__main__":
    unittest.main()

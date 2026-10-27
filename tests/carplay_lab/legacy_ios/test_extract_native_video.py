import random
import unittest
from extract_native_video import extract


def record(number, data):
    text = f"[native-video-qemu] begin={number} bytes={len(data)}\n"
    for offset in range(0, len(data), 256):
        text += f"[native-video-qemu] record={number} offset={offset} hex={data[offset:offset+256].hex()}\n"
    return text+f"[native-video-qemu] end={number} complete=1\n"


class ExtractVideoTests(unittest.TestCase):
    def test_primary_and_cluster_records_are_kept_separate(self):
        primary = b"\0\0\0\1\x67main"
        cluster = b"\0\0\0\1\x67cluster"
        log = record(1,primary)+record(1,cluster).replace("native-video-qemu","native-cluster-qemu")
        self.assertEqual(extract(log),(primary,1))
        self.assertEqual(extract(log,"[native-cluster-qemu] "),(cluster,1))
        with self.assertRaises(ValueError): extract(log,"untrusted")
    def test_uart_prefixes_and_crlf_preserve_exact_buffers(self):
        a, b = b"\0\0\0\1\x67abc", b"\0\0\0\1\x65def"
        log = "unrelated\n"+record(1, a)+record(2, b)
        log = log.replace("[native-video", "console prefix [native-video").replace("\n", "\r\r\n")
        self.assertEqual(extract(log), (a+b, 2))

    def test_truncated_duplicate_reordered_or_failed_records_are_rejected(self):
        valid = record(1, b"\0\0\0\1\x67abc")
        for log in ("", valid.rsplit("\n", 2)[0], valid+valid,
                    valid.replace("offset=0", "offset=1"), valid.replace("complete=1", "complete=0"),
                    valid.replace("bytes=8", "bytes=7"), valid.replace("bytes=8", "bytes=9"),
                    valid.replace("begin=1", "begin=2"), valid.replace("hex=00", "hex=0"),
                    valid.replace("[native-video-qemu] end", "[native-video-qemu] corrupted-end"),
                    record(1, b"not annex b")):
            with self.subTest(log=log), self.assertRaises(ValueError):
                extract(log)

    def test_declared_sizes_and_chunk_lengths_are_bounded(self):
        for log in ("[native-video-qemu] begin=1 bytes=262145\n",
                    "[native-video-qemu] begin=4097 bytes=8\n",
                    "[native-video-qemu] begin=1 bytes=300\n[native-video-qemu] record=1 offset=0 hex="+"00"*257):
            with self.assertRaises(ValueError): extract(log)

    def test_long_capture_and_timestamp_validation(self):
        data = b"\0\0\0\1\x65abc"
        log = "".join(record(i, data).replace(f"bytes={len(data)}", f"bytes={len(data)} mono_ns={i*100000000}")
                      for i in range(1, 101))
        self.assertEqual(extract(log), (data*100, 100))
        with self.assertRaises(ValueError):
            extract(record(1, data).replace("bytes=8", f"bytes=8 mono_ns={2**64}"))

    def test_random_chunk_boundaries_round_trip_and_missing_chunk_fails(self):
        rng = random.Random(8030)
        for _ in range(1000):
            data = b"\0\0\0\1"+rng.randbytes(rng.randint(1, 1800))
            log = record(1, data)
            self.assertEqual(extract(log), (data, 1))
            lines = log.splitlines()
            del lines[rng.randrange(1, len(lines)-1)]
            with self.assertRaises(ValueError): extract("\n".join(lines))


if __name__ == "__main__": unittest.main()

import random
import struct
import unittest
import io

import ncm_link


class NCMCodecTests(unittest.TestCase):
    def test_fragmented_qemu_socket_frames_and_eof(self):
        class Peer:
            def __init__(self, data):
                self.stream = io.BytesIO(data)
            def recv(self, count):
                return self.stream.read(min(count, 3))
        frame = bytes(range(60))
        peer = Peer(struct.pack(">I", len(frame)) + frame + struct.pack(">I", len(frame)) + frame)
        self.assertEqual(ncm_link.receive_ethernet(peer), frame)
        self.assertEqual(ncm_link.receive_ethernet(peer), frame)
        with self.assertRaises(EOFError):
            ncm_link.receive_ethernet(peer)
        with self.assertRaises(EOFError):
            ncm_link.receive_ethernet(Peer(struct.pack(">I", 60) + frame[:20]))
        for size in (0, 13, 1515, 65535, 4294967295):
            with self.assertRaises(ValueError):
                ncm_link.receive_ethernet(Peer(struct.pack(">I", size)))
    def test_round_trip_and_ip_alignment(self):
        rng = random.Random(0x4E434D)
        for _ in range(1000):
            frame = rng.randbytes(rng.randrange(14, 1515))
            divisor, alignment = rng.choice((1, 4, 8, 16)), rng.choice((1, 4, 8, 16))
            remainder = rng.randrange(divisor)
            block = ncm_link.encode_ntb(frame, 65537, divisor, remainder, alignment)
            _, _, seq, size, ndp = struct.unpack_from("<4sHHHH", block)
            offset = struct.unpack_from("<H", block, ndp + 8)[0]
            self.assertEqual((seq, size, ndp % alignment), (1, len(block), 0))
            self.assertEqual((offset + 14) % divisor, remainder)
            self.assertEqual(ncm_link.decode_ntb(block), [frame])

    def test_malformed_ntb_objects(self):
        good = ncm_link.encode_ntb(bytes(60), 0)
        ndp = struct.unpack_from("<H", good, 10)[0]
        cases = [b"", good[:10], b"bad!" + good[4:]]
        for offset, value in ((4, 11), (8, len(good) + 1), (10, 1),
                              (ndp + 4, 11), (ndp + 6, ndp),
                              (ndp + 8, ndp), (ndp + 10, 1515), (ndp + 14, 1)):
            changed = bytearray(good)
            struct.pack_into("<H", changed, offset, value)
            cases.append(bytes(changed))
        for block in cases:
            with self.subTest(block=block.hex()[:40]):
                with self.assertRaises(ValueError):
                    ncm_link.decode_ntb(block)

    def test_multiple_datagrams_and_chained_tables(self):
        block = bytearray(112)
        struct.pack_into("<4sHHHH", block, 0, b"NCMH", 12, 0, 112, 80)
        block[16:46] = b"a" * 30
        block[48:78] = b"b" * 30
        struct.pack_into("<4sHHHHHH", block, 80, b"NCM0", 16, 96, 16, 30, 0, 0)
        struct.pack_into("<4sHHHHHH", block, 96, b"NCM0", 16, 0, 48, 30, 0, 0)
        self.assertEqual(ncm_link.decode_ntb(block), [b"a" * 30, b"b" * 30])
        struct.pack_into("<H", block, 104, 32)
        with self.assertRaisesRegex(ValueError, "overlapping"):
            ncm_link.decode_ntb(block)

    def test_invalid_encode_parameters(self):
        for count, divisor, remainder, alignment in ((13, 4, 0, 4), (1515, 4, 0, 4),
                (60, 0, 0, 4), (60, 4, -1, 4), (60, 4, 4, 4), (60, 4, 0, 0)):
            with self.assertRaises(ValueError):
                ncm_link.encode_ntb(bytes(count), 0, divisor, remainder, alignment)

    def test_seeded_parser_fuzz(self):
        rng = random.Random(0x16CDC)
        for _ in range(20000):
            block = rng.randbytes(rng.randrange(2048))
            try:
                frames = ncm_link.decode_ntb(block)
                self.assertLessEqual(len(frames), 64)
                self.assertTrue(all(14 <= len(f) <= 1514 for f in frames))
            except ValueError:
                pass

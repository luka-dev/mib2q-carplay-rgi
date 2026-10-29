"""Reject incomplete or corrupted native iAP2 smoke evidence offline."""
import random
import unittest

from tests.carplay_lab.legacy_ios.usb_probe import link_frame
from tests.carplay_lab.vphone.iap import verify, verify_transport


class NativeIAPTests(unittest.TestCase):
    syn = bytes.fromhex("ff5a0017c0ee0100e1017fffff0fa001f403030a0001cd")
    request = bytes.fromhex("ff5a001040ef010a5d40400006aa00d0")

    def log(self, syn=None, request=None):
        frames = [bytes.fromhex("ff550200ee10"), self.syn if syn is None else syn,
                  self.request if request is None else request]
        return "\n".join([f"native iAP2 outgoing length={len(p)} hex={p.hex()}" for p in frames] +
                         ["native SYNACK acknowledged=238", "native smoke connection cleanup complete",
                          "native iAP2 profile connection_type=6 endpoint_transport=0 endpoint_protocol=4"])

    def test_bluetooth_requires_actual_connection_and_endpoint_readback(self):
        profile="connection_type=2 endpoint_transport=2 endpoint_protocol=4"
        log=self.log().replace("connection_type=6 endpoint_transport=0", "connection_type=2 endpoint_transport=2")
        with self.assertRaises(ValueError):verify(log,"bluetooth")
        log+="\nnative iAP2 readback "+profile
        result=verify(log,"bluetooth")
        self.assertTrue(result["non_usb_endpoint_verified"])
        self.assertFalse(result["wireless_carplay_verified"])
        self.assertFalse(result["physical_bluetooth_verified"])
        for bad in (log.replace("readback connection_type=2", "readback connection_type=6"),
                    log.replace("readback "+profile,"readback connection_type=2 endpoint_transport=0 endpoint_protocol=4"),
                    log+"\nnative iAP2 profile "+profile):
            with self.assertRaises(ValueError):verify(bad,"bluetooth")
        with self.assertRaises(ValueError):verify_transport(log,"wifi")

    def test_native_capture_reaches_certificate_request_with_explicit_limits(self):
        report = verify(self.log())
        self.assertTrue(report["link_synchronized"])
        self.assertEqual(report["certificate_request"], "0xAA00")
        self.assertEqual(report["control_sessions"], [[10, 0, 1]])
        self.assertFalse(report["usb_forwarding_verified"])
        self.assertFalse(report["mfi_acceptance_verified"])
        self.assertFalse(report["video_verified"])

    def test_unpublished_observer_is_not_reported_as_non_usb_endpoint_proof(self):
        log=self.log().replace("connection_type=6 endpoint_transport=0","connection_type=2 endpoint_transport=2")
        log+="\nnative iAP2 readback connection_type=11 endpoint_transport=18 endpoint_protocol=0"
        report=verify(log,"bluetooth")
        self.assertFalse(report["non_usb_endpoint_verified"])
        self.assertFalse(report["connection_info_published"])
        with self.assertRaises(ValueError):verify_transport(log,"bluetooth")

    def test_partial_probe_is_not_reported_as_success(self):
        original = self.log()
        for fragment in ("native iAP2 outgoing length=16 hex=ff5a00", "native SYNACK acknowledged=238",
                         "native smoke connection cleanup complete", ""):
            with self.subTest(fragment=fragment), self.assertRaises(ValueError):
                verify(fragment)
        for omitted in ("ff550200ee10", "ff5a0017", "ff5a0010", "acknowledged=", "cleanup complete"):
            with self.subTest(omitted=omitted), self.assertRaises(ValueError):
                verify("\n".join(line for line in original.splitlines() if omitted not in line))

    def test_wrong_ack_session_and_request_are_rejected(self):
        variants = [(link_frame(0xC0, 238, 2, payload=self.syn[9:-1]), self.request),
                    (link_frame(0xD0, 238, 1, payload=self.syn[9:-1]), self.request),
                    (self.syn, link_frame(0x40, 239, 1, 11, self.request[9:-1])),
                    (self.syn, link_frame(0x50, 239, 1, 10, self.request[9:-1])),
                    (self.syn, link_frame(0x40, 239, 2, 10, self.request[9:-1])),
                    (self.syn, link_frame(0x40, 240, 1, 10, self.request[9:-1])),
                    (self.syn, link_frame(0x40, 239, 1, 10, bytes.fromhex("40400006aa05")))]
        for syn, request in variants:
            with self.subTest(syn=syn.hex(), request=request.hex()), self.assertRaises(ValueError):
                verify(self.log(syn, request))

    def test_sequence_wrap_uses_the_captured_peer_sequence(self):
        syn = link_frame(0xC0, 255, 1, payload=self.syn[9:-1])
        request = link_frame(0x40, 0, 1, 10, self.request[9:-1])
        report = verify(self.log(syn, request).replace("acknowledged=238", "acknowledged=255"))
        self.assertEqual(report["request"]["sequence"], 0)

    def test_fuzz_corruption_and_truncation_of_native_packets(self):
        rng = random.Random(26)
        for _ in range(256):
            damaged = bytearray(self.request)
            damaged[rng.randrange(len(damaged))] ^= rng.randrange(1, 256)
            with self.assertRaises(ValueError):
                verify(self.log(request=bytes(damaged)))
        for length in range(len(self.syn)):
            with self.subTest(length=length), self.assertRaises(ValueError):
                verify(self.log(syn=self.syn[:length]))


if __name__ == "__main__":
    unittest.main()

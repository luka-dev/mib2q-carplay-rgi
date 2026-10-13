"""Bounds and framing checks for the native API test peer, without a guest."""
import base64
import random
import struct
import unittest
from unittest.mock import patch

from wireless.ios_vm_lab import usb_probe as wire
from wireless.vphone_lab import mfi_probe


class NativePeerTests(unittest.TestCase):
    def setUp(self):
        self.peer = mfi_probe.NativeAPIPeer({}, "/owned.inbox", "/owned.log")

    def send(self, report):
        return self.peer.ctrl_transfer(0x21, 9, 0x0201, 2, report)

    def test_only_complete_checked_packets_are_uploaded(self):
        frame = wire.link_frame(0x40, 1, 7)
        with patch.object(mfi_probe.lab, "rpc") as rpc:
            self.send(b"\x01\x02" + frame[:4])
            rpc.assert_not_called()
            self.send(b"\x01\x01" + frame[4:] + bytes(12))
            params = rpc.call_args.args[2]
            self.assertEqual(params["path"], "/owned.inbox")
            self.assertEqual(base64.b64decode(params["content"]), struct.pack(">I", len(frame)) + frame)

    def test_fuzz_invalid_reports_never_write_guest_files(self):
        rng = random.Random(26)
        with patch.object(mfi_probe.lab, "rpc") as rpc:
            for _ in range(128):
                control = rng.randrange(4, 256)
                with self.assertRaises(ValueError):
                    self.send(bytes((1, control, rng.randrange(256))))
            with self.assertRaises(ValueError):
                self.send(b"\x01\x01abc")
            rpc.assert_not_called()

    def test_corrupted_packet_and_nonzero_padding_are_rejected(self):
        frame = wire.link_frame(0x40, 1, 7)
        with patch.object(mfi_probe.lab, "rpc") as rpc:
            for packet in (frame[:-1] + bytes((frame[-1] ^ 1,)), frame + b"\x01", frame[:5]):
                with self.assertRaises(ValueError):
                    self.send(b"\x01\x00" + packet)
            rpc.assert_not_called()

    def test_history_bound_is_checked_before_upload(self):
        self.peer.history = bytes(65530)
        with patch.object(mfi_probe.lab, "rpc") as rpc, self.assertRaises(ValueError):
            self.send(b"\x01\x00" + wire.link_frame(0x40, 1, 7))
        rpc.assert_not_called()

    def test_duplicate_synack_is_drained_before_control_data(self):
        syn = wire.link_frame(0xC0, 4, 1, payload=bytes.fromhex("010810000fa001f403030a0001"))
        request = wire.link_frame(0x40, 5, 1, 10, bytes.fromhex("40400006aa00"))
        log = "\n".join(f"native iAP2 outgoing length={len(p)} hex={p.hex()}" for p in (syn, syn, request)) + "\n"
        with patch.object(mfi_probe.lab, "rpc", return_value={"result": {"content": log}}):
            self.assertEqual(self.peer.read(0x83, 1025, 1000)[2:], syn)
            self.assertEqual(self.peer.read(0x83, 1025, 1000)[2:], request)
        self.assertEqual(self.peer.discarded_synacks, 1)

    def test_unknown_negative_control_does_not_load_fixture_or_contact_guest(self):
        with patch.object(mfi_probe, "fixture_key") as fixture, patch.object(mfi_probe.lab, "rpc") as rpc:
            with self.assertRaises(ValueError):
                mfi_probe.run(negative="unknown")
            fixture.assert_not_called()
            rpc.assert_not_called()

    def test_invalid_session_options_fail_before_fixture_or_guest_access(self):
        variants = [{"identify": 1}, {"identify": True, "negative": "signature"},
                    {"start_carplay": True}, {"hold_seconds": True}, {"hold_seconds": 31},
                    {"receiver_ipv6": "::1"}, {"receiver_ipv6": "fe80::2%en0"},
                    {"public_key": bytes(31)}, {"public_key": "0" * 32},
                    {"carplay_interface": True}, {"carplay_interface": 3}]
        with patch.object(mfi_probe, "fixture_key") as fixture, patch.object(mfi_probe.lab, "rpc") as rpc:
            for options in variants:
                with self.subTest(options=options), self.assertRaises(ValueError):
                    mfi_probe.run(**options)
            fixture.assert_not_called()
            rpc.assert_not_called()


if __name__ == "__main__":
    unittest.main()

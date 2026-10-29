"""Bounds and framing checks for the native API test peer, without a guest."""
import base64
import json
from pathlib import Path
import random
import struct
import unittest
from unittest.mock import patch

from tests.carplay_lab.legacy_ios import usb_probe as wire
from tests.carplay_lab.vphone import mfi_probe


class NativePeerTests(unittest.TestCase):
    def test_identification_holds_connection_before_cleanup_for_pairing_ui(self):
        events = []
        def rpc(settings, method, params=None):
            if method == "device.info":
                return {"result": {"model": "iPhone99,11", "ios_version": "26.6.2"}}
            if method == "files.write" and params["path"].endswith(".done"):
                events.append("done")
            return {"ok": True}
        def native(mode, **options):
            options["on_launch"]("/owned.log")
            return {"status": {"last_exit_status": 0}}
        def handshake(peer, descriptors, certificate, result, **options):
            result.update(link_synchronized=True, first_control_message="0xaa00",
                          authentication_messages=["0xaa05"], challenge_bytes=32,
                          signature_bytes=64, identification_accepted=True)
        with patch.object(mfi_probe, "fixture_key", return_value=(None, b"fixture")), \
                patch.object(mfi_probe.lab, "profile", return_value={}), \
                patch.object(mfi_probe.lab, "require_machine"), \
                patch.object(mfi_probe.lab, "rpc", side_effect=rpc), \
                patch.object(mfi_probe.lab, "save"), \
                patch.object(mfi_probe.usb, "run", side_effect=native), \
                patch.object(wire, "handshake", side_effect=handshake), \
                patch.object(mfi_probe.time, "sleep", side_effect=lambda seconds: events.append(seconds)):
            result = mfi_probe.run(identify=True, hold_seconds=30)
        self.assertNotIn("error", result)
        self.assertEqual(result["identification_hold_seconds"], 30)
        self.assertEqual(events, [30, "done"])

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
                    {"transport": "wifi"},
                    {"transport": "bluetooth", "start_carplay": True},
                    {"start_carplay": True}, {"hold_seconds": True}, {"hold_seconds": 121},
                    {"receiver_ipv6": "::1"}, {"receiver_ipv6": "fe80::2%en0"},
                    {"public_key": bytes(31)}, {"public_key": "0" * 32},
                    {"carplay_interface": True}, {"carplay_interface": 3}]
        with patch.object(mfi_probe, "fixture_key") as fixture, patch.object(mfi_probe.lab, "rpc") as rpc:
            for options in variants:
                with self.subTest(options=options), self.assertRaises(ValueError):
                    mfi_probe.run(**options)
            fixture.assert_not_called()
            rpc.assert_not_called()

    def test_bluetooth_identity_has_only_bluetooth_and_wireless_transport_components(self):
        mid,fields=wire.parse_control_message(mfi_probe.bluetooth_identification())
        self.assertEqual(mid,0x1D01)
        self.assertNotIn(15,fields);self.assertNotIn(16,fields)
        nested=lambda data:wire.parse_control_message(b"\x40\x40"+struct.pack(">HH",len(data)+6,0x1D01)+data)[1]
        bt=nested(fields[17]);wifi=nested(fields[24])
        self.assertEqual(bt[0],b"\0\0");self.assertEqual(bt[3],bytes.fromhex("020000000001"))
        self.assertEqual(wifi[0],b"\0\x01");self.assertEqual(wifi[4],b"")
        self.assertIn(bytes.fromhex("5703"),fields[6]);self.assertIn(bytes.fromhex("5702"),fields[7])

    def test_cached_retransmissions_are_drained_without_another_rpc(self):
        first=wire.link_frame(0x40,4,1,10,wire.control_message(0xAA00))
        second=wire.link_frame(0x40,5,2,10,wire.control_message(0xAA02,b"x"*32))
        log="\n".join(f"native iAP2 outgoing length={len(p)} hex={p.hex()}" for p in (first,first,second))+"\n"
        with patch.object(mfi_probe.lab,"rpc",return_value={"result":{"content":log}}) as rpc:
            self.assertEqual(self.peer.read(0x83,1025,1000)[2:],first)
            self.assertEqual(self.peer.read(0x83,1025,1000)[2:],second)
            self.assertEqual(rpc.call_count,1)
        self.assertEqual(self.peer.discarded_data_retransmits,1)

    def test_only_native_authenticated_bluetooth_wifi_request_proves_bootstrap(self):
        log=("native iAP2 profile connection_type=2 endpoint_transport=2 endpoint_protocol=4\n"
             "native iAP2 readback connection_type=11 endpoint_transport=18 endpoint_protocol=0\n")
        packet={"session":10,"control":0x40,"payload":wire.control_message(0x5702).hex()}
        result={"authentication_succeeded":True,"identification_accepted":True,"post_identification_packets":[packet]}
        proof=mfi_probe.verify_bluetooth_bootstrap(result,log,identify=True)
        self.assertTrue(proof["wireless_bootstrap_requested"])
        self.assertTrue(proof["transport_verification"]["non_usb_endpoint_verified"])
        self.assertFalse(proof["transport_verification"]["connection_info_published"])
        self.assertFalse(proof["transport_verification"]["wireless_carplay_verified"])
        for change in ({"session":11},{"control":0x50},{"payload":wire.control_message(0x4300).hex()}):
            with self.subTest(change=change),self.assertRaises(ValueError):
                mfi_probe.verify_bluetooth_bootstrap(result|{"post_identification_packets":[packet|change]},log,identify=True)
        for missing in ("authentication_succeeded","identification_accepted"):
            with self.subTest(missing=missing),self.assertRaises(ValueError):
                mfi_probe.verify_bluetooth_bootstrap(result|{missing:False},log,identify=True)
        with self.assertRaises(ValueError):
            mfi_probe.verify_bluetooth_bootstrap(result,log.replace("profile connection_type=2 endpoint_transport=2","profile connection_type=6 endpoint_transport=0"),identify=True)

    def test_rejected_identification_ids_are_not_declared_as_capabilities(self):
        _,fields=wire.parse_control_message(mfi_probe.bluetooth_identification())
        capture=json.loads((Path(__file__).parents[1]/"fixtures/native_bluetooth_identification.json").read_text())
        mid,rejected_params=wire.parse_control_message(bytes.fromhex(capture["identification_rejected_body"]))
        self.assertEqual(mid,0x1D03)
        for kind,raw in rejected_params.items():
            rejected={int.from_bytes(raw[i:i+2],"big") for i in range(0,len(raw),2)}
            ids={int.from_bytes(fields[kind][i:i+2],"big") for i in range(0,len(fields[kind]),2)}
            self.assertFalse(ids & rejected)

    def test_api_codec_options_reject_unbounded_or_wrong_messages_before_io(self):
        for value in (True,9,46,"45"):
            with self.subTest(timeout=value),self.assertRaises(ValueError):
                wire.handshake(object(),[],exchange_timeout=value)
        for payload in (b"",b"bad",wire.control_message(0x4301),"not bytes",b"x"*4097):
            with self.subTest(payload=repr(payload)[:30]),self.assertRaises(ValueError):
                wire.handshake(object(),[],identification_payload=payload)


if __name__ == "__main__":
    unittest.main()

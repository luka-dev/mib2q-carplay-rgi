import random
import struct
import tempfile
from pathlib import Path
import json
import unittest
from unittest.mock import patch, MagicMock

from tests.carplay_lab.vphone import carplay_input as control


class CarPlayInputTests(unittest.TestCase):
    def test_invalid_coordinates_fail_before_guest_or_socket_access(self):
        rng = random.Random(32503)
        values = [(True, 1), (1.0, 1), ("1", 1), (None, 1), (0, -1), (0, 480)]
        values += [(rng.randrange(800, 65536), rng.randrange(480)) for _ in range(128)]
        with patch.object(control.lab, "require_machine") as owned, patch.object(control.socket, "create_connection") as connect:
            for x, y in values:
                with self.subTest(x=x, y=y), self.assertRaises(ValueError):
                    control.tap(x, y)
            owned.assert_not_called(); connect.assert_not_called()

    def test_disabled_input_build_does_not_connect(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out/"qnx-network").mkdir()
            (out/"qnx-network/build-record.json").write_text(json.dumps({"software_mfi": True}))
            with patch.object(control.lab, "OUT", out), patch.object(control.qnx_receiver, "OUT", out/"qnx-network"), patch.object(control.qnx_receiver, "set_output"), patch.object(control.lab, "require_machine"), \
                    patch.object(control.qnx_receiver, "running", return_value=42), \
                    patch.object(control.socket, "create_connection") as connect:
                with self.assertRaises(RuntimeError): control.tap(374, 184)
                connect.assert_not_called()

    def test_fragmented_native_result_and_loopback_only_transport(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out/"qnx-network").mkdir()
            (out/"qnx-network/build-record.json").write_text(json.dumps({"software_mfi": True, "input_control": True}))
            peer = MagicMock()
            peer.recv.side_effect = [b"\0", b"\0\0", b"\0"]
            connection = MagicMock(); connection.__enter__.return_value = peer
            with patch.object(control.lab, "OUT", out), patch.object(control.qnx_receiver, "OUT", out/"qnx-network"), patch.object(control.qnx_receiver, "set_output"), patch.object(control.lab, "require_machine"), \
                    patch.object(control.qnx_receiver, "running", return_value=42), \
                    patch.object(control.socket, "create_connection", return_value=connection) as connect:
                result = control.tap(374, 184)
            connect.assert_called_once_with(("127.0.0.1", 32503), timeout=3)
            peer.sendall.assert_called_once_with(b"MHT" + struct.pack(">HH", 374, 184))
            self.assertEqual(result["native_hid_status"], 0)
            self.assertFalse(result["visible_effect_verified"])

    def test_snapshot_invalid_paths_fail_before_contacting_receiver(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(control.lab, "OUT", Path(directory)), \
                patch.object(control, "receiver") as receiver:
            for channel, target in (("unknown", None), ("cluster", Path(directory).parent/"outside.h264"),
                                    ("primary", Path(directory)/"invalid.txt")):
                with self.subTest(channel=channel, target=target), self.assertRaises(ValueError):
                    control.snapshot(channel, target)
            receiver.assert_not_called()

    def test_bad_or_truncated_snapshot_preserves_last_good_file(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(control.lab, "OUT", Path(directory)), \
                patch.object(control, "receiver", return_value=42):
            target = Path(directory)/"capture.h264"
            target.write_bytes(b"last good capture")
            for chunks in ([struct.pack(">I", 0)], [struct.pack(">I", 2*1024*1024+1)],
                           [struct.pack(">I", 6), b"badbad"], [struct.pack(">I", 6), b"x", b""]):
                peer = MagicMock(); peer.recv.side_effect = chunks
                connection = MagicMock(); connection.__enter__.return_value = peer
                with patch.object(control.socket, "create_connection", return_value=connection), \
                        self.assertRaises((ValueError, RuntimeError)):
                    control.snapshot("cluster", target)
                self.assertEqual(target.read_bytes(), b"last good capture")

    def test_fresh_snapshot_requests_native_refresh_and_reads_fragmented_bytes(self):
        data = b"\0\0\0\1\x67config\0\0\1\x65picture"
        with tempfile.TemporaryDirectory() as directory, patch.object(control.lab, "OUT", Path(directory)), \
                patch.object(control, "receiver", return_value=42):
            peer = MagicMock(); peer.recv.side_effect = [struct.pack(">I", len(data)), data[:5], data[5:]]
            connection = MagicMock(); connection.__enter__.return_value = peer
            with patch.object(control.socket, "create_connection", return_value=connection):
                result = control.snapshot("primary", fresh=True)
            peer.sendall.assert_called_once_with(b"MFM"+bytes(4))
            peer.settimeout.assert_called_once_with(10)
            self.assertTrue(result["native_keyframe_requested"])
            self.assertEqual(Path(result["output"]).read_bytes(), data)

    def test_timing_excludes_payload_and_rejects_unbounded_counts(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(control.lab, "OUT", Path(directory)), \
                patch.object(control, "receiver", return_value=42):
            peer = MagicMock(); peer.recv.side_effect = [struct.pack(">I", 3), struct.pack(">QQQ", 10, 33333343, 66666676)]
            connection = MagicMock(); connection.__enter__.return_value = peer
            with patch.object(control.socket, "create_connection", return_value=connection):
                result = control.timing("cluster")
                self.assertEqual(result["picture_input"]["events"], 3)
                peer.sendall.assert_called_once_with(b"MDC"+bytes(4))
                peer.recv.side_effect = [struct.pack(">I", 8193)]
                with self.assertRaises(ValueError): control.timing("cluster")

    def test_clock_query_reports_acceleration_and_signed_timer_scale(self):
        peer=MagicMock()
        peer.recv.side_effect=[struct.pack('>QQQQQqQ',1000000000,1,10,24000000,83333333,-15,1000000),
                               struct.pack('>QQQQQqQ',6208333300,2,62500010,24000000,83333333,-15,1000000)]
        connection=MagicMock();connection.__enter__.return_value=peer
        with tempfile.TemporaryDirectory() as directory, patch.object(control.lab,'OUT',Path(directory)), \
                patch.object(control,'receiver',return_value=42), \
                patch.object(control.socket,'create_connection',return_value=connection), \
                patch.object(control.time,'monotonic_ns',side_effect=[10,12,1000000010,1000000012]),patch.object(control.time,'sleep'):
            result=control.clock(2,.1)
            self.assertAlmostEqual(result['guest_to_host_rate'],5.2083333)
            self.assertEqual(result['cycles_hz_observed'],62500000)
            self.assertEqual(result['samples'][0]['timer_scale'],-15)

    def test_delayed_clock_reply_does_not_claim_a_reliable_rate(self):
        peer=MagicMock();peer.recv.side_effect=[struct.pack('>QQQQQqQ',1000000000,1,10,62500000,16000000,-15,1000000),
                                               struct.pack('>QQQQQqQ',1291000000,2,18187510,62500000,16000000,-15,1000000)]
        connection=MagicMock();connection.__enter__.return_value=peer
        with tempfile.TemporaryDirectory() as directory, patch.object(control.lab,'OUT',Path(directory)), \
                patch.object(control,'receiver',return_value=42),patch.object(control.socket,'create_connection',return_value=connection), \
                patch.object(control.time,'monotonic_ns',side_effect=[0,1366000000,1616000000,1638000000]),patch.object(control.time,'sleep'):
            result=control.clock(2,.1)
            self.assertFalse(result['rate_estimate_reliable'])
            self.assertLess(result['rate_bounds']['lower'],1)
            self.assertGreater(result['rate_bounds']['upper'],1)


if __name__ == "__main__": unittest.main()

import random
import sys
from types import SimpleNamespace
import unittest
from unittest import mock

import usb_probe


# Read from the synthetic iOS 18A5351d guest's configuration 2, interface 2.
DESCRIPTOR = bytes.fromhex(
    "0600ff0901a10175082680001500090185019505820201090185029509820201"
    "09018503950d820201090185049511820201090185059519820201090185069531820201"
    "09018507955f8202010901850895c1820201090185099601018202010901850a968101820201"
    "0901850b9601028202010901850c96ff028202010901850d95059202010901850e9509920201"
    "0901850f950d920201090185109511920201090185119519920201090185129531920201"
    "09018513955f9202010901851495c19202010901851595ff920201c0"
)


class HIDProbeTests(unittest.TestCase):
    def test_combined_profile_keeps_mux_and_network_configuration(self):
        class USBError(Exception):
            pass
        class Interface(list):
            def __init__(self, number, cls, subclass, endpoints=()):
                super().__init__(SimpleNamespace(bEndpointAddress=e) for e in endpoints)
                self.bInterfaceNumber, self.bInterfaceClass, self.bInterfaceSubClass = number, cls, subclass
        class Configuration(list):
            bConfigurationValue = 1
        configuration = Configuration([Interface(0, 255, 254), Interface(1, 3, 0, (0x83,)),
                                       Interface(2, 2, 13)])
        phone = mock.Mock(iSerialNumber=1)
        phone.get_active_configuration.return_value = configuration
        phone.is_kernel_driver_active.return_value = False
        phone.ctrl_transfer.side_effect = [DESCRIPTOR, 10]
        phone.read.return_value = bytes.fromhex("0200ff550200ee100000")
        util = mock.Mock()
        util.get_string.return_value = usb_probe.SERIAL
        core = SimpleNamespace(find=lambda **_: [phone], USBError=USBError)
        modules = {"usb": SimpleNamespace(core=core, util=util), "usb.core": core, "usb.util": util}
        with mock.patch.dict(sys.modules, modules), mock.patch.object(usb_probe, "require_companion"), \
                mock.patch.object(usb_probe.subprocess, "run") as stop, \
                mock.patch.object(usb_probe.subprocess, "Popen") as restart:
            result = usb_probe.probe("detect")
        self.assertTrue(result["iap2_detect_echo"])
        self.assertTrue(result["combined_usb"])
        stop.assert_not_called()
        restart.assert_not_called()
        phone.set_configuration.assert_not_called()
        util.claim_interface.assert_called_once_with(phone, 1)
        util.release_interface.assert_called_once_with(phone, 1)

    def test_identification_declares_only_requested_application_messages(self):
        message, params = usb_probe.parse_control_message(usb_probe.identification_message())
        self.assertEqual(message, 0x1D01)
        self.assertEqual((params[6], params[7]), (b"\x43\x01", b"\x43\x00"))
        _, host = usb_probe.parse_control_message(b"\x40\x40" + (len(params[16]) + 6).to_bytes(2, "big") + b"\0\0" + params[16])
        self.assertEqual(host[3], b"\2")
        message, start = usb_probe.parse_control_message(usb_probe.carplay_start_message(bytes(range(32))))
        self.assertEqual((message, start[4]), (0x4301, bytes(range(32)).hex().encode() + b"\0"))
        with self.assertRaises(ValueError):
            usb_probe.carplay_start_message(bytes(31))

    def test_guest_descriptor_and_detect_wire_bytes(self):
        self.assertEqual(len(DESCRIPTOR), 208)
        reports = usb_probe.output_reports(DESCRIPTOR)
        self.assertEqual(reports, list(zip(range(13, 22), (5, 9, 13, 17, 25, 49, 95, 193, 255))))
        self.assertEqual(usb_probe.detect_report(reports), bytes.fromhex("0e00ff550200ee100000"))

    def test_16_bit_count_and_capacity_boundary(self):
        for count in (7, 256, 1024):
            descriptor = bytes.fromhex("7508850196") + count.to_bytes(2, "little") + b"\x92\x02\x01"
            reports = usb_probe.output_reports(descriptor)
            self.assertEqual(reports, [(1, count)])
            self.assertEqual(len(usb_probe.detect_report(reports)), count + 1)

    def test_malformed_or_unsupported_descriptors(self):
        cases = (b"", b"\x97\x01", b"\xfe", b"x" * 4097,
                 bytes.fromhex("750885009507920201"),
                 bytes.fromhex("750785019507920201"),
                 bytes.fromhex("75088501960104920201"),
                 bytes.fromhex("7508850195079202019509920201"),
                 bytes.fromhex("a4"), bytes.fromhex("b4"), bytes.fromhex("75089507820201"))
        for descriptor in cases:
            with self.subTest(descriptor=descriptor[:20]):
                with self.assertRaises(ValueError):
                    usb_probe.output_reports(descriptor)

    def test_report_selection_uses_capacity(self):
        self.assertEqual(usb_probe.detect_report([(9, 255), (14, 9), (1, 5)])[0], 14)
        for reports in ([], [(13, 5)], [(256, 9)], [(1, 1025)]):
            with self.assertRaises(ValueError):
                usb_probe.detect_report(reports)

    def test_serial_normalization_preserves_identity(self):
        expected = "000080301122334455667788"
        self.assertEqual(usb_probe.normalized_serial(usb_probe.SERIAL), expected)
        self.assertEqual(usb_probe.normalized_serial(expected + "\0\0"), expected)
        self.assertNotEqual(usb_probe.normalized_serial(expected + "9"), expected)

    def test_host_is_rejected_before_usb_access(self):
        with mock.patch.object(usb_probe.Path, "is_file", return_value=False):
            with self.assertRaisesRegex(RuntimeError, "ARM QEMU"):
                usb_probe.probe("descriptor")
        with mock.patch.object(usb_probe.Path, "is_file", return_value=True), \
                mock.patch.object(usb_probe.Path, "read_bytes", return_value=b"real-board\0"):
            with self.assertRaises(RuntimeError):
                usb_probe.require_companion()

    def test_usb_failures_still_restore_configuration_and_restart_mux(self):
        class USBError(Exception):
            pass

        for fail_release in (False, True):
            with self.subTest(fail_release=fail_release):
                interface = SimpleNamespace(bInterfaceClass=255, bInterfaceSubClass=254,
                                            bInterfaceNumber=2)

                class Configuration(list):
                    bConfigurationValue = 4

                class Interface(list):
                    bInterfaceClass = 3
                    bInterfaceSubClass = 0
                    bInterfaceNumber = 2

                previous = Configuration([interface])
                active = Configuration([Interface([SimpleNamespace(bEndpointAddress=0x83)])])
                active.bConfigurationValue = 2
                current = [previous]

                phone = mock.Mock(iSerialNumber=1)
                phone.get_active_configuration.side_effect = lambda: current[0]
                phone.set_configuration.side_effect = lambda value: current.__setitem__(0, active if value == 2 else previous)
                phone.is_kernel_driver_active.return_value = False
                phone.ctrl_transfer.side_effect = [DESCRIPTOR, USBError("write failed")]
                util = mock.Mock()
                util.get_string.return_value = usb_probe.SERIAL
                if fail_release:
                    util.release_interface.side_effect = USBError("release failed")
                core = SimpleNamespace(find=lambda **_: [phone], USBError=USBError)
                modules = {"usb": SimpleNamespace(core=core, util=util),
                           "usb.core": core, "usb.util": util}
                with mock.patch.dict(sys.modules, modules), \
                        mock.patch.object(usb_probe, "require_companion"), \
                        mock.patch.object(usb_probe.subprocess, "run") as stop, \
                        mock.patch.object(usb_probe.subprocess, "Popen") as restart, \
                        mock.patch.object(usb_probe.time, "sleep"), \
                        mock.patch("builtins.open", mock.mock_open()):
                    result = usb_probe.probe("detect")
                self.assertEqual(result["error"], "write failed")
                self.assertEqual(result["configuration_restored"], 4)
                self.assertTrue(result["usbmuxd_restarted"])
                self.assertEqual(phone.set_configuration.call_args_list, [mock.call(2), mock.call(4)])
                stop.assert_called_once()
                restart.assert_called_once()
                util.dispose_resources.assert_called_once_with(phone)

    def test_fragmentation_preserves_payload_and_link_control(self):
        reports = usb_probe.output_reports(DESCRIPTOR)
        payload = bytes(range(256)) * 3
        encoded = usb_probe.hid_reports(payload, reports)
        self.assertEqual([report[1] for report in encoded], [2, 3, 3, 1])
        joined = b"".join(report[2:] for report in encoded)
        self.assertEqual(joined[:len(payload)], payload)
        self.assertFalse(any(joined[len(payload):]))
        for packet in (b"", bytes(8193)):
            with self.assertRaises(ValueError):
                usb_probe.hid_reports(packet, reports)

    def test_actual_synack_and_control_messages(self):
        sync = bytes.fromhex("017fffff0fa001f403030a0001")
        frame = usb_probe.link_frame(0xC0, 154, 1, payload=sync)
        parsed = usb_probe.parse_link_frame(frame + b"\0\0")
        self.assertEqual(parsed["payload"], sync)
        self.assertEqual(parsed["acknowledgement"], 1)
        self.assertEqual(usb_probe.control_message(0xAA00), bytes.fromhex("40400006aa00"))
        message, parameters = usb_probe.parse_control_message(usb_probe.control_message(0xAA02, bytes(32)))
        self.assertEqual((message, parameters), (0xAA02, {0: bytes(32)}))

    def test_checksum_and_parameter_corruption_rejected(self):
        frame = usb_probe.link_frame(0x40, 2, 1, 10, b"payload")
        for offset in (0, 4, 9, len(frame) - 1):
            damaged = bytearray(frame)
            damaged[offset] ^= 1
            with self.assertRaises(ValueError):
                usb_probe.parse_link_frame(damaged)
        for body in (b"", bytes.fromhex("40400006aa000000"),
                     bytes.fromhex("4040000aaa0200000000"),
                     bytes.fromhex("4040000eaa020004000000040000")):
            with self.assertRaises(ValueError):
                usb_probe.parse_control_message(body)

    def test_seeded_link_and_control_fuzz_is_bounded(self):
        rng = random.Random(0xAA05)
        for _ in range(25000):
            raw = rng.randbytes(rng.randrange(512))
            for parse in (usb_probe.parse_link_frame, usb_probe.parse_control_message):
                try:
                    parse(raw)
                except ValueError:
                    pass

    def test_structured_frame_fuzz_and_hid_round_trip(self):
        rng = random.Random(0xFF5A)
        reports = usb_probe.output_reports(DESCRIPTOR)
        for _ in range(1000):
            payload = rng.randbytes(rng.randrange(4097))
            frame = usb_probe.link_frame(0x40, rng.randrange(256), rng.randrange(256), 10, payload)
            encoded = usb_probe.hid_reports(frame, reports)
            joined = b"".join(report[2:] for report in encoded)
            self.assertEqual(usb_probe.parse_link_frame(joined)["payload"], payload)
            damaged = bytearray(frame)
            damaged[rng.randrange(len(frame))] ^= 1
            with self.assertRaises(ValueError):
                usb_probe.parse_link_frame(damaged)

    def test_seeded_descriptor_fuzz_is_bounded(self):
        rng = random.Random(0x18A5351D)
        for _ in range(25000):
            descriptor = rng.randbytes(rng.randrange(257))
            try:
                reports = usb_probe.output_reports(descriptor)
            except ValueError:
                continue
            self.assertLessEqual(len(reports), 255)
            for rid, count in reports:
                self.assertTrue(1 <= rid <= 255 and 2 <= count <= 1024)
            try:
                report = usb_probe.detect_report(reports)
            except ValueError:
                continue
            self.assertTrue(8 <= len(report) <= 1025)
            self.assertEqual(report[1:8], b"\0" + usb_probe.IAP2_DETECT)


if __name__ == "__main__":
    unittest.main()

import copy
import fcntl
from pathlib import Path
import struct
import random
import tempfile
import unittest
from unittest.mock import patch

from wireless.vphone_lab import ethernet, usb


class EthernetTests(unittest.TestCase):
    udid = "0000FE01-0123456789ABCDEF"

    def tree(self):
        return [{"USB Serial Number": self.udid.replace("-", "") + "\0\0",
                 "USB Product Name": "iPhone Research Environment Virtual Machine", "idVendor": 0x05AC,
                 "idProduct": 0x12A8, "IORegistryEntryID": 10,
                 "IORegistryEntryChildren": [{"IOObjectClass": "IOUSBHostInterface", "IORegistryEntryName": "NCM Data",
                     "IORegistryEntryLocation": "2", "IORegistryEntryChildren": [{"IOObjectClass": "AppleUSBNCMData",
                         "IORegistryEntryChildren": [{"IOObjectClass": "IOEthernetInterface",
                             "IORegistryEntryName": "en22", "IORegistryEntryID": 20}]}]}]}]

    def test_owned_primary_selected_despite_other_devices_and_auxiliary(self):
        tree = self.tree()
        aux = copy.deepcopy(tree[0]["IORegistryEntryChildren"][0]); aux["IORegistryEntryLocation"] = "4"
        tree[0]["IORegistryEntryChildren"].insert(0, aux)
        self.assertEqual(ethernet.owned_interface([{"USB Serial Number": "other"}] + tree, self.udid)["interface"], "en22")

    def test_ambiguous_or_unexpected_usb_identity_rejected(self):
        for key, value in (("USB Product Name", "iPhone"), ("idVendor", 1), ("idProduct", 1),
                           ("USB Serial Number", "0000FE01-OTHER")):
            tree = self.tree(); tree[0][key] = value
            with self.assertRaises(RuntimeError):
                ethernet.owned_interface(tree, self.udid)
        with self.assertRaises(RuntimeError):
            ethernet.owned_interface(self.tree() * 2, self.udid)
        with self.assertRaises(ValueError):
            ethernet.owned_interface(self.tree(), "00008030-0123456789ABCDEF")

    def test_auxiliary_only_and_ambiguous_ethernet_rejected(self):
        tree = self.tree(); data = tree[0]["IORegistryEntryChildren"][0]
        data["IORegistryEntryLocation"] = "4"
        with self.assertRaises(RuntimeError):
            ethernet.owned_interface(tree, self.udid)
        data["IORegistryEntryLocation"] = "2"
        data["IORegistryEntryChildren"] *= 2
        with self.assertRaises(RuntimeError):
            ethernet.owned_interface(tree, self.udid)

    def test_fragmented_and_coalesced_qemu_records(self):
        expected = [bytes(range(14)), b"x" * ethernet.MAX_FRAME]
        encoded = b"".join(struct.pack(">I", len(x)) + x for x in expected)
        for chunk in (1, 3, 7, 1024, len(encoded)):
            buffer = bytearray(); found = []
            for start in range(0, len(encoded), chunk):
                buffer.extend(encoded[start:start + chunk]); found.extend(ethernet.frames(buffer))
            self.assertEqual(found, expected); self.assertFalse(buffer)

    def test_darwin_capture_padding_is_not_part_of_timestamp(self):
        wire = struct.pack("<qi", 1791753000, 999999) + b"\xff" * 4 + struct.pack("<II", 14, 14)
        header = ethernet.Header.from_buffer_copy(wire)
        self.assertEqual(header.timestamp.seconds, 1791753000)
        self.assertEqual(header.timestamp.microseconds, 999999)
        self.assertEqual(header.captured, 14)
        self.assertEqual(header.length, 14)

    def test_invalid_length_is_rejected_before_buffering_payload(self):
        for length in (0, 13, ethernet.MAX_FRAME + 1, 0xFFFFFFFF):
            buffer = bytearray(struct.pack(">I", length))
            with self.assertRaises(ValueError):
                ethernet.frames(buffer)
            self.assertEqual(len(buffer), 4)

    def test_seeded_record_fragmentation_fuzz(self):
        rng = random.Random(0xFE01)
        for _ in range(200):
            expected = [rng.randbytes(rng.randint(14, ethernet.MAX_FRAME)) for _ in range(rng.randint(1, 8))]
            wire = b"".join(struct.pack(">I", len(x)) + x for x in expected)
            pos = 0; buffer = bytearray(); actual = []
            while pos < len(wire):
                size = rng.randint(1, 4096); buffer.extend(wire[pos:pos + size]); pos += size
                actual.extend(ethernet.frames(buffer))
                self.assertLess(len(buffer), ethernet.MAX_FRAME + 4)
            self.assertEqual(actual, expected); self.assertFalse(buffer)

    def test_concurrent_tools_cannot_overwrite_active_capture_or_helper(self):
        for module, lock_name, arguments in ((ethernet, ".ethernet.lock", (1,)), (usb, ".usb.lock", ("probe",))):
            with tempfile.TemporaryDirectory() as directory, patch.object(module.lab, "OUT", Path(directory)), \
                    patch.object(module, "_run") as work, open(Path(directory) / lock_name, "w") as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                with self.assertRaises(BlockingIOError):
                    module.run(*arguments)
                work.assert_not_called()


if __name__ == "__main__":
    unittest.main()

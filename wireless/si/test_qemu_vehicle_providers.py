#!/usr/bin/env python3
"""Input provenance failures must stop the disposable vehicle profile."""
import hashlib
from pathlib import Path
import tempfile
import unittest

from qemu_vehicle_providers import (apply_carplay_coding, vehicle_adaptation,
                                    vehicle_coding, vehicle_fec_container)
from qemu_stage1 import acceptance_markers


class VehicleInputs(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.backup = Path(self.tmp.name)
        self.coding = bytes(range(25))
        self.cfa = bytes([0x05, 0x05, 0x01, 0x00] + [0x05] * 26 + [0x00] * 26 +
                         [0x05, 0x00, 0x00, 0x00])
        assert len(self.cfa) == 60
        self.gen2 = bytes(60)
        self.adaptation = self.cfa + self.gen2
        self.raw = (b"header\0VW_Coding=" + self.coding.hex().encode() +
                    b"\0Car_Function_Adaptations=" + self.cfa.hex().encode() +
                    b"\0Car_Function_Adaptations_Gen2_Extended=" +
                    self.gen2.hex().encode() + b"\0tail")
        for name in ("storage1.raw", "storage2.raw"):
            (self.backup / name).write_bytes(self.raw)

    def test_unchanged_input_and_content_binding(self):
        value, record = vehicle_coding(self.backup)
        self.assertEqual(value, self.coding)
        self.assertEqual(record["coding_sha256"], hashlib.sha256(value).hexdigest())
        for name in ("storage1.raw", "storage2.raw"):
            self.assertEqual((self.backup / name).read_bytes(), self.raw)

    def test_adaptation_assembly_and_provenance(self):
        blob, record = vehicle_adaptation(self.backup)
        self.assertEqual(blob, self.adaptation)
        self.assertEqual(record["blob_size"], 120)
        self.assertEqual(record["blob_sha256"], hashlib.sha256(blob).hexdigest())
        self.assertEqual(record["carplay_coding_bit"]["value"], self.cfa[51] & 1)
        self.assertEqual(record["native_usb_ports_from_adaptation"],
                         (self.cfa[30] & 1) | ((self.cfa[31] & 1) << 1) |
                         ((self.cfa[32] & 1) << 2) | ((self.cfa[33] & 1) << 3))
        for name in ("storage1.raw", "storage2.raw"):
            self.assertEqual((self.backup / name).read_bytes(), self.raw)

    def test_adaptation_conflicting_generations_fail(self):
        mutated = self.cfa.hex().replace("05", "06")
        (self.backup / "storage2.raw").write_bytes(
            self.raw.replace(self.cfa.hex().encode(), mutated.encode()))
        with self.assertRaisesRegex(ValueError, "disagree"):
            vehicle_adaptation(self.backup)

    def test_adaptation_missing_or_ambiguous_setting_fails(self):
        for bad in (b"no settings here",
                    self.raw + b"\0Car_Function_Adaptations=00\0"):
            with self.subTest(data=bad):
                (self.backup / "storage1.raw").write_bytes(bad)
                with self.assertRaisesRegex(ValueError, "exactly one"):
                    vehicle_adaptation(self.backup)

    def test_adaptation_shorter_than_native_consumers_fails(self):
        short = self.raw.replace(self.gen2.hex().encode(), b"0000")
        (self.backup / "storage1.raw").write_bytes(short)
        (self.backup / "storage2.raw").write_bytes(short)
        with self.assertRaisesRegex(ValueError, "shorter than native consumers"):
            vehicle_adaptation(self.backup)

    def test_carplay_coding_bit_flip_is_scoped(self):
        mutated = bytearray(apply_carplay_coding(self.adaptation))
        self.assertEqual(mutated[51] & 1, 1)
        self.assertEqual((mutated[51] & 0x60), self.adaptation[51] & 0x60)
        mutated[51] ^= 1
        self.assertEqual(bytes(mutated), self.adaptation)

    def test_fec_container_records_active_fsids(self):
        import struct
        fw = self.backup.parent
        fec = fw / "MU1316-special/rcc/mnt/efs-persist/FEC/FecContainer.fec"
        fec.parent.mkdir(parents=True, exist_ok=True)
        fsids = [0x00040100, 0x02310026, 0x00050000]
        body = bytearray(0xF3)
        body[0x00:0x04] = struct.pack("<I", 1)
        body[0x04:0x08] = struct.pack("<I", 0xBF)
        body[0x13] = ord("V")
        body[0x14:0x25] = b"VWATESTVIN0000001"
        body[0x2A] = len(fsids)
        for i, v in enumerate(fsids):
            body[0x2B + 4 * i:0x2F + 4 * i] = struct.pack(">I", v)
        body[0xC7:0xCB] = struct.pack("<I", len(fsids))
        for i, v in enumerate(fsids):
            body[0xCB + 4 * i:0xCF + 4 * i] = struct.pack("<I", v)
        fec.write_bytes(bytes(body))
        parsed, record = vehicle_fec_container(fw)
        self.assertEqual(parsed, fsids)
        self.assertEqual(record["vin"], "VWATESTVIN0000001")
        self.assertEqual(record["active_fsids"], [f"{v:08x}" for v in fsids])

    def test_fec_container_rejects_inconsistent_lists(self):
        import struct
        fw = self.backup.parent
        fec = fw / "MU1316-special/rcc/mnt/efs-persist/FEC/FecContainer.fec"
        fec.parent.mkdir(parents=True, exist_ok=True)
        body = bytearray(0xF3)
        body[0x00:0x04] = struct.pack("<I", 1)
        body[0x04:0x08] = struct.pack("<I", 0xBF)
        body[0x2A] = 2
        body[0x2B:0x33] = struct.pack(">II", 0x00040100, 0x02310026)
        body[0xC7:0xCB] = struct.pack("<I", 2)
        body[0xCB:0xD3] = struct.pack("<II", 0x00040100, 0x00050000)
        fec.write_bytes(bytes(body))
        with self.assertRaisesRegex(ValueError, "disagree"):
            vehicle_fec_container(fw)

    def test_conflicting_generations_fail(self):
        (self.backup / "storage2.raw").write_bytes(self.raw.replace(b"000102", b"000103"))
        with self.assertRaisesRegex(ValueError, "disagree"):
            vehicle_coding(self.backup)

    def test_missing_or_ambiguous_key_fails(self):
        for bad in (b"not coding", self.raw + b"\0" + self.raw):
            with self.subTest(data=bad):
                (self.backup / "storage1.raw").write_bytes(bad)
                with self.assertRaisesRegex(ValueError, "exactly one"):
                    vehicle_coding(self.backup)

    def test_short_blob_fails(self):
        (self.backup / "storage1.raw").write_bytes(b"VW_Coding=0011\0")
        with self.assertRaisesRegex(ValueError, "at least 25"):
            vehicle_coding(self.backup)

    def test_diagnostic_cannot_pass_on_si_startup_alone(self):
        baseline = acceptance_markers(True, False)
        providers = acceptance_markers(True, False, True)
        self.assertNotIn("native_coding_delivery", baseline)
        self.assertIn("native_coding_delivery", providers)
        self.assertIn("native_adaptation_delivery", providers)
        self.assertTrue(all("provider_running_" + app in providers
                            for app in ("persistence", "diagnosis", "fec")))

    def test_positive_admission_profile_is_separate_and_stricter(self):
        strict = acceptance_markers(True, False, True)
        coded = acceptance_markers(True, False, True, True)
        # The strict vehicle profile asserts this car's truthful decode.
        self.assertIn("SI-NATIVE: configuration usb_mode=3 carplay_coding=0",
                      strict["native_vehicle_values"])
        self.assertIn("not granted by Coding", strict["native_coding_gate_denies"])
        self.assertNotIn("native_carplay_coded", strict)
        # The opt-in CarPlay-coded profile swaps the expected decode and the
        # denial boundary: Coding passes, FEC (still absent) denies.
        self.assertIn("carplay_coding=1", coded["native_carplay_coded"])
        self.assertIn("not granted by FEC", coded["native_coding_gate_passes"])
        self.assertNotIn("native_vehicle_values", coded)

    def test_rcc_peer_requires_positive_lifecycle(self):
        positive=acceptance_markers(True,False,True,True,True)
        negative=acceptance_markers(True,False,True,True)
        self.assertIn("native_admission_refusal",negative)
        self.assertNotIn("native_admission_refusal",positive)
        self.assertNotIn("native_coding_gate_passes",positive)
        for name in ("native_fec_grants","dio_native_asi_setup","dio_receiver_ready",
                     "dio_cancel","dio_reconnect","mdns_preserved"):
            self.assertIn(name,positive)


if __name__ == "__main__":
    unittest.main()

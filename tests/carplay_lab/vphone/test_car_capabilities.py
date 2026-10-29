import unittest

from .car_capabilities import (MAPS30_PROFILE_PPID, MAPS_RATE, USER_INFO,
                               restore_maps_rate, with_maps_rate, with_maps30_ppid)
from tests.carplay_lab.legacy_ios.usb_probe import (identification_message,
                                                  parameter, parse_control_message)


class MapsRateRestorationTests(unittest.TestCase):
    def setUp(self):
        self.original = {"CarCapabilitiesContentVersion": 40,
                         "owned": {USER_INFO: {}, "other_setting": 7}}

    def test_exact_restoration_without_modifying_original(self):
        applied = with_maps_rate(self.original, "owned")
        self.assertEqual(applied["owned"][USER_INFO][MAPS_RATE], 30)
        self.assertEqual(self.original["owned"][USER_INFO], {})
        self.assertEqual(restore_maps_rate(applied, self.original, "owned"), self.original)

    def test_restores_previous_value_and_preserves_unrelated_changes(self):
        self.original["owned"][USER_INFO][MAPS_RATE] = 20
        current = with_maps_rate(self.original, "owned")
        current["another_car"] = {USER_INFO: {"example": True}}
        current["owned"][USER_INFO]["new_setting"] = 9
        restored = restore_maps_rate(current, self.original, "owned")
        self.assertEqual(restored["owned"][USER_INFO], {MAPS_RATE: 20, "new_setting": 9})
        self.assertEqual(restored["another_car"], current["another_car"])

    def test_conflicting_rate_or_missing_profile_is_not_overwritten(self):
        current = with_maps_rate(self.original, "owned")
        current["owned"][USER_INFO][MAPS_RATE] = 25
        with self.assertRaises(ValueError):
            restore_maps_rate(current, self.original, "owned")
        with self.assertRaises(ValueError):
            restore_maps_rate({}, self.original, "owned")

    def test_already_restored_value_is_accepted(self):
        self.assertEqual(restore_maps_rate(self.original, self.original, "owned"), self.original)


class MapsProfileIdentificationTests(unittest.TestCase):
    def test_only_appends_ppid_and_updates_control_length(self):
        original = identification_message()
        patched = with_maps30_ppid(original)
        self.assertEqual(patched[4:len(original)], original[4:])
        self.assertEqual(int.from_bytes(patched[2:4], "big"), len(patched))
        _, fields = parse_control_message(patched)
        self.assertEqual(fields.pop(34), MAPS30_PROFILE_PPID.encode() + b"\0")
        self.assertEqual(fields, parse_control_message(original)[1])
        self.assertEqual(with_maps30_ppid(patched), patched)

    def test_existing_foreign_identity_is_rejected(self):
        original = identification_message()
        payload = original + parameter(34, b"existing-car\0")
        payload = payload[:2] + len(payload).to_bytes(2, "big") + payload[4:]
        with self.assertRaises(ValueError):
            with_maps30_ppid(payload)

    def test_malformed_or_different_message_is_rejected(self):
        original = identification_message()
        for payload in (original[:-1], original[:4] + b"\x43\x01" + original[6:],
                        original + b"\0", b"", bytearray(original)):
            with self.subTest(payload=payload[:8]):
                with self.assertRaises(ValueError):
                    with_maps30_ppid(payload)


if __name__ == "__main__":
    unittest.main()

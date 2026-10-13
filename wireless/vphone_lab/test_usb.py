"""Ownership, privilege and recovery checks without contacting a guest."""
import json
from pathlib import Path
import plistlib
import random
import string
import tempfile
import unittest
from unittest.mock import patch

from wireless.vphone_lab import lab, usb


class NativeUSBTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.out = Path(directory.name)
        self.udid = "0000FE01-1122334455667788"
        self.binary = self.out / "helper"
        self.binary.write_bytes(b"test-only binary")
        self.info = {"model": "iPhone99,11", "ios_version": "26.6.2"}
        self.exit_status = 0
        self.truncated = False
        self.log = "native result"
        self.calls = []
        self.patches = [patch.object(lab, "OUT", self.out),
                        patch.object(lab, "profile", return_value={}),
                        patch.object(lab, "require_machine", return_value={"udid": self.udid}),
                        patch.object(usb, "build", return_value=self.binary),
                        patch.object(lab, "rpc", side_effect=self.rpc)]
        self.mocks = [p.start() for p in self.patches]
        for p in self.patches:
            self.addCleanup(p.stop)

    def rpc(self, settings, method, params=None):
        self.calls.append((method, params))
        if method == "device.info":
            return {"result": self.info}
        if method == "services.status":
            return {"result": {"running": False, "last_exit_status": self.exit_status}}
        if method == "files.read":
            return {"result": {"content": self.log, "truncated": self.truncated}}
        return {"ok": True}

    def test_invalid_actions_and_class_selection_do_not_contact_guest(self):
        for mode, selected in [("unknown", None), ("manager-apply", "ACCTransportIOAccessoryPort"),
                               ("iap-inspect", "NSObject"), ("iap-inspect", "ACCTransport"),
                               ("iap-inspect", "ACCTransport" + "X" * 81)]:
            with self.subTest(mode=mode, selected=selected), self.assertRaises(ValueError):
                usb._run(mode, selected)
        self.mocks[1].assert_not_called()
        self.assertEqual(self.calls, [])

    def test_fuzz_class_names_reject_control_and_path_characters(self):
        generator = random.Random(26)
        for _ in range(128):
            selected = "ACCTransport" + generator.choice("/\\\x00\n:;.💥") + "".join(generator.choices(string.ascii_letters, k=8))
            with self.subTest(selected=repr(selected)), self.assertRaises(ValueError):
                usb._run("iap-inspect", selected)
        self.assertEqual(self.calls, [])

    def test_changed_model_and_firmware_fail_before_guest_writes(self):
        for info in ({"model": "iPhone17,3", "ios_version": "26.6.2"},
                     {"model": "iPhone99,11", "ios_version": "26.7"}):
            self.calls.clear()
            self.info = info
            with self.assertRaises(RuntimeError):
                usb._run("manager-apply")
            self.assertEqual([m for m, _ in self.calls], ["device.info"])
        self.mocks[3].assert_not_called()

    def test_other_serial_rejected_before_guest_rpc(self):
        self.mocks[2].return_value = {"udid": "00008030-1122334455667788"}
        with self.assertRaises(ValueError):
            usb._run("manager-apply")
        self.assertEqual(self.calls, [])

    def test_diagnostics_do_not_request_catalogue_privilege(self):
        for action in usb.READ_ONLY_ACTIONS:
            with self.subTest(action=action):
                usb._run(action)
                self.mocks[3].assert_called_with(catalogue=False)

    def test_manager_actions_request_catalogue_privilege(self):
        for action in ("manager-apply", "manager-remove"):
            with self.subTest(action=action):
                usb._run(action)
                self.mocks[3].assert_called_with(catalogue=True)

    def test_port_experiment_requires_explicit_reviewed_integer_before_rpc(self):
        for stream in (None, True, 0, 2, 3, 6, -1, "1"):
            with self.subTest(stream=stream), self.assertRaises(ValueError):
                usb._run("port-prepare", stream_type=stream)
        with self.assertRaises(ValueError):
            usb._run("iap-inspect", stream_type=1)
        self.assertEqual(self.calls, [])
        self.mocks[1].assert_not_called()

    def test_port_experiment_records_literal_type_and_catalogue_privilege(self):
        for stream in (1, 4, 5):
            self.calls.clear()
            result = usb._run("port-prepare", stream_type=stream)
            self.assertEqual(result["stream_type"], stream)
            self.mocks[3].assert_called_with(catalogue=True)
            jobs = [plistlib.loads(p["content"].encode()) for m, p in self.calls
                    if m == "files.write" and p["path"].endswith(".plist")]
            self.assertEqual(jobs[0]["ProgramArguments"][-1], str(stream))

    def test_peer_nonce_and_observer_are_required_before_rpc(self):
        for nonce in (None, True, "", "0" * 15, "A" * 16, "0" * 17, "../" + "0" * 13):
            with self.subTest(nonce=nonce), self.assertRaises(ValueError):
                usb._run("iap-peer", peer_nonce=nonce, on_launch=lambda path: None)
        with self.assertRaises(ValueError):
            usb._run("iap-peer", peer_nonce="a" * 16)
        with self.assertRaises(ValueError):
            usb._run("iap-smoke", peer_nonce="a" * 16)
        self.assertEqual(self.calls, [])

    def test_peer_job_observer_and_literal_nonce_do_not_add_privileges(self):
        observed = []
        usb._run("iap-peer", peer_nonce="0123456789abcdef", on_launch=observed.append)
        self.mocks[3].assert_called_with(catalogue=False)
        jobs = [plistlib.loads(p["content"].encode()) for m, p in self.calls
                if m == "files.write" and p["path"].endswith(".plist")]
        self.assertEqual(jobs[0]["ProgramArguments"][-1], "0123456789abcdef")
        self.assertEqual(observed, [jobs[0]["StandardOutPath"]])

    def test_native_class_selection_is_a_literal_program_argument(self):
        selected = "ACCTransportIOAccessorySharedManager"
        usb._run("iap-inspect", selected)
        jobs = [plistlib.loads(p["content"].encode()) for m, p in self.calls
                if m == "files.write" and p["path"].endswith(".plist")]
        self.assertEqual(jobs[0]["ProgramArguments"], [usb.GUEST_DIR + "/usb-description", "iap-inspect", self.udid, selected])

    def test_restart_creates_guest_directory_before_existing_backup_upload(self):
        (self.out / "usb-original.json").write_text(json.dumps({"udid": self.udid, "description": {"uniqueDeviceID": self.udid}}))
        usb._run("restore")
        mkdir = next(i for i, (m, _) in enumerate(self.calls) if m == "files.mkdir")
        backup = next(i for i, (m, p) in enumerate(self.calls) if m == "files.write" and p["path"].endswith("usb-original.plist"))
        self.assertLess(mkdir, backup)

    def test_direct_ncm_saves_original_and_restore_reuses_it_without_catalogue(self):
        original = {"uniqueDeviceID": self.udid, "ConfigurationDescriptors": [{"Interfaces": ["AppleUSBMux"]}]}
        self.log = "owned controller\n" + plistlib.dumps(original).decode()
        usb._run("ncm-direct")
        saved = json.loads((self.out / "usb-original.json").read_text())
        self.assertEqual(saved["description"], original)
        self.mocks[3].assert_called_with(catalogue=False)
        self.log = "native restored"
        usb._run("restore")
        uploads = [plistlib.loads(p["content"].encode()) for m, p in self.calls
                   if m == "files.write" and p["path"].endswith("usb-original.plist")]
        self.assertEqual(uploads, [original, original])

    def test_direct_ncm_rejects_backup_from_different_guest_before_build(self):
        (self.out / "usb-original.json").write_text(json.dumps({"udid": "other", "description": {}}))
        with self.assertRaisesRegex(RuntimeError, "another guest"):
            usb._run("ncm-direct")
        self.mocks[3].assert_not_called()
        self.assertFalse(any(m == "files.write" for m, _ in self.calls))

    def test_failed_native_result_keeps_evidence_and_removes_only_own_job(self):
        self.exit_status = 39 << 8
        with self.assertRaises(RuntimeError):
            usb._run("manager-remove")
        self.assertEqual(json.loads((self.out / "usb-manager-remove.json").read_text())["status"]["last_exit_status"], self.exit_status)
        unload = [p for m, p in self.calls if m == "services.unload"]
        self.assertEqual(len(unload), 1)
        self.assertTrue(unload[0]["force"])
        self.assertTrue(unload[0]["paths"][0].startswith(usb.GUEST_DIR + "/usb-manager-remove-"))
        self.assertEqual(len([m for m, _ in self.calls if m == "files.remove"]), 2)

    def test_truncated_diagnostic_is_not_reported_as_success(self):
        self.truncated = True
        with self.assertRaisesRegex(RuntimeError, "truncated"):
            usb._run("iap-inspect")
        self.assertTrue(any(m == "services.unload" for m, _ in self.calls))


if __name__ == "__main__":
    unittest.main()

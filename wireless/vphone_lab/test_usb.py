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
            return {"result": {"content": "native result", "truncated": self.truncated}}
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

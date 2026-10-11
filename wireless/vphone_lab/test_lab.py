import copy
import json
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

from wireless.vphone_lab import lab
from wireless.lab_common import qnx_receiver


class VPhoneLabTests(unittest.TestCase):
    def setUp(self):
        self.settings = lab.profile()
        self.machine = {"name": self.settings["machine"], "libraryRoot": str(lab.LIBRARY),
                        "bundle": self.settings["bundle"], "state": "stopped"}

    def test_creation_pins_firmware_units_and_native_services(self):
        arguments = lab.creation_arguments(self.settings)
        for option, expected in [("--bundle", "2.9.2"), ("--memory", "6144"),
                                 ("--disk-size", "64"), ("--slim", "off"),
                                 ("--network", "nat"), ("--device", "iPhone17,3")]:
            self.assertEqual(arguments[arguments.index(option) + 1], expected)
        self.assertIn(self.settings["iphone_source"], arguments)
        self.assertIn(self.settings["cloudos_source"], arguments)
        self.assertNotIn("latest", arguments)
        self.assertNotIn("--keep-artifacts", arguments)

    def test_profile_rejects_unpinned_sources_and_invalid_names(self):
        mutations = [("machine", "../other"), ("machine", "phone;touch"), ("bundle", "latest"),
                     ("iphone_source", "http://updates.cdn-apple.com/a"),
                     ("iphone_source", "https://updates.cdn-apple.com.evil/a"),
                     ("cloudos_source", "https://updates.cdn-apple.com/a?token=x"),
                     ("memory_mb", True), ("cpu", 0), ("slim", "on"), ("network", "tunnel")]
        for key, value in mutations:
            changed = copy.deepcopy(self.settings)
            changed[key] = value
            with self.subTest(key=key, value=value), patch.object(Path, "read_text", return_value=json.dumps(changed)):
                with self.assertRaises(ValueError):
                    lab.profile()

    def test_other_library_and_duplicate_machine_ownership(self):
        other = dict(self.machine, libraryRoot="/tmp/other-library")
        self.assertIsNone(lab.owned_machine(self.settings, [other]))
        self.assertEqual(lab.owned_machine(self.settings, [other, self.machine]), self.machine)
        with self.assertRaises(ValueError):
            lab.owned_machine(self.settings, [self.machine, self.machine])

    def test_blocked_or_existing_guest_cannot_create(self):
        for report in ({"machine": None, "ready": False, "blockers": ["Research Guests disabled"]},
                       {"machine": self.machine, "ready": True, "blockers": []}):
            with patch.object(lab, "doctor", return_value=report), patch.object(lab, "client") as client:
                with self.assertRaises(RuntimeError):
                    lab.create(self.settings)
                client.assert_not_called()

    def test_low_disk_and_concurrent_operation_cannot_create(self):
        report = {"machine": None, "ready": True, "blockers": [], "free_gb": 89.99}
        with patch.object(lab, "doctor", return_value=report), patch.object(lab, "client") as client:
            with self.assertRaisesRegex(RuntimeError, "free space"):
                lab.create(self.settings)
            client.assert_not_called()
        report["free_gb"] = 100
        with patch.object(lab, "doctor", return_value=report), \
                patch.object(lab, "client", return_value=[{"state": "busy: restore"}]) as client:
            with self.assertRaisesRegex(RuntimeError, "serial"):
                lab.create(self.settings)
            client.assert_called_once_with("vm", "list")

    def test_success_requires_stopped_guest_and_correct_bundle(self):
        report = {"machine": None, "ready": True, "blockers": [], "free_gb": 100}
        for final in (self.machine, dict(self.machine, state="running"),
                      dict(self.machine, bundle="2.8.0")):
            with patch.object(lab, "doctor", return_value=report), \
                    patch.object(lab, "client", side_effect=[[], {"created": True}, [final]]) as client, \
                    patch.object(lab, "save"):
                if final == self.machine:
                    self.assertEqual(lab.create(self.settings), final)
                else:
                    with self.assertRaises(RuntimeError):
                        lab.create(self.settings)
                self.assertEqual(client.call_args_list[1].kwargs, {"timeout": None})

    def test_rpc_requires_the_owned_running_guest(self):
        with patch.object(lab, "client", return_value=[self.machine]) as client:
            with self.assertRaisesRegex(RuntimeError, "running"):
                lab.rpc(self.settings, "device.info")
            client.assert_called_once_with("vm", "list")

    def test_reports_are_atomic_private_and_preserve_previous_on_bad_json(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(lab, "OUT", Path(directory)):
            file = lab.save("doctor.json", {"ready": False})
            self.assertEqual(stat.S_IMODE(file.stat().st_mode), 0o600)
            with self.assertRaises(TypeError):
                lab.save("doctor.json", {"bad": object()})
            self.assertEqual(json.loads(file.read_text()), {"ready": False})
            self.assertEqual(list(Path(directory).iterdir()), [file])

    def test_preexisting_or_changed_identity_cannot_be_operated(self):
        running = dict(self.machine, state="running", udid="vm-original")
        with tempfile.TemporaryDirectory() as directory, patch.object(lab, "OUT", Path(directory)), \
                patch.object(lab, "client", return_value=[running]):
            with self.assertRaisesRegex(RuntimeError, "not created"):
                lab.require_machine(self.settings)
            lab.save("machine.json", dict(running, udid="vm-other"))
            with self.assertRaisesRegex(RuntimeError, "identity changed"):
                lab.require_machine(self.settings)
            lab.save("machine.json", running)
            self.assertEqual(lab.require_machine(self.settings), running)

    def test_fresh_host_doctor_does_not_require_bundle_discovery(self):
        status = {"machines": 0, "developerTools": False, "canInstallBundles": False,
                  "bundleReady": False, "hostReady": False}
        with patch.object(lab, "client", return_value=status) as client, \
                patch.object(lab.subprocess, "run", return_value=SimpleNamespace(
                    returncode=0, stdout="Allow Research Guests status: disabled\n")) as run, \
                patch.object(lab.shutil, "disk_usage", return_value=SimpleNamespace(free=100*1024**3)), \
                patch.object(lab.plistlib, "loads", return_value={"CFBundleShortVersionString": "2.9.2"}), \
                patch.object(Path, "read_bytes", return_value=b"fixture"), patch.object(lab, "save"):
            report = lab.doctor(self.settings)
            self.assertFalse(report["ready"])
            self.assertFalse(report["research_guests_enabled"])
            client.assert_called_once_with("status")
            self.assertEqual(run.call_args.args[0], ["csrutil", "allow-research-guests", "status"])

    def test_qnx_artifacts_cannot_escape_owned_build(self):
        original = qnx_receiver.OUT
        try:
            with self.assertRaises(ValueError):
                qnx_receiver.set_output(Path("/tmp/other-qnx"))
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "build").mkdir()
                outside = root / "other"
                outside.mkdir()
                (root / "build/escape").symlink_to(outside)
                with patch.object(qnx_receiver, "ROOT", root):
                    with self.assertRaises(ValueError):
                        qnx_receiver.set_output(root / "build/escape")
                    qnx_receiver.set_output(root / "build/vphone-lab/qnx-network")
                    self.assertEqual(qnx_receiver.OUT, (root / "build/vphone-lab/qnx-network").resolve())
        finally:
            qnx_receiver.OUT = original


if __name__ == "__main__":
    unittest.main()

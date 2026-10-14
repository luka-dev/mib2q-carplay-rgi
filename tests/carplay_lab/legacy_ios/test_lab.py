"""Controller regressions; these tests never start an emulator or attach images."""
import importlib.util
from pathlib import Path
import plistlib
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("ios_vm_lab", Path(__file__).with_name("lab.py"))
lab_module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(lab_module)


class ControllerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.lab = self.root / "lab"
        self.lab.mkdir()
        self.disk = self.lab / "root"
        self.disk.touch()
        self.command = ["qemu", "-drive", f"file={self.disk},format=raw,if=none,id=root"]

    def test_accepts_detached_local_image(self):
        with patch.object(lab_module.subprocess, "check_output", return_value=plistlib.dumps({"images": []})):
            lab_module.check_guest_disks(self.lab, self.command)

    def test_rejects_attached_image_through_symlink(self):
        alias = self.lab / "alias"
        alias.symlink_to(self.disk)
        attached = {"images": [{"image-path": str(alias)}]}
        with patch.object(lab_module.subprocess, "check_output", return_value=plistlib.dumps(attached)):
            with self.assertRaisesRegex(RuntimeError, "still attached"):
                lab_module.check_guest_disks(self.lab, self.command)

    def test_rejects_symlink_to_external_disk(self):
        external = self.root / "external"
        external.touch()
        alias = self.lab / "alias"
        alias.symlink_to(external)
        with self.assertRaisesRegex(RuntimeError, "inside the laboratory"):
            lab_module.check_guest_disks(self.lab, ["qemu", "-drive", f"file={alias}"])

    def test_rejects_malformed_and_alternative_drive_records(self):
        for command in (["qemu", "-drive"], ["qemu", "-drive", "format=raw"],
                        ["qemu", "-drive", "bad"], ["qemu", "-blockdev", "file=/dev/disk0"], ["qemu"]):
            with self.subTest(command=command), self.assertRaises(RuntimeError):
                lab_module.check_guest_disks(self.lab, command)

    def test_does_not_signal_reused_pid(self):
        (self.lab / "ios.pid").write_text("123\n")
        other = subprocess.CompletedProcess([], 0, stdout="/usr/bin/sleep 60\n")
        with patch.object(lab_module.subprocess, "run", return_value=other), patch.object(lab_module.os, "kill") as kill:
            self.assertIsNone(lab_module.stop_guest(self.lab))
            kill.assert_not_called()

    def test_stop_waits_for_exit(self):
        with patch.object(lab_module, "running_pid", side_effect=[123, 123, None]), \
                patch.object(lab_module.os, "kill") as kill, patch.object(lab_module.time, "sleep") as sleep:
            self.assertEqual(lab_module.stop_guest(self.lab), 123)
            kill.assert_called_once_with(123, signal.SIGTERM)
            sleep.assert_called_once()

    def test_stop_reports_timeout_without_claiming_exit(self):
        with patch.object(lab_module, "running_pid", return_value=123), \
                patch.object(lab_module.os, "kill") as kill:
            with self.assertRaisesRegex(RuntimeError, "has not exited"):
                lab_module.stop_guest(self.lab, timeout=0)
            kill.assert_called_once_with(123, signal.SIGTERM)

    def test_forced_stop_escalates_only_owned_guest(self):
        with patch.object(lab_module, "running_pid", side_effect=[123, 123, None]), \
                patch.object(lab_module.os, "kill") as kill, patch.object(lab_module.time, "sleep"):
            self.assertEqual(lab_module.stop_guest(self.lab, force=True, timeout=0), 123)
            self.assertEqual(kill.call_args_list[0].args, (123, signal.SIGTERM))
            self.assertEqual(kill.call_args_list[1].args, (123, signal.SIGKILL))

    def test_query_targets_selected_loopback_tunnel(self):
        argv = ["lab.py", "query", "--lab", str(self.lab), "--usbmux-port", "32229"]
        with patch.object(sys, "argv", argv), patch.object(lab_module, "running_pid", return_value=None), \
                patch.object(lab_module.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as run:
            self.assertEqual(lab_module.main(), 0)
            self.assertEqual(run.call_args.kwargs["env"]["USBMUXD_SOCKET_ADDRESS"], "127.0.0.1:32229")
            self.assertEqual(run.call_args.args[0], [str(self.lab / "query-restore")])

    def test_query_rejects_invalid_port_before_connecting(self):
        for port in ("0", "65536"):
            with self.subTest(port=port), patch.object(sys, "argv", ["lab.py", "query", "--usbmux-port", port]), \
                    patch.object(lab_module.subprocess, "run") as run, patch.object(sys, "stderr"), \
                    self.assertRaises(SystemExit):
                lab_module.main()
            run.assert_not_called()

    def test_start_sets_display_tick_from_launch_record(self):
        import json
        from types import SimpleNamespace

        for display, expected in (("none", "1"), ("cocoa", "0")):
            command = [str(self.lab / "inferno/build/qemu-system-aarch64"),
                       "-display", display, "-drive", f"file={self.disk},format=raw"]
            (self.lab / "ios-command.json").write_text(json.dumps(command))
            with self.subTest(display=display), \
                    patch.object(sys, "argv", ["lab.py", "start", "--lab", str(self.lab)]), \
                    patch.object(lab_module, "running_pid", return_value=None), \
                    patch.object(lab_module, "check_guest_disks"), \
                    patch.dict(lab_module.os.environ, MHI2_IOS_VM_HEADLESS_DISPLAY="unexpected",
                               MHI2_IOS_VM_TRISTAR="1", MHI2_IOS_VM_TRISTAR_TRACE="1"), \
                    patch.object(lab_module.subprocess, "Popen", return_value=SimpleNamespace(pid=123)) as popen:
                self.assertEqual(lab_module.main(), 0)
                self.assertEqual(popen.call_args.kwargs["env"]["MHI2_IOS_VM_HEADLESS_DISPLAY"], expected)
                self.assertEqual(popen.call_args.kwargs["env"]["MHI2_IOS_VM_DIAGNOSTICS"], "0")
                self.assertEqual(popen.call_args.kwargs["env"]["MHI2_IOS_VM_TRISTAR"], "0")
                self.assertEqual(popen.call_args.kwargs["env"]["MHI2_IOS_VM_TRISTAR_TRACE"], "0")

    def test_virtual_dock_requires_normal_start(self):
        for argv in (["lab.py", "start", "--virtual-dock"],
                     ["lab.py", "status", "--virtual-dock"]):
            with self.subTest(argv=argv), patch.object(sys, "argv", argv), \
                    patch.object(lab_module.subprocess, "Popen") as popen, \
                    patch.object(sys, "stderr"), self.assertRaises(SystemExit):
                lab_module.main()
            popen.assert_not_called()

    def test_ncm_visibility_requires_explicit_clone_and_preserves_record(self):
        import json
        from types import SimpleNamespace
        with patch.object(sys, "argv", ["lab.py", "start", "--ncm-visible"]), \
                patch.object(sys, "stderr"), self.assertRaises(SystemExit):
            lab_module.main()
        command = [str(self.lab / "inferno/build/qemu-system-aarch64"),
                   "-append", "serial=3", "-drive", f"file={self.disk}"]
        record = self.lab / "ios-normal-combined-usb-command.json"
        record.write_text(json.dumps(command))
        argv = ["lab.py", "start", "--boot", "normal", "--virtual-dock",
                "--combined-usb", "--ncm-visible", "--lab", str(self.lab)]
        with patch.object(sys, "argv", argv), patch.object(lab_module, "running_pid", return_value=None), \
                patch.object(lab_module, "check_guest_disks"), \
                patch.object(lab_module.subprocess, "Popen", return_value=SimpleNamespace(pid=123)) as popen:
            self.assertEqual(lab_module.main(), 0)
        self.assertIn("ncm-no-hide=1", popen.call_args.args[0][2])
        self.assertEqual(json.loads(record.read_text()), command)
        self.assertEqual((self.lab / "ios-ncm-visible.txt").read_text().strip(), "1")

    def test_virtual_dock_is_explicit_and_does_not_change_launch_record(self):
        import json
        from types import SimpleNamespace
        command = [str(self.lab / "inferno/build/qemu-system-aarch64"),
                   "-append", "serial=3", "-display", "none", "-drive", f"file={self.disk}"]
        record = self.lab / "ios-normal-command.json"
        record.write_text(json.dumps(command))
        with patch.object(sys, "argv", ["lab.py", "start", "--boot", "normal",
                                         "--virtual-dock", "--lab", str(self.lab)]), \
                patch.object(lab_module, "running_pid", return_value=None), \
                patch.object(lab_module, "check_guest_disks"), \
                patch.object(lab_module.subprocess, "Popen", return_value=SimpleNamespace(pid=123)) as popen:
            self.assertEqual(lab_module.main(), 0)
        self.assertEqual(popen.call_args.args[0][2], "serial=3 force-usb-test-mode=1")
        self.assertEqual(popen.call_args.kwargs["env"]["MHI2_IOS_VM_TRISTAR"], "1")
        self.assertEqual((self.lab / "ios-virtual-dock.txt").read_text().strip(), "1")
        self.assertEqual(json.loads(record.read_text()), command)


if __name__ == "__main__":
    unittest.main()

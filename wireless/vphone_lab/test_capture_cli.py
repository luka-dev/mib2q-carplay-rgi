"""Shared capture commands retain legacy defaults and contain lab artifacts."""
from contextlib import chdir, redirect_stderr, redirect_stdout
import io
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from wireless.lab_common import extract_native_video, frame_pacing, screen_wire_pacing


class CaptureCLITests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.lab = self.root / "build/vphone-lab"
        self.lab.mkdir(parents=True)
        self.data = b"\0\0\0\1\x65abc"
        self.log = self.lab / "serial.log"
        self.log.write_text("[native-video-qemu] begin=1 bytes=8 mono_ns=1000000000\n"
                            "[native-video-qemu] record=1 offset=0 hex=" + self.data.hex() +
                            "\n[native-video-qemu] end=1 complete=1\n")
        self.pcap = self.lab / "ethernet.pcap"
        self.pcap.write_bytes(struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
        self.modules = (extract_native_video, frame_pacing, screen_wire_pacing)

    def run_cli(self, module, *options):
        source = self.pcap if module is screen_wire_pacing else self.log
        with patch.object(module, "__file__", str(self.root / "wireless/lab_common/tool.py")), \
                patch("sys.argv", ["tool", str(source), *map(str, options)]), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            module.main()

    def test_explicit_vphone_lab_writes_valid_capture_and_reports(self):
        for module in self.modules:
            suffix = ".h264" if module is extract_native_video else ".json"
            output = self.lab / (module.__name__.split(".")[-1] + suffix)
            self.run_cli(module, "--lab-dir", self.lab, "--output", output)
            self.assertTrue(output.is_file())
            if suffix == ".h264":
                self.assertEqual(output.read_bytes(), self.data)

    def test_legacy_and_explicit_extraction_defaults_remain_separate(self):
        self.run_cli(extract_native_video)
        legacy = self.root / "build/ios-vm-lab/combined-usb/native-carplay.h264"
        self.assertEqual(legacy.read_bytes(), self.data)
        self.run_cli(extract_native_video, "--lab-dir", self.lab)
        self.assertEqual((self.lab / "native-carplay.h264").read_bytes(), self.data)

    def test_legacy_report_scope_does_not_depend_on_working_directory(self):
        legacy = self.root / "build/ios-vm-lab"
        legacy.mkdir()
        for module in (frame_pacing, screen_wire_pacing):
            output = legacy / (module.__name__.split(".")[-1] + ".json")
            with chdir(self.lab):
                self.run_cli(module, "--output", output)
            self.assertTrue(output.is_file())

    def test_traversal_and_external_symlink_do_not_overwrite_other_files(self):
        outside = self.root / "outside"
        outside.mkdir()
        (self.lab / "escape").symlink_to(outside, target_is_directory=True)
        for module in self.modules:
            suffix = ".h264" if module is extract_native_video else ".json"
            target = outside / ("sentinel" + suffix)
            target.write_bytes(b"preserve")
            for output in (self.lab / "escape" / target.name,
                           self.lab / "../../outside" / target.name):
                with self.subTest(module=module.__name__, output=output), self.assertRaises(SystemExit):
                    self.run_cli(module, "--lab-dir", self.lab, "--output", output)
                self.assertEqual(target.read_bytes(), b"preserve")

    def test_build_root_and_outside_lab_directory_are_rejected(self):
        for module in self.modules:
            for directory in (self.root / "build", self.root / "outside"):
                suffix = ".h264" if module is extract_native_video else ".json"
                with self.subTest(module=module.__name__, directory=directory), self.assertRaises(SystemExit):
                    self.run_cli(module, "--lab-dir", directory, "--output", directory / ("output" + suffix))


if __name__ == "__main__":
    unittest.main()

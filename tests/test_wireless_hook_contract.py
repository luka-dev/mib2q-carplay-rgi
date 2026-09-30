#!/usr/bin/env python3
"""Exercise real wireless/altScreen callbacks with host CF/key/ARM-pointer fixtures."""
from pathlib import Path
import os
import platform
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class WirelessHookContract(unittest.TestCase):
    def test_callbacks(self):
        with tempfile.TemporaryDirectory(prefix="wireless-contract-") as directory:
            binary = Path(directory) / "contract"
            flags = ["-Wl,-dead_strip"] if platform.system() == "Darwin" else [
                "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-ldl"]
            command = [os.environ.get("CC", "cc"), "-O1", "-g", "-pthread",
                       "-DENABLE_LOGGING=0", "-fsanitize=address,undefined",
                       "tests/wireless_owner_setup_test.c", "tests/wireless_media_contract_test.c",
                       "tests/wireless_altscreen_policy_test.c", "hook/altscreen/altscreen_av.c",
                       *flags, "-o", str(binary)]
            subprocess.run(command, cwd=ROOT, check=True)
            for suite in ("media", "altscreen", "owner"):
                with self.subTest(suite=suite):
                    subprocess.run([str(binary), suite], cwd=ROOT, check=True, timeout=20)

if __name__ == "__main__":
    unittest.main()

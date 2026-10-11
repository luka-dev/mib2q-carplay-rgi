"""Keep capture permissions and renderer dependencies out of the default helper."""
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import prepare_combined_usb as prepare


class ScreenRenderBuildTests(unittest.TestCase):
    def test_default_helper_keeps_its_existing_permissions_and_libraries(self):
        self.check_build(False)

    def test_capture_helper_limits_iokit_permission_to_iosurface(self):
        self.check_build(True)

    def test_framebuffer_role_is_exclusive_to_explicit_framebuffer_build(self):
        self.check_build(False, framebuffer=True)

    def check_build(self, enabled, framebuffer=False):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            response = subprocess.CompletedProcess([], 0, stdout="", stderr="CDHash=" + "a"*40 + "\n")
            with patch.object(prepare.subprocess, "run", return_value=response) as run:
                self.assertEqual(prepare.build_helper(base, enabled, framebuffer), "a"*40)
            rights = plistlib.loads((base / "entitlements.plist").read_bytes())
            link = next(call.args[0] for call in run.call_args_list if call.args[0][0] == "ld")
            if enabled or framebuffer:
                self.assertEqual(rights["com.apple.security.iokit-user-client-class"], ["IOSurfaceRootUserClient"])
                self.assertTrue(rights["com.apple.QuartzCore.global-capture"])
                self.assertIn(str(base / "screen_render_probe.o"), link)
                self.assertIn(str(base / "QuartzCore.tbd"), link)
                self.assertNotIn("platform-application", rights)
                if framebuffer:
                    self.assertTrue(rights["com.apple.backboardd"])
                else:
                    self.assertNotIn("com.apple.backboardd", rights)
            else:
                self.assertEqual(rights, {"com.apple.private.usbdevice.setdescription": True})
                self.assertNotIn(str(base / "screen_render_probe.o"), link)
                self.assertNotIn(str(base / "QuartzCore.tbd"), link)


if __name__ == "__main__":
    unittest.main()

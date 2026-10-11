import hashlib
import io
from pathlib import Path
import plistlib
import shutil
import tempfile
import unittest
from unittest.mock import patch

from wireless.vphone_lab import bootstrap


class BootstrapIntegrityTests(unittest.TestCase):
    def test_bad_release_never_replaces_existing_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "release.zip"
            target.write_bytes(b"existing")
            with self.assertRaisesRegex(ValueError, "mismatch"), \
                    patch.object(bootstrap.urllib.request, "urlopen") as network:
                bootstrap.download("release.zip", "0"*64, "2.9.2", target)
            network.assert_not_called()
            self.assertEqual(target.read_bytes(), b"existing")

    def test_hash_failure_removes_only_owned_partial(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "release.zip"
            with patch.object(bootstrap.urllib.request, "urlopen", return_value=io.BytesIO(b"wrong")):
                with self.assertRaisesRegex(ValueError, "mismatch"):
                    bootstrap.download("release.zip", "0"*64, "2.9.2", target)
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_concurrent_partial_download_is_retained(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "release.zip"
            partial = target.with_suffix(".download")
            partial.write_bytes(b"another invocation")
            with patch.object(bootstrap.urllib.request, "urlopen", return_value=io.BytesIO(b"wrong")):
                with self.assertRaises(FileExistsError):
                    bootstrap.download("release.zip", "0"*64, "2.9.2", target)
            self.assertEqual(partial.read_bytes(), b"another invocation")

    def test_verified_download_becomes_reusable_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "release.zip"
            digest = hashlib.sha256(b"release").hexdigest()
            with patch.object(bootstrap.urllib.request, "urlopen", return_value=io.BytesIO(b"release")) as network:
                self.assertEqual(bootstrap.download("release.zip", digest, "2.9.2", target), target)
                self.assertEqual(bootstrap.download("release.zip", digest, "2.9.2", target), target)
            network.assert_called_once()

    def test_app_extraction_is_temporary_and_existing_binaries_must_match(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            target = base / "Applications/vphone-launchpad.app"
            cli = target / "Contents/MacOS/vphone-launchpad-cli"
            def make_app(app):
                for relative in ("Contents/MacOS/vphone-launchpad", "Contents/MacOS/vphone-launchpad-cli",
                                 "Contents/Library/LaunchServices/com.vphone.launchpad.helper"):
                    path = app / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b"verified executable")
                (app / "Contents/Info.plist").write_bytes(plistlib.dumps({"CFBundleShortVersionString": "2.9.2"}))
            def run(arguments, **kwargs):
                if arguments[:3] == ["ditto", "-x", "-k"]:
                    make_app(Path(arguments[4]) / "vphone-launchpad.app")
                elif arguments[0] == "ditto":
                    shutil.copytree(arguments[1], arguments[2])
            with patch.object(bootstrap, "CLI", cli), patch.object(bootstrap.subprocess, "run", side_effect=run):
                bootstrap.install_launchpad(base / "release.zip", base, "2.9.2")
                bootstrap.install_launchpad(base / "release.zip", base, "2.9.2")
                cli.write_bytes(b"another build using the same version")
                with self.assertRaisesRegex(RuntimeError, "differ"):
                    bootstrap.install_launchpad(base / "release.zip", base, "2.9.2")
            self.assertEqual(cli.read_bytes(), b"another build using the same version")
            self.assertFalse(list(base.glob(".launchpad-extract-*")))


if __name__ == "__main__":
    unittest.main()

import hashlib
import io
from pathlib import Path
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


if __name__ == "__main__":
    unittest.main()

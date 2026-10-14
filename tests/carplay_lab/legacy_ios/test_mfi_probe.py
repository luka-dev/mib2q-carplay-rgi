import importlib.util
import unittest
from unittest import mock

import mfi_probe


@unittest.skipUnless(importlib.util.find_spec("cryptography"), "host cryptography package required")
class MfiProbeTests(unittest.TestCase):
    def test_public_fixture_signs_prehashed_challenge_and_rejects_mutations(self):
        from cryptography.exceptions import InvalidSignature
        from cryptography.hazmat.primitives import hashes
        from cryptography.hazmat.primitives.asymmetric import ec, utils
        key, certificate = mfi_probe.fixture_key()
        self.assertEqual(len(certificate), 607)
        digest = bytes(range(32))
        signature = mfi_probe.sign_digest(key, digest)
        self.assertEqual(len(signature), 64)
        def verify(raw, value):
            der = utils.encode_dss_signature(int.from_bytes(raw[:32], "big"),
                                             int.from_bytes(raw[32:], "big"))
            key.public_key().verify(der, value, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
        verify(signature, digest)
        with self.assertRaises(InvalidSignature):
            verify(bytes((signature[0] ^ 1,)) + signature[1:], digest)
        with self.assertRaises(InvalidSignature):
            verify(signature, bytes((digest[0] ^ 1,)) + digest[1:])

    def test_other_challenge_lengths_rejected(self):
        key, _ = mfi_probe.fixture_key()
        for size in (0, 20, 31, 33, 128):
            with self.subTest(size=size), self.assertRaises(ValueError):
                mfi_probe.sign_digest(key, bytes(size))

    def test_modified_identity_rejected_before_signing(self):
        with mock.patch.object(mfi_probe.Path, "read_bytes", return_value=b"different identity"):
            with self.assertRaisesRegex(ValueError, "integrity"):
                mfi_probe.fixture_key()

    def test_certificate_negative_control_preserves_der_and_public_key(self):
        from cryptography.hazmat.primitives.serialization import pkcs7
        key, data = mfi_probe.fixture_key()
        original = pkcs7.load_der_pkcs7_certificates(data)[0]
        offset = data.index(original.signature) + len(original.signature) - 1
        changed = bytearray(data)
        changed[offset] ^= 1
        damaged = pkcs7.load_der_pkcs7_certificates(bytes(changed))[0]
        self.assertEqual(damaged.public_key().public_numbers(), key.public_key().public_numbers())
        self.assertNotEqual(damaged.signature, original.signature)


if __name__ == "__main__":
    unittest.main()

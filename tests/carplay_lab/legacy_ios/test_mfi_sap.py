import hashlib
import struct
import unittest
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils, x25519
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from mfi_probe import fixture_key
from mfi_sap import respond


class MfiSapTests(unittest.TestCase):
    def test_peer_independently_verifies_live_response(self):
        key, certificate = fixture_key()
        peer = x25519.X25519PrivateKey.generate()
        public = peer.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
        request = b'\x01' + public
        response = respond(request, key, certificate)
        self.assertEqual(len(response), 711)
        self.assertEqual(struct.unpack_from('>I', response, 32)[0], len(certificate))
        self.assertEqual(response[36:36 + len(certificate)], certificate)
        offset = 36 + len(certificate)
        self.assertEqual(struct.unpack_from('>I', response, offset)[0], 64)
        shared = peer.exchange(x25519.X25519PublicKey.from_public_bytes(response[:32]))
        decrypt = Cipher(algorithms.AES(hashlib.sha1(b'AES-KEY' + shared).digest()[:16]),
                         modes.CTR(hashlib.sha1(b'AES-IV' + shared).digest()[:16])).decryptor()
        raw = decrypt.update(response[offset + 4:]) + decrypt.finalize()
        der = utils.encode_dss_signature(int.from_bytes(raw[:32], 'big'), int.from_bytes(raw[32:], 'big'))
        # Verify the original public-key concatenation, independently hashing it.
        key.public_key().verify(der, response[:32] + public, ec.ECDSA(hashes.SHA256()))
        from cryptography.exceptions import InvalidSignature
        with self.assertRaises(InvalidSignature):
            key.public_key().verify(der, response[:32] + bytes(32), ec.ECDSA(hashes.SHA256()))
        self.assertNotEqual(respond(request, key, certificate)[:32], response[:32])

    def test_invalid_version_length_and_low_order_key(self):
        key, certificate = fixture_key()
        for request in (b'', bytes(33), b'\x01' + bytes(31), b'\x01' + bytes(33), b'\x01' + bytes(32)):
            with self.subTest(size=len(request)), self.assertRaises(ValueError):
                respond(request, key, certificate)

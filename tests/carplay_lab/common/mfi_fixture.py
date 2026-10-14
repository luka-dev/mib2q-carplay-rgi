"""Integrity-checked public MFi fixture; independent of the iOS VM backend."""
import hashlib
from pathlib import Path
CERTIFICATE_SHA256 = "634a93dd6c4338080524025411752597ecb828591839cd062a8292fd7e9e844e"
ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/carplay_lab/fixtures/diplay_mfi"
KEY_SHA256 = "bd50eda2d8dd95a8464440f1aebca7068dcab46a2461ecaf826c7f98f5621a75"


def fixture_key():
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.serialization import pkcs7

    private = (FIXTURE / "identity.pk8").read_bytes()
    certificate = (FIXTURE / "certificate.p7b").read_bytes()
    if (hashlib.sha256(private).hexdigest() != KEY_SHA256 or
            hashlib.sha256(certificate).hexdigest() != CERTIFICATE_SHA256):
        raise ValueError("public MFi fixture integrity check failed")
    key = serialization.load_der_private_key(private, password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(key.curve, ec.SECP256R1):
        raise ValueError("expected the public P-256 fixture")
    certificates = pkcs7.load_der_pkcs7_certificates(certificate)
    if len(certificates) != 1 or certificates[0].public_key().public_numbers() != key.public_key().public_numbers():
        raise ValueError("fixture certificate and key do not match")
    return key, certificate


def sign_digest(key, digest: bytes) -> bytes:
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import ec, utils
    if len(digest) != 32:
        raise ValueError("expected a 32-byte prehashed challenge")
    signature = key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    key.public_key().verify(signature, digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    r, s = utils.decode_dss_signature(signature)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")

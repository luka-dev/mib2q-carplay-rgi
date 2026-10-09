# DiPlay public-release MFi identity fixture

## 📦 Provenance

`identity.pk8` and `certificate.p7b` are copied byte-for-byte from
`assets/offline-mfi/` in the public
[DiPlay 0.2.16 APK](https://github.com/shihabal3amri/DiPlay/releases/tag/v0.2.16).
Release commit: `bac419695ca26b7e67d204a279333ca52132c2cb`.
The APK SHA-256 was checked against the digest published in GitHub's release
asset metadata before extraction. The [manifest](manifest.json) records the
download URL, asset ID, archive/member lengths and SHA-256 hashes.

The upstream [authentication-data notice](https://github.com/shihabal3amri/DiPlay/blob/bac419695ca26b7e67d204a279333ca52132c2cb/docs/THIRD_PARTY_NOTICES.md)
attributes this identity to public Carlinkit C2Air Allwinner V821 firmware.
It states that the data are not relicensed as DiPlay source code. This copy
preserves that attribution and does not assign a new license to the identity.
The identity is distributed in the APK; it is excluded from upstream's source
archive. Its presence in a public release is the reason for versioning this
specific pair here.

## 🔐 Contents and validation

| File | Format | Bytes |
|---|---|---|
| `identity.pk8` | DER PKCS#8 private key, EC P-256 | 67 |
| `certificate.p7b` | DER PKCS#7 containing one EC P-256 accessory certificate | 607 |

Local validation confirmed that the certificate public key matches the private
key, ECDSA signs and verifies a prehashed 32-byte SHA-256 digest, and DER/raw
64-byte signature conversion preserves the signature. Changed digests and
changed signatures failed verification. These checks establish key consistency;
they do not establish iPhone acceptance in this project.

## 🧪 Integration status

This directory stores an experimental fixture. The host `TestSoftwareMfi`
provider still generates its own marked synthetic identity; neither that
provider nor the default QNX daemon loads these files. Existing QEMU tests still
use generated, challenge-bound vectors. See
[software MFi test documentation](../../../docs/wireless-carplay/TEST_SOFTWARE_MFI.md).

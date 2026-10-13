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
changed signatures failed verification.

On 2026-10-10 the [iOS VM laboratory](../../../docs/wireless-carplay/IOS_VM_LAB.md)
completed native iAP2 authentication on emulated iOS `14.0 / 18A5351d`: the phone
issued a fresh 32-byte challenge and accepted the host's 64-byte response with
`AA05`. A changed response produced `AA04`; changing a byte of the accessory
certificate's signature produced `AA04` before any challenge. Positive and
negative response checks passed again after restarting the guest. The separate
live MFi-SAP responder subsequently passed native AirPlay authentication and
established a QNX CarPlay control session. Physical iPhone acceptance and decoded
CarPlay media on physical devices remain unverified.

On 2026-10-13 the [vphone lab](../../../docs/wireless-carplay/VPHONE_LAB.md)
repeated native iAP2 authentication on iOS `26.6.2 / 23G90` through a
CoreAccessories API test connection. The valid fixture received `AA05`;
changed response and certificate signatures received `AA04`, with certificate
rejection before the challenge. Native identification received `1D02` and the
guest announced CarPlay availability. A separate primary Direct NCM run then
completed QNX pairing and MFi-SAP, main-screen and stream-111 SETUP, and independent
FFmpeg decoding: 31 primary frames at 800×480 show the native CarPlay app launcher;
22 cluster frames at 1440×542 are black. The manifest records this session
separately from the API authentication probe. USB-port forwarding and physical
iPhone operation remain unverified.

A later run on the same date independently decoded actual Maps tiles at
1440×542 from stream 111 using a bounded RAM GOP capture. A native virtual HID
tap on GO changed the cluster's blue location dot to a navigation arrow.
The `vphone_native_qnx_maps` manifest entry records the distinct capture hashes
and the official guest location simulation used for this test. Longer wire
pacing now has measured evidence, including long gaps; stable 30 fps remains
unverified. A repeated 40-second simulated-motion test visibly moved both Maps
screens. Its pacing report compares sender presentation targets with TCP delivery;
it does not measure encoder completion or physical display timing. Explicit native
IDR requests now refresh capped RAM captures, with a UUID-targeted cluster command.
Turn-by-turn navigation remains unverified. See the
[vphone runbook](../../../docs/wireless-carplay/VPHONE_LAB.md).

## 🧪 Integration status

This directory stores an experimental fixture. The host `TestSoftwareMfi`
provider still generates its own marked synthetic identity; neither that
provider nor the default QNX daemon loads these files. Existing QEMU tests still
use generated, challenge-bound vectors. The opt-in
[host probe](../../ios_vm_lab/mfi_probe.py) loads this pinned fixture for native
iAP2 authentication against the synthetic iPhone. The separate
[MFi-SAP responder](../../ios_vm_lab/mfi_sap.py) supplies live authentication
responses to the explicitly enabled QNX laboratory adapter. Both keep the
private key on the host. The [vphone API peer](../../vphone_lab/mfi_probe.py)
uses the same fixture and signer with bounded, nonce-scoped RPC packet queues;
it also keeps the key on the host. The held iAP2 session and NCM Ethernet bridge are
separate laboratory tools; the default production daemon remains unchanged. See
[software MFi test documentation](../../../docs/wireless-carplay/TEST_SOFTWARE_MFI.md).

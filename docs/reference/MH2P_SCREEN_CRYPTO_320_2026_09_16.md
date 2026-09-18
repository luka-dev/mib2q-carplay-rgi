# MH2P screen crypto: 320.17.1 — 2026-09-16

The MHI2Q `OFF_AESKEY=456` is not portable to this MH2P build. There are two
separate differences: the legacy AES field moved, and a paired screen session
uses ChaCha20-Poly1305 instead of AES-CTR.

## Binary and evidence

- Firmware: `MH2p_US_LB636_P2712`.
- Binary: `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MH2p_US_LB636_P2712/extracted_app_mh2p_lb/eso/lib/libairplay.so`.
- Version string: `320.17.1` at `0xc1e1c`; `AirPlay/320.17.1` at `0xc8014`.
- SHA-256: `f94f1b661be0ff9c1131dbaa2c74c7fe924cd7548e7eb794b5b8b44f15ddc5dc`.
- ELF32 ARM, little-endian, Thumb code, QNX firmware; image base zero. Listed
  code/rodata VAs also equal file offsets. Add the loaded SO base for runtime
  code addresses; account for Thumb state when installing hooks.
- Evidence: `output/r2/mh2p-screen-crypto-20260916/`.

Disassembly was produced with radare2. Its automatic PLT symbol labels were
incorrect for some calls in this ELF. Branch destinations were independently
decoded with Capstone, and all 1450 PLT entries mapped to their GOT slots and
actual `.rel.plt` / `.dynsym` symbols with pyelftools. `ELF-verified` annotations
and `resolved-plt.json` record the corrected mappings. Inferred r2 C prototypes
are not evidence for function argument widths.

## Session fields and algorithm selection

| Fact | Binary evidence |
| --- | --- |
| Main session pointer | `AirPlayReceiverSessionSetup @ 0x370f0`, `mov sl,r0 @ 0x37102`. |
| Legacy AES session key is 16 bytes at `session+0x1f8` (504) | `AirPlayReceiverSessionSetSecurityInfo @ 0x35894` copies the key to `+0x1f8..+0x204`, and IV to `+0x208..+0x214`. |
| Screen setup tests `pairVerifySession` at `session+0x3e4` (996) | Load/test at `0x37382..0x3738a`. |
| Paired branch derives a 32-byte output key | Calls helper `0x34d44` at `0x373a4`, then `AirPlayReceiverSessionScreen_SetChaChaSecurityInfo` at `0x373b4`. |
| Unpaired branch uses legacy AES | `0x3858a` forms `session+0x1f8`; `0x38594` calls `AirPlay_DeriveAESKeySHA512ForScreen`, then `0x385a0` calls screen `SetSecurityInfo`. |

These offsets are verified for this fingerprint, not every library called
“320”. A zero legacy key does not establish failure if the paired branch is in
use. The friend's exact binary and active runtime branch have not been observed.

## Paired screen key

```text
IKM  = the active pair-verify session's 32-byte shared secret
salt = ASCII("DataStream-Salt" + unsigned_decimal(streamConnectionID))
info = ASCII("DataStream-Output-Encryption-Key")
key  = HKDF-SHA512(IKM, salt, info, output_length=32)
```

No trailing NUL participates in the salt or info. The salt uses the screen
stream's `streamConnectionID`, not a random data-channel seed or the session ID.
For altScreen 111 use 111's own connection ID; the stock 110 key cannot simply
be reused. The label is **Output** even though the HU is receiving the picture.

Evidence:

- Helper `0x34d44` uses format `%s%llu` at `0xc49e8`, prefix at `0xc49f0`.
- The output branch calls `PairingSessionDeriveKey` at `0x34d9e`, using the
  32-byte info string at `0xc4a00` and a 32-byte output length.
- The separate input branch uses the 31-byte label at `0xc4a24`; its call is
  `0x34dc4`. Screen setup requests the output branch only.
- `PairingSessionDeriveKey @ 0x81528` validates pairing state, uses the 32-byte
  shared secret at pairing-object `+0x156`, and calls `HKDF_SHA512_compat`.

Prefer the exported `PairingSessionDeriveKey(pairVerifySession, salt, saltLen,
info, infoLen, 32, key)` over manually extracting the secret. The internal helper
has a compiler-specialized signature; do not call it using the reference-source
signature. `Control-Read-Encryption-Key` belongs to another derivation and is
not selected by this screen setup.

## Screen AEAD framing

For an encrypted video-data record (opcode 0):

```text
AAD        = the original complete 128-byte screen header
ciphertext = body[0 : bodySize-16]
tag        = body[bodySize-16 : bodySize]
nonce      = per-stream 8-byte little-endian counter, initially zero
algorithm  = chacha20_poly1305_init_64x64 + decrypt + verify
```

`bodySize` in AAD still includes the tag. Do not subtract the tag from the
header until after authentication. After a successfully verified video record,
increment the nonce once; do not increment it for plaintext video-config records
or individual TCP reads. Starting capture mid-stream does not imply nonce zero.

Binary evidence in `AirPlayReceiverSessionScreen_ProcessFrames @ 0x3bf70`:

- `0x3c630` tests screen-object ChaCha validity at `+0x1d4`.
- `0x3cd64`: `chacha20_poly1305_init_64x64`, key at screen `+0x2c4`, nonce at
  `+0x2e4`.
- `0x3cd6e`: `chacha20_poly1305_add_aad` with length `0x80`.
- `0x3cd7c`: decrypts `bodySize-0x10` bytes.
- `0x3cd96`: verifies the trailing tag; failure exits the processing path.
- `0x3cdae..0x3cdd8`: updates body size and increments the eight-byte nonce
  little-endian through screen `+0x2eb`.
- `AirPlayReceiverSessionScreen_SetChaChaSecurityInfo @ 0x3b74c` zeros the
  cryptor (including nonce), copies exactly 32 key bytes and sets valid.

Use the stock 64x64 API or an explicitly compatible implementation; a generic
ChaCha API may expect different nonce/counter framing. After tag verification,
the video plaintext is parsed as AVCC length-prefixed NAL units. A single H.264
`forbidden_zero_bit` failure is a downstream symptom, not a way to identify the
correct key derivation.

## Practical confirmation

Observe which stock setter is called for stream 110:
`AirPlayReceiverSessionScreen_SetChaChaSecurityInfo` versus `SetSecurityInfo`.
Then reproduce that algorithm for the separate 111 connection ID. Validate the
first opcode-0 record's authentication tag before interpreting NAL bytes.

This is a static finding corroborated by local reference source under
`Refferences/WirelessCarPlay/source/`. No friend's session key/capture was
available, no live decrypt was performed, and no production hook was modified.

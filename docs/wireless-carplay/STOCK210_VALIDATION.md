# Stock 210 + hook/wireless: offline validation, 2026-09-29

_MHI2Q wireless CarPlay — technical reference, integrated 2026-09-29._

---

[Wireless overview](README.md) · [Current stock210 lifecycle](STOCK210_RECEIVER_LIFECYCLE.md)

Maintained technical reference. The evidence below distinguishes static analysis, local QNX tests and observations from the unit.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../wireless/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📊 Component flow

```mermaid
flowchart LR
    accTitle: Offline validation evidence boundaries
    accDescr: Host tests and QNX fixtures support the offline implementation, while real radio, MFi, media rendering and USB remain a separate validation stage.
    host["Host protocol and ownership tests"]
    qnx["QNX builds and ABI guards"]
    pairing["Independent pairing and crypto"]
    lifecycle["Three owner generations"]
    hardware["Real hardware checks pending"]
    host --> qnx --> pairing --> lifecycle
    lifecycle -.-> hardware
```

The pairing implementation is part of `libcarplay_hook.so`. The tested
`dio_manager` process loads the original MU1316 210.81 `libairplay.so`, not a
280 receiver. Firmware images were read only; no HU connection or deployment.

Current milestone: [receiver lifecycle](STOCK210_RECEIVER_LIFECYCLE.md) adds
stable advertised identity, supported capabilities, production stock210 bridge,
supervisor restart and wired priority. Its combined QEMU test uses three DIO
processes and two media sessions. Current hashes are in that report/package;
the milestone tables below record earlier builds, not the current artifact.

<a id="initial-pairing-milestone"></a>

## 🔐 Initial pairing milestone

- Independent client SRP proof (Setup M1–M4), mutual Ed25519 identities
  (Setup M1–M6), Verify M1–M4, then encrypted stock `/info` HTTP 200.
- Plaintext Verify M4 is parsed before the first encrypted response.
- One HTTP header split across three encrypted records; TCP deliveries split
  record length and the remainder. Nonces continue across records.
- Two requests in one decrypted record. The first has a deliberately invalid
  plist body and gets stock 400; the following clean `/info` gets 200. This
  exercises body consumption, Reset's extra bytes and response nonce order.
- Invalid ChaCha/Poly1305 tag closes the connection, without plaintext fallback.
- Invalid Verify signature produces authentication failure. A subsequent valid
  connection verifies with the saved peer and starts with fresh record nonces.
- QNX4 filesystem: persistent identity survives store close/reopen, another
  process cannot acquire the owner lock, insecure permissions/corruption/
  directory symlink are rejected. A corrupt record is not silently replaced.
- The same control regression passes with `HOOK_AUDIO=0` and with the full
  audio-enabled hook. The recorded hashes below predate the media extension;
  build/log paths now contain the current milestone recorded later in this file.
- Hook ABI checks: 30 exact dynamic exports for wireless/full audio; ordinary
  wired build has its expected 26. No emutls, C++ runtime, exception/RTTI or
  unresolved private pairing/XPC symbols. APAC remains an optional dlopen.
- 23 daemon host tests, including active receiver identity binding, identifier
  mismatch, truncated/malformed/fragmented IPC replies and STOP after missing
  identity. QNX daemon/phone-probe cross-build passed (existing linker warnings
  about Rust EABI attributes remain).
- Receiver bridge QNX cross-build and host TXT/IPC parser tests passed. The
  parser rejects duplicate `pi` keys (including case variants), malformed
  lengths/control bytes and clears stale identity after an invalid update.

<a id="evidence"></a>

## 🧪 Evidence

Reproduction commands are in [hook/wireless/README.md](../../hook/wireless/README.md).
The log is [qemu_stock210_hook_pairing.log](../../wireless/receiver_bridge/out/qemu_stock210_hook_pairing.log).
The fixture uses a disposable QNX4 RAM disk, synthetic random/peripheral
devices and a loopback network. The independent client implements the SRP,
HKDF, TLV and HTTP protocol; raw Ed/Curve/ChaCha primitives are shared SDK code
loaded in its own process. This is not an independent cryptographic primitive
audit or an iPhone/real-MFi-chip test.

| Artifact | SHA-256 |
|---|---|
| `build/wireless/libcarplay_hook.so`, 1,515,883 bytes | `590d0adb88b4ea9f6f5098664b3cbcf4bad72d46df335b202543069783c61f55` |
| `build/wireless/libcarplay_apac.so` | `1c25799f4dd47549335cbc22cc9a059efab030d4e489937fc4d8460e341e09ce` |
| `receiver_bridge/out/libdio_receiver_bridge.offline.so` | `47551272b10e5efa84ca1a35cc7bc6f6ab587d859c622d3055d825113d006527` |
| QEMU log above | `0cece54c928b5bdc6347c353ac72cfd9b600a68768ae6f7fa9c15ff8637e6599` |
| QNX `mhi2q-wireless-daemon` | `3d30a5bf9515e017b3a1f8da08aced9aa30fb43d9cad9659736693be3551d8a1` |

Build logs: `build/wireless-full-build.log`, `build/wired-regression-build.log`,
`build/wireless-rust-tests.log`, `build/wireless-rust-qnx.log`,
`build/wireless-bridge-build.log`. Private SDK source hashes and symbol map:
`build/wireless-sdk/sources.json`, `build/wireless-sdk/namespace.map`.

Two failures found and fixed during this work: peer/temp filenames exceeded
QNX4's 48-byte limit; ELF assets appended after the large plugin tail of this
QEMU IFS failed to load. The fixture now puts executable assets near the boot
programs. Neither fix modifies the original receiver.

<a id="events-and-media-milestone"></a>

## 📋 Events and media milestone

The full hook now includes private events, screen, PCM and microphone adapters
around stock 210, gated by `MHI2Q_CP_WIRELESS_MEDIA=1`. Verified offsets and
lifetime rules are in [stock210_media_crypto_abi.md](STOCK210_MEDIA_CRYPTO_ABI.md).
The media fixture passed with the full audio-enabled hook:

- Pair Setup/Verify, bad-signature rejection, and reconnect using the stored peer.
- Paired encrypted SETUP containing `et=32` succeeds after normalizing the obsolete
  legacy AES fields; stock Setup/Start remain the actual receiver implementation.
- Real stock HTTPClient sends encrypted events; its completion callback sees 0
  for a valid encrypted response and -6754 for a corrupted response tag.
- One authenticated AVC frame reaches stock OMX input (`nFilledLen=8`, flags 0x10).
  The next corrupted screen tag returns -6754 before OMX; only one frame is delivered.
- Exactly 80 authenticated PCM packets reach the stock jitter buffer with expected
  little-endian samples. Packets with altered tag and timestamp AAD are rejected.
- Stock WriteAudio packetizes 1440 PCM bytes. The adapter emits encrypted UDP
  packets 1452/60 bytes; the independent client verifies keys, byte order, nonce,
  RTP sequence continuity, sample timestamp adjustment, and the whole payload.
- Encrypted TEARDOWN returns 200. Stock stops its workers/server and releases DIO.

The test uses fake HMI/audio edges and an **input-only** fake OMX, with corrected
UseBuffer ownership. It does not decode/render video, exercise real QSA audio,
validate decoder drain with pending output frames, or talk to an iPhone/MFi chip.
Stored-peer reconnect is tested before the media session; stock DIO exits after
full media teardown. The later combined lifecycle fixture covers supervisor
restart/readiness and the next media session; see the current report above.

Reproduce by adding `--wireless-media-qemu` to the control QEMU command above.
Log: [qemu_stock210_hook_media.log](../../wireless/receiver_bridge/out/qemu_stock210_hook_media.log).
That media milestone build had 36 dynamic exports, no private undefined symbols/emutls,
and passes the build ABI checks. New C adapters also pass `-Wall -Wextra -Werror`.

| Media milestone artifact (historical) | Bytes | SHA-256 |
|---|---:|---|
| `build/wireless/libcarplay_hook.so` | 1,528,599 | `8fdfbeb3146d1d7ff5fa4a12cb0b9928d140e20a3192340489b6042211e21383` |
| `build/wireless/libcarplay_apac.so` | 696,491 | `1c25799f4dd47549335cbc22cc9a059efab030d4e489937fc4d8460e341e09ce` |
| Media log above | 171,154 | `2f825477efc098445d9be8f643fe55ccde3875e2fdb81a83ad36e59b9645fa10` |
| Current control log | 124,459 | `146643f3aafea67d87c639fe77bfdd721e4dd9ad013b0d046518b200c703b6d4` |

The current media build also passes the separate pairing/control regression
(with media disabled) and the ordinary wired rebuild: 26 exact exports and its
existing ABI checks. Logs: `build/wired-media-regression-build.log` and the
control QEMU log linked above. These are build/protocol regressions, not a
physical wired-session or USB handoff test.

Build log: `build/wireless-media-build.log`. Media fixture code is not linked
into the hook; original firmware binaries remain unchanged.

<a id="remaining-work"></a>

## 🎯 Remaining work

Media stays disabled by default (encrypted SETUP returns 461 without its flag).
No wireless feature bits or Bonjour `pi` are added. The bridge still requires
hook ID == published `pi`, and cannot acknowledge complete stock 210 readiness.
The old replacement 280 deployment bundle must not install this candidate.

Next: integrate capability/`pi` publication, readiness and stock DIO exit/restart
with receiver bridge and owner supervisor; verify a second complete media
session and wired SI priority. Selective stream replacement, unsupported-format
negotiation and real output drain need further tests. Real-chip Method 1 Setup,
physical iPhone interoperability, persistent mount ownership and power-loss
store durability remain untested. No HU deployment was performed.

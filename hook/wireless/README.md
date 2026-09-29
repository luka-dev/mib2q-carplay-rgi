# Wireless module inside libcarplay_hook

Offline candidate for the original MU1316 AirPlay 210.81. The module is C,
uses the existing hook framework, and links a private archive built from the
owner-provided 280.33.8 SDK. It is **not** a replacement `libairplay.so` or a
separate pairing daemon. The Bluetooth/iAP2 bridge remains in the Rust daemon.

Implemented here:

- Pair Setup and Pair Verify server state machines, with identity/peer store.
- Plaintext Verify M4 completion followed by encrypted control HTTP on the
  same stock socket, with the stock message parser and response writer.
- Original MFi signature/certificate exports as the real chip adapter.
- Same-process pairing ID export for receiver bridge; ID is separate from pk.
- Pinned 210 ABI guard, at most eight connections, bounded pairing bodies,
  private SDK symbols, and no private CF object crossing into stock 210.

Events and media adapters are implemented behind the additional development
flag `MHI2Q_CP_WIRELESS_MEDIA=1`: events, screen 110 and 16-bit PCM 100/101,
including microphone encryption. Without this flag, encrypted SETUP returns 461.
Bonjour and `/info` now publish the persisted `pi` and supported capabilities.
The stock210 bridge acknowledges readiness only after the hook, advertisement
and listener agree; iAP2 StartSession uses that same identity.
This build must not be installed by the older 280 replacement bundle.

## Offline build and test

```sh
bash scripts/build_hook_wireless.sh
python3 wireless/receiver_bridge/qemu_capture.py --wireless-hook-qemu \
  --mesh --start-experiment --seed-display --skip-hid-qemu \
  --mdnsd --mock-mac-qemu --interface-lo0-qemu
# Add --wireless-media-qemu to the QEMU command for media validation.
# Add both --wireless-media-qemu --wireless-lifecycle-qemu for three owner
# generations, real daemon StartSession, media reconnect and wired priority.
python3 wireless/integration/build_stock210_review.py
cargo test --manifest-path wireless/daemon/Cargo.toml
```

The dedicated build writes `build/wireless/libcarplay_hook.so`; the ordinary
wired build keeps `HOOK_WIRELESS=0`. `HOOK_AUDIO=0` can isolate control testing.
The SDK builder reads `Refferences/Carplay` in place, records source hashes,
and renames SDK globals `cp280_*`. All those symbols remain hidden.
The build requires the local `qnx65-armv7-toolchain:latest` Docker image.

Runtime configuration for the offline candidate:

```text
MHI2Q_DIO_OWNER_MODE=wireless
MHI2Q_CP_WIRELESS_PAIRING=1
MHI2Q_CP_WIRELESS_MEDIA=1
MHI2Q_CP_PAIRING_DIR=/mnt/persist/mhi2q-carplay/pairing
```

Default pairing parent is created with mode 0700. A configured directory must
be absolute and private to the process UID. `.owner` uses a process lock;
records use 0600, checked fixed lengths/magic, fsync and same-directory rename.
Peer filenames contain 160 SHA-512 bits and fit QNX4's 48-byte name limit;
the record also stores and verifies the complete identifier. Production keys
must live on persistent storage, never `/tmp`. The QEMU fixture deliberately
uses a disposable QNX4 RAM disk and synthetic random/peripheral devices.

The QEMU client independently implements SRP, HKDF, TLV and HTTP exchange.
Only raw Ed25519/Curve25519/ChaCha primitives come from a separate test helper
in the **client process**. The dio process uses the exact stock 210 receiver.
This is not a physical iPhone or real MFi-chip interoperability test.

2026-09-29 validation passed with both `HOOK_AUDIO=0` and the full audio build:
Setup M1–M6, Verify M1–M4, fragmented encrypted `/info`, pipelined requests,
bad signature/tag rejection and reconnect using the stored peer. QNX4 store
tests cover reopen, process lock, permissions, corruption and directory symlink.
The ordinary wired build passes its existing ABI/export checks. Daemon host
tests (23), QNX daemon cross-build, bridge cross-build and identity protocol
tests pass. Evidence and exact artifact hashes:
[pairing validation](../../wireless/stock210_pairing_validation.md).

Verified structure offsets and commit timing:
[stock210_http_pairing_abi.md](../../wireless/stock210_http_pairing_abi.md).
Protocol recipe and remaining media keys:
[airplay280_pairing_recipe.md](../../wireless/airplay280_pairing_recipe.md).

Stock media offsets, key direction, lifecycle and supported-format limits:
[stock210_media_crypto_abi.md](../../wireless/stock210_media_crypto_abi.md).
The media fixture replaces HMI/audio hardware edges and fixes ownership in
its fake OMX UseBuffer implementation; it retains stock crypto consumers,
jitter buffer, microphone packetizer and screen processing. Fake OMX consumes
AVC input without decoding/displaying it.

The opt-in media fixture now passes events request/response and bad-tag
rejection, AVC plaintext delivery to stock OMX input, exactly 80 valid PCM
packets into the stock jitter buffer, and microphone encryption/fragmentation.
Full TEARDOWN returns 200 and stock DIO exits cleanly. The combined lifecycle
fixture also passes media restart, active-session drain for a wired owner,
and wireless resume, retaining the same pairing identity in three DIO processes.
The wired child and MFi/Wi-Fi hardware are synthetic in this test. Pairing-only
regression and the ordinary wired build also pass. Current implementation,
review package and remaining hardware checks:
[receiver lifecycle](../../wireless/stock210_receiver_lifecycle.md).

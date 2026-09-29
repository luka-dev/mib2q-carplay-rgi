# Wireless receiver identity and pairing gap (MU1316, offline)

**Static follow-up, 2026-09-28:** the owner's `Refferences/Carplay` source is
280.33.8. [The pairing recipe](../airplay280_pairing_recipe.md) now traces
Setup/Verify, corroborates `3939` in the iOS no-PIN branch, and distinguishes
StartSession field 4 / Bonjour `pi` (pairing identifier) from `pk` (public
key). The daemon's current 64-hex validation conflates these values and still
needs correction. Runtime results below belong to their recorded artifacts;
this follow-up did not rerun them or establish real MFi/iPhone compatibility.

The stock MU1316 `libairplay.so` and `dio_manager` were launched together in
QNX 6.5 ARM QEMU. A QEMU-only interposer copied the complete TXT bytes passed
to `DNSServiceRegister` for `_airplay._tcp.` without modifying them. The same
run read the stock receiver's HTTP `/info` response. Reproduce with:

```sh
python3 wireless/receiver_bridge/qemu_capture.py --mesh \
  --start-experiment --seed-display --skip-hid-qemu --feature37 \
  --mdnsd --mock-mac-qemu --interface-lo0-qemu --dump-txt-qemu
```

The captured TXT had `deviceid=02:00:00:00:00:01`,
`srcvers=210.81`, `features=0x44440B80,0x21`, `flags=0x4`, and
`model=AirPlayGeneric1,1`. `/info` returned the same `deviceID` and
`sourceVersion`. Neither surface had `pi` or `pk`. The MAC and interface in
this run are QEMU substitutions; they are not values for a head unit.
Evidence: `out/qemu_start_iface_lo0.log`. This verifies the in-process
registration arguments, not successful multicast delivery.

The local MFi Accessory Interface Specification R29, sections 30.3.1.2 and
50.5, describes the AirPlay Bonjour `deviceid` as the accessory's stable
identifier and the Device IE ID as the same identifier used for IP discovery.
It marks Bonjour `pi` as required for wireless CarPlay. Section 30.3.2.1
describes `/pair-setup` and `/pair-verify` before encrypted CarPlay control.
The pinned stock 210.81 library has no such endpoints according to the
separate static comparison in
`../../../dio_asm_to_c/_legacy_attempt/libairplay-mhi2q/FUNCTIONAL_PARITY.md`
(Q1/Q2). The QEMU `/info` feature mask also lacks its pairing/encryption bit.
Therefore the current one-byte feature-37 patch only advertises CarPlay
Control; it does not establish that a modern wireless phone can pair and
start media. The synthetic phone fixture does not test pairing.

The daemon now uses `MHI2Q_CP_DEVICE_ID` for the Apple Device IE,
`CarPlayStartSession.deviceIdentifier`, and the
`AirPlay-Receiver-Device-ID` header. The QEMU configuration sets it to the
captured synthetic Bonjour ID and the phone fixture checks the resulting
iAP2 and HTTP values. A real unit's value must be obtained from its own
stock Bonjour or `/info`; it must not be inferred from the AP interface MAC.

Next offline work was to evaluate the existing from-source AirPlay 280.33.8
candidate as a replacement for the stock feature-bit-only patch. Its source
contains the pairing implementation, but the existing build intentionally
disables its publication and routes to retain stock 210 behavior. No head-unit
file was changed.
The local `libairplay.dev.so` candidate is ARM ELF32 (SHA-256
`7c548f50765d8d54ca06e1665b52902a8086e3d888423dc83320cdffc02d6b7e`);
its string table contains `/pair-setup`, `/pair-verify`, and `280.33.8`, and
it exports `AirPlayCopyHomeKitPairingIdentity`. These are static indicators,
not a completed pairing test.

An additional QEMU-only run staged that exact candidate as `/proc/boot/libairplay.so`
with the pinned MU1316 `dio_manager` (`--airplay280-qemu --dump-txt-qemu` and the
same mesh/start/display/mdns/mac/interface options above). It loaded, but the
manager terminated with SIGSEGV at `libairplay.so@CFLDictionaryGetValue+0x3c`
immediately after `StartAirPlay` was enqueued. There was no `/info` response or
Bonjour TXT to compare; see `out/qemu_start_airplay280.log`. The candidate is
the **on-unit** build, whose own build notes say QEMU requires the separate
null-audio/software-MFi variant. The crash establishes that this exact artifact
cannot pass this QEMU startup probe. It does not establish a hardware failure
or disprove the source receiver's pairing implementation.

## AirPlay 280 pairing candidate, QEMU (2026-09-28)

`build_airplay280_qemu.sh` now stages a separate source tree from the local
legacy receiver and builds it with null audio and software MFi; it never uses
the legacy SSH build script. The staged overlay in `stage_airplay280_qemu.py`
adapts `ScreenCreate` to the **one-argument MU1316 ABI**. QEMU traces established
the mismatch: the original 280 function expected `(outScreen, properties)`;
`dio_manager` passed only `outScreen`, leaving `r1=0x002db128`, which crashed
in `CFLDictionaryGetValue`. A QEMU interposer that supplied `NULL` for the
second argument progressed past the crash; then the rebuilt library with an
actual one-argument export progressed without that interposer. Internal 280
calls retain property dictionaries through `ScreenCreateWithProperties`.

The staged source also removes the legacy 210 compatibility clamps on AirPlay
version, CarPlay Control and pairing feature bits, HomeKit identity in Bonjour
and `/info`, and `/pair-setup`/`/pair-verify` routes. It sets both pairing
response MIME types to `application/pairing+tlv8`, as required by local MFi
Accessory Interface Specification R29 §30.3.2.1. The unmodified legacy source
used binary-plist or generic-binary MIME despite returning TLV8 data.

Build command: `sh wireless/receiver_bridge/build_airplay280_qemu.sh`.
The QEMU build directs its KeychainLite files to `/dev/shmem/airplay_keychains`;
the default `~/Library/Keychains` failed with ENOENT in the QEMU image at M6.
This setting is exclusive to the emulator variant and is not suitable as
persistent storage on a real unit. The on-unit build uses
`/mnt/persist/mhi2q-carplay/keychains`; KeychainLite creates its parent with
mode `0700` and the staged source opens the keychain file with mode `0600`.
The actual writable mount and ownership remain unverified. QEMU candidate SHA-256:
`19ded2634146a3d06cbc1988589bb5ca85f929e60a34b4e896fbe8ad3aa108a8`.
The QEMU run with this candidate and pinned MU1316 `dio_manager` passed:

```sh
python3 wireless/receiver_bridge/qemu_capture.py --mesh \
  --start-experiment --seed-display --skip-hid-qemu --mdnsd \
  --mock-mac-qemu --interface-lo0-qemu --airplay280-qemu \
  --airplay280-image wireless/receiver_bridge/out/airplay280_qemu/libairplay.qemu.so \
  --dump-txt-qemu
```

The receiver advertised `_airplay._tcp` on port 7000. Bonjour and `/info`
agreed on synthetic `deviceid=02:00:00:00:00:01`, `srcvers=280.33.8`, and
feature mask `0x0000006144440B80` (CarPlay Control bit 37 and pairing bit 38).
Bonjour contained `pi`; `pk` remained absent. A QEMU-only client in
`airplay_pairing_probe.c` used the 280 SDK's pairing client against the
receiver's HTTP server on a persistent TCP connection. It sent `/pair-setup`
M1, M3 and M5 with `X-Apple-HKP: 0`; the server returned M2, M4 and M6. M2 was
HTTP 200 with `application/pairing+tlv8`, state 2, 16-byte salt and 384-byte
SRP public key. Both sides finished pair-setup with `done=1`; the QEMU
keychain was nonempty and had mode `0600`. A new connection
then completed `/pair-verify` M1–M4 with `done=1`. After verification, the
client derived the control keys, sent a ChaCha20-Poly1305 encrypted `GET /info`
frame, decrypted the receiver's answer, and checked HTTP 200 with a complete
HTTP body. The harness requires every success marker; evidence is
`out/qemu_start_airplay280_emulator.log`. A repeat run with the obsolete
dictionary traces disabled passed cleanly.

This proves the pairing and encrypted control path **between two components of
the same 280 SDK** in QEMU. It does not prove independent iPhone
interoperability, media, actual MFi-chip access, or hardware display/audio.
An additional QNX client implements its own HTTP/TLV8/SRP pairing state
machine and completed pair-setup, pair-verify and encrypted `/info` against
the same receiver, using only the receiver's raw Curve25519, Ed25519 and
ChaCha primitives (`independent_pairing_probe.md`).
The staged build uses software MFi and QEMU substitutions. The 280 source
currently uses a fixed pairing code `3939`; its suitability for the phone path
must be verified before any hardware use. The review bundle now includes the
on-unit AirPlay 280 candidate, but remains unapproved for on-unit use.

The production bridge now expects this candidate's port 7000, CarPlay Control
and pairing feature bits 37 and 38, and Bonjour `pi` identity. In QEMU, the
AirPlay 280 receiver and bridge completed two START/STOP generations with
port and readiness checks (`out/qemu_bridge_airplay280.log`). The per-unit
environment must set `MHI2Q_CP_AIRPLAY_PORT=7000`.
The same QEMU image also completed the ordinary Rust daemon's synthetic phone
path through iAP2, mock MFi, `CarPlayStartSession` on port 7000, and receiver
STOP after disconnect (the same log, regenerated with `--daemon-phone-qemu`).
This fixture does not use an independent iPhone AirPlay client.

A second QNX/QEMU process now checks pair-setup M1–M6 with its own HTTP/TLV8
state machine and OpenSSL SRP-6a/HKDF implementation. Its calculated client
proof was accepted, its calculated server proof matched M4, and M5/M6
completed with an independently constructed identity payload and verified
accessory signature. It calls low-level Ed25519 and ChaCha primitives from
`libairplay`, but never the SDK pairing state machine. See
`independent_pairing_probe.md` and `out/qemu_independent_srp_airplay280.log`.
Pair-verify remains a same-SDK test.

## Uninstalled hardware candidate

The same source overlay also builds without QEMU's null-audio, software-MFi,
or RAM-keychain substitutions:

```sh
sh wireless/receiver_bridge/build_airplay280_qemu.sh on_unit
```

Output: `out/airplay280_on_unit/libairplay.on_unit.so`, SHA-256
`1ad8a4f641646754f185e808e48837c2f845af5452f047d62affca4fe85c9050`.
The on-unit source now uses the QSA backend in `audio_qsa.c`. Capture and
playback open in nonblocking mode and wait on the PCM descriptor, allowing
`AudioStreamStop` to join the I/O thread even when capture has no data or
playback cannot accept more data. Partial playback writes are completed in a
loop. This is a compiled hardware candidate; actual PCM behavior and routing
still require testing on the unit.
This build supplies QNX's missing `random()` through `RandomBytes` so the
AirPlay timing thread can run. It also removes an out-of-bounds legacy
`ScreenRegisterDelegates` write against the 280 SDK's smaller `ScreenPrivate`;
the QEMU screen-110 probe now reaches SETUP 200 and clean TEARDOWN/shutdown
in 25 repeat runs (`independent_pairing_probe.md`).
It is an ARM EABI5 QNX shared library with soname `libairplay.so`. Its imports
include QNX `devctlv` and `snd_pcm_*`; its own exports include
`MFiPlatform_Initialize`, `MFiPlatform_CopyCertificate`,
`MFiPlatform_CreateSignature`, and the one-argument `ScreenCreate`. The ten
DT_NEEDED entries match the intended QNX platform libraries. Comparing
`dio_manager` imports against stock `libairplay.so` exports found 58 shared
symbols; the candidate exports all 58. The legacy `symcheck.sh` run against
the full matching MU1316 extraction found only four unresolved symbols, each
`WEAK` (`_ITM_*` and `__{register,deregister}_frame_info`); no strong import
was missing. The first audit against the app image alone produced false
missing imports because system libraries live in its IFS images.

This candidate has **not** been loaded on the head. QEMU tested the
software-MFi/null-audio sibling, not this exact binary. Persistent keychain
access to its `/mnt/persist/mhi2q-carplay/keychains` path, I²C transactions,
QSA/OMX audio routing, actual phone pairing, and `dio_manager`
behavior on hardware remain unverified. It is included in the offline review
bundle and guarded installer candidate, without any on-unit action.

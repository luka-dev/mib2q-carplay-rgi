# MHI2Q wireless CarPlay: current implementation

Status: **2026-09-29, offline mod candidate** for MU1316 / P5087, QNX 6.5 ARM.
The receiver remains the factory `libairplay.so` **210.81**. Pairing and media
security are implemented in the C `hook/wireless` module; the 280 SDK supplies
private crypto/pairing code, not a replacement receiver.

## Components and ownership

| Component | Responsibility |
|---|---|
| [btstack graft](graft/README.md) | iAP2/CarPlay discovery, RFCOMM channel and bounded local byte bridge. |
| [Rust daemon](daemon/README.md) | iAP2, MFi adapter, Wi-Fi credentials/IE, CarPlay Control and receiver START/STOP. |
| [Receiver bridge](receiver_bridge/README.md) | Starts stock DIO receiver, binds readiness and pairing identity to the active generation. Stock210 uses port5000. |
| [C wireless hook](../hook/wireless/README.md) | Pair Setup/Verify, persistent peers, encrypted control/events/screen/PCM/microphone, pi and capability publication. |
| [Owner/supervisor integration](stock210_receiver_lifecycle.md) | One DIO owner; restart after media teardown, wired priority/drain, then wireless resume. |

The persistent pairing identifier is shared by Bonjour, `/info`, signed
PairSetup and iAP2 StartSession. Pairing keys and owner lock/marker files belong
on `/mnt/persist`; `/tmp` is transient RAM. The ordinary wired hook build remains
separate. A wireless artifact activates its wireless module only in wireless
owner mode with the pairing/media flags enabled.

## Verified offline

- Combined QNX/QEMU: three DIO owner generations retain one pi, two media
  sessions, stored-peer Verify after restart, active-session wired drain and
  wireless resume after the synthetic wired child exits.
- Independent phone fixture: pairing/control authentication, invalid
  signature/tag rejection, events, two authenticated AVC frames to stock OMX
  input, 160 valid PCM packets to its jitter buffer, microphone encryption.
- Production daemon QNX synthetic phone/control test and separate pairing
  regression; 23 Rust host tests; native owner/supervisor/QUIESCE tests.
- Wireless build: 41 expected exports. Ordinary wired build: 26. QNX build,
  C warning checks and all eight static graft checks pass.

Exact evidence and hashes: [receiver lifecycle report](stock210_receiver_lifecycle.md).
Earlier milestones: [pairing/media validation](stock210_pairing_validation.md).
QEMU uses synthetic MFi/Wi-Fi/radio-facing input and an input-only OMX; these
results do not establish physical iPhone interoperability or decoded output.

## Build and reproduce

Commands run from the repository root. They require the local
`qnx65-armv7-toolchain:latest` Docker image, pinned firmware inputs under
`Firmwares/HU`, and owner-provided SDK sources in `Refferences/Carplay`.
Firmware and SDK inputs are not added to this repository by this change.

```sh
bash scripts/build_hook_wireless.sh
python3 wireless/receiver_bridge/qemu_capture.py --wireless-hook-qemu \
  --wireless-media-qemu --wireless-lifecycle-qemu --mesh --start-experiment \
  --seed-display --skip-hid-qemu --mdnsd --mock-mac-qemu --interface-lo0-qemu
python3 wireless/daemon/probes/qemu_daemon_bridge.py --phone-probe --control-probe
python3 wireless/integration/build_stock210_review.py
```

Review artifact: `out/mhi2q-stock210-hook-review.tar`, SHA-256
`8780e8bc8050bd70dfed2cbb0702a3d8291ed2846802e120eaa425b7f960436c`.
It contains 16 hashed files plus manifest/README, including validation logs.
It has no installer or replacement libairplay. Existing btstack graft,
AP/firewall and wired-mod files remain integration prerequisites.
Generated binaries, archives, logs and raw analysis output stay local and
are ignored by Git. Rebuilding can change hashes; consult the new manifest.

Historical `build_offline_bundle.py`, `offline_bundle.md` and
`integration/install_wireless_candidate.sh` describe the experimental280
replacement. **Do not use that installer for this stock210 candidate.**

## Next steps and constraints

1. Review deployment integration for the stock210 hook, existing graft,
   per-unit configuration, AP/firewall and wired wrapper dependencies.
2. When hardware work is separately authorized, validate real MFi Method1,
   current iPhone pairing/discovery, radio traffic and persistence on the unit.
3. Validate decoded video, real audio/microphone, disconnect/reconnect and
   physical SI/USB takeover; QEMU's wired child only checks ownership logic.

Current constraint remains **offline only**. No HU connection, writes,
process starts/stops or reboot were performed for this milestone; original
receiver/DIO images match their pinned hashes. The initial RE prompt and
continuation notes are preserved in [CODEX_HANDOFF.md](CODEX_HANDOFF.md).

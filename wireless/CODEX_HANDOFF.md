# Handoff: graft an iAP2-over-Bluetooth service into MHI2Q btstack

Current entry point: [wireless implementation status](README.md), including
components, reproducible checks, review artifact and remaining hardware work.

**2026-09-29 continuation:** the opening scope below is historical. Static iOS
analysis disproved the assumption that everything above Bluetooth was ready:
stock 210 also needs CU pairing and modern control/event/media security.
The requested RFCOMM layout is in `rfcomm_channel_layout.md`. Pairing now lives
inside `hook/wireless`, embedded in `libcarplay_hook.so`, with a private SDK
backend and pinned stock-210 HTTP adapter. Offline QEMU passed Setup, Verify,
encrypted `/info`, fragmentation, reconnect, invalid signature/tag rejection,
and QNX4 store tests. Opt-in event/media adapters now also pass QEMU: events,
AVC to OMX input, PCM jitter input, microphone encryption/splitting, bad-tag
rejection and full TEARDOWN. This uses fake hardware edges and input-only OMX,
not a real iPhone/decoder. The stock210 bridge and daemon profile now integrate
stable `pi`, capabilities and readiness. Combined QEMU passes three DIO owner
generations: media teardown/restart, a second media session drained for wired
priority, and wireless resume with stored-peer Verify. It uses the real native
supervisor/owner helper and Rust daemon, with synthetic MFi/Wi-Fi and wired child.
See [receiver lifecycle](stock210_receiver_lifecycle.md),
`stock210_media_crypto_abi.md`, [hook/wireless](../hook/wireless/README.md) and
`stock210_http_pairing_abi.md`. Do not install the old 280 replacement bundle.
All work remains offline; no head-unit writes/restarts/reboots.
Review bundle: `wireless/out/mhi2q-stock210-hook-review.tar`, built by
`integration/build_stock210_review.py`. No replacement libairplay or installer.
Next: hardware validation of MFi, current iPhone, multicast, actual media and
SI/USB ownership; only after lifting the current offline-only constraint.

You are continuing a reverse-engineering task on an Audi MHI2Q head unit (QNX 6.5, ARM).
Goal: make the head unit host **wireless CarPlay**, which stock MHI2Q does not. Everything
above the Bluetooth layer is available; the single missing piece is an iAP2 SDP record plus
an RFCOMM listener inside the stripped static binary `btstack`. This document is the complete
state so you can continue without re-deriving it.

This is interop RE on the owner's own hardware. Work is read-only on the unit unless a step
explicitly says otherwise; the btstack binary also drives the car's hands-free telephony, so a
crash there means no Bluetooth in the whole car until reboot. Never restart or reboot the unit.

---

## The binaries

| Role | Path | Notes |
|---|---|---|
| Target (ours) | `Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/apps/btstack` | 1,852,334 B, stripped, ARM LE, image base 0x100000. Has NO iAP module. |
| Donor | `Firmwares/HU/MHI2_US_AUG22_K3347_MU1439/extracted/app50/eso/bin/apps/btstack` | 1,912,417 B. Same Marvell 8787 over SDIO, same `PM_build_swdl` line, **identical Blue SDK core (31/31 `bluesdk/*.c` paths, zero delta)**. HAS the iAP module (`btstack::IapServices`). This is the reference. |
| Factory full | `Firmwares/HU/MH2p_US_AUG35_P2873/extracted/app/eso/bin/apps/btstack` | Different platform (Tegra, 8897/UART), but has iAP **and** `WirelessCarPlayServices` + `CarPlaySdpAttributes`. Only reference for the CarPlay half. |

Cross-binary function matching that has held perfectly so far: pick a string present in both
target and donor, take its function, confirm the two decompile line-for-line, then the callee
at the same call position is the same Blue SDK function. Anchors used: the DID record path
(`DevIdAddServiceRecord: DID SDP record succeeded`, both 0x51C) and the server-bind path
(`Fatal Error: Unable to bind the server to the protocol stack`, both 0x100).

---

## Blue SDK entry points, already mapped and verified line-for-line

| Blue SDK operation | Donor MU1439 | **Target MU1316** | Signature |
|---|---|---|---|
| SDP add record | 0x20BE50 | **0x2073C4** | `int f(record*)`, returns 18 for null/invalid record (validator `0x204C38` in target) |
| SDP remove record | 0x20BDD0 | **0x207344** | `int f(record*)` |
| RFCOMM register server channel | 0x24257C | **0x277B98** | `int f(chan*, u8* grantedChanOut, u16 prio)`; rejects unless MTU (u16 at chan+12) in [23,5035] |
| RFCOMM deregister server channel | 0x242518 | **0x277B34** | `int f(chan*, u8* chan)`; rejects unless `*chan-1 <= 5` |
| SDP record list head | dword_4A0BE8 | **dword_490288** | intrusive list |
| SDP record handle counter | dword_4A0BF0 | **dword_490290** | post-incremented into record+20 |
| stack lock / unlock | 0x2888D4 / 0x288848 | **0x27A2AC / 0x27A220** | held across both operations |

Record struct (Blue SDK): `+8` u8 attribute count, `+12` pointer to a 12-byte-per-entry attribute
array (entry `+8` is a u16 cache the add-function zeroes), `+16` flags masked `0xFFC000`, `+20`
assigned handle, `+24` from an internal call.

---

## The iAP SDP record bytes (verified in the donor image at file offset 0x1d1634)

Two attributes; the trailing RFCOMM channel byte is 0 in the image and is written at runtime once
RFCOMM grants a channel — do NOT hardcode it.

```
attr 0x0001 ServiceClassIDList     len 19  35 11 1C 00000000 decafade decadeaf decacaff
attr 0x0004 ProtocolDescriptorList len 14  35 0C 35 03 19 01 00 35 05 19 00 03 08 <channel>
```

Read: seq{ UUID128 } ; seq{ seq{UUID16 0x0100 L2CAP}, seq{UUID16 0x0003 RFCOMM, uint8 channel} }.

`wireless/verify_graft_points.py` re-checks all of the above straight from the two binaries; it
currently passes 8/8. Run it after any firmware re-extraction.

---

## Caller to imitate (all in the donor)

- `IapServices::registerService` @ 0x2379A4 — registers the RFCOMM channel first (0x24257C),
  patches the granted channel into the record's protocol descriptor, then adds the record
  (0x20BE50). Self-documents via `snprintf("[%2d] %s::%s",0,"IapServices","registerService")`,
  `IapServices.cxx` lines 796/808.
- `IapServices::deregisterService` @ 0x23773C — reverse; disconnects a live device first.
- `IapServices::init` @ 0x23B8A0 — **NOT radio work**; it publishes the `IapDeviceServices` ASI
  interface (UUID `59B50458-C282-42BD-BB46-167700 19A762`, = the unused stub lib
  `lib_59b50458-…so` already on our unit). A graft can SKIP ASI entirely and hand the RFCOMM
  payload to our own daemon over a socket (btstack links libsocket).
- `IapServices::queryAttributes` @ 0x237D90 — the phone-capability probe: SDP-searches the phone
  for the reverse UUID (`…decacafe`) with pattern `35 17 1C 00000000decafadedecadeafdecacafe
  19 01 00 19 00 03` + `00 64` + attrs `35 06 09 00 04 09 00 09`. Produces "Checking iAP2
  capability". A graft can skip it and accept whatever connects.

---

## THE TASK FOR YOU

**Recover the RFCOMM server-channel object layout from the donor**, so a graft can allocate and
initialise one, register it via 0x277B98 (target address), and receive inbound iAP2 bytes.

Specifically, from the donor `btstack` (open in IDA/Ghidra/radare2, your choice):

1. In `IapServices::registerService` @ 0x2379A4, the channel object is `instance+16` (i.e. an
   offset into the `IapServices` instance). Map every field of that channel object the register
   path (0x24257C -> 0x2081B4) reads or writes: the non-zero gate at `+8`, the MTU u16 at `+12`,
   `+96`, `+134/139/140`, and the receive/event callback pointer. Give offsets, widths, and what
   sets each one.
2. Find where the channel's inbound-data callback is installed and what its signature is — this
   is the seam where iAP2 bytes arrive. Trace one delivery from the RFCOMM layer to a handler.
3. Produce a C struct (or annotated offset table) for the channel object, plus the minimal init
   sequence `registerService` performs before calling 0x24257C.
4. Note anything allocator-related: is the channel embedded in the `IapServices` instance
   (allocated by `IapServices::init`'s `operator new(0x6C)`), or separately? A graft has no
   `IapServices` instance, so it must allocate the channel itself.

Deliver: an offset-annotated struct + the init recipe, written to
`wireless/rfcomm_channel_layout.md`, plus the target-binary addresses for any additional Blue SDK
callees you touch (matched by the anchor method above, then verified by line-for-line decompile).

Do NOT write to the head unit. Do NOT modify either binary yet. This step is pure static RE.

---

## After you (context, not your task)

Update after the channel analysis: `wireless/rfcomm_channel_layout.md` contains the
verified layout and inbound-data callback. The channel is embedded in the 0x190-byte
`IapServices` object; `operator new(0x6C)` in `init` allocates an ASI registration
object, not the channel. The earlier 5043 MTU limit was a transcription error:
`23 + 0x1394 = 5035`.
`wireless/graft_insertion_plan.md` proves SDP and RFCOMM core init precede the
candidate startup hook `0x11ECD0`, including when DID is disabled. The SDP
add-record status 18 is a record-validation failure, not an SDP-ready flag.
`wireless/rfcomm_duplex.md` maps the incoming-connection accept API, transmit
packet ABI, completion event, and receive-credit API into the target binary.
Unlike a receiving graft, donor `IapServices` rejects event-1 inbound requests.

**Later correction (2026-09-27):** the opening claim that Bluetooth is the
only missing piece is too narrow for a complete wireless mod. The stock PF
profiles block inbound CarPlay traffic to `uap0`; separate offline copies are
under `wireless/firewall/`. More significantly, the wireless daemon does not
trigger dio's `CAirPlayMainThread::run`, which creates the AirPlay TCP 7000
listener. The stock path is tied to Cinemo's wired iAP2 lifecycle. See
`wireless/receiver_start.md` before treating the graft as a complete mod.

Graft plan once the channel layout is known: find a code cave or append a PT_LOAD in the target
btstack; hook the epilogue of the DID-record registration (`sub_26C070` in target, runs after SDP
is up) as the init trigger; call 0x277B98 then 0x2073C4 with the record above; deliver RFCOMM
payload to a userspace daemon over a socket. Then: add the CarPlay UUID to the Bluetooth EIR
(reference: MH2p `WirelessCarPlayServices`), inject the Apple Device IE into the AP beacon
(`uaputl sys_cfg_custom_ie <idx> 0x12A <bytes>`, 8 free slots confirmed on the unit; corrected from 0x101 using the binary's bit map), point mdnsd
at `uap0`, open PF, and write the CarPlay Control client (browse `_carplay-ctrl._tcp`, GET
`/ctrl-int/1/connect`). iAP2 wire format + the Wi-Fi handover messages 0x5700-0x5703 are in
`Refferences/LIVI/native/livi-helperd/crates/iap2-csm` and the MFi spec r29 §30.2.5.

## Unit facts (car-verified, read-only over SSH 2026-09-27)

QNX 6.5.0 APQ8064 armle; one running btstack = the target bytes; no `/dev/iap*`; only
`libicuuc.so.58`; `/dev/i2c8` = MFi bus (dio uses `i2c:///dev/i2c8:17`). AP `uap0` hostap active,
channel 11, Band 2.4GHz, HT20, 8 empty custom-IE slots. Radio sees only 2.4 GHz (three scans; a
dual-band AP nearby showed only its 2.4 GHz BSS). 2.4 GHz is spec-legal for CarPlay; once the
CarPlay session starts the phone tells the accessory to drop HFP/A2DP/AVRCP, so 2.4 GHz coexistence
is a non-issue. Full write-up: `docs/connect-and-usb/WIRELESS_CARPLAY_HU_FEASIBILITY.md`.

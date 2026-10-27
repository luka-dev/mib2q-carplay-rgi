# CarPlay Bluetooth discovery: factory path and MHI2Q graft

_MHI2Q wireless CarPlay — technical reference, integrated 2026-09-29._

---

[Wireless overview](README.md) · [Current stock210 lifecycle](STOCK210_RECEIVER_LIFECYCLE.md)

Maintained technical reference. The evidence below distinguishes static analysis, local QNX tests and observations from the unit.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../wireless/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📊 Component flow

```mermaid
flowchart LR
    accTitle: Bluetooth discovery publication order
    accDescr: The graft publishes discovery only after RFCOMM, both SDP records and the worker are ready, then requests a fresh EIR build.
    rfcomm["Register RFCOMM channel"]
    sdp["Add iAP2 and CarPlay SDP"]
    worker["Start bridge worker"]
    ready["Publish discovery readiness"]
    eir["Refresh CarPlay UUID EIR"]
    rfcomm --> sdp --> worker --> ready --> eir
```

Static RE, 2026-09-27. All addresses below are ELF virtual addresses. Factory
reference: `MH2p_US_AUG35_P2873/extracted/app/eso/bin/apps/btstack` (ARM Thumb,
base zero). Target: MU1316 MHI2Q `btstack` identified in
[Rfcomm Channel Layout](RFCOMM_CHANNEL_LAYOUT.md) (ARM, base `0x100000`). No unit access was used.

<a id="factory-behaviour"></a>

## 📋 Factory behaviour

The exported `btstack::CarPlaySdpAttributes` at factory `0x1FA02C` is a single
12-byte attribute: ID `0x0001`, value length 19, value pointer `0x1FA018`.
Its value bytes are:

```text
35 11 1c ec 88 43 48 cd 41 40 a2 97 27 57 5d 50 bf 1f d3
```

This is `ServiceClassIDList` containing UUID
`ec884348-cd41-40a2-9727-575d50bf1fd3`. Factory
`WirelessCarPlayServices::init` (`0x58DB0`) sets its record count at object
`+0x14` to one and attribute pointer at `+0x18` to this array (`0x58DF8..0x58DFA`).
It does not expose an RFCOMM protocol descriptor: the iAP2 service is the byte
transport; this CarPlay record is a discovery marker.

Separately, `WirelessCarPlayServices::setConnectable` (`0x58F38`) calls the
factory `BluetoothServices` connectability setter (`0x45F40`), which calls
`updateEIR` (`0x45A14`). After the Blue SDK builds an inquiry response,
`injectCustomEIREntries` (`0x456AC`) inserts 16 bytes into the existing AD
type `0x07` (complete 128-bit service UUID list) when the CarPlay connectable
flag is set. The bytes copied at `0x45754..0x4575C` are the UUID in Bluetooth
least-significant-byte-first order:

```text
d3 1f bf 50 5d 57 27 97 a2 40 41 cd 48 43 88 ec
```

The factory injection occurs before `ME_SetExtInquiryRsp` (`0x45A76`). Its
space test is `current_length <= 223`; otherwise it logs that no 128-bit UUID
fits. The target has no CarPlay-specific injection, but its normal EIR refresh
is visible in Bluetooth event `0x41`: `sub_1DE134(buffer,240)` at `0x12A704`,
result in `r0/r2`, store to `BluetoothServices+0x40C` at `0x12A710`, and
`sub_1DD1E0(0,buffer,length)` at `0x12A718`. Target `sub_1DE134` delegates to
`sub_1DDEC8`, which zeroes the buffer then returns the exact number of AD bytes
written (the spare capacity is not counted).

The factory discovery parser provides independent evidence that EIR matters.
`BtJobDeviceDiscovery::btEvent` (`0x61C44`) scans AD type `0x07`, compares
128-bit entries to the CarPlay UUID at `0x1A730C`, and sets the wireless
CarPlay capability bit `0x1000` at `0x61FE4`. Its separate iAP2 UUID check
sets bit `0x800`. This is a discovery observation, not proof that a phone will
accept a particular advertisement.

<a id="offline-graft-now-built"></a>

## 📋 Offline graft now built

`wireless/graft/graft.c` registers a second SDP record containing that one
factory CarPlay attribute after registering the iAP2 RFCOMM service. Failure
removes the first record and channel. The extra target hook at `0x12A710`
branches to `graft_eir_hook`, calls `eir_append_carplay_uuid` on the freshly
built 240-byte EIR buffer, replays the displaced `STRB R0,[R6,#0x40C]`, then
resumes at `0x12A714` with the new length in `r0/r2`. The helper detects an
existing UUID, inserts into an existing valid type-7 list, or appends a new
type-7 AD field when space permits. It refuses malformed lists and never
writes beyond 240 bytes. Unlike the factory routine, it handles a missing
type-7 list because the MHI2Q builder may have a different set of services.
The production hook gates insertion on `discovery_ready`, which is published
only after RFCOMM, both SDP records and the worker have initialized. An EIR
event before readiness is deliberately left unchanged.

<a id="refresh-after-registration"></a>

## 📋 Refresh after registration

Successful SDP insertion itself queues a Blue SDK EIR refresh: donor
`SDP_ADD 0x20BE50` calls `0x1E00D4` at `0x20BF4C`; the identical target
`SDP_ADD 0x2073C4` calls `0x1DB648` at `0x2074C0`. Both wrappers lock the
stack, install a callback, set queue type 2, and enqueue the operation. The
callbacks are also identical: donor `0x1E033C` and target `0x1DB8B0` log
“Requesting an EIR update.” from the same `bluesdk/me/meif.c` line 5033,
set the EIR request byte to 65, and enqueue mask `0x40000000`.

The graft adds both SDP records before publishing `discovery_ready`, so either
of their refresh operations could execute too early for the EIR hook to
insert the CarPlay UUID. The production graft now calls target `0x1DB648`
once more immediately after publishing readiness. If the earlier requests
already ran, this last request schedules another build with the UUID; if they
are pending, the extra request uses the same factory queue mechanism. Whether
the real controller accepts and advertises the resulting EIR still requires
on-unit verification.

`wireless/graft/test_eir.py` exercises duplicate detection, malformed and
near-capacity buffers, and 1000 randomized valid layouts on the host; the
offline ELF builder verifies both hook branches and return literals. QNX
loader behaviour, the timing of EIR refresh, actual radio advertisement, and
phone interpretation remain unverified without a live or faithful emulated
Bluetooth environment.

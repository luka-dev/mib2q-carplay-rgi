# Offline insertion survey for the MHI2Q btstack (2026-09-27)

_MHI2Q wireless CarPlay — technical reference, integrated 2026-09-29._

---

[Wireless overview](README.md) · [Current stock210 lifecycle](STOCK210_RECEIVER_LIFECYCLE.md)

Maintained technical reference. The evidence below distinguishes static analysis, local QNX tests and observations from the unit.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../wireless/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📊 Component flow

```mermaid
flowchart LR
    accTitle: Offline graft image construction
    accDescr: The builder works on a copy, preserves displaced ARM instructions, and verifies the resulting load segments before QEMU.
    stock["Pinned stock btstack input"]
    copy["Build separate ELF copy"]
    segments["Add code and state segments"]
    hooks["Insert guarded ARM hooks"]
    verify["Verify bytes and load layout"]
    qemu["Test disposable QNX loader"]
    stock --> copy --> segments --> hooks --> verify --> qemu
```

This is a static survey of the target binary identified in
[Rfcomm Channel Layout](RFCOMM_CHANNEL_LAYOUT.md). No executable was patched and the head unit was not
accessed. The channel and SDP layouts are already documented there and in
[Codex Handoff](history/CODEX_HANDOFF.md).

<a id="startup-trigger"></a>

## 📋 Startup trigger

The originally proposed DID inner function `0x26C070` is not a universal startup
hook. Its sole caller is the DID wrapper `0x26C628`, which acquires the stack mutex
at `0x26C658`, invokes `0x26C070` at `0x26C674`, and unlocks at `0x26C690`.
That wrapper is called from the startup fragment at `0x11ED4C` only when a
configuration byte tested at `0x11ECB4..0x11ECBC` is nonzero. DID success proves
SDP was usable on that path, but DID can be skipped. An inserted call inside
`0x26C070` would also run before the outer stack lock is released. The existing
SDP add-record function acquires that mutex recursively, so this is not evidence
of an inevitable deadlock; it is simply a less isolated insertion point.

The startup fragment has a convergence point at **`0x11ECD0`**: the no-DID branch
falls through there, and the DID success/failure logging paths branch there.
The instruction there is `LDRB R3, [R5,#0x764]`, followed by `CMP R3,#0`.
This is a candidate for a one-time trampoline after the SDK init calls at
`0x11EC88..0x11ECB0` and after the optional DID wrapper returns. A trampoline
would have to execute the displaced load and resume at `0x11ECD4`, preserve ARM
calling convention and stack alignment, and guard initialization against repeat
entry. It must leave the host's subsequent Bluetooth startup flow intact.

The init ordering is now statically established even when DID is disabled.
`BluetoothServices::startBtStack` calls `OSAPI::initialize` via `0x27B310`
at `0x11EB20`, before entering the later fragment containing `0x11ECD0`.
`OSAPI::initialize` (`0x27A7E0`) calls event-manager init `0x1E5144`
at `0x27A888`. That calls `ME_Init` `0x1D6110` at `0x1E5324`.
`ME_Init` invokes SDP init `0x204860` (`0x1D627C`), which invokes
`0x2079B0` (`0x2048B4`): on success it registers the SDP L2CAP server,
initializes the SDP record-list head at `0x490288`, inserts the built-in
record, and sets the handle counter to `0x10000` (`0x207A6C..0x207BC4`).
Only after SDP init succeeds does `ME_Init` call RFCOMM init `0x2045EC`
(`0x1D6290`). That function registers its L2CAP protocol, initializes the
six service lists (`0x20465C..0x2046A4`), and installs the RFCOMM state
handlers. Failure returns an error through `ME_Init`/event-manager init.
The optional DID branch occurs later at `0x11ECB4`; therefore DID does
not gate SDK SDP/RFCOMM initialization. This certifies the **ordering** of
`0x11ECD0` for a successful stack startup. A one-time guard remains necessary,
and the eventual trampoline must preserve the surrounding startup state.

The SDP add-record wrapper `0x2073C4` returns 18 when its record pointer is
null or the record validator `0x204C38` rejects its attribute layout. That
check does not test an SDP-ready flag. The earlier handoff's description of
18 as “SDP not ready” was incorrect.

<a id="placement-constraints"></a>

## 🎯 Placement constraints

The ELF is ARM ET_EXEC with five program headers. The executable `PT_LOAD`
starts at file offset 0 / VA `0x100000` and has file and memory size `0x1B9978`.
The writable `PT_LOAD` starts at file offset `0x1BA000` / VA `0x2BA000`.
The `0x688`-byte intervening file gap is all zero, but it is **outside** the
executable segment's declared `p_filesz` and `p_memsz`. It becomes usable for
code only if those sizes are deliberately extended, without overlapping the
next segment. It should not be treated as an existing mapped code cave.

A scan of file-backed RX bytes found no plausible large blank cave: the longest
runs of at least 64 identical bytes are two 133-byte `0xFF` spans in read-only
data and one 75-byte zero span. Their surrounding references must be checked
before any use. The header has no free program-header slot: the current PHDR
table ends exactly where `.interp` begins (`0xD4`). Appending a new `PT_LOAD`
therefore requires relocating/expanding the program-header table and checking
QNX loader behavior offline. Extending the RX segment into its alignment gap
gives at most `0x688` bytes; it may fit a trampoline and small initialization
stub, but the socket bridge's size is not established.

Persistent writable state is also required: at least the `0xB0`-byte channel,
the 1-byte granted-channel storage, an SDP record and attribute array, an
idempotence flag, and transport state. The current RW load ends in BSS at
`0x507FB0`; extending its `p_memsz` is a possible allocation method, pending
loader and overlap checks. These are placement options, not edits made now.

<a id="required-runtime-behavior-and-failure-boundaries"></a>

## 📋 Required runtime behavior and failure boundaries

1. Initialize a persistent channel and a persistent granted-channel byte, then
   call target RFCOMM registration `0x277B98` (MTU 5035, priority 0).
2. On success, write the granted byte into the iAP SDP protocol descriptor and
   call target SDP add-record `0x2073C4`. On add failure, deregister the channel
   via `0x277B34`; do not leave an advertised or half-registered service.
3. Install a local callback with the two-pointer ABI and event layout in
   [Rfcomm Channel Layout](RFCOMM_CHANNEL_LAYOUT.md). On event 1 call the target accept API
   `0x2774BC(channel,1)`; on event 5 copy the payload before returning and
   replenish receive credits through `0x2772DC`. See [Rfcomm Duplex](RFCOMM_DUPLEX.md).
4. Send daemon-to-phone bytes through `0x277378` using an owned, zeroed
   `0x64`-byte packet and the payload limit from `0x2771F4`. Retain each
   packet and buffer until event 6 or disconnect. See [Rfcomm Duplex](RFCOMM_DUPLEX.md).
5. Keep any blocking socket operation outside the RFCOMM callback. The target
   imports `socket` (`0x117618`), `connect` (`0x117660`), `send` (`0x117318`),
   `recv` (`0x117BE8`), and `pthread_create` (`0x1173FC`), so a worker is
   possible without adding dynamic imports. The framing, queue limits, and
   disconnect/reconnect behavior still need to be specified and reviewed before
   implementation. A failed bridge must not block the car's HFP traffic.

The placement and bounded callback-to-daemon framing above are now implemented
in `graft/` as an **offline copy**, with an additional EIR hook and CarPlay SDP
record. See [Btstack Graft](BTSTACK_GRAFT.md) and [Bluetooth Discovery](BLUETOOTH_DISCOVERY.md). The original binary
and the unit remain untouched. The generated copy has since passed offline
QNX loader and synthetic registration/bridge probes; live-stack validation
remains. An iAP2/MFi consumer daemon now exists under `daemon/`, with a QEMU
bridge startup proof and a QNX I²C adapter, but no phone or actual MFi
coprocessor transaction has been tested.

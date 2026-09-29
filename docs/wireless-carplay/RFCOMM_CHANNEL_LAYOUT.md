# RFCOMM server channel for iAP2 (static RE, 2026-09-27)

_MHI2Q wireless CarPlay — technical reference, integrated 2026-09-29._

---

[Wireless overview](README.md) · [Current stock210 lifecycle](STOCK210_RECEIVER_LIFECYCLE.md)

Maintained technical reference. The evidence below distinguishes static analysis, local QNX tests and observations from the unit.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../wireless/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📊 Component flow

```mermaid
flowchart LR
    accTitle: RFCOMM server registration sequence
    accDescr: The channel and callback are initialized before server registration; the granted channel is written into SDP before publication.
    storage["Allocate stable channel storage"]
    initialize["Set callback and MTU"]
    register["Register RFCOMM server"]
    channel["Copy granted channel byte"]
    sdp["Publish iAP2 SDP record"]
    storage --> initialize --> register --> channel --> sdp
```

<a id="scope-and-binary-identity"></a>

## 🔍 Scope and binary identity

Addresses below are ARM virtual addresses (image base `0x100000`). The donor is
`Firmwares/HU/MHI2_US_AUG22_K3347_MU1439/extracted/app50/eso/bin/apps/btstack`
(SHA-256 `403990c1838bc939eaa532e2249925553274bd78127fa417f4fac426f643ae79`);
the target is
`Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/apps/btstack`
(SHA-256 `154c17826f0773a57b2004f5b17d818e3437c46fcbb6a857d40c696898c29bc5`).
Those paths are relative to `/Users/luka/Desktop/AUDI_2/`. The `wireless/verify_graft_points.py`
checks passed against both binaries (8/8). Neither binary nor the head unit was modified.

<a id="ownership-and-initialization-in-the-donor"></a>

## 🔄 Ownership and initialization in the donor

`BluetoothServices` allocates **`IapServices` itself with `operator new(0x190)`** at
`0x1274E8` and calls its constructor `0x2392F8` at `0x1274EC`. The RFCOMM channel is
embedded at **`IapServices + 0x10`**, occupying the `0xB0` bytes cleared at
`0x2393FC` (`instance+0x10` through `instance+0xBF`). The SDP record begins at
`instance+0xC0`, immediately after that region. The `operator new(0x6C)` at
`IapServices::init` `0x23BA04` allocates an ASI `IapDeviceServicesServiceRegistration`
object; it does **not** allocate the channel or the `IapServices` object.

Offsets in this table are relative to `channel = instance + 0x10`. Widths are for
32-bit ARM. “Registration” covers `0x24257C → 0x2081B4` and the list helpers it calls.

| Offset | Width | Field / observed use | Writer / evidence |
|---|---:|---|---|
| `+0x00` | 4 | intrusive list `next`; checked for identity while looking for an already registered channel | list insertion `0x1D1458` writes it; list search `0x1D14CC` traverses it |
| `+0x04` | 4 | intrusive list `prev` | list insertion `0x1D1458` writes it |
| `+0x08` | 4 | `callback(channel *, event *)`; **must be nonzero** for registration | constructor writes `cbRf` (`0x23D508`) at `0x23942C`; register gate at `0x2425B8`; dispatch at `0x208948/0x20895C` |
| `+0x0C` | 2 | requested RFCOMM MTU, constrained to **23..5035** | constructor writes `5035` at `0x239428`; gate/copy at `0x2425B8/0x2425CC` |
| `+0x0E` | 1 | explicitly cleared by constructor; no read found in registration path | `0x239418`; also in preceding zero fill |
| `+0x14` | 4 | opaque upper-layer context; donor callback reads this as its `IapServices *` | constructor writes `instance` at `0x239420`; `cbRf` reads `channel[5]` at `0x23D560`; **not read by registration** |
| `+0x60` | 2 | runtime state word, reset on registration | wrapper `0x2425E8` writes zero |
| `+0x86` | 2 | priority/config word; wrapper receives `u16 prio` | wrapper `0x2425D8` writes `prio` (donor iAP passes `0`) |
| `+0x8B` | 1 | low byte of `prio`, stored separately | wrapper `0x2425D0` writes it (donor iAP passes `0`); exact later meaning not established here |
| `+0x8C` | 2 | working MTU copy | wrapper `0x2425E0` copies `channel+0x0C` |

The constructor zeroes the entire `0xB0`-byte channel before writing `+0x0C`,
`+0x14`, and `+0x08`. The list helpers do not require preinitialized `next/prev`:
they search the global per-service list for the *channel address*, then fill both
links on insertion. A standalone graft should still zero the whole object before
registration, since later RFCOMM code uses the remaining state bytes. The wrapper
sets `+0x60`, `+0x86`, `+0x8B`, and `+0x8C`; the graft should not prefill those as
a substitute for calling the wrapper. No particular meaning beyond the observed
writes is claimed for `+0x60` or `+0x8B`.
The upper MTU limit follows directly from the unsigned check
`(uint16_t)(mtu - 23) <= 0x1394`: `23 + 0x1394 = 5035`.
The `5043` upper bound in the earlier handoff was a transcription error.

One useful partial declaration (the padding represents SDK-owned runtime state):

```c
typedef struct RfEvent RfEvent;
typedef void (*RfCallback)(void *channel, const RfEvent *event);

typedef struct RfServerChannel {
    void *next;                 /* +0x00 */
    void *prev;                 /* +0x04 */
    RfCallback callback;        /* +0x08 */
    uint16_t requested_mtu;    /* +0x0C */
    uint8_t  reserved_0e[6];   /* +0x0E .. +0x13 */
    void    *user_context;      /* +0x14 */
    uint8_t  sdk_state_18[0x48]; /* +0x18 .. +0x5F */
    uint16_t state_60;          /* +0x60 */
    uint8_t  sdk_state_62[0x24]; /* +0x62 .. +0x85 */
    uint16_t priority_86;       /* +0x86 */
    uint8_t  sdk_state_88[3];   /* +0x88 .. +0x8A */
    uint8_t  priority_low_8b;   /* +0x8B */
    uint16_t working_mtu;       /* +0x8C */
    uint8_t  sdk_state_8e[0x22]; /* +0x8E .. +0xAF */
} RfServerChannel;             /* sizeof == 0xB0, ARM 32-bit ABI */
```

The `0xB0` size describes the donor's cleared channel region, **not** a separately
observed SDK allocation size. A graft may allocate this region on its own; it does
not need an `IapServices` object if its own callback does not dereference the
donor's `user_context`. Do not reuse `cbRf` unchanged: it expects an actual
`IapServices *` at `+0x14` and calls `IapServices::rfEvent`.

<a id="minimal-registration-sequence"></a>

## 📋 Minimal registration sequence

The donor performs the first four actions in the **constructor**, before
`IapServices::registerService` (`0x2379A4`) is called. `registerService` itself
immediately calls the wrapper with `instance+0x10`, `instance+0x0C`, and priority 0
at `0x2379FC`; it does not allocate or initialize the channel then.

```c
RfServerChannel *ch = allocate_at_least_0xB0_bytes();
memset(ch, 0, 0xB0);
ch->requested_mtu = 5035;
ch->callback = graft_rf_callback;
ch->user_context = graft_context; /* optional to SDK; callback-defined */
static uint8_t granted_channel; /* persistent, separate storage; instance+0x0C in donor */
int status = target_rf_register(ch, &granted_channel, 0); /* 0x277B98 */
/* Only on success: put granted_channel into the SDP protocol descriptor,
   then call the existing SDP add-record entry point 0x2073C4. */
```

`granted_channel` must remain valid while the channel is registered: the inner
register function stores its pointer in the service slot at donor `0x208324`
(target `0x203898`), rather than just writing the byte once. Thus do **not** use
a transient stack local as the final graft's out-byte; put it in persistent graft
state. Donor `instance+0x0C` has that lifetime. Registration selects/validates a
server channel in `1..6`, rejects duplicates, links the channel into the service
list, and stores the out-byte pointer. The SDP descriptor's final byte is patched
from this granted value, not a fixed channel number.

<a id="inbound-delivery-and-event-abi"></a>

## 📦 Inbound delivery and event ABI

The channel has one callback for both events and data. In donor `rfcomm/rfc_sm.c`,
`RfDlcStateConnected` (`0x212C70`) receives a data frame event (`239` or `255`),
checks receive length against `channel+0x0C`, and calls RFCOMM's event builder
`0x2088D8` with event **5** at `0x212F38`. In `rfcomm/rfc_util.c`, that builder
extracts the packet payload, checks/decrements receive credits, assembles a
stack-local event, and invokes `channel->callback(channel, &event)` at
`0x20895C` (other event paths use the same pointer at `0x2089A0/0x208AC8`).
The callback's return register is not used as a data-delivery result by the iAP
handler; the ABI-relevant arguments are two pointers in `r0/r1`.

Observed event layout at this call site:

```c
struct RfEvent {
    uint8_t  code;          /* +0: 5 = received data */
    uint8_t  reserved_01;   /* +1 */
    uint16_t status;        /* +2: donor checks 17 for an unhandled packet */
    uint16_t detail;        /* +4: used by other event types */
    uint8_t  reason;        /* +6: other event types */
    uint8_t  reserved_07;   /* +7 */
    uint16_t length;        /* +8: data length for code 5 */
    uint8_t  reserved_0a[2];
    const uint8_t *data;    /* +12: received bytes for code 5 */
};
```

The iAP callback `cbRf` (`0x23D508`) checks its two arguments, loads the owner
pointer from `channel+0x14`, and calls `IapServices::rfEvent` (`0x23C030`).
Its case 5 checks `event+2` for 17, then copies `*(u16 *)(event+8)` bytes from
`*(u8 **)(event+12)` into an owned vector (`0x23C4A4..0x23C4D4`) before handing
it to the iAP receive queue (`0x23C52C`). A graft callback should copy or consume
the payload during the call; the event itself is built on the RFCOMM stack.
For accepting an inbound connection, sending data back, and replenishing
receive credits, see [Rfcomm Duplex](RFCOMM_DUPLEX.md). The donor's event-1 handler takes
the reject branch; a receiving graft must take the accept branch explicitly.

<a id="donor--target-blue-sdk-address-checks"></a>

## 🔍 Donor → target Blue SDK address checks

The wrapper is already anchored through the identical server-bind path; its
decompilation in both binaries has the same tests, field offsets, stores, lock,
inner call and return. The inner callee at that same call position was then
decompiled in both binaries: both select a slot, check the intrusive list,
insert the object, set the service flag and retain the granted-channel pointer.
The delivery builder was anchored independently by the shared string
`txCredit overflow`; the two decompilations have the same event switch,
data/credit handling, event layout, and callback calls. The connected-state
caller was anchored by `Invalid receive length, disconnecting...` and likewise
matches line for line. The list helpers are the callees at the same inner
registration call positions and their decompilations match line for line.

| Operation | Donor | Target |
|---|---:|---:|
| RFCOMM register wrapper | `0x24257C` | `0x277B98` |
| RFCOMM inner service registration | `0x2081B4` | `0x203728` |
| intrusive list contains | `0x1D14CC` | `0x1CCAC4` |
| intrusive list insert | `0x1D1458` | `0x1CCA50` |
| RFCOMM event builder / callback dispatch | `0x2088D8` | `0x203E4C` |
| connected-state data delivery | `0x212C70` | `0x20E1E4` |

The last four target addresses document the data path; a graft only needs to
call the public registration wrapper for the initialization shown above.

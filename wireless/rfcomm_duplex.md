# RFCOMM connection, transmit, and credit ABI (static RE, 2026-09-27)

This extends `rfcomm_channel_layout.md` for a *bidirectional* iAP2 bridge.
Addresses are ARM virtual addresses, donor MU1439 / target MU1316. The target
binary and the head unit were not changed.

## Incoming connection must be accepted

The SDK dispatches event code **1** to the channel callback for an incoming
RFCOMM connection request. `IapServices::rfEvent` in the donor calls
`sub_241EA0(channel, 0)` at `0x23C118`; decompilation of that callee shows
the zero branch sends control value **31** and transitions with argument 0.
The donor's own log says “Reject connection”. The nonzero branch sends
control value **115** and transitions with argument 1. Thus a graft that
expects the phone to connect inbound must call the **accept** branch
`target 0x2774BC(channel, 1)` on event 1. It should record event 2 as connected
and event 4 as disconnected. An event-1 callback that merely returns will
not complete the incoming connection handshake.

The target address was anchored by the shared HFP string
`HF: RFCOMM incoming hands-free connection`: donor `HfRfCallback` at
`0x25810C` and target at `0x248370` decompile with the same event switch
and call position. The donor HFP failure path calls `0x241EA0(channel,0)`;
the target calls `0x2774BC(channel,0)`. The two callees also decompile line
for line, including the nonzero accept branch.

## Sending bytes back to the phone

The donor's `/dev/iapDevice` write handler `0x23B290` queues an owned byte
vector. Its send scheduler `0x237CF4` calls `sub_241D5C(channel, packet)`
at `0x237D74`. This is the Blue SDK transmit API in the target at
**`0x277378(channel, packet)`**. It checks the link and packet length,
requires the channel's connection state at `+0x5C` to equal 1, puts the
packet in the channel's `+0x64` intrusive transmit list, and returns **2**
when queued. A later callback event **6** reports completion; the event's
pointer at `+0x0C` refers to the packet. The donor clears its pending-send
flag, releases the owned packet/buffer, and starts the next queued send in
`IapServices::rfEvent` case 6 (`0x23C200..0x23C318`). A graft must likewise
retain the packet *and its data buffer* until event 6 or disconnection.

The donor allocates `_BtPacket` with `operator new(0x64)`, clears all `0x64`
bytes, then writes `packet+0x08 = data pointer` and
`packet+0x0C = (uint16_t)length` (`0x23AF30..0x23AF5C`). The SDK fills
`packet+0x14 = channel` and its list links `+0x00/+0x04` when queued
(`0x241E54..0x241E58`). `packet+0x0E` is used as a transmission flag by the
HFP caller; the iAP caller leaves it zero. Other packet fields are SDK
state and should be initialized to zero, not guessed. An illustrative
minimum prefix is:

```c
typedef struct RfPacket {
    void *next;               /* +0x00, SDK writes on queue */
    void *prev;               /* +0x04, SDK writes on queue */
    uint8_t *data;            /* +0x08, graft writes */
    uint16_t length;          /* +0x0C, graft writes */
    uint16_t flags;           /* +0x0E, zero for donor iAP path */
    uint8_t sdk_state_10[4];  /* +0x10..0x13 */
    void *channel;            /* +0x14, SDK writes on queue */
    uint8_t sdk_state_18[0x4C]; /* through +0x63 */
} RfPacket;                  /* 0x64 bytes on ARM 32-bit */
```

Before constructing a packet, the donor HFP sender asks the SDK for the
current payload limit via `0x241BD8(channel)` / **target `0x2771F4`**.
Its matched decompilation returns zero unless connected, then uses the
peer MTU, subtracting one byte for credit-flow frames. Split longer daemon
writes accordingly, and serialize sends until completion. The send function
itself checks the remote MTU and rejects oversized packets with 18. The graft
worker now splits each daemon frame at the live limit and waits for event 6
before reusing the packet.

Transmit mapping used the HFP send caller: donor `0x2549C0` and target
`0x244BD8` have matching length/flag logic and call `0x241D5C` /
`0x277378` at the same position. The send functions and payload-limit
functions were then decompiled on both sides and match field for field.

## Receive credits

For event **5**, RFCOMM's event builder decrements the channel's receive
credit at `+0x8A` (`0x208B4C` donor / `0x2040C0` target) before invoking the
callback. The donor HFP callback calls `sub_241CC0(channel, 1)` per received
data event; the target's matched function is **`0x2772DC(channel, count)`**.
This function checks the connected state, adds `count` to pending credits at
`channel+0x86` if the total remains at most 255, and schedules an update.
The donor iAP path replenishes in batches when its receive-credit counter
falls below a threshold (`0x23C578..0x23C59C`). A graft must replenish
credits or inbound iAP2 transfer eventually stalls. The simple HFP policy
of one credit per delivered frame is a useful initial reference; queue
backpressure must be considered before using it in a daemon bridge.

The credit helper mapping is anchored by the same HFP event-5 call:
donor `HfRfCallback` calls `0x241CC0` at `0x258370`, and target calls
`0x2772DC` at `0x2485D4`. Both helper decompilations match line for line.

| Blue SDK operation | Donor | Target |
|---|---:|---:|
| accept/reject incoming connection | `0x241EA0` | `0x2774BC` |
| current payload limit | `0x241BD8` | `0x2771F4` |
| replenish receive credits | `0x241CC0` | `0x2772DC` |
| queue transmit packet | `0x241D5C` | `0x277378` |

The queue-to-daemon transport and its bounds are still an implementation
decision. The Bluetooth callback must do bounded work; blocking socket I/O
there would affect the car's other Bluetooth services.
